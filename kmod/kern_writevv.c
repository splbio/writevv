/*-
 * Copyright (c) 2011 Research Engineering Development, Inc.
 * Author: Alfred Perlstein <alfred@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/signalvar.h>
#include <sys/sysctl.h>
#include <sys/socketvar.h>
#include <sys/syscall.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/proc.h>
#include <sys/fcntl.h>

#include <sys/file.h>
#include <sys/filio.h>

#if __FreeBSD_version > 900000
#include <sys/capability.h>
#endif
#include <sys/syscallsubr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/protosw.h>

#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/uio.h>

#include <net/vnet.h>

#include <machine/stdarg.h>


#include "writevv.h"

#ifndef CURVNET_SET
#define CURVNET_SET(x)
#define CURVNET_RESTORE()
#endif

#ifndef SBL_WAIT
#define SBL_WAIT M_WAITOK
#endif


struct kern_writevv_args {
	struct writevv_args *wargs;
};
static int sys_writevv(struct thread *td, struct kern_writevv_args *karg);
static int writevv_internal_user(struct thread *td,
    const int *user_fds, int fdcnt,
    struct uio *auio, struct mbuf *m,
    size_t *user_returns, int *user_errors);

struct sysent writevv_sysent = {
	.sy_narg = sizeof(void *) / sizeof(register_t),
	.sy_call = (sy_call_t *)sys_writevv,
};
static int writevv_syscall = NO_SYSCALL;

static int writevv_debug = 0;
static int writevv_calls = 0;

struct sysent oldsysent;

SYSCTL_NODE(_debug, OID_AUTO, writevv, CTLFLAG_RD, 0, "writevv");
SYSCTL_INT(_debug_writevv, OID_AUTO, syscall, CTLFLAG_RD,
    &writevv_syscall, NO_SYSCALL, "syscall number for writevv");

SYSCALL_MODULE(writevv, &writevv_syscall, &writevv_sysent, NULL, NULL);

SYSCTL_INT(_debug_writevv, OID_AUTO, debug, CTLFLAG_RW, &writevv_debug,
    0, "debug output level"); 
SYSCTL_INT(_debug_writevv, OID_AUTO, calls, CTLFLAG_RW, &writevv_calls,
    0, "calls"); 

static void dbg(int level, const char *fmt, ...) __printflike(2, 3);

static void
dbg(int level, const char *fmt, ...)
{
        va_list ap;

	if (writevv_debug < level)
		return;

        va_start(ap, fmt);
        (void)vprintf(fmt, ap);
        va_end(ap);
}


static int
sys_writevv(struct thread *td, struct kern_writevv_args *karg)
{
    struct writevv_args *argp = karg->wargs;
    struct writevv_args arg;
    struct uio *auio, *temp_uio;
    struct mbuf *m;
    int error;

    writevv_calls++;
	
    auio = NULL;
    m = NULL;

    dbg(1, "user arg struct @ %p\n", argp);
    error = copyin(argp, &arg, sizeof(arg));
    if (error) {
            dbg(1, "copyin() failed: %d\n", error);
	    return error;
    }
    error = copyinuio(__DECONST(struct iovec *, arg.iov), arg.iovcnt, &auio);
    if (error) {
            dbg(1, "copyinuio() failed: %d\n", error);
	    return error;
    }
    auio->uio_rw = UIO_WRITE;
    auio->uio_td = td;
    auio->uio_offset = (off_t)-1;
    /* m_uiotombuf consumes the uio and iovs, so pass a temp copy */
    temp_uio = cloneuio(auio);

#if __FreeBSD_version >= 801000
    m = m_uiotombuf(temp_uio, M_WAIT, 0, 0, 0);
#else
    m = m_uiotombuf(temp_uio, M_WAIT, 0, 0);
#endif
    free(temp_uio, M_IOV);
    if (m == NULL) {
            dbg(1, "m_uiotombuf() failed NULL\n");
	    error = ENOBUFS;
	    goto out;
    }

    error = writevv_internal_user(td, arg.fds, arg.fdcnt, auio, m,
	arg.returns, arg.errors);
out:
    m_freem(m);
    free(auio, M_IOV);
    return (error);
}

static int
writevv_kernel(struct thread *td, int fd, struct uio *uio,
    struct mbuf *m)
{
        struct file *fp;
	struct socket *so;
	int error;
	struct mbuf *m2;
	int sblocked;
	int space;

	so = NULL;
	sblocked = 0;
	m2 = NULL;

	/* refcount the struct file. */
#if __FreeBSD_version > 900000
	error = fget_write(td, fd, CAP_WRITE | CAP_SEEK, &fp);
#else
	error = fget_write(td, fd, &fp);
#endif
	if (error)
		return error;

	/* Handle non-sockets by just calling kernel write routine. */
	if (fp->f_type != DTYPE_SOCKET) {
		struct uio *temp_uio;
		
		/* make a copy of the uio so that kern_writev does not change
		 * our caller's uio
		 */
		temp_uio = cloneuio(uio);
		error = kern_writev(td, fd, temp_uio);
		free(temp_uio, M_IOV);
		goto out;
	}

	/* copy our mbuf chain. */
	m2 = m_copym(m, 0, M_COPYALL, M_WAITOK);
	if (m2 == NULL) {
		error = ENOBUFS;
		goto out;
	}

	so = (struct socket *)fp->f_data;
	error = sblock(&so->so_snd, SBL_WAIT);
	if (error)
		goto out;
	sblocked = 1;

	SOCKBUF_LOCK(&so->so_snd);
	if (so->so_snd.sb_lowat < so->so_snd.sb_hiwat / 2)
		so->so_snd.sb_lowat = so->so_snd.sb_hiwat / 2;
retry_space:
	if (so->so_snd.sb_state & SBS_CANTSENDMORE) {
		error = EPIPE;
		SOCKBUF_UNLOCK(&so->so_snd);
		goto out;
	} else if (so->so_error) {
		error = so->so_error;
		so->so_error = 0;
		SOCKBUF_UNLOCK(&so->so_snd);
		goto out;
	}
	space = sbspace(&so->so_snd);
	if (space < uio->uio_resid &&
	    (space <= 0 ||
	     space < so->so_snd.sb_lowat)) {
		if (so->so_state & SS_NBIO) {
			SOCKBUF_UNLOCK(&so->so_snd);
			error = EAGAIN;
			goto out;
		}
		/*
		 * sbwait drops the lock while sleeping.
		 * When we loop back to retry_space the
		 * state may have changed and we retest
		 * for it.
		 */
		error = sbwait(&so->so_snd);
		/*
		 * An error from sbwait usually indicates that we've
		 * been interrupted by a signal. If we've sent anything
		 * then return bytes sent, otherwise return the error.
		 */
		if (error) {
			SOCKBUF_UNLOCK(&so->so_snd);
			goto out;
		}
		goto retry_space;
	}
	SOCKBUF_UNLOCK(&so->so_snd);
	CURVNET_SET(so->so_vnet);
	error = (*so->so_proto->pr_usrreqs->pru_send)
	    (so, 0, m2, NULL, NULL, td);
	m2 = NULL; /* it's consumed by the pru_send routine */
	td->td_retval[0] = uio->uio_resid;
	CURVNET_RESTORE();
	if (error == EPIPE && (so->so_options & SO_NOSIGPIPE) == 0) {
		PROC_LOCK(uio->uio_td->td_proc);
#if __FreeBSD_version > 900000
		tdsignal(uio->uio_td, SIGPIPE);
#else
		psignal(uio->uio_td->td_proc, SIGPIPE);
#endif
		PROC_UNLOCK(uio->uio_td->td_proc);
	}
out:
	if (so && sblocked) {
		sbunlock(&so->so_snd);
	}
	m_freem(m2);
	fdrop(fp, td);
	return (error);
}


/* this is on the stack so don't make it too high otherwise we'll
 * blow out the kernel stack and crash in very hard to track down
 * ways.
 * at 128 we're already eating 128 * sizeof(int) * 3 bytes of stack!
 */
#define BATCH_SIZE  128

static int
writevv_internal_user(struct thread *td,
    const int *user_fds, int fdcnt, struct uio *auio,
    struct mbuf *m, size_t *user_returns, int *user_errors)
{
    int fdoffset, toprocess;
    int error;
    int fds[BATCH_SIZE];
    int errors[BATCH_SIZE];
    size_t returns[BATCH_SIZE];
    int i;

    fdoffset = 0;
    error = 0;

    while (fdoffset < fdcnt) {
	    toprocess = min(BATCH_SIZE, fdcnt - fdoffset);
	    error = copyin(user_fds + fdoffset, fds, toprocess * sizeof(*fds));
	    if (error)
		    goto out;
	    for (i = 0; i < toprocess; i++) {
		    error = errors[i] = writevv_kernel(td, fds[i], auio, m);
		    returns[i] = td->td_retval[0];
		    dbg(3, "writevv_kernel: data sent: %lu\n",
			(unsigned long)returns[i]);
#if __FreeBSD_version > 900000
		    maybe_yield();
#endif
	    }
	    error = copyout(errors, user_errors + fdoffset,
		toprocess * sizeof(*errors));
	    if (error) {
                    dbg(1, "writevv_internal_user: copyout failed (errors). uaddr %p, len %d, error %d\n",
                        user_errors, (int)(toprocess * sizeof(*errors)), error);
		    goto out;
            }
	    error = copyout(returns, user_returns + fdoffset,
		toprocess * sizeof(*returns));
	    if (error) {
                    dbg(1, "writevv_internal_user: copyout failed (returns). uaddr %p, len %d, error %d\n",
                        user_returns, (int)(toprocess * sizeof(*returns)), error);
		    goto out;
            }
	    fdoffset += BATCH_SIZE;
    }
out:
    dbg(1, "writevv_internal_user: error: %d\n", error);
    return (error);
}

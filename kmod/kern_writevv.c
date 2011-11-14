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

#include <sys/capability.h>
#include <sys/syscallsubr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/protosw.h>

#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/vnet.h>


#include <sys/uio.h>

#include "writevv.h"


/*static int writevv_mod_event(module_t mod, int event, void *data);*/
static int sys_writevv(struct thread *td, void *argp);
static int writevv_internal_user(struct thread *td,
    const int *user_fds, int fdcnt,
    struct uio *auio, struct mbuf *m, size_t *user_returns, int *user_errors);

#if 0
static moduledata_t writevv_mod = {
	"writevv",
	writevv_mod_event,
	NULL
};

DECLARE_MODULE(writevv, writevv_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
#endif



struct sysent writevv_sysent = {
	.sy_narg = sizeof(void *) / sizeof(register_t),
	.sy_call = sys_writevv,
};
static int writevv_syscall = NO_SYSCALL;

struct sysent oldsysent;

SYSCTL_NODE(_kern, OID_AUTO, syscall, CTLFLAG_RD, 0, "added syscalls");
SYSCTL_INT(_kern_syscall, OID_AUTO, writevv, CTLFLAG_RD,
    &writevv_syscall, NO_SYSCALL, "syscall number for writevv");

SYSCALL_MODULE(writevv, &writevv_syscall, &writevv_sysent, NULL, NULL);

#if 0
static int
writevv_mod_event(module_t mod, int event, void *data)
{
        int error;

	error = 0;

        switch (event) {
        case MOD_LOAD:
/*		error = syscall_register(&writevv_syscall, &writevv_sysent,
		    &oldsysent);
		    */
		break;

        case MOD_UNLOAD:
		/*error = syscall_deregister(&writevv_syscall, &oldsysent); */
                break;

        case MOD_SHUTDOWN:
                break;

        default:
                error = EOPNOTSUPP;
                break;
        }

        return (error);
}
#endif

static int
sys_writevv(struct thread *td, void *argp)
{
    struct writevv_args arg;
    struct uio *auio;
    struct mbuf *m;
    int error;

    auio = NULL;
    m = NULL;

    error = copyin(argp, &arg, sizeof(arg));
    if (error)
	    return error;

    error = copyinuio(arg.iov, arg.iovcnt, &auio);
    if (error)
	    return error;

    m = m_uiotombuf(auio, M_WAIT, 0, 0, 0);
    if (m == NULL) {
	    error = ENOBUFS;
	    goto out;
    }

    error = writevv_internal_user(td, arg.fds, arg.fdcnt, auio, m,
	arg.returns, arg.errors);
out:
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

	/* refcount the struct file. */
	error = fget_write(td, fd, CAP_WRITE | CAP_SEEK, &fp);
	if (error)
		return error;

	/* Handle non-sockets by just calling kernel write routine. */
	if (fp->f_type != DTYPE_SOCKET) {
		error = kern_writev(td, fd, uio);
		goto out;
	}

	/* copy our mbuf chain. */
	m2 = m_copym(m, 0, M_COPYALL, M_WAITOK);
	if (m2 == NULL)
		goto out;

	so = (struct socket *)fp->f_data;
	SOCKBUF_LOCK(&so->so_snd);
	if (so->so_snd.sb_state & SBS_CANTSENDMORE) {
		error = EPIPE;
		SOCKBUF_UNLOCK(&so->so_snd);
		goto out;
	}
	SOCKBUF_UNLOCK(&so->so_snd);
	CURVNET_SET(so->so_vnet);
	error = (*so->so_proto->pr_usrreqs->pru_send)
	    (so, 0, m, NULL, NULL, td);
	CURVNET_RESTORE();
        if (error == EPIPE && (so->so_options & SO_NOSIGPIPE) == 0) {
                PROC_LOCK(uio->uio_td->td_proc);
                tdsignal(uio->uio_td, SIGPIPE);
                PROC_UNLOCK(uio->uio_td->td_proc);
        }
out:
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
    int returns[BATCH_SIZE];
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
	    }
	    error = copyout(errors, user_errors,
		toprocess * sizeof(*errors));
	    if (error)
		    goto out;
	    error = copyout(returns, user_returns,
		toprocess * sizeof(*returns));
	    if (error)
		    goto out;
	    fdoffset += BATCH_SIZE;
    }
out:
    return (error);
}

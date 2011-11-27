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
#include <sys/uio.h>

#include <net/vnet.h>

#include <machine/stdarg.h>

#include "rcvsocknull.h"

struct kern_getrcvsocknull_args {
	struct getrcvsocknull_args *wargs;
};
struct kern_setrcvsocknull_args {
	struct setrcvsocknull_args *wargs;
};
static int sys_getrcvsocknull(struct thread *td, struct kern_getrcvsocknull_args *karg);
static int sys_setrcvsocknull(struct thread *td, struct kern_setrcvsocknull_args *karg);

struct sysent getrcvsocknull_sysent = {
	.sy_narg = sizeof(void *) / sizeof(register_t),
	.sy_call = (sy_call_t *)sys_getrcvsocknull,
};
static int getrcvsocknull_syscall = NO_SYSCALL;
SYSCTL_NODE(_debug, OID_AUTO, getrcvsocknull, CTLFLAG_RD, 0, "added syscalls");
SYSCTL_INT(_debug_getrcvsocknull, OID_AUTO, syscall, CTLFLAG_RD,
    &getrcvsocknull_syscall, NO_SYSCALL, "syscall number for getrcvsocknull");
SYSCALL_MODULE(getrcvsocknull, &getrcvsocknull_syscall, &getrcvsocknull_sysent, NULL, NULL);

struct sysent setrcvsocknull_sysent = {
	.sy_narg = sizeof(void *) / sizeof(register_t),
	.sy_call = (sy_call_t *)sys_setrcvsocknull,
};
static int setrcvsocknull_syscall = NO_SYSCALL;
SYSCTL_NODE(_debug, OID_AUTO, setrcvsocknull, CTLFLAG_RD, 0, "added syscalls");
SYSCTL_INT(_debug_setrcvsocknull, OID_AUTO, syscall, CTLFLAG_RD,
    &setrcvsocknull_syscall, NO_SYSCALL, "syscall number for setrcvsocknull");
SYSCALL_MODULE(setrcvsocknull, &setrcvsocknull_syscall, &setrcvsocknull_sysent, NULL, NULL);

static int rcvsocknull_debug = 0;
/*
SYSCTL_NODE(_debug, OID_AUTO, writevv, CTLFLAG_RD, 0, "writevv");
SYSCTL_INT(_debug_writevv, OID_AUTO, debug, CTLFLAG_RW, &writevv_debug,
    0, "debug output level"); 
SYSCTL_INT(_debug_writevv, OID_AUTO, calls, CTLFLAG_RW, &writevv_calls,
    0, "calls"); 
    */

static void dbg(int level, const char *fmt, ...) __printflike(2, 3);

static void
dbg(int level, const char *fmt, ...)
{
        va_list ap;

	if (rcvsocknull_debug < level)
		return;

        va_start(ap, fmt);
        (void)vprintf(fmt, ap);
        va_end(ap);
}

static int so_discard_rcv_calls;

static int
so_discard_rcv(struct socket *so, void *arg, int waitflag)
{
       struct sockbuf *sb;

       so_discard_rcv_calls++;
       sb = &so->so_rcv;
       SOCKBUF_LOCK_ASSERT(sb);
       sbflush_locked(sb);
       return (SU_OK);
}

static int
sys_setrcvsocknull(struct thread *td, struct kern_setrcvsocknull_args *karg)
{
    struct setrcvsocknull_args *argp = karg->wargs;
    struct setrcvsocknull_args arg;
    struct file *fp;
    struct socket *so;
    struct sockbuf *sb;
    int error;

    dbg(1, "user arg struct @ %p\n", argp);
    error = copyin(argp, &arg, sizeof(arg));
    if (error) {
            dbg(1, "copyin() failed: %d\n", error);
	    return error;
    }

    error = fget(td, arg.fd, CAP_SETSOCKOPT, &fp);
    if (error)
	    return (error);

    if (fp->f_type != DTYPE_SOCKET) {
	    error = ENOTSOCK;
	    goto drop;
    }
    so = fp->f_data;
    sb = &so->so_rcv;
    SOCKBUF_LOCK(&so->so_rcv);
    if (arg.onoff == 1) {
	    if (sb->sb_upcall != NULL) {
		    error = EBUSY;
	    } else {
		    soupcall_set(so, SO_RCV, &so_discard_rcv, NULL);
		    (void)so_discard_rcv(so, NULL, M_NOWAIT);
	    }
    } else if (arg.onoff == 0) {
	    if (sb->sb_upcall == so_discard_rcv)
		    soupcall_clear(so, SO_RCV);
	    else
		    error = EINVAL;
    } else {
	    error = ENOPROTOOPT;
    }
    SOCKBUF_UNLOCK(&so->so_rcv);
drop:
    fdrop(fp, td);
    return (error);
}

static int
sys_getrcvsocknull(struct thread *td, struct kern_getrcvsocknull_args *karg)
{
    struct getrcvsocknull_args *argp = karg->wargs;
    struct getrcvsocknull_args arg;
    struct file *fp;
    struct socket *so;
    struct sockbuf *sb;
    int out;
    int error;

    dbg(1, "user arg struct @ %p\n", argp);
    error = copyin(argp, &arg, sizeof(arg));
    if (error) {
            dbg(1, "copyin() failed: %d\n", error);
	    return error;
    }

    error = fget(td, arg.fd, CAP_GETSOCKOPT, &fp);
    if (error)
	    return (error);

    if (fp->f_type != DTYPE_SOCKET) {
	    error = ENOTSOCK;
	    goto drop;
    }
    so = fp->f_data;
    sb = &so->so_rcv;
    out = (sb->sb_upcall == so_discard_rcv) ? 1 : 0;
    error = copyout(&out, arg.onoff, sizeof out);
drop:
    fdrop(fp, td);
    return (error);
}

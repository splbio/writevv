/*-
 * Copyright (c) 2000 Paycounter, Inc.
 * Author: Alfred Perlstein <alfred@paycounter.com>, <alfred@FreeBSD.org>
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

static moduledata_t writevv_mod = {
	"writevv",
	writevv_mod_event,
	&writevv_filter
};

DECLARE_MODULE(writevv, writevv_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);


struct sysent writevv_sysent = {
	.sy_narg = sizeof(void *) / sizeof(register_t);
	.sy_call = writevv;
}
static int writevv_syscall = NO_SYSCALL;

struct sysent oldsysent;

static SYSCTL_NODE(_kern, OID_AUTO, syscall, CTLFLAG_RD, 0, "added syscalls");
static SYSCTL_INT(_kern_syscall, OID_AUTO, writevv, CTLFLAG_RD,
    &writevv_syscall, NO_SYSCALL, "syscall number for writevv");

int
writevv_mod_event(module_t mod, int event, void *data)
{
        int error;

        switch (event) {
        case MOD_LOAD:
		error = syscall_register(&writevv_syscall, &writevv_sysent,
		    &oldsysent);

        case MOD_UNLOAD:
		error = syscall_deregister(&writevv_syscall, &oldsysent);
                break;

        case MOD_SHUTDOWN:
                error = 0;
                break;

        default:
                error = EOPNOTSUPP;
                break;
        }

        return (error);
}

static int
writevv(struct writevv_args *argp)
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

    error = copyinuio(uap->iovp, uap->iovcnt, &auio);
    if (error)
	    return error;

    m = m_uiotombuf(auio, M_WAIT, 0, 0);
    if (m == NULL) {
	    error = ENOBUFS;
	    goto out;
    }


    error = writevv_internal_user(arg.fds, arg.fdcnt, auio, m,
	arg.returns, arg.errors);
out:
    free(auio, M_IOV);
    return (error);
}

/* this is on the stack so don't make it too high otherwise we'll
 * blow out the kernel stack and crash in very hard to track down
 * ways.
 * at 128 we're already eating 128 * sizeof(int) * 3 bytes of stack!
 */
#define BATCH_SIZE  128

static int
writevv_internal_user(const int *user_fds, int fdcnt, struct uio *auio,
    struct mbuf *m, size_t *user_returns, int *user_errors)
{
    int fdoffset = 0;
    int error;
    int fds[BATCH_SIZE];
    int errors[BATCH_SIZE];
    int returns[BATCH_SIZE];


    while (fdoffset < fdcnt) {
	    error = copyin(user_fds + fdoffset, fds, BATCH_SIZE);
	    for (i = 0; i < BATCH_SIZE; i++) {
		    error = returns[i] = writevv_kernel(fds[i], auio, m);

	    }





    }





}




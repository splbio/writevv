#include <sys/syscall.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>

#include <strings.h>
#include <errno.h>



#define _LIBWRITEVV 1

#include "syscallhelper.h"
#include "writevv.h"

static SYSCALLVAR writevv_syscall = SYSCALLVAR_INITIALIZER;

static int writevv_mode = WRITEVV_MODE_KERNEL;

int
writevv_control(int op, int *val)
{
	int oldmode;

	switch (op) {
	case WRITEVV_GETMODE:
		*val = writevv_mode;
		return 0;
	case WRITEVV_SETMODE:
		oldmode = writevv_mode;
		if (*val != WRITEVV_MODE_KERNEL && *val != WRITEVV_MODE_USER) {
			errno = EINVAL;
			return -1;
		}
		writevv_mode = *val;
		return 0;
	default:
		errno = EINVAL;
		return -1;
	}
}

int
writevv(const int *fds, int fdcnt, const struct iovec *iov, int iovcnt,
    size_t *returns, int *errors)
{
	if (writevv_mode == WRITEVV_MODE_KERNEL)
	    return writevv_kernel(fds, fdcnt, iov, iovcnt, returns, errors);
	if (writevv_mode == WRITEVV_MODE_USER) 
	    return writevv_userland(fds, fdcnt, iov, iovcnt, returns, errors);
	abort();
}

int
writevv_kernel(const int *fds, int fdcnt, const struct iovec *iov, int iovcnt,
    size_t *returns, int *errors)
{
	struct writevv_args args;
	int error;

	error = initsyscallvar("writevv", &writevv_syscall);
	if (error != 0)
		return error;

	bzero(&args, sizeof(args));
	args.version = WRITEVV_VERSION;
	args.fds = fds;
	args.fdcnt = fdcnt;
	args.iov = iov;
	args.iovcnt = iovcnt;
	args.returns = returns;
	args.errors = errors;
	return __syscall(getsyscallnum(&writevv_syscall), &args);
}

int
writevv_userland(const int *fds, int fdcnt, const struct iovec *iov, int iovcnt,
    size_t *returns, int *errors)
{
	int i, error;
	int totok;
	size_t towrite = 0;

	totok = 0;

	for (i = 0; i < iovcnt; i++)
		towrite += iov[i].iov_len;

	for (i = 0; i < fdcnt; i++) {
		error = writev(fds[i], iov, iovcnt);
		if (error == -1 && errno == EFAULT) {
			return -1;
		}
		if (error == -1) {
			returns[i] = error;
			errors[i] = errno;
		} else {
			returns[i] = error;
			errors[i] = 0;
			if (returns[i] == towrite)
				totok++;
		}
	}
	return totok;
}

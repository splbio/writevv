#include <sys/syscall.h>
#include <unistd.h>


#include <sys/types.h>
#include <sys/sysctl.h>

#include <stdio.h>
#include <stdlib.h>

#include <strings.h>



#define _LIBWRITEVV 1

#include "writevv.h"



static int
getsyscallbyname(const char *syscallname, int *syscallnum)
{
	int error;
	size_t numsize;
	char *mibname;

	numsize = sizeof(*syscallnum);

	error = asprintf(&mibname, "kern.syscall.%s", syscallname);
	if (error == -1)
		return -1;
	error = sysctlbyname(mibname, syscallnum, &numsize, NULL, 0);
	free(mibname);
	return (error);
}

#define NO_SYSCALL (-1)
#define SYSCALLVAR int
#define SYSCALLVAR_INITIALIZER	NO_SYSCALL

static int
initsyscallvar(const char *syscallname, SYSCALLVAR *syscallnum)
{
	int newsyscall, error;

	if (*syscallnum == NO_SYSCALL) {
		error = getsyscallbyname(syscallname, &newsyscall);
		if (error == -1)
			return error;
		*syscallnum= newsyscall;
	}
	return 0;
}

static int
getsyscallnum(SYSCALLVAR *syscallnum)
{
	
	return *syscallnum;
}
	

static SYSCALLVAR writevv_syscall = SYSCALLVAR_INITIALIZER;

int
writevv(const int *fds, int fdcnt, const struct iovec *iov, int iovcnt,
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
	return syscall(getsyscallnum(&writevv_syscall), args);
}

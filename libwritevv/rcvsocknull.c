
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

#include "syscallhelper.h"
#define _LIBRCVSOCKNUL
#include "rcvsocknull.h"

static SYSCALLVAR setrcvsocknull_syscall = SYSCALLVAR_INITIALIZER;
static SYSCALLVAR getrcvsocknull_syscall = SYSCALLVAR_INITIALIZER;

int
setrcvsocknull(int fd, int onoff)
{
	struct setrcvsocknull_args args;
	int error;

	error = initsyscallvar("setrcvsocknull", &setrcvsocknull_syscall);
	if (error != 0)
		return error;

	bzero(&args, sizeof(args));
	args.version = 0;
	args.fd = fd;
	args.onoff = onoff;
	return __syscall(getsyscallnum(&setrcvsocknull_syscall), &args);
}

int
getrcvsocknull(int fd, int *onoff)
{
	struct getrcvsocknull_args args;
	int error;

	error = initsyscallvar("getrcvsocknull", &getrcvsocknull_syscall);
	if (error != 0)
		return error;

	bzero(&args, sizeof(args));
	args.version = 0;
	args.fd = fd;
	args.onoff = onoff;
	return __syscall(getsyscallnum(&getrcvsocknull_syscall), &args);
}

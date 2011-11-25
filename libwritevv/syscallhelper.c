#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include "syscallhelper.h"

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

int
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

int
getsyscallnum(SYSCALLVAR *syscallnum)
{
	
	return *syscallnum;
}

#ifndef SYSCALLHELPER_H
#define SYSCALLHELPER_H

#include <sys/syscall.h>

#define NO_SYSCALL (-1)
#define SYSCALLVAR int
#define SYSCALLVAR_INITIALIZER	NO_SYSCALL

int initsyscallvar(const char *syscallname, SYSCALLVAR *syscallnum);
int getsyscallnum(SYSCALLVAR *syscallnum);

#endif


#ifndef RCVSOCKNULL_H
#define RCVSOCKNULL_H

#if defined(_KERNEL) || defined(_LIBRCVSOCKNUL)
struct setrcvsocknull_args {
	int version;
	int fd;
	int onoff;
};
struct getrcvsocknull_args {
	int version;
	int fd;
	int *onoff;
};
#define RCVSOCKNUL_VERSION	0
#endif

#ifndef _KERNEL
int setrcvsocknull(int fds, int onoff);
int getrcvsocknull(int fds, int *onoff);
#endif

#endif

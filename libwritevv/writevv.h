#ifndef WRITEVV_H
#define WRITEVV_H

#if defined(_KERNEL) || defined(_LIBWRITEVV)
struct writevv_args {
	int version;
	const int *fds;
	int fdcnt;
	const struct iovec *iov;
	int iovcnt;
	size_t *returns;
	int *errors;
};
#define WRITEVV_VERSION	1
#endif

#ifndef _KERNEL
int writevv(const int *fds, int fdcnt, const struct iovec *iov, int iovcnt,
    size_t *returns, int *errors);
int writevv_kernel(const int *fds, int fdcnt,
    const struct iovec *iov, int iovcnt,
    size_t *returns, int *errors);
int writevv_userland(const int *fds, int fdcnt,
    const struct iovec *iov, int iovcnt,
    size_t *returns, int *errors);

#define WRITEVV_GETMODE	1
#define WRITEVV_SETMODE	2

#define WRITEVV_MODE_KERNEL	10
#define WRITEVV_MODE_USER	11

int writevv_control(int op, int *val);

#endif

#endif

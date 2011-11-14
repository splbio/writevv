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
int
writevv(const int *fds, int fdcnt, const struct iovec *iov, int iovcnt,
    size_t *returns, int *errors);
#endif

#endif

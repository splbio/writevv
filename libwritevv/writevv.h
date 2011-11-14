#ifndef WRITEVV_H
#define WRITEVV_H

#if defined(_KERNEL) || defined(_LIBWRITEVV)
struct writevv_args {
	const int *fds;
	int fdcnt;
	const struct iovec *iov;
	int iovcnt;
	size_t *returns;
	int *errors;
};
#endif

#ifndef _KERNEL
int
writevv(const int *fds, int fdcnt, const struct iovec *iov, int iovcnt,
    size_t *returns, int *errors);
#endif

#endif

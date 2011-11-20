#include <event2/event.h>

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <err.h>
#include <netdb.h>

#include <arpa/inet.h>

#include "writevv.h"


#if 1
#define e()	do {fprintf(stderr, "%d\n", __LINE__); } while (0)
#else
#define e()	do { ; }while(0)
#endif

#define SOCK_BUF_SIZE (64*1024)
#define IOVCNT	4
char buff[((SOCK_BUF_SIZE / IOVCNT)/4) - 50];


int make_conn(void);

int
make_conn(void)
{
    struct sockaddr_in my_addr;
    int s, opt;
    int error;

    s = socket(PF_INET, SOCK_STREAM, 0); //protoent->p_proto);
    bzero(&my_addr, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(9999);     // short, network byte order
    my_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    e();
    error = connect(s, (struct sockaddr *)&my_addr, sizeof(my_addr));
    if (error == -1)
	    err(1, "connect");
    opt = SOCK_BUF_SIZE;
    setsockopt(s, SOL_SOCKET, SO_SNDBUF, &opt, sizeof(opt));
    return (s);
}

#define NCONN 1000

int fds[NCONN];
size_t returns[NCONN];
int errors[NCONN];

int
main(int argc __unused, char **argv __unused)
{
    int i, s;
    int error;
    struct iovec iovs[IOVCNT];
    size_t totvecbytes;
    int opt;

    for (i = 0; i < NCONN; i++) {
	    s = make_conn();
	    fds[i] = s;
    }

    totvecbytes = 0;
    for (i = 0; i < IOVCNT; i++) {
	    iovs[i].iov_base = buff;
	    iovs[i].iov_len = sizeof(buff);
	    totvecbytes += sizeof(buff);
    }

    //opt = WRITEVV_MODE_USER;
    opt = WRITEVV_MODE_KERNEL;
    writevv_control(WRITEVV_SETMODE, &opt);

    for (i = 0; i < 10; i++) {
	int k;
	fprintf(stderr, "looping... %d\n", i);
	error = writevv(fds, NCONN, iovs, IOVCNT, returns, errors);
	if (error == -1)
		err(1, "writevv");
	for (k = 0; k < NCONN; k++) {
		size_t r = returns[k];
		if (r != totvecbytes) {
			fprintf(stderr, "failed: fd %d (index %d of %d) -> r = %zu, totvecbytes = %zu\n", fds[k], k, NCONN, r, totvecbytes);
			exit(1);
		}
	}
    }
    exit(0);
}

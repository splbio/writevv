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
#include <sys/wait.h>
#include <netinet/in.h>
#include <stdio.h>
#include <err.h>
#include <netdb.h>

#include <arpa/inet.h>
#include <netinet/tcp.h>

#include "writevv.h"
#include "rcvsocknull.h"


#if 0
#define e()	do {fprintf(stderr, "%d\n", __LINE__); } while (0)
#else
#define e()	do { ; }while(0)
#endif

#define SOCK_BUF_SIZE (64*1024)
#define IOVCNT	4
static int g_mode = WRITEVV_MODE_KERNEL;
int g_children = 1;
int g_sockbufsize = SOCK_BUF_SIZE;
int g_bufsize = (((SOCK_BUF_SIZE / IOVCNT)/4) - 50);
int g_loops = 10;
int g_port = 9999;
int g_sockcnt = 1000;
int g_usetcp = 0;

char *g_buff;

int make_conn(void);
int make_conn_tcp(void);
int make_conn_socketpair(void);

int
make_conn(void)
{
    int error, opt, s;

    if (g_usetcp) {
	    s = make_conn_tcp();
    } else {
	    s = make_conn_socketpair();
    }
    opt = g_sockbufsize * 8;
    error = setsockopt(s, SOL_SOCKET, SO_SNDBUF, &opt, sizeof(opt));
    if (error) {
	    err(1, "setsockopt SO_SNDBUF %d", opt);
    }
    return s;
}

int discard_it(int sock);

int
discard_it(int sock)
{
	int opt, error;

	opt = 1;
	error = setrcvsocknull(sock, 1);
	if (error)
		err(1, "setrcvsocknull");
	return error;
}

int
make_conn_socketpair(void)
{
    int sp[2];
    int error;

    error = socketpair(AF_UNIX, SOCK_STREAM, 0, sp);
    if (error)
	    err(1, "socketpair");
    discard_it(sp[1]);
    discard_it(sp[0]);
    return (sp[0]);
}

int
make_conn_tcp(void)
{
    struct sockaddr_in my_addr;
    int s, opt;
    int error;

    s = socket(PF_INET, SOCK_STREAM, 0); //protoent->p_proto);
    bzero(&my_addr, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(g_port);     // short, network byte order
    my_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    e();
    error = connect(s, (struct sockaddr *)&my_addr, sizeof(my_addr));
    if (error == -1)
	    err(1, "connect");
    opt = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    return (s);
}

static int *g_fds;
static size_t *g_returns;
static int *g_errors;

static int child(void);

int
main(int argc, char **argv)
{
    int ch, i;

    while ((ch = getopt(argc, argv, "B:c:Kl:p:s:S:U")) != -1) {
	    switch (ch) {
	    case 'B':
		    g_bufsize = atoi(optarg);
		    break;
	    case 'c':
		    g_children = atoi(optarg);
		    break;
	    case 'K':
		    g_mode = WRITEVV_MODE_KERNEL;
		    break;
	    case 'l':
		    g_loops = atoi(optarg);
		    break;
	    case 'p':
		    g_port = atoi(optarg);
		    break;
	    case 's':
		    g_sockcnt = atoi(optarg);
		    break;
	    case 'S':
		    g_sockbufsize = atoi(optarg);
		    break;
	    case 't':
		    g_usetcp = 1;
		    break;
	    case 'U':
		    g_mode = WRITEVV_MODE_USER;
		    break;
	    }
    }

    g_buff = malloc(g_bufsize);
    g_fds = malloc(sizeof(*g_fds) * g_sockcnt);
    g_returns = malloc(sizeof(*g_returns) * g_sockcnt);
    g_errors = malloc(sizeof(*g_errors) * g_sockcnt);
    if (!g_buff || !g_fds || !g_returns || !g_errors)
	    err(1, "malloc");
    for (i = 0; i < g_children; i++) {
	    pid_t pid = fork();
	    if (pid == 0) {
		    exit(child());
	    } else if (pid == -1) {
		    err(1, "fork");
	    }
    }
    for (i = 0; i < g_children; i++) {
	    int stat;
	    wait(&stat);
	    if (!WIFEXITED(stat) || WEXITSTATUS(stat) != 0)
		    errx(1, "child error");
    }
    return 0;
}

void do_exit(int ex);

int
child(void)
{
    int i, s;
    int error;
    struct iovec iovs[IOVCNT];
    size_t totvecbytes;

    for (i = 0; i < g_sockcnt; i++) {
	    s = make_conn();
	    g_fds[i] = s;
    }

    totvecbytes = 0;
    for (i = 0; i < IOVCNT; i++) {
	    iovs[i].iov_base = g_buff;
	    iovs[i].iov_len = g_bufsize;
	    totvecbytes += g_bufsize;
    }

    writevv_control(WRITEVV_SETMODE, &g_mode);

    for (i = 0; i < g_loops; i++) {
	int k;
	fprintf(stderr, "looping... %d\n", i);
	error = writevv(g_fds, g_sockcnt, iovs, IOVCNT, g_returns, g_errors);
	if (error == -1)
		err(1, "writevv");
	for (k = 0; k < g_sockcnt; k++) {
		size_t r = g_returns[k];
		if (r != totvecbytes) {
			fprintf(stderr, "failed: fd %d (index %d of %d) -> r = %zu, totvecbytes = %zu\n", g_fds[k], k, g_sockcnt, r, totvecbytes);
			do_exit(1);
		}
	}
    }
    do_exit(0);
    return(0);
}
    
void
do_exit(int ex)
{
    int i;
    for (i = 0; i < g_sockcnt; i++) {
	    close(g_fds[i]);
    }
    exit(ex);
}

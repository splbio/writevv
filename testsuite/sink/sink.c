#include <event2/event.h>

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <stdio.h>
#include <err.h>
#include <netdb.h>

#include <arpa/inet.h>


#if 0
#define e()	do {fprintf(stderr, "%d\n", __LINE__); } while (0)
#else
#define e()	do { ; }while(0)
#endif


struct cb_arg {
	struct event_base *base;
	struct event *ev;
	const char * desc;
};

long long sunk_total, sunk_last_print;
#define PRINT_PROGRESS (1024*1024*32)

char buf[1024 * 1024 * 32];
int g_childnum;

void read_socket_cb(evutil_socket_t fd, short what, void *arg);
void listen_socket_cb(evutil_socket_t fd, short what, void *arg);
static int child(int s);

void
read_socket_cb(evutil_socket_t fd, short what __unused, void *arg)
{
    struct cb_arg *cb_arg = arg;
    int error;

    e();
    while ((error = read(fd, buf, sizeof(buf))) > 0) {
	    int nr = error;
	    sunk_total += nr;
	    if (sunk_total - sunk_last_print >= PRINT_PROGRESS) {
		    fprintf(stderr, "Child %d: Sunk %lld bytes\n", g_childnum, sunk_total);
		    sunk_last_print = sunk_total;
	    }
    }
    e();
    if (error == -1) {
	    e();
	    if (errno == EAGAIN) {
                    fprintf(stderr, "fd: %d EAGAIN!\n", fd);
		    e();
    			event_add(cb_arg->ev, NULL);
		    return;
	    }
	    perror("read");
    }

    e();
    fprintf(stderr, "closing fd %d\n", fd);
    close(fd);
    fprintf(stderr, "done closing fd %d\n", fd);
    event_free(cb_arg->ev);
    free(cb_arg);
}

void nonblockit(int s);

void
nonblockit(int s)
{
    int oflags, error;

    oflags = fcntl(s, F_GETFL, NULL);
    error = fcntl(s, F_SETFL, oflags | O_NONBLOCK);
    if (error == -1)
	    err(1, "fcntl");
}

void
listen_socket_cb(evutil_socket_t fd, short what __unused, void *arg)
{
    struct cb_arg *cb_arg;
    struct event_base *base;
    struct cb_arg *new_arg;
    struct event *ev;
    int opt;
    int error;
    int newfd;

    cb_arg = arg;
    base = cb_arg->base;

    e();
    newfd = accept(fd, NULL, NULL);
    if (newfd == -1) {
	    if (errno == EAGAIN)
		return;
	    err(1, "accept");
    }


    e();
    nonblockit(newfd);

    opt = 1024*1024;
    error = setsockopt(newfd, SOL_SOCKET, SO_RCVBUF, &opt, sizeof(opt));

    e();
    new_arg = malloc(sizeof(*new_arg));
    if (new_arg == NULL)
	    err(1, "malloc");
    new_arg->base = cb_arg->base;
    new_arg->desc = "Reading fd";
    e();
    ev = event_new(cb_arg->base, newfd, EV_READ|EV_PERSIST, read_socket_cb, new_arg);
    new_arg->ev = ev;
    if (ev == NULL)
	    err(1, "event_new");

    event_add(ev, NULL);
}


int
main(int argc, char **argv)
{
    int ch;
    int children = 1;
    char *progname = argv[0];
    struct sockaddr_in my_addr;
    int opt;

    int s;
    int i;
    int error;
    int port = 9999;

    while ((ch = getopt(argc, argv, "c:p:")) != -1) {
	    switch (ch) {
	    case 'c':
		    children = atoi(optarg);
		    break;
	    case 'p':
		    port = atoi(optarg);
		    break;
	    case '?':
	    default:
		    errx(1, "usage: %s [-c children] [-p listenport]",
			progname);
	    }
    }

    e();
    s = socket(PF_INET, SOCK_STREAM, 0); //protoent->p_proto);
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);     // short, network byte order
    my_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    e();
    error = bind(s, (struct sockaddr *)&my_addr, sizeof(my_addr));
    if (error == -1)
	    err(1, "bind");
    e();
    opt = 1;
    error = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (error == -1)
	    warn("setsockopt SO_REUSEADDR");
    opt = 1;
    error = setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    if (error == -1)
	    warn("setsockopt SO_REUSEPORT");
    error = listen(s, -1);
    if (error == -1)
	    err(1, "listen");

    nonblockit(s);
    for (i = 0; i < children; i++) {
	    pid_t pid = fork();
	    if (pid == 0) {
		    g_childnum = i;
		    exit(child(s));
	    } else if (pid == -1) {
		    err(1, "fork");
	    }
    }
    for (i = 0; i < children; i++) {
	    int status;
	    wait(&status);
    }
}


static int
child(int s)
{
    struct event_config *cfg;
    struct event_base *base;
    //struct  protoent *protoent;
    struct cb_arg *new_arg;
    struct event *ev;

    cfg = event_config_new();
    if (!cfg)
	    errx(1, "event_config_new");
    //event_config_require_features(cfg, EV_FEATURE_ET);

    base = event_base_new_with_config(cfg);
    event_config_free(cfg);
    if (!base)
	    errx(1, "event_base_new_with_config");

    e();
    new_arg = malloc(sizeof(*new_arg));
    if (new_arg == NULL)
	    err(1, "malloc");
    new_arg->base = base;
    new_arg->desc = "Listen fd";
    ev = event_new(base, s, EV_READ|EV_PERSIST, listen_socket_cb, new_arg);
    new_arg->ev = ev;
    if (ev == NULL)
	    err(1, "event_new");

    event_add(ev, NULL);
    e();
    event_base_dispatch(base);
    err(1, "returned");
}

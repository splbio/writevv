PROG=sink

CFLAGS+= -I/usr/local/include -g -O
LDFLAGS+= -L/usr/local/lib  -g
#LDFLAGS+= -L/usr/local/lib/event2 -levent2
LDADD=/usr/local/lib/event2/libevent.a
#/usr/local/lib/event2/libevent.a 
#-levent2 

WARNS=6
NO_MAN=1

.include <bsd.prog.mk>

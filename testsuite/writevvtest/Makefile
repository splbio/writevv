PROG=writevvtest

WRITEVV_INCDIR=${.CURDIR}/../../libwritevv
WRITEVV_LIBDIR=${.CURDIR}/../../libwritevv

CFLAGS+= -I/usr/local/include -g -O
CFLAGS+= -I${WRITEVV_INCDIR} -g -O
LDFLAGS+= -L/usr/local/lib  -g -L${WRITEVV_LIBDIR} 
LDADD+= ${WRITEVV_LIBDIR}/libwritevv.a
WARNS=6
#LDFLAGS+= -L/usr/local/lib/event2 -levent
#/usr/local/lib/event2/libevent.a 
#-levent2 


NO_MAN=1

.include <bsd.prog.mk>

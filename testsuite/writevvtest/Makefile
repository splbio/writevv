PROG=writevvtest

WRITEVV_INCDIR=${.CURDIR}/../../libwritevv
WRITEVV_LIBDIR=${.CURDIR}/../../libwritevv

LDFLAGS+= -L/usr/local/lib  -g -L${WRITEVV_LIBDIR}
LDADD+= ${WRITEVV_LIBDIR}/libwritevv.a

CFLAGS+= -I${WRITEVV_INCDIR}
CFLAGS+= -g -O
WARNS=6


NO_MAN=1

.include <bsd.prog.mk>

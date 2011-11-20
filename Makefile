
.include <bsd.own.mk>

SUBDIR = libwritevv \
	kmod \
	testsuite 

reload:
	-kldunload writevv
	kldload ./kmod/writevv.ko

runsink:
	./testsuite/sink/sink

runtest:
	./testsuite/writevvtest/writevvtest

.include <bsd.subdir.mk>


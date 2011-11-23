
.include <bsd.own.mk>

SUBDIR = libwritevv \
	kmod \
	testsuite 

reload:
	sudo $(MAKE) $(MAKEFLAGS) rootreload

rootreload: tunekernel
	-kldunload writevv
	kldload ./kmod/writevv.ko

runsink:
	./testsuite/sink/sink -c 10

runtest:
	./testsuite/writevvtest/writevvtest

tunekernel:
	sudo sysctl kern.ipc.somaxconn=8192

.include <bsd.subdir.mk>


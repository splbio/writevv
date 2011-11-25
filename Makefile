
.include <bsd.own.mk>

SUBDIR = libwritevv \
	kmod \
	kmod_rcvsocknull \
	testsuite 

reload:
	sudo $(MAKE) $(MAKEFLAGS) rootreload

rootreload: tunekernel
	-kldunload writevv
	kldload ./kmod/writevv.ko
	-kldunload rcvsocknull
	kldload ./kmod_rcvsocknull/rcvsocknull.ko

runsink:
	./testsuite/sink/sink -c 10

runtest:
	./testsuite/writevvtest/writevvtest

tunekernel:
	sudo sysctl kern.ipc.somaxconn=8192

.include <bsd.subdir.mk>


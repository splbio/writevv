
.include <bsd.own.mk>

SUBDIR = libwritevv \
	kmod \
	kmod_rcvsocknull \
	testsuite \
	man

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
	@echo "running user test..."
	time ./testsuite/writevvtest/writevvtest -U
	@echo "running kernel test..."
	time ./testsuite/writevvtest/writevvtest

tunekernel:
	sudo sysctl kern.ipc.somaxconn=8192

GITVER!=git log --pretty=format:'%h' -n 1
TARBALL=bitgravity-${GITVER}.tgz

tarball:
	$(MAKE) clean cleandepend
	cd .. && tar -H -czvf ${TARBALL} bitgravity && realpath ${TARBALL}

.include <bsd.subdir.mk>


CFLAGS  = -O3 -march=native -ggdb3 -m32 -std=gnu99 -fshort-wchar -Wno-multichar -Iinclude -mstackrealign
CPPFLAGS=-DNDEBUG -D_GNU_SOURCE -I. -Iintercept -Ipeloader
LDFLAGS = $(CFLAGS) -m32 -lm -ldl -Wl,--dynamic-list=exports.lst
LDLIBS  = intercept/libdisasm.a -Wl,--whole-archive,peloader/libpeloader.a,--no-whole-archive

.PHONY: clean peloader intercept

TARGETS=fxc | peloader

all: $(TARGETS)

intercept:
	make -C intercept all

peloader:
	make -C peloader all

intercept/hook.o: intercept

fxc: fxc.o intercept/hook.o | peloader
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS) $(LDFLAGS)

clean:
	rm -f a.out core *.o core.* vgcore.* gmon.out fxc
	make -C intercept clean
	make -C peloader clean

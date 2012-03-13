# FIXME: Need a real build system

all: squashfuse

clean:
	rm -rf squashfuse *.o *.inc *.dSYM

.PHONY: all clean

ARCH = 

LIBADD = -lz
ifeq ($(shell uname -s),Darwin)
	FUSE_FLAGS = -D__FreeBSD__=10 -D_FILE_OFFSET_BITS=64 -D__DARWIN_64_BIT_INO_T=1 \
		-I/usr/local/include/fuse
	ifneq ($(wildcard /usr/local/lib/libfuse_ino64.dylib),)
		LIBADD += -lfuse_ino64
	else
		LIBADD += -lfuse
	endif
else
	FUSE_FLAGS = $(shell pkg-config --cflags fuse)
	LIBADD += $(shell pkg-config --libs fuse)
endif

CFLAGS = $(ARCH) -std=c99 -g -O0 -Wall
LDFLAGS = $(ARCH)

LIBFILES = dir.o file.o fs.o swap.o table.o ll.o

squashfuse: squashfuse.o $(LIBFILES)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBADD)

%.o: %.c $(wildcard *.h) swap.h.inc
	$(CC) $(CFLAGS) $(FUSE_FLAGS) -c -o $@ $<

swap.h.inc: squashfs_fs.h gen_swap.rb
	./gen_swap.rb

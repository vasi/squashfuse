# FIXME: Need a real build system

all: squashfuse

clean:
	rm -rf squashfuse *.o *.inc *.dSYM

.PHONY: all clean

CFLAGS = -g -O0 -Wall

LIBFILES = dir.o file.o fs.o swap.o table.o
LIBADD = -lz

squashfuse: squashfuse.o $(LIBFILES)
	$(CC) -o $@ $^ $(LIBADD)

%.o: %.c $(wildcard *.h) swap.h.inc
	$(CC) $(CFLAGS) -c -o $@ $<

swap.h.inc: squashfs_fs.h gen_swap.rb
	./gen_swap.rb

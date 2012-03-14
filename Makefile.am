bin_PROGRAMS = squashfuse
squashfuse_SOURCES = std.c swap.c cache.c table.c dir.c file.c fs.c \
	ll.c squashfuse.c
BUILT_SOURCES = swap.h.inc swap.c.inc
CLEANFILES = swap.h.inc swap.c.inc
swap.h.inc swap.c.inc: Makefile gen_swap.sh
	./gen_swap.sh

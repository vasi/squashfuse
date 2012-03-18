bin_PROGRAMS = squashfuse
squashfuse_SOURCES = std.c swap.c cache.c table.c dir.c file.c fs.c \
	ll.c squashfuse.c \
	squashfs_fs.h common.h std.h swap.h cache.h table.h dir.h file.h \
	squashfuse.h ll.h

BUILT_SOURCES = swap.h.inc swap.c.inc
CLEANFILES = swap.h.inc swap.c.inc
EXTRA_DIST = gen_swap.sh
swap.h.inc swap.c.inc: gen_swap.sh squashfs_fs.h Makefile
	$(srcdir)/gen_swap.sh $(srcdir)/squashfs_fs.h

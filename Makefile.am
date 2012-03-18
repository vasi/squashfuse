# Suppress AppleDouble
if GNU_MAKE
export COPYFILE_DISABLE = 1
endif

bin_PROGRAMS = squashfuse
squashfuse_SOURCES = swap.c cache.c table.c dir.c file.c fs.c ll.c squashfuse.c \
	nonstd-pread.c nonstd-makedev.c nonstd-stat.c \
	squashfs_fs.h common.h nonstd-internal.h nonstd.h swap.h cache.h table.h \
	dir.h file.h squashfuse.h ll.h

BUILT_SOURCES = swap.h.inc swap.c.inc
CLEANFILES = swap.h.inc swap.c.inc
EXTRA_DIST = gen_swap.sh
swap.h.inc swap.c.inc: gen_swap.sh squashfs_fs.h Makefile
	$(srcdir)/gen_swap.sh $(srcdir)/squashfs_fs.h

ACLOCAL_AMFLAGS = -I m4 --install

# Suppress AppleDouble
if GNU_MAKE
export COPYFILE_DISABLE = 1
endif

bin_PROGRAMS = squashfuse
squashfuse_SOURCES = swap.c cache.c table.c dir.c file.c fs.c ll.c squashfuse.c \
	decompress.c xattr.c \
	nonstd-pread.c nonstd-makedev.c nonstd-stat.c nonstd-daemon.c nonstd-enoattr.c \
	squashfs_fs.h common.h nonstd-internal.h nonstd.h swap.h cache.h table.h \
	dir.h file.h decompress.h xattr.h squashfuse.h ll.h

dist_man_MANS = squashfuse.1

BUILT_SOURCES = swap.h.inc swap.c.inc
CLEANFILES = swap.h.inc swap.c.inc
EXTRA_DIST = gen_swap.sh
swap.h.inc swap.c.inc: gen_swap.sh squashfs_fs.h Makefile
	$(srcdir)/gen_swap.sh $(srcdir)/squashfs_fs.h

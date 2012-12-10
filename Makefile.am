ACLOCAL_AMFLAGS = -I m4 --install

# Suppress AppleDouble
if MAKE_EXPORT
export COPYFILE_DISABLE = 1
endif

bin_PROGRAMS = squashfuse
squashfuse_SOURCES = swap.c cache.c table.c dir.c file.c fs.c ll.c \
	squashfuse.c decompress.c xattr.c hash.c \
	nonstd-pread.c nonstd-makedev.c nonstd-stat.c nonstd-daemon.c nonstd-enoattr.c \
	squashfs_fs.h common.h nonstd-internal.h nonstd.h swap.h cache.h table.h \
	dir.h file.h decompress.h xattr.h squashfuse.h ll.h hash.h
squashfuse_CPPFLAGS = $(ZLIB_CPPFLAGS) $(XZ_CPPFLAGS) $(LZO_CPPFLAGS) \
	$(FUSE_CPPFLAGS)
squashfuse_LDADD = $(ZLIB_LIBS) $(XZ_LIBS) $(LZO_LIBS) $(FUSE_LIBS)

dist_man_MANS = squashfuse.1

BUILT_SOURCES = swap.h.inc swap.c.inc
CLEANFILES = swap.h.inc swap.c.inc
EXTRA_DIST = gen_swap.sh LICENSE
swap.h.inc swap.c.inc: gen_swap.sh squashfs_fs.h Makefile
	SED="$(SED)" $(srcdir)/gen_swap.sh $(srcdir)/squashfs_fs.h

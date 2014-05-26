ACLOCAL_AMFLAGS = -I m4 --install

# Suppress AppleDouble
if MAKE_EXPORT
export COPYFILE_DISABLE = 1
endif

noinst_LTLIBRARIES = libsquashfuse.la
libsquashfuse_la_SOURCES = swap.c cache.c table.c dir.c file.c fs.c \
	decompress.c xattr.c hash.c \
	nonstd-pread.c nonstd-stat.c \
	squashfs_fs.h common.h nonstd-internal.h nonstd.h swap.h cache.h table.h \
	dir.h file.h decompress.h xattr.h squashfuse.h hash.h
libsquashfuse_la_CPPFLAGS = $(ZLIB_CPPFLAGS) $(XZ_CPPFLAGS) $(LZO_CPPFLAGS) \
	$(LZ4_CPPFLAGS)
libsquashfuse_la_LIBADD = $(ZLIB_LIBS) $(XZ_LIBS) $(LZO_LIBS) $(LZ4_LIBS)

bin_PROGRAMS = squashfuse squashfuse_hl

squashfuse_SOURCES = ll.c squashfuse.c nonstd-daemon.c nonstd-enoattr.c nonstd-makedev.c
squashfuse_CPPFLAGS = $(FUSE_CPPFLAGS)
squashfuse_LDADD = $(FUSE_LIBS) libsquashfuse.la

squashfuse_hl_SOURCES = hl.c nonstd-enoattr.c nonstd-makedev.c
squashfuse_hl_CPPFLAGS = $(FUSE_CPPFLAGS)
squashfuse_hl_LDADD = $(FUSE_LIBS) libsquashfuse.la


dist_man_MANS = squashfuse.1

BUILT_SOURCES = swap.h.inc swap.c.inc
CLEANFILES = swap.h.inc swap.c.inc
EXTRA_DIST = gen_swap.sh LICENSE
swap.h.inc swap.c.inc: gen_swap.sh squashfs_fs.h Makefile
	SED="$(SED)" $(srcdir)/gen_swap.sh $(srcdir)/squashfs_fs.h

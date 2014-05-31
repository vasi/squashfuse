ACLOCAL_AMFLAGS = -I m4 --install

# Suppress AppleDouble
if MAKE_EXPORT
export COPYFILE_DISABLE = 1
endif

noinst_LTLIBRARIES = libsquashfuse.la libfuseprivate.la

libsquashfuse_la_SOURCES = swap.c cache.c table.c dir.c file.c fs.c \
	decompress.c xattr.c hash.c stack.c traverse.c util.c \
	nonstd-pread.c nonstd-stat.c \
	squashfs_fs.h common.h nonstd-internal.h nonstd.h swap.h cache.h table.h \
	dir.h file.h decompress.h xattr.h squashfuse.h hash.h stack.h traverse.h \
	util.h
libsquashfuse_la_CPPFLAGS = $(ZLIB_CPPFLAGS) $(XZ_CPPFLAGS) $(LZO_CPPFLAGS) \
	$(LZ4_CPPFLAGS)
libsquashfuse_la_LIBADD = $(ZLIB_LIBS) $(XZ_LIBS) $(LZO_LIBS) $(LZ4_LIBS)

libfuseprivate_la_SOURCES = fuseprivate.c nonstd-makedev.c nonstd-enoattr.c \
	fuseprivate.h
libfuseprivate_la_CPPFLAGS = $(FUSE_CPPFLAGS)
libfuseprivate_la_LIBADD = $(FUSE_LIBS)


bin_PROGRAMS = squashfuse squashfuse_ls

squashfuse_SOURCES = hl.c
squashfuse_CPPFLAGS = $(FUSE_CPPFLAGS)
squashfuse_LDADD = libsquashfuse.la libfuseprivate.la

squashfuse_ls_SOURCES = ls.c
squashfuse_ls_LDADD = libsquashfuse.la

if HAVE_FUSE_LOWLEVEL
bin_PROGRAMS += squashfuse_ll
squashfuse_ll_SOURCES = ll.c ll_inode.c nonstd-daemon.c ll.h
squashfuse_ll_CPPFLAGS = $(FUSE_CPPFLAGS)
squashfuse_ll_LDADD = libsquashfuse.la libfuseprivate.la
endif


dist_man_MANS = squashfuse.1

BUILT_SOURCES = swap.h.inc swap.c.inc
CLEANFILES = swap.h.inc swap.c.inc
EXTRA_DIST = gen_swap.sh autogen.sh LICENSE
swap.h.inc swap.c.inc: gen_swap.sh squashfs_fs.h Makefile
	SED="$(SED)" $(srcdir)/gen_swap.sh $(srcdir)/squashfs_fs.h

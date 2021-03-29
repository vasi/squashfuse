COMPRESSION_LIBS = $(ZLIB_LIBS) $(XZ_LIBS) $(LZO_LIBS) $(LZ4_LIBS) $(ZSTD_LIBS)

ACLOCAL_AMFLAGS = -I m4 --install

# Suppress AppleDouble
if MAKE_EXPORT
export COPYFILE_DISABLE = 1
endif

dist_man_MANS =
EXTRA_DIST = gen_swap.sh autogen.sh LICENSE CONFIGURATION PLATFORMS NEWS win

bin_PROGRAMS =
noinst_PROGRAMS =

lib_LTLIBRARIES = libsquashfuse.la
noinst_LTLIBRARIES =

pkgincludedir = @includedir@/squashfuse
pkginclude_HEADERS = squashfuse.h squashfs_fs.h \
	cache.h common.h config.h decompress.h dir.h file.h fs.h stack.h table.h \
	traverse.h util.h xattr.h

pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA 	= squashfuse.pc

# Main library: libsquashfuse
libsquashfuse_la_SOURCES = swap.c cache.c table.c dir.c file.c fs.c \
	decompress.c xattr.c hash.c stack.c traverse.c util.c \
	nonstd-pread.c nonstd-stat.c \
	squashfs_fs.h common.h nonstd-internal.h nonstd.h swap.h cache.h table.h \
	dir.h file.h decompress.h xattr.h squashfuse.h hash.h stack.h traverse.h \
	util.h fs.h
libsquashfuse_la_CPPFLAGS = $(ZLIB_CPPFLAGS) $(XZ_CPPFLAGS) $(LZO_CPPFLAGS) \
	$(LZ4_CPPFLAGS) $(ZSTD_CPPFLAGS)
libsquashfuse_la_LIBADD =

if SQ_WANT_FUSE
# Helper for FUSE clients: libfuseprivate
libfuseprivate_la_SOURCES = fuseprivate.c nonstd-makedev.c nonstd-enoattr.c \
	fuseprivate.h stat.h stat.c
libfuseprivate_la_CPPFLAGS = $(FUSE_CPPFLAGS)
libfuseprivate_la_LIBADD =
noinst_LTLIBRARIES += libfuseprivate.la
endif

# High-level squashfuse
if SQ_WANT_HIGHLEVEL
bin_PROGRAMS += squashfuse
squashfuse_SOURCES = hl.c
squashfuse_CPPFLAGS = $(FUSE_CPPFLAGS)
squashfuse_LDADD = libsquashfuse.la libfuseprivate.la $(COMPRESSION_LIBS) \
  $(FUSE_LIBS)

dist_man_MANS += squashfuse.1
endif

# Low-level squashfuse_ll, if supported
if SQ_WANT_LOWLEVEL
bin_PROGRAMS += squashfuse_ll
squashfuse_ll_SOURCES = ll.c ll_inode.c nonstd-daemon.c ll.h
squashfuse_ll_CPPFLAGS = $(FUSE_CPPFLAGS)
squashfuse_ll_LDADD = libsquashfuse.la libfuseprivate.la $(COMPRESSION_LIBS) \
  $(FUSE_LIBS)

endif


if SQ_WANT_DEMO
# Sample program squashfuse_ls
noinst_PROGRAMS += squashfuse_ls
squashfuse_ls_SOURCES = ls.c
squashfuse_ls_LDADD = libsquashfuse.la $(COMPRESSION_LIBS)
# Sample program squashfuse_extract
noinst_PROGRAMS += squashfuse_extract
squashfuse_extract_CPPFLAGS = $(FUSE_CPPFLAGS)
squashfuse_extract_SOURCES = extract.c stat.h stat.c nonstd-makedev.c 
squashfuse_extract_LDADD = libsquashfuse.la $(COMPRESSION_LIBS) \
  $(FUSE_LIBS)
endif

TESTS =
if SQ_FUSE_TESTS
TESTS += tests/ll-smoke.sh
check_PROGRAMS = endiantest
endiantest_SOURCES = tests/endiantest.c
TESTS += endiantest
endif
if SQ_DEMO_TESTS
TESTS += tests/ls.sh
endif
tests/ll-smoke.sh tests/ls.sh: tests/lib.sh


# Handle generation of swap include files
CLEANFILES = swap.h.inc swap.c.inc
EXTRA_DIST += swap.h.inc swap.c.inc
$(libsquashfuse_la_OBJECTS): swap.h.inc
swap.h.inc swap.c.inc: gen_swap.sh squashfs_fs.h Makefile
	SED="$(SED)" $(srcdir)/gen_swap.sh $(srcdir)/squashfs_fs.h

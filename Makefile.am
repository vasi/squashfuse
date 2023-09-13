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
lib_LTLIBRARIES = 
noinst_LTLIBRARIES =

pkgincludedir = @includedir@/squashfuse
pkginclude_HEADERS = squashfuse.h squashfs_fs.h \
	cache.h common.h config.h decompress.h dir.h file.h fs.h stack.h table.h \
	traverse.h util.h xattr.h
pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA 	= squashfuse.pc

# Convenience lib to we get static executables
noinst_LTLIBRARIES += libsquashfuse_convenience.la
libsquashfuse_convenience_la_SOURCES = swap.c cache.c table.c dir.c file.c fs.c \
	decompress.c xattr.c hash.c stack.c traverse.c util.c \
	nonstd-pread.c nonstd-stat.c cache_mt.c \
	squashfs_fs.h common.h nonstd-internal.h nonstd.h swap.h cache.h table.h \
	dir.h file.h decompress.h xattr.h squashfuse.h hash.h stack.h traverse.h \
	util.h fs.h
libsquashfuse_convenience_la_CPPFLAGS = $(ZLIB_CPPFLAGS) $(XZ_CPPFLAGS) $(LZO_CPPFLAGS) \
	$(LZ4_CPPFLAGS) $(ZSTD_CPPFLAGS) $(FUSE_CPPFLAGS)
libsquashfuse_convenience_la_LIBADD = $(COMPRESSION_LIBS) $(FUSE_LIBS)

# Main library: libsquashfuse
lib_LTLIBRARIES += libsquashfuse.la
libsquashfuse_la_SOURCES =
libsquashfuse_la_CPPFLAGS = $(ZLIB_CPPFLAGS) $(XZ_CPPFLAGS) $(LZO_CPPFLAGS) \
	$(LZ4_CPPFLAGS) $(ZSTD_CPPFLAGS) $(FUSE_CPPFLAGS)
libsquashfuse_la_LIBADD = libsquashfuse_convenience.la

if SQ_WANT_FUSE
# Helper for FUSE clients: libfuseprivate
libfuseprivate_la_SOURCES = fuseprivate.c nonstd-makedev.c nonstd-enoattr.c \
	fuseprivate.h stat.h stat.c
libfuseprivate_la_CPPFLAGS = $(FUSE_CPPFLAGS)
libfuseprivate_la_LIBADD = $(COMPRESSION_LIBS) $(FUSE_LIBS)
noinst_LTLIBRARIES += libfuseprivate.la
endif

# High-level squashfuse
if SQ_WANT_HIGHLEVEL
bin_PROGRAMS += squashfuse
squashfuse_SOURCES = hl.c
squashfuse_CPPFLAGS = $(ZLIB_CPPFLAGS) $(XZ_CPPFLAGS) $(LZO_CPPFLAGS) \
	$(LZ4_CPPFLAGS) $(ZSTD_CPPFLAGS) $(FUSE_CPPFLAGS)
squashfuse_LDADD = libsquashfuse_convenience.la libfuseprivate.la $(COMPRESSION_LIBS) $(FUSE_LIBS)
dist_man_MANS += squashfuse.1
endif

# Low-level squashfuse_ll, if supported
if SQ_WANT_LOWLEVEL

# convenience lib so we can link squashfuse_ll statically
noinst_LTLIBRARIES += libsquashfuse_ll_convenience.la
libsquashfuse_ll_convenience_la_SOURCES = ll.c ll_inode.c nonstd-daemon.c
libsquashfuse_ll_convenience_la_CPPFLAGS = $(ZLIB_CPPFLAGS) $(XZ_CPPFLAGS) $(LZO_CPPFLAGS) \
	$(LZ4_CPPFLAGS) $(ZSTD_CPPFLAGS) $(FUSE_CPPFLAGS)
libsquashfuse_ll_convenience_la_LIBADD = libsquashfuse_convenience.la libfuseprivate.la

# squashfuse_ll library we will install
lib_LTLIBRARIES += libsquashfuse_ll.la
libsquashfuse_ll_la_SOURCES =
libsquashfuse_ll_la_CPPFLAGS = $(ZLIB_CPPFLAGS) $(XZ_CPPFLAGS) $(LZO_CPPFLAGS) \
	$(LZ4_CPPFLAGS) $(ZSTD_CPPFLAGS) $(FUSE_CPPFLAGS)
libsquashfuse_ll_la_LIBADD = libsquashfuse_ll_convenience.la $(COMPRESSION_LIBS) $(FUSE_LIBS)

# squashfuse_ll binary that's statically linked against internal libs
bin_PROGRAMS += squashfuse_ll
squashfuse_ll_SOURCES = ll_main.c
squashfuse_ll_CPPFLAGS = $(ZLIB_CPPFLAGS) $(XZ_CPPFLAGS) $(LZO_CPPFLAGS) \
	$(LZ4_CPPFLAGS) $(ZSTD_CPPFLAGS) $(FUSE_CPPFLAGS)
squashfuse_ll_LDADD = libsquashfuse_ll_convenience.la $(COMPRESSION_LIBS) $(FUSE_LIBS)

dist_man_MANS += squashfuse_ll.1
pkgconfig_DATA += squashfuse_ll.pc
pkginclude_HEADERS += ll.h
endif


if SQ_WANT_DEMO
# Sample program squashfuse_ls
noinst_PROGRAMS += squashfuse_ls
squashfuse_ls_SOURCES = ls.c
squashfuse_ls_LDADD = libsquashfuse.la $(COMPRESSION_LIBS)
# Sample program squashfuse_extract
noinst_PROGRAMS += squashfuse_extract
squashfuse_extract_CPPFLAGS = $(FUSE_CPPFLAGS)
squashfuse_extract_SOURCES = extract.c stat.h stat.c nonstd-makedev.c nonstd-symlink.c
squashfuse_extract_LDADD = libsquashfuse.la $(COMPRESSION_LIBS) \
  $(FUSE_LIBS)
endif

TESTS =
if SQ_FUSE_TESTS
TESTS += tests/ll-smoke.sh
TESTS += tests/notify_test.sh
if MULTITHREADED
# I know this test looks backwards, but the default smoke test is multithreaded
# when threading is enabled. So we additionally run a singlethreaded test in
# that case.
TESTS += tests/ll-smoke-singlethreaded.sh
endif
if SIGTERM_HANDLER
TESTS += tests/umount-test.sh
endif
check_PROGRAMS = cachetest endiantest
cachetest_SOURCES=tests/cachetest.c
cachetest_LDADD=libsquashfuse.la $(COMPRESSION_LIBS)
endiantest_SOURCES = tests/endiantest.c
TESTS += cachetest endiantest
endif
if SQ_DEMO_TESTS
TESTS += tests/ls.sh
endif
tests/ll-smoke.sh tests/ls.sh: tests/lib.sh


# Handle generation of swap include files
CLEANFILES = swap.h.inc swap.c.inc
EXTRA_DIST += swap.h.inc swap.c.inc
$(libsquashfuse_convenience_la_OBJECTS): swap.h.inc
swap.h.inc swap.c.inc: gen_swap.sh squashfs_fs.h Makefile
	SED="$(SED)" $(srcdir)/gen_swap.sh $(srcdir)/squashfs_fs.h

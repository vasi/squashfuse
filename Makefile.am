COMPRESSION_LIBS = $(ZLIB_LIBS) $(XZ_LIBS) $(LZO_LIBS) $(LZ4_LIBS)

ACLOCAL_AMFLAGS = -I m4 --install

# Suppress AppleDouble
if MAKE_EXPORT
export COPYFILE_DISABLE = 1
endif

dist_man_MANS =
EXTRA_DIST = gen_swap.sh autogen.sh LICENSE CONFIGURATION PLATFORMS NEWS win \
  riscos

bin_PROGRAMS =
noinst_PROGRAMS =
CLEANFILES =

noinst_LTLIBRARIES = libsquashfuse.la

# Main library: libsquashfuse
libsquashfuse_la_SOURCES =    \
  common.h          \
  squashfs_fs.h     \
  squashfuse.h      \
  array.c       array.h       \
  block.c       block.h       \
  cache.c       cache.h       \
  decompress.c  decompress.h  \
  dir.c         dir.h         \
  dynstring.c   dynstring.h   \
  file.c        file.h        \
  file_index.c  file_index.h  \
  fs.c          fs.h          \
  hash.c        hash.h        \
  input.c       input.h       \
  stack.c       stack.h       \
  swap.c        swap.h        \
  table.c       table.h       \
  thread.c      thread.h      \
  traverse.c    traverse.h    \
  util.c        util.h        \
  xattr.c       xattr.h       \
  nonstd-pread.c     \
  nonstd-stat.c      \
  nonstd-vsnprintf.c \
  nonstd-internal.h  \
  nonstd.h  
libsquashfuse_la_CPPFLAGS = $(ZLIB_CPPFLAGS) $(XZ_CPPFLAGS) $(LZO_CPPFLAGS) \
	$(LZ4_CPPFLAGS) $(PTHREAD_CFLAGS)

# Helper for FUSE clients: libfuseprivate
libfuseprivate_la_SOURCES = fuseprivate.c nonstd-makedev.c nonstd-enoattr.c \
	nonstd-setenv.c fuseprivate.h
libfuseprivate_la_CPPFLAGS = $(FUSE_CPPFLAGS)

# High-level squashfuse
if SQ_WANT_HIGHLEVEL
bin_PROGRAMS += squashfuse
squashfuse_SOURCES = hl.c
squashfuse_CPPFLAGS = $(FUSE_CPPFLAGS) $(PTHREAD_CFLAGS)
squashfuse_LDADD = libsquashfuse.la libfuseprivate.la $(COMPRESSION_LIBS) \
  $(FUSE_LIBS) $(PTHREAD_CFLAGS)

noinst_LTLIBRARIES += libfuseprivate.la
dist_man_MANS += squashfuse.1
endif

# Low-level squashfuse_ll, if supported
if SQ_WANT_LOWLEVEL
bin_PROGRAMS += squashfuse_ll
squashfuse_ll_SOURCES = ll.c ll_inode.c ll_inode_indexed.c nonstd-daemon.c ll.h
squashfuse_ll_CPPFLAGS = $(FUSE_CPPFLAGS) $(PTHREAD_CFLAGS)
squashfuse_ll_LDADD = libsquashfuse.la libfuseprivate.la $(COMPRESSION_LIBS) \
  $(FUSE_LIBS) $(PTHREAD_CFLAGS)

noinst_LTLIBRARIES += libfuseprivate.la
endif

if SQ_WANT_DEMO
# Sample program squashfuse_ls
noinst_PROGRAMS += squashfuse_ls
squashfuse_ls_SOURCES = ls.c
squashfuse_ls_CPPFLAGS = $(PTHREAD_CFLAGS)
squashfuse_ls_LDADD = libsquashfuse.la $(COMPRESSION_LIBS) $(PTHREAD_CFLAGS)
endif

# Handle generation of swap include files
CLEANFILES += swap.hinc swap.cinc
EXTRA_DIST += swap.hinc swap.cinc
$(libsquashfuse_la_OBJECTS): swap.hinc
swap.hinc swap.cinc: gen_swap.sh squashfs_fs.h Makefile
	SED="$(SED)" $(srcdir)/gen_swap.sh $(srcdir)/squashfs_fs.h

# Miscellaneous scripts
CLEANFILES += riscos/build
EXTRA_DIST += riscos/build
riscos/build:
	mkdir -p riscos
	$(srcdir)/riscos/gen_build.sh \
		$(libsquashfuse_la_SOURCES) \
		$(squashfuse_ls_SOURCES) > $@

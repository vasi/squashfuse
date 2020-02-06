AC_INIT([squashfuse], [0.1.104fb1], [kvigor@fb.com])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([config.h])
AC_CANONICAL_BUILD
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([foreign -Wall subdir-objects])
AC_USE_SYSTEM_EXTENSIONS
AM_SILENT_RULES(yes)
AM_PROG_AR
LT_INIT

PKG_PROG_PKG_CONFIG
PKG_INSTALLDIR

# Compiler
AC_PROG_AWK
AC_PROG_SED
AC_PROG_CPP
AC_SYS_LARGEFILE
AM_PROG_CC_C_O
SQ_PROG_CC_WALL


# Non-POSIX declarations
SQ_CHECK_DECL_MAKEDEV
SQ_CHECK_DECL_PREAD
SQ_CHECK_DECL_S_IFSOCK
SQ_CHECK_DECL_ENOATTR([:])

# Decompression
SQ_CHECK_DECOMPRESS([ZLIB],[z],[uncompress],[zlib.h])
SQ_CHECK_DECOMPRESS([XZ],[lzma],[lzma_stream_buffer_decode],[lzma.h],[liblzma])
SQ_CHECK_DECOMPRESS([LZO],[lzo2],[lzo1x_decompress_safe],[lzo/lzo1x.h])
SQ_CHECK_DECOMPRESS([LZ4],[lz4],[LZ4_decompress_safe],[lz4.h])
SQ_CHECK_DECOMPRESS([ZSTD],[zstd],[ZSTD_decompress],[zstd.h])
AS_IF([test "x$sq_decompressors" = x],
	[AC_MSG_FAILURE([At least one decompression library must exist])])

# FUSE
SQ_FUSE_API
AS_IF([test "x$sq_fuse_found" = xyes],[
	SQ_FUSE_API_LOWLEVEL
	SQ_FUSE_API_VERSION
	SQ_FUSE_API_XATTR_POSITION
])
SQ_FUSE_RESULT

# Misc
SQ_CHECK_PROG_MAKE_EXPORT
AC_CHECK_TYPE([__le16],[
	AC_DEFINE([HAVE_LINUX_TYPES_LE16],1,
		[Define if <linux/types.h> defines the type __le16])
],,[#include <linux/types.h>])
AC_CHECK_HEADERS([asm/byteorder.h])
AC_C_INLINE


# Other options
AC_ARG_ENABLE([demo],
	AS_HELP_STRING([--disable-demo], [disable library demo]),,
	[enable_demo=yes])
AM_CONDITIONAL([SQ_WANT_DEMO], [test "x$enable_demo" = xyes])

# The 'make check' tests are only known to work on linux.
enable_tests=no
enable_gtest_tests=no
case "${build_os}" in
    linux*)
        enable_tests=yes
        # Some tests wield google test framework, which is in C++.
        # Check if we can wield those tests.
        AC_PROG_CXX
        AC_LANG_PUSH([C++])
        AC_CHECK_HEADERS([gtest/gtest.h], [enable_gtest_tests=yes])
        AC_LANG_POP([C++])
        ;;
esac
AM_CONDITIONAL([WANT_TESTS], [test "x$enable_tests" = xyes])
AM_CONDITIONAL([WANT_GTEST_TESTS], [test "x$enable_gtest_tests" = xyes])

AS_IF([test "x$enable_high_level$enable_low_level$enable_demo" = xnonono],
	AC_MSG_FAILURE([Nothing left to build]))

AC_ARG_ENABLE([multithreading],
 	AS_HELP_STRING([--enable-multithreading], [enable multi-threaded low-level FUSE driver]),
	[
    AC_CHECK_LIB([pthread], [pthread_mutex_lock], [], AC_MSG_ERROR([libpthread is required for multithreaded build]))
    AC_DEFINE(SQFS_MULTITHREADED, 1, [Enable multi-threaded low-level FUSE driver])
    ])
AM_CONDITIONAL([MULTITHREADED], [test x$enable_multithreading = xyes])

AC_CONFIG_FILES([Makefile squashfuse.pc])
AC_OUTPUT

AS_ECHO()
AS_ECHO(["Compression support ....... :$sq_decompressors"])
AS_ECHO(["High-level FUSE driver .... : $enable_high_level"])
AS_ECHO(["Low-level FUSE driver ..... : $enable_low_level"])
AS_ECHO(["Demo program .............. : $enable_demo"])
AS_ECHO()

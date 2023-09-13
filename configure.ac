AC_INIT([squashfuse], [0.5.0], [dave@vasilevsky.ca])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([config.h])
AH_TOP([
#ifndef SQFS_CONFIG_H
#define SQFS_CONFIG_H
])
AH_BOTTOM([#endif])
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
SQ_CHECK_DECL_SYMLINK

# Decompression
SQ_CHECK_DECOMPRESS([ZLIB],[z],[uncompress],[zlib.h],,[gzip])
SQ_CHECK_DECOMPRESS([XZ],[lzma],[lzma_stream_buffer_decode],[lzma.h],[liblzma],[xz])
SQ_CHECK_DECOMPRESS([LZO],[lzo2],[lzo1x_decompress_safe],[lzo/lzo1x.h],,[lzo])
SQ_CHECK_DECOMPRESS([LZ4],[lz4],[LZ4_decompress_safe],[lz4.h],,[lz4])
SQ_CHECK_DECOMPRESS([ZSTD],[zstd],[ZSTD_decompress],[zstd.h],,[zstd])
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
AC_CHECK_HEADERS([endian.h machine/endian.h], [break])
AC_C_INLINE


# Other options
AC_ARG_ENABLE([demo],
	AS_HELP_STRING([--disable-demo], [disable library demo]),,
	[sq_demo=yes])
AM_CONDITIONAL([SQ_WANT_DEMO], [test "x$sq_demo" = xyes])

# The 'make check' tests are only known to work on linux.
AC_CHECK_PROGS([sq_fusermount],[fusermount3 fusermount],[no])
AC_CHECK_PROG([sq_mksquashfs],[mksquashfs],[yes],[no])
AC_CHECK_PROGS([sq_md5sum],[md5sum gmd5sum],[no])

AS_CASE(["${build_os}__$sq_low_level$sq_fusermount$sq_mksquashfs$sq_md5sum"],
				[linux*__yesfusermount*yes*md5sum],[sq_fuse_tests=yes],
				[darwin*__yes*yes*md5sum],[sq_fuse_tests=yes],
				[freebsd*__yes*yes*md5sum],[sq_fuse_tests=yes],
				[netbsd*__yes*yes*md5sum],[sq_fuse_tests=yes],
				[sq_fuse_tests=no])
AS_IF([test "x$sq_fuse_tests" = xyes],[sq_tests="$sq_tests FUSE"])
AS_IF([test "x$sq_mksquashfs$sq_demo" = xyesyes],[
	sq_demo_tests=yes
	sq_tests="$sq_tests demo"
])
AM_CONDITIONAL([SQ_FUSE_TESTS], [test "x$sq_fuse_tests" = xyes])
AM_CONDITIONAL([SQ_DEMO_TESTS], [test "x$sq_demo_tests" = xyes])
AS_IF([test "x$sq_tests" = x], [sq_tests=" none"])

AC_SUBST([sq_mksquashfs_compressors])
AC_CONFIG_FILES([tests/ll-smoke.sh],[chmod +x tests/ll-smoke.sh])
AC_CONFIG_FILES([tests/umount-test.sh],[chmod +x tests/umount-test.sh])


AS_IF([test "x$sq_high_level$sq_low_level$sq_demo" = xnonono],
	AC_MSG_FAILURE([Nothing left to build]))

AC_ARG_ENABLE([multithreading],
 	AS_HELP_STRING([--disable-multithreading], [disable multi-threaded low-level FUSE driver]),,
    [enable_multithreading="yes"])
AS_IF([test x$enable_multithreading = xyes],
	[
    AC_CHECK_LIB([pthread], [pthread_mutex_lock], [], AC_MSG_ERROR([libpthread is required for multithreaded build]))
    AC_DEFINE(SQFS_MULTITHREADED, 1, [Enable multi-threaded low-level FUSE driver])
    ])
AM_CONDITIONAL([MULTITHREADED], [test x$enable_multithreading = xyes])

AC_ARG_ENABLE([sigterm-handler],
	AS_HELP_STRING([--enable-sigterm-handler], [enable lazy umount on SIGTERM in low-level FUSE driver]),
	[
    AC_CHECK_HEADER([linux/version.h], , [], AC_MSG_ERROR([linux host required for sigterm-handler.]))
    AC_DEFINE(SQFS_SIGTERM_HANDLER, 1, [Enable lazy umount on SIGTERM in low-level FUSE driver])
    ])
AM_CONDITIONAL([SIGTERM_HANDLER], [test x$enable_sigterm_handler = xyes])

AC_SUBST([sq_decompressors])
AC_SUBST([sq_high_level])
AC_SUBST([sq_low_level])
AC_SUBST([sq_demo])
AC_SUBST([sq_tests])
AC_CONFIG_FILES([Makefile squashfuse.pc squashfuse_ll.pc tests/lib.sh ci/features])
AC_OUTPUT

AS_ECHO()
AS_ECHO(["Compression support ....... :$sq_decompressors"])
AS_ECHO(["High-level FUSE driver .... : $sq_high_level"])
AS_ECHO(["Low-level FUSE driver ..... : $sq_low_level"])
AS_ECHO(["Demo program .............. : $sq_demo"])
AS_ECHO(["Tests ..................... :$sq_tests"])
AS_ECHO()

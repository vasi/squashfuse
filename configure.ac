AC_INIT([squashfuse], [0.1.103], [dave@vasilevsky.ca])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([config.h])
AC_CANONICAL_BUILD
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([foreign -Wall subdir-objects])
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
SQ_PROG_CPP_POSIX_2001
SQ_PROG_CC_WALL

AC_DEFINE([_POSIX_C_SOURCE], [200112L], [POSIX 2001 compatibility])
AC_DEFINE([FUSE_USE_VERSION], [26], [Version of FUSE API to use])

# Non-POSIX declarations
SQ_CHECK_DECL_MAKEDEV
SQ_CHECK_DECL_PREAD
SQ_CHECK_DECL_S_IFSOCK
SQ_CHECK_DECL_ENOATTR([:])

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


# Other options
AC_ARG_ENABLE([demo],
	AS_HELP_STRING([--disable-demo], [disable library demo]),,
	[enable_demo=yes])
AM_CONDITIONAL([SQ_WANT_DEMO], [test "x$enable_demo" = xyes])

# The 'make check' tests are only known to work on linux.
AC_CHECK_PROGS([sq_fusermount],[fusermount3 fusermount],[no])
AC_CHECK_PROG([sq_mksquashfs],[mksquashfs],[yes],[no])
AC_CHECK_PROG([sq_md5sum],[md5sum],[yes],[no])

AS_CASE(["${build_os}__$enable_low_level$sq_fusermount$sq_mksquashfs$sq_md5sum"],
				[linux*__yesfusermount*yesyes],[sq_fuse_tests=yes],
				[darwin*__yes*yesyes],[sq_fuse_tests=yes],
				[sq_fuse_tests=no])
AS_IF([test "x$sq_fuse_tests" = xyes],[sq_tests="$sq_tests FUSE"])
AS_IF([test "x$sq_mksquashfs$enable_demo" = xyesyes],[
	sq_demo_tests=yes
	sq_tests="$sq_tests demo"
])
AM_CONDITIONAL([SQ_FUSE_TESTS], [test "x$sq_fuse_tests" = xyes])
AM_CONDITIONAL([SQ_DEMO_TESTS], [test "x$sq_demo_tests" = xyes])
AS_IF([test "x$sq_tests" = x], [sq_tests=" none"])

AC_SUBST([sq_mksquashfs_compressors])
AC_CONFIG_FILES([tests/ll-smoke.sh],[chmod +x tests/ll-smoke.sh])


AS_IF([test "x$enable_high_level$enable_low_level$enable_demo" = xnonono],
	AC_MSG_FAILURE([Nothing left to build]))

AC_CONFIG_FILES([Makefile squashfuse.pc tests/lib.sh])
AC_OUTPUT

AS_ECHO()
AS_ECHO(["Compression support ....... :$sq_decompressors"])
AS_ECHO(["High-level FUSE driver .... : $enable_high_level"])
AS_ECHO(["Low-level FUSE driver ..... : $enable_low_level"])
AS_ECHO(["Demo program .............. : $enable_demo"])
AS_ECHO(["Tests ..................... :$sq_tests"])
AS_ECHO()

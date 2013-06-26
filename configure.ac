AC_INIT([squashfuse], [0.1.99], [dave@vasilevsky.ca])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([config.h])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([foreign -Wall subdir-objects])
LT_INIT

# Compiler
AC_PROG_AWK
AC_PROG_SED
AC_PROG_CPP
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
SQ_CHECK_DECOMPRESS([ZLIB],[z],[uncompress],[zlib.h])
SQ_CHECK_DECOMPRESS([XZ],[lzma],[lzma_stream_buffer_decode],[lzma.h],[liblzma])
SQ_CHECK_DECOMPRESS([LZO],[lzo2],[lzo1x_decompress_safe],[lzo/lzo1x.h])
AS_IF([test "x$sq_decompressors" = x],
	[AC_MSG_FAILURE([At least one decompression library must exist])])

# FUSE
SQ_FIND_FUSE
SQ_FUSE_API_VERSION
SQ_FUSE_API_XATTR_POSITION

# Misc
SQ_CHECK_PROG_MAKE_EXPORT
AC_CHECK_TYPE([__le16],[
	AC_DEFINE([HAVE_LINUX_TYPES_LE16],1,
		[Define if <linux/types.h> defines the type __le16])
],,[#include <linux/types.h>])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

AS_ECHO()
AS_ECHO(["Compression support:$sq_decompressors"])

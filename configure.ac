AC_INIT([squashfuse], [0.2.0], [dave@vasilevsky.ca])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([config.h])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([foreign -Wall subdir-objects])
AM_SILENT_RULES(yes)
AM_PROG_AR
LT_INIT

# Compiler
AC_PROG_AWK
AC_PROG_SED
AC_PROG_CPP
AM_PROG_CC_C_O
SQ_PROG_CPP_POSIX_2001
SQ_PROG_CC_WALL

# Run tests in POSIX mode
CPPFLAGS="$CPPFLAGS -D_POSIX_C_SOURCE=200112L"

# Miscellaneous functions
AC_SYS_LARGEFILE
AC_FUNC_STRERROR_R

# Decompression
SQ_DECOMPRESSORS
SQ_CHECK_DECOMPRESS([ZLIB],[z],[uncompress],[zlib.h])
SQ_CHECK_DECOMPRESS([XZ],[lzma],[lzma_stream_buffer_decode],[lzma.h],[liblzma])
SQ_CHECK_DECOMPRESS([LZO],[lzo2],[lzo1x_decompress_safe],[lzo/lzo1x.h])
SQ_CHECK_DECOMPRESS([LZ4],[lz4],[LZ4_decompress_safe],[lz4.h])
AS_IF([test "x$sq_decompressors" = x],
	[AC_MSG_FAILURE([At least one decompression library must exist])])

# Multithreading
SQ_THREADS

# FUSE
SQ_FUSE_API
AS_IF([test "x$sq_fuse_found" = xyes],[
  SQ_FUSE_API_OPTS
	SQ_FUSE_API_LOWLEVEL
	SQ_FUSE_API_VERSION
	SQ_FUSE_API_XATTR_POSITION
  SQ_FUSE_BREAKAGE
])
SQ_FUSE_RESULT

# Non-POSIX declarations
AS_IF([test "x$sq_fuse_found" = xyes],[
  SQ_CHECK_DECL_MAKEDEV
  SQ_CHECK_DECL_ENOATTR([:])
  SQ_CHECK_DECL_DAEMON
])
SQ_CHECK_DECL_PREAD
SQ_CHECK_DECL_S_IFSOCK
SQ_CHECK_DECL_SETENV
SQ_CHECK_DECL_VSNPRINTF

# Make
SQ_CHECK_PROG_MAKE_EXPORT

# Types
SQ_CHECK_TYPE_LE16
SQ_CHECK_TYPE_OFF_T

# Demo
AC_ARG_ENABLE([demo],
	AS_HELP_STRING([--disable-demo], [disable library demo]),,
	[enable_demo=yes])
AM_CONDITIONAL([SQ_WANT_DEMO], [test "x$enable_demo" = xyes])

# Make sure we have something to do
AS_IF([test "x$enable_high_level$enable_low_level$enable_demo" = xnonono],
	AC_MSG_FAILURE([Nothing left to build]))

# Output
AC_CONFIG_FILES([Makefile])
AC_OUTPUT

# Print some helpful details
AS_ECHO()
AS_ECHO(["Compression support ....... :$sq_decompressors"])
AS_ECHO(["Threading ................. : $enable_threads"])
AS_ECHO(["High-level FUSE driver .... : $enable_high_level"])
AS_ECHO(["Low-level FUSE driver ..... : $enable_low_level"])
AS_ECHO(["Demo program .............. : $enable_demo"])
AS_ECHO()

# Print warnings
AS_IF([test "x$sq_warn_ll" = x],,AC_MSG_WARN([$sq_warn_ll]))
AS_IF([test "x$sq_warn_fuse" = x],,AC_MSG_WARN([$sq_warn_fuse]))
AS_IF([test "x$sq_warn_offt" = x],,AC_MSG_WARN([$sq_warn_offt]))

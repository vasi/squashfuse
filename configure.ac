AC_INIT([squashfuse], [0.1], [dave@vasilevsky.ca])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([foreign -Wall])
LT_INIT
AC_CONFIG_HEADERS([config.h])

# Compiler
AC_PROG_CPP
AC_PROG_CC_C99
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
AS_IF([test "x$sq_decompress_found" = xyes],,
	[AC_MSG_FAILURE([At least one decompression library must exist])])


##### FUSE #####

AC_ARG_WITH(fuse,
	AS_HELP_STRING([--with-fuse=DIR], [FUSE prefix directory]),
	[fuse_dir=$withval])
AC_ARG_WITH(fuse-include,
	AS_HELP_STRING([--with-fuse-include=DIR], [FUSE header directory]),
	[fuse_include=$withval])
AC_ARG_WITH(fuse-lib,
	AS_HELP_STRING([--with-fuse-lib=DIR], [FUSE library directory]),
	[fuse_lib=$withval])

AS_IF([test "x$fuse_dir$fuse_include$fuse_lib" = x],[
	PKG_CHECK_MODULES([fuse],[fuse >= 2.5],[
		LIBS="$LIBS $fuse_LIBS"
		CPPFLAGS="$CPPFLAGS $fuse_CFLAGS"
		fuse_found=yes
	],[ignore=xxx])
])

save_CPPFLAGS=$CPPFLAGS
save_LIBS=$LIBS
for dir in specified usr default local osxfuse
do
	for plat in default off64
	do
		AS_IF([test "x$fuse_found" = xyes],[continue])
		CPPFLAGS="$save_CPPFLAGS"
		LIBS="$save_LIBS"
		
		AS_IF([test "x$dir" = xspecified],[
			AS_IF([test "x$fuse_dir$fuse_include$fuse_lib" = x],[continue])
			AS_IF([test "x$fuse_dir" = x],,[
				iinc="$fuse_dir/include/fuse"
				ilib="$fuse_dir/lib"])
			AS_IF([test "x$fuse_include" = x],,[iinc=$fuse_include])
			AS_IF([test "x$fuse_lib" = x],,[ilib=$fuse_lib])
		])
		AS_IF([test "x$dir" = xusr],[
			ilib=/usr/lib
			iinc=/usr/include/fuse
		])
		AS_IF([test "x$dir" = xlocal || test "x$dir" = xosxfuse],[
			AS_CASE([$target_os],[darwin*],,[continue])
			ilib=/usr/local/lib
			iinc=/usr/local/include/fuse
			AS_IF([test "x$dir" = xosxfuse],
				[iinc=/usr/local/include/osxfuse/fuse])
		])
		AS_IF([test "x$iinc" = x],,[CPPFLAGS="$CPPFLAGS -I$iinc"])
		AS_IF([test "x$ilib" = x],,[LIBS="$LIBS -L$ilib"])
		
		AS_IF([test "x$plat" = xdefault],,[
			CPPFLAGS="$CPPFLAGS -D_FILE_OFFSET_BITS=64"
			AS_CASE([$target_os],[darwin*],[
				CPPFLAGS="$CPPFLAGS -D__FreeBSD__=10 -D_DARWIN_USE_64_BIT_INODE"
			])
		])
		
		AS_UNSET([ac_cv_header_fuse_lowlevel_h]) # don't re-use cache
		AS_UNSET([ac_cv_search_fuse_lowlevel_new])
		
		AC_MSG_NOTICE([checking $plat flags in $dir directories])
		AC_SEARCH_LIBS([fuse_lowlevel_new],[osxfuse fuse_ino64 fuse],[
			AC_CHECK_HEADER([fuse_lowlevel.h],[fuse_found=yes])
		])
	done
done
AS_IF([test "x$fuse_found" = xyes],,[AC_MSG_FAILURE([FUSE not found])])


AC_CHECK_DECLS([fuse_add_direntry,fuse_add_dirent],[found_dirent=yes],,
	[#include <fuse_lowlevel.h>])
AS_IF([test "x$found_dirent" = xyes],,
	[AC_MSG_FAILURE([No way to add directory entries])])

AC_CHECK_DECLS([fuse_daemonize],,
	[SQ_CHECK_NONSTD(daemon,[#include <unistd.h>],[(void)daemon;])],
	[#include <fuse_lowlevel.h>])

AC_CHECK_DECLS([fuse_session_remove_chan],,,[#include <fuse_lowlevel.h>])

AC_MSG_CHECKING([for two-argument fuse_unmount])
AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <fuse_lowlevel.h>],[fuse_unmount(0,0)])],[
	AC_MSG_RESULT(yes)
	AC_DEFINE([HAVE_NEW_FUSE_UNMOUNT],1,
		[Define if we have two-argument fuse_unmount])
],[AC_MSG_RESULT(no)])

AC_MSG_CHECKING([for position argument to FUSE xattr operations])
AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <fuse_lowlevel.h>],[
		struct fuse_lowlevel_ops ops;
		ops.getxattr(0, 0, 0, 0, 0);
	])],[
	AC_MSG_RESULT(yes)
	AC_DEFINE([FUSE_XATTR_POSITION],1,
		[Define if FUSE xattr operations take a position argument])
],[AC_MSG_RESULT(no)])


# Misc
AC_CHECK_HEADERS([linux/types.h])
SQ_CHECK_PROG_MAKE_EXPORT

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

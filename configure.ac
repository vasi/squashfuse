AC_INIT([squashfuse], [0.1], [dave@vasilevsky.ca])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([foreign -Wall])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC_C99
CFLAGS="$CFLAGS -Wall"

AC_DEFINE([_POSIX_C_SOURCE], [200112L], [POSIX 2001 compatibility])
AC_DEFINE([FUSE_USE_VERSION], [26], [Version of FUSE API to use])

##### Non-POSIX declarations #####

# SQ_NONSTD_DEFINE(NAME,PROLOG,SOURCE)
# Check what #define we need for non-POSIX declarations
AC_DEFUN([SQ_NONSTD_DEFINE],[
	AH_TEMPLATE([AS_TR_CPP([NONSTD_]$1[_DEF])],
		[Extra definition needed by non-POSIX ]$1)
	
	AS_VAR_PUSHDEF([sq_cache],[sq_cv_decl_nonstd_$1])
	AC_CACHE_VAL([sq_cache],[
		AS_VAR_SET([sq_cache],[no])
		for def in none _DARWIN_C_SOURCE _XOPEN_SOURCE _BSD_SOURCE _GNU_SOURCE _POSIX_C_SOURCE
		do
			AS_IF([test "x$def" = "x_POSIX_C_SOURCE"],[op=undef],[op=define])
			AS_IF([test "x$def" = "x_XOPEN_SOURCE"],[val=500],[val=1])
			AS_IF([test "x$def" = "xnone"],[
				pref=
				AC_MSG_CHECKING([if $1 works without changes])
			],[
				pref="#$op $def $val"
				AC_MSG_CHECKING([if $1 requires changing $def])
			])
			AC_LINK_IFELSE([AC_LANG_PROGRAM([
					$pref
					$2
				],[$3])
			],[
				AC_MSG_RESULT(yes)
				AS_VAR_SET([sq_cache],[$def])
				break
			],[AC_MSG_RESULT(no)])
		done
	])
	AS_VAR_IF([sq_cache],[no],
		[AC_MSG_FAILURE([can't figure out how to use $1])],
		[AS_VAR_IF([sq_cache],[none],[],[
			AC_DEFINE_UNQUOTED([AS_TR_CPP([NONSTD_$1_DEF])],[CHANGE$def],[])]
		)]
	)
	AS_VAR_POPDEF([sq_cache])
])

SQ_NONSTD_DEFINE(makedev,[#include <sys/types.h>],[makedev(0,0)])
SQ_NONSTD_DEFINE(pread,[#include <unistd.h>],[(void)pread;])
SQ_NONSTD_DEFINE(S_IFDIR,[#include <sys/stat.h>],[mode_t m = S_IFDIR;])


##### Compression #####

# SQ_CHECK_DECOMPRESS(NAME,LIBRARY,FUNCTION,HEADER)
# Check for a decompression library
AC_DEFUN([SQ_CHECK_DECOMPRESS],[
	sq_check=yes
	AC_ARG_WITH($1,
		AS_HELP_STRING([--with-]$1[=DIR],$1[ prefix directory]),[
		AS_IF([test "x$withval" = xno],[
			sq_check=no
		],[
			CPPFLAGS="$CPPFLAGS -I$withval/include"
			LIBS="$LIBS -L$withval/lib"
		])
	])
	
	AS_IF([test "x$sq_check" = xyes],[
		AC_SEARCH_LIBS($3,$2,[
			AC_CHECK_HEADERS($4,[decomp_found=yes])
		])
	])
])

SQ_CHECK_DECOMPRESS([zlib],[z],[uncompress],[zlib.h])
SQ_CHECK_DECOMPRESS([xz],[lzma],[lzma_stream_buffer_decode],[lzma.h])
SQ_CHECK_DECOMPRESS([lzo],[lzo2],[lzo1x_decompress_safe],[lzo/lzo1x.h])
AS_IF([test "x$decomp_found" = xyes],,
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
	PKG_CHECK_EXISTS(fuse)
	PKG_CHECK_MODULES([PKGCONFIG_FUSE],[fuse >= 2.5],[
		LIBS="$LIBS $PKGCONFIG_FUSE_LIBS"
		CFLAGS="$CFLAGS $PKGCONFIG_FUSE_CFLAGS"
		fuse_found=yes
	],[ignore=xxx])
])

save_CPPFLAGS=$CPPFLAGS
save_LIBS=$LIBS
for dir in user default local osxfuse
do
	for plat in default off64
	do
		AS_IF([test "x$fuse_found" = xyes],[continue])
		CPPFLAGS="$save_CPPFLAGS"
		LIBS="$save_LIBS"
		
		AS_IF([test "x$dir" = xuser],[
			AS_IF([test "x$fuse_dir$fuse_include$fuse_lib" = x],[continue])
			AS_IF([test "x$fuse_dir" = x],,[
				iinc="$fuse_dir/include/fuse"
				ilib="$fuse_dir/lib"])
			AS_IF([test "x$fuse_include" = x],,[iinc=$fuse_include])
			AS_IF([test "x$fuse_lib" = x],,[ilib=$fuse_lib])
		])
		AS_IF([test "x$dir" = xlocal || test "x$dir" = xosxfuse],[
			AS_CASE([$target_os],[darwin*],,[continue])
			ilib=/usr/local/lib
			iinc=/usr/local/include/fuse
			AS_IF([test "x$dir" = xosxfuse],[iinc=/usr/local/include/osxfuse/fuse])
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
	[SQ_NONSTD_DEFINE(daemon,[#include <unistd.h>],[(void)daemon;])],
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


###### Can we suppress AppleDouble files? #####

AC_MSG_CHECKING([if ${MAKE-make} supports export])
cat > confmak <<'END'
export FOO=1
all:
END
AS_IF([${MAKE-make} -f confmak >/dev/null 2>/dev/null],[gnu_make=yes],[gnu_make=no])
rm -f confmak
AC_MSG_RESULT([$gnu_make])
AM_CONDITIONAL([GNU_MAKE],[test "x$gnu_make" == xyes])


AC_CONFIG_FILES([Makefile])
AC_OUTPUT

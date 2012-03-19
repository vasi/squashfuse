AC_INIT([squashfuse], [0.1], [dave@vasilevsky.ca])
AM_INIT_AUTOMAKE([foreign -Wall])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC_C99
CFLAGS="$CFLAGS -Wall"

AC_DEFINE([_POSIX_C_SOURCE], [200112L], [POSIX 2001 compatibility])


##### Non-POSIX declarations #####

# SQ_NONSTD_DEFINE(NAME,PROLOG,SOURCE)
# Check what #define we need for non-POSIX declarations
AC_DEFUN([SQ_NONSTD_DEFINE],[
	AH_TEMPLATE([AS_TR_CPP([NONSTD_]$1[_DEF])],
		[Change needed by non-POSIX ]$1)
	
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
			AC_DEFINE_UNQUOTED(AS_TR_CPP([NONSTD_$1_DEF]),[CHANGE$def],[])]
		)]
	)
	AS_VAR_POPDEF([sq_cache])
])

SQ_NONSTD_DEFINE(makedev,[#include <sys/types.h>],[makedev(0,0)])
SQ_NONSTD_DEFINE(pread,[#include <unistd.h>],[(void)pread;])
SQ_NONSTD_DEFINE(S_IFDIR,[#include <sys/stat.h>],[mode_t m = S_IFDIR;])


##### zlib #####

AC_CHECK_LIB(z, [uncompress],, AC_MSG_FAILURE([zlib not found]))
AC_CHECK_HEADER([zlib.h],, AC_MSG_FAILURE([zlib.h not found]))


##### FUSE #####

PKG_CHECK_EXISTS(fuse)
PKG_CHECK_MODULES([PKGCONFIG_FUSE],[fuse >= 2.5],[
	LIBS="$LIBS $PKGCONFIG_FUSE_LIBS"
	CFLAGS="$CFLAGS $PKGCONFIG_FUSE_CFLAGS"
],[
	# If we don't have pkg-config, try --with-fuse, default to /usr/local

	AC_ARG_WITH(fuse,
		AS_HELP_STRING([--with-fuse=DIR], [FUSE prefix directory]),
		[fuse_dir=$withval], [fuse_dir=/usr/local])
	
	LIBS="$LIBS -L$fuse_dir/lib"
	CPPFLAGS="$CPPFLAGS -I$fuse_dir/include/fuse"
	
	AC_SEARCH_LIBS(fuse_lowlevel_new,[fuse_ino64 fuse],
		[LIBS="$LIBS $ac_cv_search_fuse_lowlevel_new"],
		AC_MSG_FAILURE([FUSE not found]))
	AC_CHECK_HEADER([fuse_lowlevel.h],,[
		# Try harder by adding some #defines, on OS X this may be required
		
		CPPFLAGS="$CPPFLAGS -D_FILE_OFFSET_BITS=64 -D__FreeBSD__=10 -DFUSE_USE_VERSION=26"
		AS_UNSET(ac_cv_header_fuse_lowlevel_h) # don't re-use cache
		AC_CHECK_HEADER([fuse_lowlevel.h],,
			[AC_MSG_FAILURE([fuse_lowlevel.h not found])])
	])
])

AC_CHECK_DECLS([fuse_add_direntry,fuse_add_dirent],[found_dirent=yes],,
	[#include <fuse_lowlevel.h>])
AS_IF([test "x$found_dirent" = xyes],,
	[AC_MSG_FAILURE([No way to add directory entries])])


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

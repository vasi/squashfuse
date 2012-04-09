# SQ_PROG_CPP_POSIX_2001
#
# Check if a preprocessor flag is needed for POSIX-2001 headers.
# Needed at least on Solaris and derivatives.
AC_DEFUN([SQ_PROG_CPP_POSIX_2001],[
AC_CACHE_CHECK([for option for POSIX-2001 preprocessor], 
	[sq_cv_prog_cpp_posix2001],
[
	sq_cv_prog_cpp_posix2001=unknown
	sq_save_CPPFLAGS=$CPPFLAGS
	for sq_flags in none -std=gnu99 -xc99=all
	do
		AS_IF([test "x$sq_flags" = xnone],,
			[CPPFLAGS="$save_CPPFLAGS $sq_flags"])
		AC_PREPROC_IFELSE([AC_LANG_PROGRAM([
			#define _POSIX_C_SOURCE 200112L
			#include <sys/types.h>
		])],[
			sq_cv_prog_cpp_posix2001=$sq_flags
			break
		])
	done
	CPPFLAGS=$sq_save_CPPFLAGS
])
AS_IF([test "x$sq_cv_prog_cpp_posix2001" = xunknown],
	[AC_MSG_FAILURE([can't preprocess for POSIX-2001])],
	[AS_IF([test "x$sq_cv_prog_cpp_posix2001" = xnone],,
		CPPFLAGS="$CPPFLAGS $sq_cv_prog_cpp_posix2001")
])
])

# SQ_PROG_CC_WALL
#
# Check if -Wall is supported
AC_DEFUN([SQ_PROG_CC_WALL],[
AC_CACHE_CHECK([how to enable all compiler warnings],
	[sq_cv_prog_cc_wall],
[
	sq_cv_prog_cc_wall=unknown
	sq_save_CFLAGS=$CFLAGS
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM(,)],[sq_cv_prog_cc_wall="-Wall"])
])
AS_IF([test "x$sq_cv_prog_cc_wall" = xunknown],,
	[CFLAGS="$CFLAGS $sq_cv_prog_cc_wall"])
])
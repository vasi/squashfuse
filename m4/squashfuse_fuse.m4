# Copyright (c) 2012 Dave Vasilevsky <dave@vasilevsky.ca>
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# SQ_CHECK_FUSE(LIBS,[IF-FOUND],[IF-NOT-FOUND])
#
# Check if FUSE low-level compiles and links correctly.
AC_DEFUN([SQ_CHECK_FUSE],[
	sq_fuse_ok=yes
	AS_VAR_PUSHDEF([sq_cv_lib],[sq_cv_lib_fuse_""$1""_""$LIBS])
	AC_CACHE_CHECK([for FUSE library],[sq_cv_lib],[
		for sq_lib in '' $1
		do
			SQ_SAVE_FLAGS
			LIBS="$LIBS -l$sq_lib"
			AC_LINK_IFELSE([AC_LANG_CALL(,[fuse_lowlevel_new])],[
				AS_VAR_SET([sq_cv_lib],[-l$sq_lib])
			])
			SQ_RESTORE_FLAGS
			AS_VAR_SET_IF([sq_cv_lib],[break])
		done
		AS_VAR_SET_IF([sq_cv_lib],,[AS_VAR_SET([sq_cv_lib],[no])])
	])
	AS_VAR_IF([sq_cv_lib],[no],[sq_fuse_ok=no])
	
	AS_VAR_PUSHDEF([sq_cv_hdr],[sq_cv_header_fuse_""$CPPFLAGS])
	AS_IF([test "x$sq_fuse_ok" = "xno"],,[
		AC_CACHE_CHECK([for FUSE header],[sq_cv_hdr],[
			AC_COMPILE_IFELSE([AC_LANG_PROGRAM([#include <fuse_lowlevel.h>])],
				[AS_VAR_SET([sq_cv_hdr],[yes])],
				[AS_VAR_SET([sq_cv_hdr],[no])]
			)
		])
	])
	AS_VAR_IF([sq_cv_hdr],[yes],,[sq_fuse_ok=no])
	
	AS_IF([test "x$sq_fuse_ok" = "xno"],[$3],[
		AS_VAR_COPY([sq_lib_res],[sq_cv_lib])
		LIBS="$LIBS $sq_lib_res"
		$4
	])
	AS_VAR_POPDEF([sq_cv_lib])
	AS_VAR_POPDEF([sq_cv_hdr])
])

# SQ_FIND_FUSE([IF-FOUND],[IF-NOT-FOUND])
#
# Find the FUSE library
AC_DEFUN([SQ_FIND_FUSE],[
	SQ_CHECK_FUSE([fuse_ino64 fuse],,
		[AC_MSG_FAILURE([Can't find FUSE library and headers])])
])

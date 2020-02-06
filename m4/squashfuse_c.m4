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

# SQ_PROG_CC_WALL
#
# Check if -Wall is supported
AC_DEFUN([SQ_PROG_CC_WALL],[
AC_CACHE_CHECK([how to enable all compiler warnings],
	[sq_cv_prog_cc_wall],
[
	sq_cv_prog_cc_wall=unknown
	sq_save_CFLAGS=$CFLAGS
	CFLAGS="$CFLAGS -Wall"
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM(,)],[sq_cv_prog_cc_wall="-Wall"])
	CFLAGS=$sq_save_CFLAGS
])
AS_IF([test "x$sq_cv_prog_cc_wall" = xunknown],,
	[AC_SUBST([AM_CFLAGS],["$AM_CFLAGS $sq_cv_prog_cc_wall"])])
])

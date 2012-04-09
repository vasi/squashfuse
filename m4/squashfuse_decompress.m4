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

# SQ_CHECK_DECOMPRESS(NAME, LIBRARY, FUNCTION, HEADER, [PKGCONFIG])
#
# Check for a decompression library with the given library name, function and
# header. If given pkg-config package name, also look using pkg-config.
#
# On success, set sq_decompress_found to yes, and modify CPPFLAGS and LIBS.
AC_DEFUN([SQ_CHECK_DECOMPRESS],[
	sq_check=yes
	AC_ARG_WITH($1,
		AS_HELP_STRING([--with-]$1[=DIR],$1[ prefix directory]),[
		AS_IF([test "x$withval" = xno],[
			sq_check=no
		],[
			sq_pkg=no
			CPPFLAGS="$CPPFLAGS -I$withval/include"
			LIBS="$LIBS -L$withval/lib"
		])
	])
	
	AS_IF([test "x$sq_check" = xyes],[
		m4_ifval($5,[AS_IF([test "x$sq_pkg" = xno],,[
			PKG_CHECK_MODULES($5,$5,[
				LIBS="$LIBS $]$5[_LIBS"
				CPPFLAGS="$CPPFLAGS $]$5[_CFLAGS"
			],[:])
		])])
		AC_SEARCH_LIBS($3,$2,[AC_CHECK_HEADERS($4,[sq_decompress_found=yes])])
	])
])

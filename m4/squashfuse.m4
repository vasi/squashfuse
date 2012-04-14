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

# SQ_CHECK_PROG_MAKE_EXPORT
#
# Check if make supports exporting variables. Define the MAKE_EXPORT
# conditional on success.
AC_DEFUN([SQ_CHECK_PROG_MAKE_EXPORT],[
AC_CACHE_CHECK([if ${MAKE-make} supports export], [sq_cv_prog_make_export],[
	sq_cv_prog_make_export=no
	cat > confmak <<'END'
export FOO=1
all:
END
	AS_IF([${MAKE-make} -f confmak >/dev/null 2>/dev/null],
		[sq_cv_prog_make_export=yes])
	rm -f confmak
])
AM_CONDITIONAL([MAKE_EXPORT],[test "x$sq_cv_prog_make_export" == xyes])
])


# SQ_SUFFIX(STRING, PREFIX)
#
# If STRING starts with PREFIX, return the part of string after the PREFIX.
# Otherwise, return the original string.
AC_DEFUN([SQ_SUFFIX],[
	`echo | $AWK '{ i=index(v,o); if(i==1){print substr(v,i+length(o))}else{print v} }' v="$1" o="$2"`
])

# SQ_SAVE_FLAGS
# SQ_RESTORE_FLAGS([PREFIX])
#
# Save and restore compiler flags. If PREFIX is given, substitute
# variables containing the changes in the flags. Eg: If saved when LIBS="foo",
# and restored when LIBS="foo bar", PREFIX_LIBS would be set to "bar".
AC_DEFUN([SQ_SAVE_FLAGS],[
	sq_save_LIBS=$LIBS
	sq_save_CPPFLAGS=$CPPFLAGS
])
AC_DEFUN([SQ_RESTORE_FLAGS],[
	m4_ifval($1,[
		m4_foreach_w([sq_flag],[LIBS CPPFLAGS],[
			AS_VAR_PUSHDEF([sq_saved],[sq_save_]sq_flag)
			AS_VAR_PUSHDEF([sq_tgt],$1[_]sq_flag)
			AS_VAR_SET([sq_tgt], [SQ_SUFFIX([$sq_flag],[$sq_saved])])
			AC_SUBST(sq_tgt)
			AS_VAR_POPDEF([sq_saved])
			AS_VAR_POPDEF([sq_tgt])
		])
	])
	LIBS=$sq_save_LIBS
	CPPFLAGS=$sq_save_CPPFLAGS
])


# SQ_PKG(NAME, PKG, [IF-FOUND], [IF-NOT-FOUND])
#
# Like PKG_CHECK_MODULES, but sets non-prefixed LIBS and CPPFLAGS
AC_DEFUN([SQ_PKG],[
	AS_VAR_PUSHDEF([sq_pkg],[pkgconfig_]$1)
	PKG_CHECK_MODULES(sq_pkg,[$2],[
		LIBS="$LIBS $[]pkgconfig_[]$1[]_LIBS"
		# yes, CFLAGS, we want the preprocessor to work
		CPPFLAGS="$CPPFLAGS $[]pkgconfig_[]$1[]_CFLAGS"
		$3
	],[$4])
])

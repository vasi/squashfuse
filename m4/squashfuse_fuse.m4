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

# SQ_TRY_FUSE_LIBS(LIBGROUPS, LDFLAGS, [IF-FOUND], [IF-NOT-FOUND])
#
# Check if any of the space-separated library groups can successfully link
# a FUSE driver. A library group is a colon-separated list of libraries.
# On success, set sq_fuse_ok=yes and add the required linker flags to $LIBS.
AC_DEFUN([SQ_TRY_FUSE_LIBS],[
  sq_fuse_ok=yes
  AS_VAR_PUSHDEF([sq_cv_lib],[sq_cv_lib_fuse_""$1""_""$LIBS])
  AC_CACHE_CHECK([for FUSE library],[sq_cv_lib],[
    for sq_lib in '' $1
    do
      SQ_SAVE_FLAGS
      SQ_SPLIT(sq_lib_add,$sq_lib,[:],[-l],,[ ])
      AS_IF([test "x$2" = x],,[sq_lib_add="$sq_lib_add $2"])
      AS_IF([test "x$sq_lib_add" = x],,[LIBS="$LIBS $sq_lib_add"])
      AC_LINK_IFELSE([AC_LANG_CALL(,[fuse_get_context])],[
        AS_IF([test "x$sq_lib" = x],[sq_lib_out="already present"],
          [sq_lib_out="$sq_lib_add"])
        AS_VAR_SET([sq_cv_lib],[$sq_lib_out])
      ])
      SQ_RESTORE_FLAGS
      AS_VAR_SET_IF([sq_cv_lib],[break])
    done
    AS_VAR_SET_IF([sq_cv_lib],,[AS_VAR_SET([sq_cv_lib],[no])])
  ])
  AS_VAR_IF([sq_cv_lib],[no],[sq_fuse_ok=no])
  
  AS_IF([test "x$sq_fuse_ok" = "xno"],,[
    AS_VAR_PUSHDEF([sq_cv_hdr],[sq_cv_header_fuse_""$CPPFLAGS])
    AC_CACHE_CHECK([for FUSE header],[sq_cv_hdr],[
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
          #include <fuse.h>
          #include <fuse_opt.h>
        ])],
        [AS_VAR_SET([sq_cv_hdr],[yes])],
        [AS_VAR_SET([sq_cv_hdr],[no])]
      )
    ])
    AS_VAR_IF([sq_cv_hdr],[yes],,[sq_fuse_ok=no])
    AS_VAR_POPDEF([sq_cv_hdr])
  ])
  
  AS_IF([test "x$sq_fuse_ok" = "xno"],[$4],[
    AS_VAR_COPY([sq_lib_res],[sq_cv_lib])
    AS_IF([test "x$sq_lib_res" = "xalready present"],,
      [LIBS="$LIBS $sq_lib_res"])
    $3
  ])
  AS_VAR_POPDEF([sq_cv_lib])
])

# SQ_TRY_FUSE_DIRS(NAME, INCDIR, LIBDIR, LIBGROUPS, [IF-FOUND], [IF-NOT-FOUND])
#
# Check if the FUSE headers and libraries are in the given directories.
# Set sq_fuse_ok=yes on success, and set FUSE_CPPFLAGS and FUSE_LIBS.
AC_DEFUN([SQ_TRY_FUSE_DIRS],[
  AS_IF([test "x$sq_fuse_found" = xyes],,[
    AS_IF([test "x$1" = x],,[AC_MSG_NOTICE([Checking for FUSE in $1])])
    
    SQ_SAVE_FLAGS
    AS_IF([test "x$2" = x],,[CPPFLAGS="$CPPFLAGS -I$2"])
    AS_IF([test "x$3" = x],,[LIBS="$LIBS -L$3"])
    CPPFLAGS="$CPPFLAGS $sq_fuse_cppflags"
    SQ_TRY_FUSE_LIBS($4,[$sq_fuse_ldflags],[sq_fuse_found=yes],
      [sq_fuse_found=])
    SQ_KEEP_FLAGS([FUSE],[$sq_fuse_found])
    AS_IF([test "x$sq_fuse_found" = xyes],$5,$6)
  ])
])

# SQ_SEARCH_FUSE_DIRS
#
# Nobody told us where FUSE is, search some common places.
AC_DEFUN([SQ_SEARCH_FUSE_DIRS],[
  AS_CASE([$target_os],[darwin*],[
    SQ_TRY_FUSE_DIRS([OSXFUSE directories],
      [/usr/local/include/osxfuse/fuse],[/usr/local/lib],
      [$sq_fuse_libs])
  ])
  AS_CASE([$target_os],[haiku*],[
    SQ_TRY_FUSE_DIRS([Haiku directories],
      [/boot/develop/headers/userlandfs/fuse],,[$sq_fuse_libs])
  ])
  SQ_TRY_FUSE_DIRS([default directories],,,[$sq_fuse_libs])
  SQ_TRY_FUSE_DIRS([/usr],[/usr/include/fuse],[/usr/lib],[$sq_fuse_libs])
  SQ_TRY_FUSE_DIRS([/usr/local],[/usr/local/include/fuse],[/usr/local/lib],
    [$sq_fuse_libs])
  
  AS_IF([test "x$sq_fuse_found" = xyes],[
    sq_cv_lib_fuse_LIBS="$FUSE_LIBS"
    sq_cv_lib_fuse_CPPFLAGS="$FUSE_CPPFLAGS"
  ],[sq_cv_lib_fuse_LIBS=no])
])

# SQ_FUSE_OS_SPECIFIC
#
# Setup OS-specific flags for FUSE
AC_DEFUN([SQ_FUSE_OS_SPECIFIC],[
  # Some version of MacFUSE require some define's before inclusion
  AS_CASE([$target_os],[darwin*],[
    sq_fuse_cppflags="$sq_fuse_cppflags -D__FreeBSD__=10 -D_DARWIN_USE_64_BIT_INODE"
    # So many FUSE implementations!
    sq_fuse_libs="osxfuse fuse4x fuse_ino64 $sq_fuse_libs"
  ])
  
  # 'refuse' requires _NETBSD_SOURCE, not POSIX
  AS_CASE([$target_os],[netbsd*|minix*],[
    sq_fuse_cppflags="$sq_fuse_cppflags -U_POSIX_C_SOURCE -D_NETBSD_SOURCE"
  ])
  
  # Minix needs to be static, to wrap main(), and a ton of libraries
  AS_CASE([$target_os],[minix*],[
    sq_fuse_ldflags="-Xcompiler -static -Wl,-wrap,main -lminixfs -lsys -lminlib"
  ])
  
  # Haiku needs POSIX error compatibility
  AS_CASE([$target_os],[haiku*],[
    sq_fuse_cppflags="$sq_fuse_cppflags -DB_USE_POSITIVE_POSIX_ERRORS=1"
    sq_fuse_libs="$sq_fuse_libs userlandfs_fuse:posix_error_mapper"
  ])
])

# SQ_FIND_FUSE
#
# Find the FUSE library
AC_DEFUN([SQ_FIND_FUSE],[
  AC_DEFINE([FUSE_USE_VERSION], [26], [Version of FUSE API to use])

  # FUSE headers usually demand _FILE_OFFSET_BITS=64
  sq_fuse_cppflags="-D_FILE_OFFSET_BITS=64"
  sq_fuse_libs="fuse refuse:puffs"
  
  SQ_FUSE_OS_SPECIFIC

  AC_ARG_WITH(fuse-cppflags,
    AS_HELP_STRING([--with-fuse-cppflags=FLAGS], [FUSE compiler flags]),
    [sq_fuse_cppflags="$withval"])
  AC_ARG_WITH(fuse-ldflags,
    AS_HELP_STRING([--with-fuse-ldflags=FLAGS], [FUSE linker flags]),
    [sq_fuse_ldflags="$withval"])
  
  AC_ARG_WITH(fuse-soname,
    AS_HELP_STRING([--with-fuse-soname=SONAME], [FUSE library name]),
    [sq_fuse_libs="$withval"])
  sq_fuse_found=
  
  # Specified in arguments
  AC_ARG_WITH(fuse,
    AS_HELP_STRING([--with-fuse=DIR], [FUSE prefix directory]),[
    fuse_inc="$withval/include/fuse"
    fuse_lib="$withval/lib"
  ])
  
  AC_ARG_WITH(fuse-include,
    AS_HELP_STRING([--with-fuse-include=DIR], [FUSE header directory]),
    [fuse_inc=$withval])
  AC_ARG_WITH(fuse-lib,
    AS_HELP_STRING([--with-fuse-lib=DIR], [FUSE library directory]),
    [fuse_lib=$withval])
  AS_IF([test "x$fuse_inc$fuse_lib" = x],,[
    SQ_TRY_FUSE_DIRS(,[$fuse_inc],[$fuse_lib],[$sq_fuse_libs],,
      [AC_MSG_FAILURE([Can't find FUSE in specified directories])])
  ])
  
  # pkgconfig
  AS_IF([test "x$sq_fuse_found" = xyes],,[
    SQ_SAVE_FLAGS
    SQ_PKG([fuse],[fuse >= 2.5],
      [SQ_TRY_FUSE_LIBS(,,[sq_fuse_found=yes],
        [AC_MSG_FAILURE([Can't find FUSE with pkgconfig])])],
      [:])
    SQ_KEEP_FLAGS([FUSE],[$sq_fuse_found])
  ])
  
  # Default search locations
  AS_IF([test "x$sq_cv_lib_fuse_LIBS" = x],[SQ_SEARCH_FUSE_DIRS],[
    AS_IF([test "x$sq_cv_lib_fuse_LIBS" = xno],,[
      AC_CACHE_CHECK([FUSE libraries],[sq_cv_lib_fuse_LIBS])
      AC_CACHE_CHECK([FUSE preprocessor flags],[sq_cv_lib_fuse_CPPFLAGS])
      FUSE_LIBS=$sq_cv_lib_fuse_LIBS
      FUSE_CPPFLAGS=$sq_cv_lib_fuse_CPPFLAGS
      sq_fuse_found=yes
    ])
  ])
  
  AS_IF([test "x$sq_fuse_found" = xyes],,
    [AC_MSG_FAILURE([Can't find FUSE])])
])

# SQ_FUSE_API
#
# Check for the high-level FUSE API
AC_DEFUN([SQ_FUSE_API],[
  AC_ARG_ENABLE([high-level],
    AS_HELP_STRING([--disable-high-level], [disable high-level FUSE driver]),,
    [enable_high_level=yes])
  AC_ARG_ENABLE([low-level],
    AS_HELP_STRING([--disable-low-level], [disable low-level FUSE driver]),,
    [enable_low_level=check])
  AC_ARG_ENABLE(fuse,
    AS_HELP_STRING([--disable-fuse], [disable all FUSE drivers]))
  AS_IF([test "x$enable_fuse" = xno],[
    enable_high_level=no
    enable_low_level=no
  ])

  AS_IF([test "x$enable_high_level$enable_low_level" = xnono],,[SQ_FIND_FUSE])
])

# SQ_FUSE_API_LOWLEVEL
#
# Check if we have the low-level FUSE API available
AC_DEFUN([SQ_FUSE_API_LOWLEVEL],[
  AS_IF([test "x$enable_low_level" = xno],,[
    SQ_SAVE_FLAGS
    LIBS="$LIBS $FUSE_LIBS"
    CPPFLAGS="$CPPFLAGS $FUSE_CPPFLAGS"
  
    sq_fuse_lowlevel_found=yes
    AC_CHECK_DECL([fuse_lowlevel_new],,[sq_fuse_lowlevel_found=no],
      [#include <fuse_lowlevel.h>])
    AC_CHECK_FUNC([fuse_lowlevel_new],,[sq_fuse_lowlevel_found=no])
  
    SQ_RESTORE_FLAGS
    
    AS_IF([test "x$sq_fuse_lowlevel_found" = xno],[
      sq_err="The low-level FUSE API is not available"
      AS_IF([test "x$enable_low_level" = xyes],[AC_MSG_FAILURE($sq_err)],
        [sq_warn_ll=$sq_err])
    ])
    enable_low_level="$sq_fuse_lowlevel_found"
  ])
])

# SQ_FUSE_RESULT
#
# Handle the results of FUSE checks
AC_DEFUN([SQ_FUSE_RESULT],[
  AS_IF([test "x$enable_high_level$enable_low_level" = xnono],[
    sq_warn_fuse="Without any FUSE support, you will not be able to mount squashfs archives"
  ])
  AM_CONDITIONAL([SQ_WANT_HIGHLEVEL], [test "x$enable_high_level" = xyes])
  AM_CONDITIONAL([SQ_WANT_LOWLEVEL], [test "x$enable_low_level" = xyes])
])

# SQ_FUSE_API_VERSION
#
# Check various things that are different in different versions of FUSE
AC_DEFUN([SQ_FUSE_API_VERSION],[
  SQ_SAVE_FLAGS
  LIBS="$LIBS $FUSE_LIBS"
  CPPFLAGS="$CPPFLAGS $FUSE_CPPFLAGS"
  
  AC_CACHE_CHECK([for user_data in high-level FUSE init],
    [sq_cv_decl_fuse_user_data],[
    AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <fuse.h>],[
        struct fuse_operations ops;
        ops.init(NULL);
      ])],
      [sq_cv_decl_fuse_user_data=yes],
      [sq_cv_decl_fuse_user_data=no])
  ])
  AS_IF([test "x$sq_cv_decl_fuse_user_data" = xyes],[
    AC_DEFINE([HAVE_FUSE_INIT_USER_DATA],1,
        [Define if op.init() takes a user_data parameter])
  ])

  AS_IF([test "x$enable_low_level" = xyes],[
    AC_CHECK_DECLS([fuse_add_direntry,fuse_add_dirent],[found_dirent=yes],,
      [#include <fuse_lowlevel.h>])
    AS_IF([test "x$found_dirent" = xyes],,
      [AC_MSG_FAILURE([No way to add directory entries])])

    AC_CHECK_DECLS([fuse_daemonize],,
      [SQ_CHECK_NONSTD(daemon,[#include <unistd.h>],[(void)daemon;])],
      [#include <fuse_lowlevel.h>])

    AC_CHECK_DECLS([fuse_session_remove_chan],,,
      [#include <fuse_lowlevel.h>])
  
    AC_CACHE_CHECK([for two-argument fuse_unmount],
        [sq_cv_decl_fuse_unmount_two_arg],[
      AC_LINK_IFELSE(
        [AC_LANG_PROGRAM([#include <fuse_lowlevel.h>],
          [fuse_unmount(0,0)])],
        [sq_cv_decl_fuse_unmount_two_arg=yes],
        [sq_cv_decl_fuse_unmount_two_arg=no])
    ])
    AS_IF([test "x$sq_cv_decl_fuse_unmount_two_arg" = xyes],[
      AC_DEFINE([HAVE_NEW_FUSE_UNMOUNT],1,
          [Define if we have two-argument fuse_unmount])
    ])
  ])
  
  SQ_RESTORE_FLAGS
])

# SQ_FUSE_API_OPTS
#
# Check which FUSE option parsing routines are available
AC_DEFUN([SQ_FUSE_API_OPTS],[
  SQ_SAVE_FLAGS
  LIBS="$LIBS $FUSE_LIBS"
  CPPFLAGS="$CPPFLAGS $FUSE_CPPFLAGS"
  
  AC_CACHE_CHECK([for fuse_parse_cmdline],
    [sq_cv_decl_fuse_parse_cmdline],[
    AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <fuse.h>],[
        fuse_parse_cmdline(NULL, NULL, NULL, NULL);
      ])],
      [sq_cv_decl_fuse_parse_cmdline=yes],
      [sq_cv_decl_fuse_parse_cmdline=no])
  ])
  AS_IF([test "x$sq_cv_decl_fuse_parse_cmdline" = xyes],[
    AC_DEFINE([FUSE_PARSE_CMDLINE],1,
      [Define if fuse_parse_cmdline() is available])
  ])
  
  AC_CHECK_FUNCS([fuse_opt_parse])
  
  SQ_RESTORE_FLAGS
])

# SQ_FUSE_API_XATTR_POSITION
#
# Check for OS X's special flag to getxattr.
AC_DEFUN([SQ_FUSE_API_XATTR_POSITION],[
  SQ_SAVE_FLAGS
  LIBS="$LIBS $FUSE_LIBS"
  CPPFLAGS="$CPPFLAGS $FUSE_CPPFLAGS"
  
  AC_CACHE_CHECK([for position argument to FUSE xattr operations],
    [sq_cv_decl_fuse_xattr_position],[
    AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <fuse.h>],[
        struct fuse_operations ops;
        ops.getxattr(0, 0, 0, 0, 0);
      ])],
      [sq_cv_decl_fuse_xattr_position=yes],
      [sq_cv_decl_fuse_xattr_position=no])
  ])
  AS_IF([test "x$sq_cv_decl_fuse_xattr_position" = xyes],[
    AC_DEFINE([FUSE_XATTR_POSITION],1,
      [Define if FUSE xattr operations take a position argument])
  ])
  
  SQ_RESTORE_FLAGS
])

# SQ_FUSE_PLATFORM([MACRO],[PLATFORMS],[MSG])
#
# Check if the target matches PLATFORMS, defining MACRO if so
AC_DEFUN([SQ_FUSE_PLATFORM],[
  AC_MSG_CHECKING([if ]$3)
  AS_CASE([$target_os],$2,[
    AC_DEFINE($1,1,[Define if ]$3)
    AC_MSG_RESULT([yes])
  ],[AC_MSG_RESULT([no])])
])

# SQ_FUSE_BREAKAGE
#
# Set defines for things that are known to be broken at runtime.
# We can't practically test for this at configure time, we just hardcode
# per target.
AC_DEFUN([SQ_FUSE_BREAKAGE],[
  SQ_FUSE_PLATFORM([SQFS_CONTEXT_BROKEN],[minix*|haiku*],
    [fuse_get_context() returns garbage])
  SQ_FUSE_PLATFORM([SQFS_NO_POSITIONAL_ARGS],[minix*],
    [FUSE can't access positional arguments])
  SQ_FUSE_PLATFORM([SQFS_READDIR_NO_OFFSET],[openbsd*|gnu*|*qnx*],
    [FUSE readdir() callback can't use offsets])
  SQ_FUSE_PLATFORM([SQFS_OPEN_BAD_FLAGS],[*qnx*],
    [FUSE open() callback flags are garbage])
  SQ_FUSE_PLATFORM([SQFS_MUST_ALLOW_OTHER],[*qnx*],
    [FUSE requires the allow_other option])
])

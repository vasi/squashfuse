# Copyright (c) 2014 Dave Vasilevsky <dave@vasilevsky.ca>
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

# SQ_THREADS
#
# Check if/how we want to enable threading support
AC_DEFUN([SQ_THREADS],[
  AC_ARG_ENABLE([threads],
    AS_HELP_STRING([--disable-threads], [disable thread support]),,
    [enable_threads=check])

  AS_IF([test "x$enable_threads" = xno],,[
    sq_found_threads=no
    SQ_WINTHREADS([sq_found_threads=yes])
    AS_IF([test "x$sq_found_threads" = xyes],,
      [SQ_PTHREADS([sq_found_threads=yes])])
    
    AS_IF([test "x$sq_found_threads$enable_threads" = xnoyes],
      [AC_MSG_FAILURE([Can't find threading support])])
    enable_threads="$sq_found_threads"
  ])
])

# SQ_WINTHREADS([IF-FOUND], [IF-NOT-FOUND])
#
# Try to find Windows thread support. On success, modify CPPFLAGS as necessary
AC_DEFUN([SQ_WINTHREADS],[
  sq_winthreads_cppflags="-D_WIN32_WINNT=0x0600"
  AC_CACHE_CHECK([Windows threads], [sq_cv_winthreads],[
    sq_cv_winthreads='not available'
    
    SQ_SAVE_FLAGS
    CPPFLAGS="$CPPFLAGS $sq_winthreads_cppflags"
    AC_LINK_IFELSE([
      AC_LANG_PROGRAM([[#include <windows.h>]],
        [[InitializeConditionVariable((CONDITION_VARIABLE*)0)]])
      ],[sq_cv_winthreads=yes])
    SQ_RESTORE_FLAGS
  ])
  
  AS_IF([test "x$sq_cv_winthreads" = "xnot available"],[$2],[
    CPPFLAGS="$CPPFLAGS $sq_winthreads_cppflags"
    $1
  ])
])

# SQ_PTHREADS([IF-FOUND], [IF-NOT-FOUND])
#
# Try to find pthreads support. On success, define HAVE_PTHREAD, and set
# PTHREAD_CFLAGS.
AC_DEFUN([SQ_PTHREADS],[
  AC_CACHE_CHECK([flags needed for pthreads], [sq_cv_pthread],[
    sq_cv_pthread='not available'
    for sq_pthread_flags in none '-pthread'
    do
      SQ_TRY_PTHREADS([$sq_pthread_flags],[sq_cv_pthread="$sq_pthread_flags"])
      AS_IF([test "x$sq_cv_pthread" = "xnot available"],,[break])
    done
  ])
  
  AS_IF([test "x$sq_cv_pthread" = "xnot available"],[$2],[
    AC_DEFINE(HAVE_PTHREAD, 1, [POSIX threads available])
    PTHREAD_CFLAGS=
    AS_IF([test "x$sq_cv_pthread" = "xnone"],,[PTHREAD_CFLAGS=$sq_cv_pthread])
    AC_SUBST([PTHREAD_CFLAGS])
    $1
  ])
])

# SQ_TRY_PTHREADS(FLAGS, [IF-FOUND], [IF-NOT-FOUND])
#
# Check if pthreads works with the given flags
AC_DEFUN([SQ_TRY_PTHREADS],[
  SQ_SAVE_FLAGS
  AS_IF([test "x$1" = xnone],,[CPPFLAGS="$CPPFLAGS $1"])
  
  # Some libc's may include pthread stubs, so try a few functions
  AC_LINK_IFELSE([
    AC_LANG_PROGRAM([[#include <pthread.h>]],[[
      pthread_t pth;
      pthread_mutex_t mutex;
      pthread_join(pth, 0);
      pthread_mutex_lock(&mutex);
    ]])
  ],[$2],[$3])
  
  SQ_RESTORE_FLAGS
])

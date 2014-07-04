/*
 * Copyright (c) 2014 Dave Vasilevsky <dave@vasilevsky.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "input.h"

#include "dynstring.h"
#include "nonstd.h"
#include "thread.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #include <io.h>
  #define lseek _lseeki64
  #define OFLAGS O_BINARY
#else
  #include <unistd.h>
  #define OFLAGS 0
#endif

void sqfs_input_init(sqfs_input *in) {
  in->data = NULL;
}

static bool sqfs_no_seek_error(sqfs_input *SQFS_UNUSED(in)) {
  return false;
}


#ifdef _WIN32
/* Implementation for Windows */
typedef struct {
  HANDLE file;
  DWORD errcode;
} sqfs_input_windows;

static void sqfs_input_windows_close(sqfs_input *in) {
  sqfs_input_windows *iw = (sqfs_input_windows*)in->data;
  CloseHandle(iw->file);
  free(iw);
  in->data = NULL;
}

static ssize_t sqfs_input_windows_pread(sqfs_input *in, void *buf,
    size_t count, sqfs_off_t off) {
  DWORD bread;
  ssize_t ret;
  sqfs_input_windows *iw = (sqfs_input_windows*)in->data;
  OVERLAPPED ov = { 0 };
  ov.Offset = (DWORD)off;
  ov.OffsetHigh = (DWORD)(off >> 32);

  ret = -1;
  if (ReadFile(iw->file, buf, count, &bread, &ov))
    ret = bread;
  iw->errcode = GetLastError();
  return ret;
}

static char *sqfs_input_windows_error(sqfs_input *in) {
  /* FIXME: Use FormatMessage() to return a real error */
  sqfs_input_windows *iw = (sqfs_input_windows*)in->data;
  return sqfs_asprintf("File error #%d", iw->errcode);
}

static sqfs_err sqfs_input_windows_create(sqfs_input *in, HANDLE file) {
  sqfs_input_windows *iw =
    (sqfs_input_windows*)malloc(sizeof(sqfs_input_windows));
  if (!iw)
    return SQFS_ERR;
  
  iw->file = file;
  iw->errcode = 0;
  
  in->data = iw;
  in->i_close = &sqfs_input_windows_close;
  in->i_pread = &sqfs_input_windows_pread;
  in->i_error = &sqfs_input_windows_error;
  in->i_seek_error = &sqfs_no_seek_error; /* TODO: Any way to find out? */
  return SQFS_OK;
}

static sqfs_err sqfs_input_windows_open(sqfs_input *in, const char *path) {
  sqfs_err err;
  sqfs_input_windows *iw;
  if ((err = sqfs_input_windows_create(in, 0)))
    return err;

  iw = (sqfs_input_windows*)in->data;
  iw->file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (iw->file != INVALID_HANDLE_VALUE)
    return SQFS_OK;

  iw->errcode = GetLastError();
  return SQFS_ERR;
}

static sqfs_err sqfs_input_windows_open_stdin(sqfs_input *in) {
  sqfs_err err;
  sqfs_input_windows *iw;
  if ((err = sqfs_input_windows_create(in, 0)))
    return err;

  iw = (sqfs_input_windows*)in->data;
  iw->file = GetStdHandle(STD_INPUT_HANDLE);
  if (iw->file != INVALID_HANDLE_VALUE)
    return SQFS_OK;

  iw->errcode = GetLastError();
  return SQFS_ERR;
}

#endif /* _WIN32 */


/* Implementation for POSIX file descriptor */
typedef struct {
  int fd;
  int errnum;
  sqfs_mutex mutex;
} sqfs_input_posix;

static void sqfs_input_posix_close(sqfs_input *in) {
  sqfs_input_posix *ip = (sqfs_input_posix*)in->data;
#if !HAVE_PREAD
  sqfs_mutex_destroy(&ip->mutex);
#endif
  close(ip->fd);
  free(ip);
  in->data = NULL;
}

static ssize_t sqfs_input_posix_pread(sqfs_input *in, void *buf, size_t count,
    sqfs_off_t off) {
  ssize_t ret;
  sqfs_input_posix *ip = (sqfs_input_posix*)in->data;
#if HAVE_PREAD
  ret = sqfs_pread(ip->fd, buf, count, off);
  ip->errnum = errno;
#else
  sqfs_mutex_lock(&ip->mutex);
  if (lseek(ip->fd, off, SEEK_SET) == -1)
    ret = -1;
  else
    ret = read(ip->fd, buf, count);
  ip->errnum = errno;
  sqfs_mutex_unlock(&ip->mutex);
#endif
  return ret;
}

static char *sqfs_input_posix_error(sqfs_input *in) {
  sqfs_input_posix *ip = (sqfs_input_posix*)in->data;
  
  if (ip->errnum == 0)
    return NULL; /* No error */

#if HAVE_STRERROR_R
  {
    char buf[1024]; /* FIXME: 1K is enough for anyone */
    char *ret = buf;
    #ifdef STRERROR_R_CHAR_P
      if (!(ret = strerror_r(ip->errnum, buf, sizeof(buf))))
        return NULL;
    #else
      int err = strerror_r(ip->errnum, buf, sizeof(buf));
      if (err != 0 && err != ERANGE && errno != ERANGE)
        return NULL; /* Unclear spec! */
      buf[sizeof(buf)-1] = '\0'; /* Unclear POSIX spec */
    #endif
    return sqfs_strdup(ret);
  }
#else
  return sqfs_strdup(strerror(ip->errnum));
#endif
}

static bool sqfs_input_posix_seek_error(sqfs_input *in) {
  sqfs_input_posix *ip = (sqfs_input_posix*)in->data;
  return ip->errnum == ESPIPE;
}

static sqfs_err sqfs_input_posix_open(sqfs_input *in, const char *path) {
  sqfs_err err;
  sqfs_input_posix *ip;
  if ((err = sqfs_input_posix_create(in, 0)))
    return err;
  
  ip = (sqfs_input_posix*)in->data;
  ip->fd = open(path, O_RDONLY | OFLAGS);
  ip->errnum = errno;
  
#if !HAVE_PREAD
  sqfs_mutex_init(&ip->mutex);
#endif
  
  return (ip->fd == -1) ? SQFS_ERR : SQFS_OK;
}

sqfs_err sqfs_input_posix_create(sqfs_input *in, int fd) {
  sqfs_input_posix *ip = (sqfs_input_posix*)malloc(sizeof(sqfs_input_posix));
  if (!ip)
    return SQFS_ERR;
  
  ip->fd = fd;
  ip->errnum = 0;
  
  in->data = ip;
  in->i_close = &sqfs_input_posix_close;
  in->i_pread = &sqfs_input_posix_pread;
  in->i_error = &sqfs_input_posix_error;
  in->i_seek_error = &sqfs_input_posix_seek_error;
  return SQFS_OK;
}

sqfs_err sqfs_input_posix_open_stdin(sqfs_input *in) {
  if (isatty(STDIN_FILENO))
    return SQFS_ERR;
  return sqfs_input_posix_create(in, STDIN_FILENO);
}


/* Implementation for memory buffer */
typedef enum {
  SQFS_MEMORY_OK = 0,
  SQFS_MEMORY_EOF /* Read past end of buffer */
} sqfs_input_memory_err;

typedef struct {
  char *buf;
  size_t length;
  sqfs_input_memory_err err;
} sqfs_input_memory;

static void sqfs_input_memory_close(sqfs_input *SQFS_UNUSED(in)) {
  /* pass */
}

static ssize_t sqfs_input_memory_pread(sqfs_input *in, void *buf, size_t count,
    sqfs_off_t off) {
  sqfs_input_memory *im = (sqfs_input_memory*)in->data;
  if (off > im->length) {
    im->err = SQFS_MEMORY_EOF;
    return -1;
  }
  
  if (count > im->length - off)
    count = im->length - off;
  
  memcpy(buf, im->buf + off, count);
  im->err = SQFS_MEMORY_OK;
  return count;
}

static char *sqfs_input_memory_error(sqfs_input *in) {
  sqfs_input_memory *im = (sqfs_input_memory*)in->data;
  switch (im->err) {
    case SQFS_MEMORY_OK:
      return NULL;
    case SQFS_MEMORY_EOF:
      return sqfs_strdup("Read past end of buffer");
    default:
      return sqfs_strdup("Unknown error");
  }
}

sqfs_err sqfs_input_memory_create(sqfs_input *in, void *buf, size_t len) {
  sqfs_input_memory *im =
    (sqfs_input_memory*)malloc(sizeof(sqfs_input_memory));
  if (!im)
    return SQFS_ERR;
  
  im->buf = (char*)buf;
  im->length = len;
  im->err = SQFS_MEMORY_OK;
  
  in->data = im;
  in->i_close = &sqfs_input_memory_close;
  in->i_pread = &sqfs_input_memory_pread;
  in->i_error = &sqfs_input_memory_error;
  in->i_seek_error = &sqfs_no_seek_error;
  return SQFS_OK;
}


#ifdef _WIN32
  sqfs_err sqfs_input_open(sqfs_input *in, const char *path) {
    return sqfs_input_windows_open(in, path);
  }
  sqfs_err sqfs_input_open_stdin(sqfs_input *in) {
    return sqfs_input_windows_open_stdin(in);
  }
#else /* !_WIN32 */
  sqfs_err sqfs_input_open(sqfs_input *in, const char *path) {
    return sqfs_input_posix_open(in, path);
  }
  sqfs_err sqfs_input_open_stdin(sqfs_input *in) {
    return sqfs_input_posix_open_stdin(in);
  }
#endif /* _WIN32 */

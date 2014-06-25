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

#include "nonstd.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* FIXME:
  - Add windows
  - Handle no pread
  - Handle no strerror_r
*/
void sqfs_input_init(sqfs_input *in) {
  in->data = NULL;
}

/* Implementation for POSIX file descriptor */
typedef struct {
  int fd;
  int errnum;
} sqfs_input_posix;

static void sqfs_input_posix_close(sqfs_input *in) {
  sqfs_input_posix *ip = (sqfs_input_posix*)in->data;
  close(ip->fd);
  free(ip);
  in->data = NULL;
}

static ssize_t sqfs_input_posix_pread(sqfs_input *in, void *buf, size_t count,
    sqfs_off_t off) {
  sqfs_input_posix *ip = (sqfs_input_posix*)in->data;
  ssize_t ret = sqfs_pread(ip->fd, buf, count, off);
  ip->errnum = errno;
  return ret;
}

static char *sqfs_input_posix_error(sqfs_input *in) {
  sqfs_input_posix *ip = (sqfs_input_posix*)in->data;
  
  size_t bsize = 256;
  char *buf = NULL;
  int r;
  
  if (ip->errnum == 0)
    return NULL; /* No error */
  
  while (true) {
    if (!(buf = malloc(bsize)))
      return NULL; /* What else can we do? */
    
    r = strerror_r(ip->errnum, buf, bsize);
    if (r != ERANGE)
      return buf;
    
    bsize *= 2;
    free(buf);
  }
}

static sqfs_err sqfs_input_posix_open(sqfs_input *in, const char *path) {
  sqfs_err err;
  sqfs_input_posix *ip;
  if ((err = sqfs_input_posix_create(in, 0)))
    return err;
  
  ip = (sqfs_input_posix*)in->data;
  ip->fd = open(path, O_RDONLY);
  ip->errnum = errno;
  
  return (ip->fd == -1) ? SQFS_ERR : SQFS_OK;
}

sqfs_err sqfs_input_posix_create(sqfs_input *in, int fd) {
  sqfs_input_posix *ip = malloc(sizeof(sqfs_input_posix));
  if (!ip)
    return SQFS_ERR;
  
  ip->fd = fd;
  ip->errnum = 0;
  
  in->data = ip;
  in->close = &sqfs_input_posix_close;
  in->pread = &sqfs_input_posix_pread;
  in->error = &sqfs_input_posix_error;
  return SQFS_OK;
}

sqfs_err sqfs_input_open(sqfs_input *in, const char *path) {
  return sqfs_input_posix_open(in, path);
}


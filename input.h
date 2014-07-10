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
#ifndef SQFS_INPUT_H
#define SQFS_INPUT_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Abstract out the input for squashfuse */
typedef struct sqfs_input sqfs_input;
struct sqfs_input {
  void *data;
  
  /* Close this input source */
  void (*i_close)(sqfs_input *in);
  
  /* Read data at an offset */
  ssize_t (*i_pread)(sqfs_input *in, void *buf, size_t count, sqfs_off_t off);
  
  /* Return an error message, caller must free it */
  char *(*i_error)(const sqfs_input *in);
  
  /* Is the input non-seekable? */
  bool (*i_seek_error)(const sqfs_input *in);
};

/* Initialize the structure */
void sqfs_input_init(sqfs_input *in);

/* Entry point to posix implementation */
sqfs_err sqfs_input_posix_create(sqfs_input *in, int fd);

/* Entry point to memory implementation */
sqfs_err sqfs_input_memory_create(sqfs_input *in, const void *buf, size_t len);

/* Open a file by name, or from stdin */
sqfs_err sqfs_input_open(sqfs_input *in, sqfs_host_path path);
sqfs_err sqfs_input_open_stdin(sqfs_input *in);

#ifdef __cplusplus
}
#endif

#endif

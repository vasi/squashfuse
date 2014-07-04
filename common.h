/*
 * Copyright (c) 2012 Dave Vasilevsky <dave@vasilevsky.ca>
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
#ifndef SQFS_COMMON_H
#define SQFS_COMMON_H

#include "config.h"
#ifdef _WIN32
  #include <win32.h>
#endif

#include <stdbool.h>
#if HAVE_STDINT_H
  #include <stdint.h>
#else
  #include <inttypes.h>
#endif
#include <sys/types.h>

#ifndef _WIN32
  typedef mode_t sqfs_mode_t;
  typedef uid_t sqfs_id_t;
  typedef off_t sqfs_off_t;
#endif

#ifdef __GNUC__
  #define SQFS_UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
  #define SQFS_UNUSED(x) UNUSED_ ## x
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  SQFS_OK,
  SQFS_ERR,
  SQFS_BADFORMAT,   /* unsupported file format */
  SQFS_BADVERSION,  /* unsupported squashfs version */
  SQFS_BADCOMP,   /* unsupported compression method */
  SQFS_UNSUP,     /* unsupported feature */
  SQFS_BLOCK_OVERFLOW, /* too many blocks */
  SQFS_UNSEEKABLE      /* file not seekable */
} sqfs_err;

#define SQFS_INODE_ID_BYTES 6
typedef uint64_t sqfs_inode_id;
typedef uint32_t sqfs_inode_num;

typedef struct sqfs sqfs;
typedef struct sqfs_inode sqfs_inode;

typedef struct {
  sqfs_off_t block;
  size_t offset;
} sqfs_md_cursor;

extern unsigned char sqfs_embed[];
extern size_t sqfs_embed_len;

#ifdef __cplusplus
}
#endif

#endif

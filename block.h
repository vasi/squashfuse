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
#ifndef SQFS_BLOCK_H
#define SQFS_BLOCK_H

#include "common.h"

#include "cache.h"
#include "fs.h"

typedef struct {
  sqfs_cache_entry *cache_entry;
  sqfs_err error;
  size_t raw_size; /* Bytes actually read from the file */
  size_t size;
  char data[1];
} sqfs_block;

/* Put in pos the position of the next metadata block after pos */
sqfs_err sqfs_md_skip(sqfs *fs, off_t *pos);

/* Initialize a block cache, with an initial and maximum capacity */
sqfs_err sqfs_block_cache_init(sqfs_cache *cache, size_t block_size,
  size_t initial, size_t max);

/* Parse data block header into its component parts */
void sqfs_data_header(uint32_t hdr, bool *compressed, uint32_t *size);

/* Get a data/metadata block from the cache. */
sqfs_err sqfs_md_cache(sqfs *fs, sqfs_off_t pos, sqfs_block **block);
sqfs_err sqfs_data_cache(sqfs *fs, sqfs_cache *cache, sqfs_off_t pos,
  uint32_t hdr, sqfs_block **block);

/* Indicate that we're done with a block */
sqfs_err sqfs_block_release(sqfs_block *block);

#endif

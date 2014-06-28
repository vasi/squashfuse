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
#include "block.h"

#include "nonstd.h"
#include "squashfs_fs.h"
#include "swap.h"

#include <stdlib.h>

/* Read a block from disk, decompressing if necessary. The block should have
   'size' set to the maximum expected output size, and data big enough to
   hold it. */
sqfs_err sqfs_block_read(sqfs *fs, sqfs_off_t pos, bool compressed,
  uint32_t size, sqfs_block *block);



static void sqfs_md_header(uint16_t hdr, bool *compressed, uint16_t *size) {
  *compressed = !(hdr & SQUASHFS_COMPRESSED_BIT);
  *size = hdr & ~SQUASHFS_COMPRESSED_BIT;
  if (!*size)
    *size = SQUASHFS_COMPRESSED_BIT;
}

sqfs_err sqfs_md_skip(sqfs *fs, off_t *pos) {
  bool compressed;
  uint16_t hdr, size;
  
  if (fs->input->pread(fs->input, &hdr, sizeof(hdr), *pos) != sizeof(hdr))
    return SQFS_ERR;
  
  sqfs_md_header(hdr, &compressed, &size);
  *pos += sizeof(hdr) + size;
  return SQFS_OK;
}

void sqfs_data_header(uint32_t hdr, bool *compressed, uint32_t *size) {
  *compressed = !(hdr & SQUASHFS_COMPRESSED_BIT_BLOCK);
  *size = hdr & ~SQUASHFS_COMPRESSED_BIT_BLOCK;
}

sqfs_err sqfs_block_cache_init(sqfs_cache *cache, size_t block_size,
    size_t initial, size_t max) {
  return sqfs_cache_init(cache,
    sizeof(sqfs_block) - sizeof(((sqfs_block*)NULL)->data) + block_size,
    initial, max, NULL);
}
  
sqfs_err sqfs_block_read(sqfs *fs, sqfs_off_t pos, bool compressed,
    uint32_t size, sqfs_block *block) {
  sqfs_err err = SQFS_ERR;
  void *read_bytes;
  
  if (compressed) {
    if (!(read_bytes = malloc(size)))
      return SQFS_ERR;
  } else {
    read_bytes = block->data;
    block->raw_size = size;
  }
  
  if (fs->input->pread(fs->input, read_bytes, size, pos) != (ssize_t)size)
    goto error;

  if (compressed)
    err = fs->decompressor(read_bytes, size, block->data, &block->size);
  
  err = SQFS_OK;

error:
  if (compressed)
    free(read_bytes);
  return err;
}

/* Reads block from disk, sets size and data of block. Sets *pos to position
   of next block. */
static sqfs_err sqfs_md_block_read(sqfs *fs, sqfs_off_t pos,
    uint32_t SQFS_UNUSED(data_header), sqfs_block *block) {
  sqfs_err err = SQFS_OK;
  uint16_t hdr;
  bool compressed;
  uint16_t size;
  
  if (fs->input->pread(fs->input, &hdr, sizeof(hdr), pos) != sizeof(hdr))
    return SQFS_ERR;
  sqfs_swapin16(&hdr);
  sqfs_md_header(hdr, &compressed, &size);
  pos += sizeof(hdr);
  
  block->size = SQUASHFS_METADATA_SIZE;
  err = sqfs_block_read(fs, pos, compressed, size, block);
  block->raw_size = sizeof(hdr) + size;
  
  return err;
}

/* Reads block from disk, sets size and data of block */
static sqfs_err sqfs_data_block_read(sqfs *fs, sqfs_off_t pos,
    uint32_t hdr, sqfs_block *block) {
  bool compressed;
  uint32_t size;
  sqfs_data_header(hdr, &compressed, &size);
  block->size = fs->sb.block_size;
  block->raw_size = size;
  return sqfs_block_read(fs, pos, compressed, size, block);
}


/* Common function for getting cached blocks */
typedef sqfs_err (*sqfs_block_reader)(sqfs *fs, sqfs_off_t pos, uint32_t hdr,
  sqfs_block *block);
static sqfs_err sqfs_cached_block(sqfs *fs, sqfs_cache *cache, sqfs_off_t pos,
    uint32_t hdr, sqfs_block **block, sqfs_block_reader reader) {
  sqfs_err err;
  sqfs_cache_entry *entry;

  if ((err = sqfs_cache_get(cache, pos, &entry)))
    return err;
  *block = (sqfs_block*)sqfs_cache_entry_value(entry);
  (*block)->cache_entry = entry;

  if (!sqfs_cache_entry_is_initialized(entry)) {
    (*block)->error = reader(fs, pos, hdr, *block);
    err = sqfs_cache_entry_ready(entry);
  }

  return err ? err : (*block)->error;
}

sqfs_err sqfs_md_cache(sqfs *fs, sqfs_off_t pos, sqfs_block **block) {
  return sqfs_cached_block(fs, &fs->md_cache, pos, 0, block,
    sqfs_md_block_read);
}

sqfs_err sqfs_data_cache(sqfs *fs, sqfs_cache *cache, sqfs_off_t pos,
    uint32_t hdr, sqfs_block **block) {
  return sqfs_cached_block(fs, cache, pos, hdr, block, sqfs_data_block_read);
}

sqfs_err sqfs_block_release(sqfs_block *block) {
  return sqfs_cache_entry_release(block->cache_entry);
}

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
#include "ll.h"

#include "array.h"
#include "hash.h"
#include "stack.h"

#include <stddef.h>
#include <stdlib.h>

/* 
We have to fit a 48-bit inode ID into a 32-bit fuse_ino_t. The inode ID is
made up of a 32-bit block location and a 16-bit offset.

The maximum metadata-block size is 8012, so only 13 bits of the offset are used.
Also, each inode is at least 20 bytes, so an inode can be uniquely identified
without the bottom 4 bits of the offset. This leaves 9 significant offset bits

Instead of a 32-bit block location, we can then use a 23-bit block identifier.
We will allocate these identifiers as-needed.

TODO: Reserve some inodes, in case we run out of room for more block IDs.
TODO: Handle root inode?
*/
#define BITS_OFFSET_IGN 4
#define BITS_OFFSET_KEEP (13-OFFSET_BITS_IGNORE)
#define BITS_BLOCK (32-BITS_OFFSET_KEEP)

typedef uint32_t sqfs_iidx_offset_sig;  /* Significant bits of offset */
typedef uint32_t sqfs_iidx_block_id;    /* Block identifier */
typedef uint32_t sqfs_iidx_block_loc;

typedef struct {
  uint32_t refcount;
  sqfs_iidx_block_id block_id;
  uint16_t min_offset; /* The smallest known inode offset in this block */
  
  char end_of_struct;
} sqfs_iidx_entry;

typedef struct {
  /* Mapping from block ID -> block location */
  sqfs_array id_to_loc;
  
  /* Block IDs to reallocate */
  sqfs_stack id_freelist;
  
  /* Mapping block location to block info */
  sqfs_hash loc_info;
  
  /* Bias of block IDs when forming fuse_ino_t */
  size_t id_bias;
} sqfs_iidx;



sqfs_err sqfs_iidx_init(sqfs_ll *ll) {
  sqfs_err err;
  sqfs_iidx *iidx;
  
  if (!(iidx = malloc(sizeof(*iidx))))
    return SQFS_ERR;
  
  sqfs_array_init(&iidx->id_to_loc);
  sqfs_stack_init(&iidx->id_freelist);
  sqfs_hash_init(&iidx->loc_info);
  
  err = sqfs_array_create(&iidx->id_to_loc, sizeof(sqfs_iidx_block_loc),
    0, NULL);
  if (err)
    goto error;
  
  err = sqfs_stack_create(&iidx->id_freelist, sizeof(sqfs_iidx_block_id),
    0, NULL);
  if (err)
    goto error;

  err = sqfs_hash_create(&iidx->loc_info,
    offsetof(sqfs_iidx_entry, end_of_struct), 0);
  if (err)
    goto error;
  
  // ll->ino_sqfs = sqfs_iidx_sqfs;
  // ll->ino_fuse_num = sqfs_iidx_fuse_num;
  // ll->ino_register = sqfs_ll_iidx_register;
  // ll->ino_forget = sqfs_ll_iidx_forget;
  // ll->ino_destroy = sqfs_ll_iidx_destroy;
  // ll->ino_data = iidx;
  
  return SQFS_OK;

error:
  sqfs_stack_destroy(&iidx->id_freelist);
  sqfs_array_destroy(&iidx->id_to_loc);
  sqfs_hash_destroy(&iidx->loc_info);
  return err;
}

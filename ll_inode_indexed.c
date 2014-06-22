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
#include "thread.h"

#include <limits.h>
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
#define BITS_OFFSET_KEEP (13-BITS_OFFSET_IGN)
#define BITS_BLOCK (32-BITS_OFFSET_KEEP)

#define MASK_OFFSET_KEEP ((1<<BITS_OFFSET_KEEP)-1)

#define BLOCK_LOC_INVALID UINT_MAX

typedef uint32_t sqfs_iidx_offset_sig;  /* Significant bits of offset */
typedef uint32_t sqfs_iidx_block_id;    /* Block identifier */
typedef uint32_t sqfs_iidx_block_loc;

typedef struct {
  uint32_t refcount;
  sqfs_iidx_block_id block_id;
  uint16_t min_offset; /* The smallest known inode offset in this block */
  
  char end_of_struct;
} sqfs_iidx_block_info;

typedef struct {
  /* Mapping from block ID -> block location */
  sqfs_array id_to_loc;
  
  /* Block IDs to reallocate */
  sqfs_stack id_freelist;
  
  /* Mapping block location to block info */
  sqfs_hash loc_info;
  
  /* Bias of block IDs when forming fuse_ino_t */
  size_t id_bias;
  
  sqfs_mutex mutex;
} sqfs_iidx;

/* Lock/unlock our data structures */
static sqfs_err sqfs_iidx_lock(sqfs_ll *ll);
static void sqfs_iidx_unlock(sqfs_ll *ll);

/* Decompose/decompose a fuse_ino_t into the block ID and offset parts */
static void sqfs_iidx_decompose(sqfs_iidx *iidx, fuse_ino_t fi,
  sqfs_iidx_block_id *bid, sqfs_iidx_offset_sig *sig);
static fuse_ino_t sqfs_iidx_compose(sqfs_iidx *iidx, sqfs_iidx_block_id bid,
  sqfs_iidx_offset_sig sig);

/* Find an inode_id given a fuse_ino_t */
static sqfs_err sqfs_iidx_inode_id(sqfs_iidx *iidx, sqfs *fs, fuse_ino_t fi,
  sqfs_inode_id *iid);

/* Get a reference to a fuse_ino_t for an inode id */
static sqfs_err sqfs_iidx_ref(sqfs_iidx *iidx, sqfs *fs,
  sqfs_inode_id iid, fuse_ino_t *fi);

/* Allocate a new block ID */
static sqfs_err sqfs_iidx_allocate(sqfs_iidx *iidx, sqfs_iidx_block_loc loc,
  sqfs_iidx_block_info **info);


static sqfs_err sqfs_iidx_lock(sqfs_ll *ll) {
  return sqfs_mutex_lock(&((sqfs_iidx*)ll->ino_data)->mutex);
}

static void sqfs_iidx_unlock(sqfs_ll *ll) {
  sqfs_mutex_unlock(&((sqfs_iidx*)ll->ino_data)->mutex);
}

static void sqfs_iidx_decompose(sqfs_iidx *iidx, fuse_ino_t fi,
    sqfs_iidx_block_id *bid, sqfs_iidx_offset_sig *sig) {
  *bid = (fi >> BITS_OFFSET_KEEP) - iidx->id_bias;
  *sig = (fi & MASK_OFFSET_KEEP);
}

static fuse_ino_t sqfs_iidx_compose(sqfs_iidx *iidx, sqfs_iidx_block_id bid,
    sqfs_iidx_offset_sig sig) {
  return sig | (((bid + iidx->id_bias) << BITS_OFFSET_KEEP));
}

static sqfs_err sqfs_iidx_inode_id(sqfs_iidx *iidx, sqfs *fs, fuse_ino_t fi,
    sqfs_inode_id *iid) {
  sqfs_err err;
  sqfs_iidx_block_id bid;
  sqfs_iidx_block_loc *loc;
  sqfs_iidx_block_info *info;
  sqfs_iidx_offset_sig sig;
  off_t block;
  sqfs_md_cursor cur;
  
  /* The root always goes to the root */
  if (fi == FUSE_ROOT_ID) {
    *iid = sqfs_inode_root(fs);
    return SQFS_OK;
  }
  
  sqfs_iidx_decompose(iidx, fi, &bid, &sig);
  
  /* Find the block location */
  if ((err = sqfs_array_at(&iidx->id_to_loc, bid, &loc)))
    return err;
  if (*loc == BLOCK_LOC_INVALID)
    return SQFS_ERR;
  
  /* Find the block info */
  if (!(info = sqfs_hash_get(&iidx->loc_info, *loc)))
    return SQFS_ERR;
  
  /* Keep skipping til we've found the inode */
  block = *loc + fs->sb.inode_table_start;
  cur.block = block;
  cur.offset = info->min_offset;
  while (cur.block == block) {
    if ((cur.offset >> BITS_OFFSET_IGN) == sig) { /* Found it! */
      *iid = (*loc << 16) | cur.offset;
      return SQFS_OK;
    }
    
    if ((err = sqfs_inode_skip(fs, &cur)))
      return err;
  }
  return SQFS_ERR; /* Not found */
}

static sqfs_err sqfs_iidx_allocate(sqfs_iidx *iidx, sqfs_iidx_block_loc loc,
    sqfs_iidx_block_info **info) {
  sqfs_err err;
  sqfs_iidx_block_loc *locp;
  sqfs_iidx_block_id bid, *bidp;
  
  if (sqfs_stack_top(&iidx->id_freelist, &bidp) == SQFS_OK) {
    /* Reuse an ID in the freelist */
    bid = *bidp;
    sqfs_stack_pop(&iidx->id_freelist);
  } else {
    /* Nothing in the freelist, append to the array */
    bid = sqfs_array_size(&iidx->id_to_loc);
    if ((err = sqfs_array_append(&iidx->id_to_loc, NULL)))
      return err;
  }

  /* Map the ID to the location */
  if ((err = sqfs_array_at(&iidx->id_to_loc, bid, &locp)))
    return err;
  *locp = loc;
  
  /* Create the block info */
  /*FIXME*/
  return SQFS_OK;
}

static sqfs_err sqfs_iidx_ref(sqfs_iidx *iidx, sqfs *fs,
    sqfs_inode_id iid, fuse_ino_t *fi) {
  sqfs_err err;
  sqfs_iidx_block_loc loc;
  sqfs_iidx_block_info *info;
  
  /* The root always goes to the root */
  if (iid == sqfs_inode_root(fs)) {
    *fi = FUSE_ROOT_ID;
    return SQFS_OK;
  }
  
  /* Check if we already have this block */
  loc = (iid >> 16);
  info = sqfs_hash_get(&iidx->loc_info, loc);
  
  /* If we don't have it, allocate an ID for it */
  if (!info) {
    if ((err = sqfs_iidx_allocate(iidx, loc, &info)))
      return err;
  }
  
  /* TODO */
  return SQFS_OK;
}


static sqfs_inode_id sqfs_iidx_sqfs(sqfs_ll *ll, fuse_ino_t i) {
  sqfs_inode_id iid;
  sqfs_err err;
  
  if (sqfs_iidx_lock(ll))
    return SQFS_INODE_NONE;
  err = sqfs_iidx_inode_id(ll->ino_data, &ll->fs, i, &iid);
  sqfs_iidx_unlock(ll);
  return err ? SQFS_INODE_NONE : iid;
}

static fuse_ino_t sqfs_iidx_fuse_num(sqfs_ll *ll, sqfs_dir_entry *e) {
  return FUSE_INODE_NONE; /* Unknown FUSE inode */
}

static fuse_ino_t sqfs_iidx_register(sqfs_ll *ll, sqfs_dir_entry *e) {
  return 0; /* FIXME */
}

static void sqfs_iidx_forget(sqfs_ll *ll, fuse_ino_t i, size_t refs) {
}

static void sqfs_iidx_destroy(sqfs_ll *ll) {
  sqfs_iidx *iidx = ll->ino_data;
  if (iidx) {
    sqfs_array_destroy(&iidx->id_to_loc);
    sqfs_stack_destroy(&iidx->id_freelist);
    sqfs_hash_destroy(&iidx->loc_info);
    sqfs_mutex_destroy(&iidx->mutex);
    free(iidx);
  }
}

sqfs_err sqfs_iidx_init(sqfs_ll *ll) {
  sqfs_err err;
  sqfs_iidx *iidx;
  
  if (!(iidx = malloc(sizeof(*iidx))))
    return SQFS_ERR;
  ll->ino_data = iidx;
  
  sqfs_array_init(&iidx->id_to_loc);
  sqfs_stack_init(&iidx->id_freelist);
  sqfs_hash_init(&iidx->loc_info);
  sqfs_mutex_init(&iidx->mutex);
  
  err = sqfs_array_create(&iidx->id_to_loc, sizeof(sqfs_iidx_block_loc),
    0, NULL);
  if (err)
    goto error;
  
  err = sqfs_stack_create(&iidx->id_freelist, sizeof(sqfs_iidx_block_id),
    0, NULL);
  if (err)
    goto error;

  err = sqfs_hash_create(&iidx->loc_info,
    offsetof(sqfs_iidx_block_info, end_of_struct), 0);
  if (err)
    goto error;
  
  iidx->id_bias = 1; /* FIXME: reserve space */
  
  ll->ino_sqfs = sqfs_iidx_sqfs;
  ll->ino_fuse_num = sqfs_iidx_fuse_num;
  ll->ino_register = sqfs_iidx_register;
  ll->ino_forget = sqfs_iidx_forget;
  ll->ino_destroy = sqfs_iidx_destroy;
  
  return SQFS_OK;

error:
  sqfs_iidx_destroy(ll);
  return err;
}

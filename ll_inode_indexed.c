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
#include "squashfs_fs.h"
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

/* Find the block info for a fuse_ino_t */
static sqfs_err sqfs_iidx_info(sqfs_iidx *iidx, sqfs *fs, fuse_ino_t fi,
  sqfs_iidx_block_info **info, sqfs_iidx_block_loc **loc);

/* Find an inode_id given a fuse_ino_t */
static sqfs_err sqfs_iidx_inode_id(sqfs_iidx *iidx, sqfs *fs, fuse_ino_t fi,
  sqfs_inode_id *iid);

/* Get a reference to the fuse_ino_t for an inode id */
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

/* Find the block info for a fuse_ino_t */
static sqfs_err sqfs_iidx_info(sqfs_iidx *iidx, sqfs *fs, fuse_ino_t fi,
    sqfs_iidx_block_info **info, sqfs_iidx_block_loc **loc) {
  sqfs_err err;
  sqfs_iidx_block_id bid;
  sqfs_iidx_offset_sig sig;
  
  sqfs_iidx_decompose(iidx, fi, &bid, &sig);
  
  /* Find the block location */
  if ((err = sqfs_array_at(&iidx->id_to_loc, bid, loc)))
    return err;
  if (**loc == BLOCK_LOC_INVALID)
    return SQFS_ERR;
  
  /* Find the block info */
  if (!(*info = sqfs_hash_get(&iidx->loc_info, **loc)))
    return SQFS_ERR;
  
  return SQFS_OK;
}

static sqfs_err sqfs_iidx_inode_id(sqfs_iidx *iidx, sqfs *fs, fuse_ino_t fi,
    sqfs_inode_id *iid) {
  sqfs_err err;
  sqfs_iidx_block_id bid;
  sqfs_iidx_offset_sig sig;
  sqfs_iidx_block_loc *loc;
  sqfs_iidx_block_info *info;
  off_t block;
  sqfs_md_cursor cur;
  
  /* The root always goes to the root */
  if (fi == FUSE_ROOT_ID) {
    *iid = sqfs_inode_root(fs);
    return SQFS_OK;
  }
  
  fprintf(stderr, "SQFS: fuse_ino_t = %lu\n", fi);
  sqfs_iidx_decompose(iidx, fi, &bid, &sig);
  fprintf(stderr, "   bid = %d, off = %d\n", bid, sig);
  if ((err = sqfs_iidx_info(iidx, fs, fi, &info, &loc)))
    return err;
  fprintf(stderr, "   loc = %d\n", *loc);
  
  /* Keep skipping til we've found the inode */
  block = *loc + fs->sb.inode_table_start;
  cur.block = block;
  cur.offset = info->min_offset;
  while (cur.block == block) {
    if ((cur.offset >> BITS_OFFSET_IGN) == sig) { /* Found it! */
      *iid = (((sqfs_inode_id)*loc) << 16) | cur.offset;
      fprintf(stderr, "   iid = %lld\n", *iid);
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
  
  fprintf(stderr, "   ALLOCATING\n");
  if (sqfs_stack_top(&iidx->id_freelist, &bidp) == SQFS_OK) {
    /* Reuse an ID in the freelist */
    bid = *bidp;
    fprintf(stderr, "      freelist -> %u\n", bid);
    sqfs_stack_pop(&iidx->id_freelist);
  } else {
    /* Nothing in the freelist, append to the array */
    bid = sqfs_array_size(&iidx->id_to_loc);
    fprintf(stderr, "      new -> %u\n", bid);
    if ((err = sqfs_array_append(&iidx->id_to_loc, NULL)))
      return err;
  }

  /* Map the ID to the location */
  if ((err = sqfs_array_at(&iidx->id_to_loc, bid, &locp)))
    return err;
  *locp = loc;
  
  /* Create the block info */
  if ((err = sqfs_hash_add(&iidx->loc_info, loc, info)))
    return err;
  (*info)->refcount = 0;
  (*info)->block_id = bid;
  (*info)->min_offset = SQUASHFS_METADATA_SIZE;
  
  return SQFS_OK;
}

static sqfs_err sqfs_iidx_ref(sqfs_iidx *iidx, sqfs *fs,
    sqfs_inode_id iid, fuse_ino_t *fi) {
  sqfs_err err;
  sqfs_iidx_block_loc loc;
  sqfs_iidx_block_info *info;
  uint16_t offset;
  
  /* The root always goes to the root */
  if (iid == sqfs_inode_root(fs)) {
    *fi = FUSE_ROOT_ID;
    return SQFS_OK;
  }
  
  /* Check if we already have this block */
  loc = (iid >> 16);
  offset = (iid & 0xffff);
  fprintf(stderr, "REF: iid = %lld\n", iid);
  fprintf(stderr, "   loc = %u, off = %u\n", loc, offset);
  info = sqfs_hash_get(&iidx->loc_info, loc);
  
  /* If we don't have it, allocate an ID for it */
  if (!info) {
    if ((err = sqfs_iidx_allocate(iidx, loc, &info)))
      return err;
  }
  
  /* Update the location info */
  info->refcount++;
  if (info->min_offset > offset)
    info->min_offset = offset;
  
  fprintf(stderr, "   bid = %d, off = %d\n", info->block_id,
    offset >> BITS_OFFSET_IGN);
  *fi = sqfs_iidx_compose(iidx, info->block_id, offset >> BITS_OFFSET_IGN);
  fprintf(stderr, "   fi = %lu\n", *fi);
  return SQFS_OK;
}


static sqfs_inode_id sqfs_iidx_sqfs(sqfs_ll *ll, fuse_ino_t fi) {
  sqfs_inode_id iid;
  sqfs_err err;
  
  if (sqfs_iidx_lock(ll))
    return SQFS_INODE_NONE;
  err = sqfs_iidx_inode_id(ll->ino_data, &ll->fs, fi, &iid);
  sqfs_iidx_unlock(ll);
  return err ? SQFS_INODE_NONE : iid;
}

static fuse_ino_t sqfs_iidx_fuse_num(sqfs_ll *ll, sqfs_dir_entry *e) {
  /* We don't want to allocate a new block info, so just return more-or-less
     the inode number. It should be ok. */
  if (sqfs_dentry_inode(e) == sqfs_inode_root(&ll->fs))
    return FUSE_ROOT_ID;
  
  return sqfs_dentry_inode_num(e) + 2; /* FIXME */
}

static fuse_ino_t sqfs_iidx_register(sqfs_ll *ll, sqfs_dir_entry *e) {
  fuse_ino_t fi;
  sqfs_err err;
  
  if (sqfs_iidx_lock(ll))
    return FUSE_INODE_NONE;
  err = sqfs_iidx_ref(ll->ino_data, &ll->fs, sqfs_dentry_inode(e), &fi);
  sqfs_iidx_unlock(ll);
  return err ? FUSE_INODE_NONE : fi;
}

static void sqfs_iidx_forget(sqfs_ll *ll, fuse_ino_t fi, size_t refs) {
  sqfs_iidx_block_id *bid;
  sqfs_iidx_block_loc *loc;
  sqfs_iidx_block_info *info;
  sqfs_iidx *iidx = ll->ino_data;
  
  if (fi == FUSE_ROOT_ID)
    return;
  if (sqfs_iidx_lock(ll))
    return;
  
  if (sqfs_iidx_info(iidx, &ll->fs, fi, &info, &loc))
    goto done;
  
  /* Decrement the refcount, and possibly deallocate the block ID */
  info->refcount -= refs;
  if (info->refcount == 0) {
    /* Remove the block info */
    if (sqfs_hash_remove(&iidx->loc_info, *loc))
      goto done;

    /* Add to the freelist */
    if (sqfs_stack_push(&iidx->id_freelist, &bid))
      goto done;
    *bid = info->block_id;

    *loc = BLOCK_LOC_INVALID; /* Mark unused in id_to_loc */
  }
  
done:
  sqfs_iidx_unlock(ll);
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
  
  iidx->id_bias = 1 << 9; /* FIXME: reserve space */
  
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

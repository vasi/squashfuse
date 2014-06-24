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

#include "block.h"
#include "squashfs_fs.h"

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
We will read all the block locations at startup, in groups of N. Then
the last log(N) bits of the block ID will indicate the block within a group,
and the first 23-log(N) will indicate the group.

TODO: Reserve some inodes, in case we run out of room for more block IDs.
*/
#define BITS_OFFSET_IGN 4
#define BITS_OFFSET_KEEP (13-BITS_OFFSET_IGN)
#define BITS_BLOCK (32-BITS_OFFSET_KEEP)

#define MASK_OFFSET_KEEP ((1<<BITS_OFFSET_KEEP)-1)

#define MAX_BLOCK_GROUPS 8192
#define BLOCK_IDX_INVALID UINT16_MAX

typedef uint32_t offset_sig;  /* Significant bits of offset */
typedef size_t block_grp;   /* Index of a block group */
typedef uint16_t block_idx;   /* Index within a block group */
typedef uint32_t block_pos;   /* Block part of an inode_id */

typedef struct {
  block_pos pos;
  
  /* The smallest known inode location in this group */
  block_idx first_idx;
  uint16_t first_offset;
} block_group;

typedef struct {
  /* Block groups */
  block_group *groups;
  size_t ngroups;
  
  /* How many blocks in a group? 2^stride_bits */
  size_t stride_bits;
  
  /* Bias of block IDs when forming fuse_ino_t */
  size_t bias;
} sqfs_iidx;

/* Decompose/compose a fuse_ino_t into its parts */
static void sqfs_iidx_decompose(sqfs_iidx *iidx, fuse_ino_t fi,
  block_grp *grp, block_idx *idx, offset_sig *sig);
static fuse_ino_t sqfs_iidx_compose(sqfs_iidx *iidx,
  block_grp grp, block_idx idx, offset_sig sig);

/* Build an index of inode block groups */
static sqfs_err sqfs_iidx_scan_blocks(sqfs_iidx *iidx, sqfs *fs);

/* Search for the block group containing a block_pos */
static sqfs_err sqfs_iidx_find_block(sqfs_iidx *iidx, sqfs *fs,
  block_pos pos, block_grp *grp);


static void sqfs_iidx_decompose(sqfs_iidx *iidx, fuse_ino_t fi,
    block_grp *grp, block_idx *idx, offset_sig *sig) {
  *sig = fi & MASK_OFFSET_KEEP;
  fi = (fi >> BITS_OFFSET_KEEP) - iidx->bias;
  *idx = fi & ((1 << iidx->stride_bits) - 1);
  *grp = fi >> iidx->stride_bits;
}

static fuse_ino_t sqfs_iidx_compose(sqfs_iidx *iidx,
    block_grp grp, block_idx idx, offset_sig sig) {
  fuse_ino_t b = ((grp << iidx->stride_bits) | idx) + iidx->bias;
  return (b << BITS_OFFSET_KEEP) | sig;
}

static sqfs_err sqfs_iidx_scan_blocks(sqfs_iidx *iidx, sqfs *fs) {
  sqfs_err err;
  size_t stride, count, i;
  sqfs_off_t pos = fs->sb.inode_table_start;
  
  iidx->ngroups = 0;
  iidx->stride_bits = 0;
  stride = 1;
  
  count = 0;
  while (pos < fs->sb.directory_table_start) {
    if (count % stride == 0) {
      if (iidx->ngroups == MAX_BLOCK_GROUPS) { /* Increase the stride */
        for (i = 0; i < MAX_BLOCK_GROUPS / 2; ++i)
          iidx->groups[i].pos = iidx->groups[2*i].pos;
        iidx->ngroups /= 2;
        iidx->stride_bits++;
        stride = 1 << iidx->stride_bits;
      } else {
        iidx->groups[iidx->ngroups++].pos = pos - fs->sb.inode_table_start;
      }
    }
    
    if ((err = sqfs_md_skip(fs, &pos)))
      return err;
    ++count;
  }
  
  for (i = 0; i < iidx->ngroups; ++i)
    iidx->groups[i].first_idx = BLOCK_IDX_INVALID;
  
  return SQFS_OK;
}

static sqfs_err sqfs_iidx_find_block(sqfs_iidx *iidx, sqfs *fs,
  block_pos pos, block_grp *grp) {
  /* Binary search */
  block_grp min = 0, max = iidx->ngroups;
  if (pos > fs->sb.directory_table_start - fs->sb.inode_table_start)
    return SQFS_ERR;
  
  while (max - min > 1) {
    block_grp mid = (max + min) / 2;
    block_pos mpos = iidx->groups[mid].pos;
    if (pos == mpos) {
      *grp = mid;
      return SQFS_OK;
    } else if (pos > mpos) {
      min = mid;
    } else {
      max = mid;
    }
  }
  
  *grp = min;
  return SQFS_OK;
}


static sqfs_inode_id sqfs_iidx_sqfs(sqfs_ll *ll, fuse_ino_t fi) {
  block_group *group;
  block_grp grp;
  block_idx idx, fidx;
  offset_sig sig;
  sqfs_off_t pos;
  sqfs_md_cursor cur;
  sqfs_iidx *iidx = ll->ino_data;
  
  if (fi == FUSE_ROOT_ID)
    return sqfs_inode_root(&ll->fs);
  
  /* Find the group */
  sqfs_iidx_decompose(iidx, fi, &grp, &idx, &sig);
  if ((idx >> iidx->stride_bits) > 0 || grp >= iidx->ngroups)
    return SQFS_INODE_NONE; /* Failed sanity check */
  group = &iidx->groups[grp];
  
  /* Get a cursor */
  pos = group->pos + ll->fs.sb.inode_table_start;
  fidx = group->first_idx;
  while (fidx--) {
    if (sqfs_md_skip(&ll->fs, &pos))
      return SQFS_INODE_NONE;
  }
  cur.block = pos;
  cur.offset = group->first_offset;
  
  /* Iterate our cursor until we've found our inode */
  while (true) {
    offset_sig csig = cur.offset >> BITS_OFFSET_IGN;
    if (idx == 0 && csig == sig)
      break; /* Found it! */
    
    /* Make sure we're not too far */
    if (cur.block >= ll->fs.sb.directory_table_start)
      return SQFS_INODE_NONE;
    if (idx == 0 && csig > sig)
      return SQFS_INODE_NONE;
    
    if (sqfs_inode_skip(&ll->fs, &cur))
      return SQFS_INODE_NONE;
    
    /* Check if we're on a new block */
    if (cur.block != pos) {
      if (idx == 0)
        return SQFS_INODE_NONE;
      --idx;
      pos = cur.block;
    }
  }
  
  return ((cur.block - ll->fs.sb.inode_table_start) << 16) | cur.offset;
}

static fuse_ino_t sqfs_iidx_fuse_num(sqfs_ll *ll, sqfs_dir_entry *e) {
  block_group *group;
  block_grp grp;
  block_idx idx;
  block_pos pos;
  uint16_t offset;
  sqfs_off_t dpos, ipos;
  sqfs_iidx *iidx = ll->ino_data;
  sqfs_inode_id iid = sqfs_dentry_inode(e);
  
  if (iid == sqfs_inode_root(&ll->fs))
    return FUSE_ROOT_ID;
  
  /* Find our group */
  pos = iid >> 16;
  offset = (iid & 0xffff);
  if (sqfs_iidx_find_block(iidx, &ll->fs, pos, &grp))
    return FUSE_INODE_NONE;
  group = &iidx->groups[grp];
  
  /* Found our index */
  idx = 0;
  dpos = pos + ll->fs.sb.inode_table_start;
  ipos = group->pos + ll->fs.sb.inode_table_start;
  while (ipos < dpos) {
    if (sqfs_md_skip(&ll->fs, &ipos))
      return FUSE_INODE_NONE;
    ++idx;
  }
  
  /* Setup a cursor for this group */
  bool set_curs = group->first_idx == BLOCK_IDX_INVALID ||
    idx < group->first_idx ||
    (idx == group->first_idx && offset < group->first_offset);
  if (set_curs) {
    group->first_idx = idx;
    group->first_offset = offset;
  }
  
  return sqfs_iidx_compose(iidx, grp, idx, offset >> BITS_OFFSET_IGN);
}

static void sqfs_iidx_destroy(sqfs_ll *ll) {
  sqfs_iidx *iidx = ll->ino_data;
  if (iidx) {
    free(iidx->groups);
    free(iidx);
  }
}

sqfs_err sqfs_iidx_init(sqfs_ll *ll) {
  sqfs_iidx *iidx;
  sqfs_err err = SQFS_OK;
  
  if (!(iidx = malloc(sizeof(*iidx))))
    return SQFS_ERR;
  iidx->groups = NULL;
  iidx->bias = 1 << 9; /* FIXME: reserve space */
  ll->ino_data = iidx;
  
  if (!(iidx->groups = malloc(MAX_BLOCK_GROUPS * sizeof(block_group))))
    goto error;
  
  if ((err = sqfs_iidx_scan_blocks(iidx, &ll->fs)))
    goto error;
  
  ll->ino_sqfs = sqfs_iidx_sqfs;
  ll->ino_fuse_num = sqfs_iidx_fuse_num;
  ll->ino_destroy = sqfs_iidx_destroy;
  
  return SQFS_OK;

error:
  sqfs_iidx_destroy(ll);
  return err ? err : SQFS_ERR;
}

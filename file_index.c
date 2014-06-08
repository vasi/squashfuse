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
#include "file_index.h"

#include "fs.h"
#include "squashfs_fs.h"

#include <stdlib.h>

/*
To read block N of a M-block file, we have to read N blocksizes from the,
metadata. This is a lot of work for large files! So for those files, we use
an index to speed it up.

The M blocksizes are split between M / SQUASHFS_METADATA_SIZE MD-blocks.
For each of these blocks, we maintain in the index the location of the
MD-block, and the location of the data block corresponding to the start
of that MD-block.

Then to read block N, we just calculate which metadata block index
("metablock") we want, and get that block-index entry. Then we
only need to read that one MD-block to seek within the file.
*/

typedef struct {
	uint64_t data_block;	/* A data block where the file continues */
	uint32_t md_block;		/* A metadata block with blocksizes that continue from
													 data_block */
} sqfs_blockidx_entry;

typedef struct {
	sqfs_err error;
	sqfs_blockidx_entry *entries;
} sqfs_blockidx;


/* Is a file worth indexing? */
static bool sqfs_blockidx_indexable(sqfs *fs, sqfs_inode *inode) {
	size_t blocks = sqfs_blocklist_count(fs, inode);
	size_t md_size = blocks * sizeof(sqfs_blocklist_entry);
	return md_size >= SQUASHFS_METADATA_SIZE;
}

static sqfs_err sqfs_blockidx_dispose(sqfs_cache_value v) {
	sqfs_blockidx *idx = (sqfs_blockidx*)v;
	free(idx->entries);
	return SQFS_OK;
}

sqfs_err sqfs_blockidx_init(sqfs_cache *cache) {
	return sqfs_cache_init(cache, sizeof(sqfs_blockidx),
		SQUASHFS_META_SLOTS, SQUASHFS_META_SLOTS, &sqfs_blockidx_dispose);
}

/* Fill idx with all the block-index entries for this file */
static sqfs_err sqfs_blockidx_add(sqfs *fs, sqfs_inode *inode,
		sqfs_blockidx *idx) {
	sqfs_err err;
	sqfs_blockidx_entry *blockidx;
	sqfs_blocklist bl;
	
	size_t blocks;	/* Number of blocks in the file */
	size_t md_size; /* Amount of metadata necessary to hold the blocksizes */
	size_t count; 	/* Number of block-index entries necessary */
	
	size_t i = 0;
	bool first = true;
	
	idx->entries = NULL;
	
	blocks = sqfs_blocklist_count(fs, inode);
	md_size = blocks * sizeof(sqfs_blocklist_entry);
	count = (inode->next.offset + md_size - 1)
		/ SQUASHFS_METADATA_SIZE;
	if (!(idx->entries = malloc(count * sizeof(sqfs_blockidx_entry))))
		return SQFS_ERR;
	
	blockidx = idx->entries;
	sqfs_blocklist_init(fs, inode, &bl);
	while (bl.remain && i < count) {
		/* If the MD cursor offset is small, we found a new MD-block.
		 * Skip the first MD-block, because we already know where it is:
		 * inode->next.offset */
		if (bl.cur.offset < sizeof(sqfs_blocklist_entry) && !first) {
			blockidx[i].data_block = bl.block + bl.input_size;
			blockidx[i++].md_block = (uint32_t)(bl.cur.block - fs->sb.inode_table_start);
		}
		first = false;
		
		err = sqfs_blocklist_next(&bl);
		if (err)
			break;
	}
	
	if (err && idx->entries)
		free(idx->entries);
	return err;
}

sqfs_err sqfs_blockidx_blocklist(sqfs *fs, sqfs_inode *inode,
		sqfs_blocklist *bl, sqfs_off_t start) {
	sqfs_err err, ret;
	size_t block, metablock, skipped;
	sqfs_blockidx *idx;
	sqfs_blockidx_entry *ie;
	sqfs_cache_entry *entry;
	
	size_t ble = sizeof(sqfs_blocklist_entry);
	
	/* Start with a regular blocklist */
	sqfs_blocklist_init(fs, inode, bl);
	block = (size_t)(start / fs->sb.block_size);
	if (block > bl->remain) { /* fragment */
		bl->remain = 0;
		return SQFS_OK;
	}
	
	/* How many MD-blocks do we want to skip? */
	metablock = (bl->cur.offset + block * ble)
		/ SQUASHFS_METADATA_SIZE;
	if (metablock == 0)
		return SQFS_OK; /* no skip needed, don't want an index */
	if (!sqfs_blockidx_indexable(fs, inode))
		return SQFS_OK; /* too small to index */
	
	/* Get the block-index, creating it if necessary */
	if ((err = sqfs_cache_get(&fs->blockidx, inode->base.inode_number, &entry)))
		return err;
	idx = sqfs_cache_entry_value(entry);
	ret = SQFS_OK;
	if (!sqfs_cache_entry_is_initialized(entry)) {
		idx->error = sqfs_blockidx_add(fs, inode, idx);
		ret = sqfs_cache_entry_ready(entry);
	}
	if (!ret)
		ret = idx->error;
	
	/* Use the block index to skip ahead */
	if (!err) {
		skipped = (metablock * SQUASHFS_METADATA_SIZE / ble)
			- (bl->cur.offset / sizeof(sqfs_blocklist_entry));
	
		ie = idx->entries + metablock - 1;
		bl->cur.block = ie->md_block + fs->sb.inode_table_start;
		bl->cur.offset %= sizeof(sqfs_blocklist_entry);
		bl->remain -= skipped;
		bl->pos = (uint64_t)skipped * fs->sb.block_size;
		bl->block = ie->data_block;
	}
	
	if ((err = sqfs_cache_entry_release(entry)))
		ret = err;
	return ret;
}


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
#include "file.h"

#include <stddef.h>
#include <string.h>
#include <sys/stat.h>

#include "squashfuse.h"

sqfs_err sqfs_frag_entry(sqfs *fs, struct squashfs_fragment_entry *frag,
		uint32_t idx) {
	if (idx == SQUASHFS_INVALID_FRAG)
		return SQFS_ERR;
	
	sqfs_err err = sqfs_table_get(&fs->frag_table, fs, idx, frag);
	sqfs_swapin_fragment_entry(frag);
	return err;
}

sqfs_err sqfs_frag_block(sqfs *fs, sqfs_inode *inode,
		size_t *offset, size_t *size, sqfs_block **block) {
	if (!S_ISREG(inode->base.mode))
		return SQFS_ERR;
	
	struct squashfs_fragment_entry frag;
	sqfs_err err = sqfs_frag_entry(fs, &frag, inode->xtra.reg.frag_idx);
	if (err)
		return err;
	
	err = sqfs_data_cache(fs, &fs->frag_cache, frag.start_block,
		frag.size, block);
	if (err)
		return SQFS_ERR;
	
	*offset = inode->xtra.reg.frag_off;
	*size = inode->xtra.reg.file_size % fs->sb.block_size;
	return SQFS_OK;
}

size_t sqfs_blocklist_count(sqfs *fs, sqfs_inode *inode) {
	uint64_t size = inode->xtra.reg.file_size, block = fs->sb.block_size;
	if (inode->xtra.reg.frag_idx == SQUASHFS_INVALID_FRAG) {
		return sqfs_divceil(size, block);
	} else {
		return size / block;
	}
}

void sqfs_blocklist_init(sqfs *fs, sqfs_inode *inode, sqfs_blocklist *bl) {
	bl->fs = fs;
	bl->remain = sqfs_blocklist_count(fs, inode);
	bl->cur = inode->next;
	bl->started = false;
	bl->pos = 0;
	bl->block = inode->xtra.reg.start_block;
	bl->input_size = 0;
}

sqfs_err sqfs_blocklist_next(sqfs_blocklist *bl) {
	if (bl->remain == 0)
		return SQFS_ERR;
	--(bl->remain);
	
	sqfs_err err = sqfs_md_read(bl->fs, &bl->cur, &bl->header,
		sizeof(bl->header));
	if (err)
		return err;
	sqfs_swapin32(&bl->header);
	
	bl->block += bl->input_size;
	bool compressed;
	sqfs_data_header(bl->header, &compressed, &bl->input_size);
	
	if (bl->started)
		bl->pos += bl->fs->sb.block_size;
	bl->started = true;
	
	return SQFS_OK;
}

sqfs_err sqfs_read_range(sqfs *fs, sqfs_inode *inode, off_t start,
		off_t *size, void *buf) {
	if (!S_ISREG(inode->base.mode))
		return SQFS_ERR;
	
	off_t file_size = inode->xtra.reg.file_size;
	size_t block_size = fs->sb.block_size;
	
	if (size < 0 || start > file_size)
		return SQFS_ERR;
	if (start == file_size) {
		*size = 0;
		return SQFS_OK;
	}
	
	sqfs_blocklist bl;
	sqfs_blocklist_init(fs, inode, &bl);
	
	size_t read_off = start % block_size;
	char *buf_orig = buf;
	while (*size > 0) {
		sqfs_block *block = NULL;
		sqfs_err err;
		size_t data_off, data_size;
		
		bool fragment = (bl.remain == 0);
		if (fragment) { // fragment
			if (inode->xtra.reg.frag_idx == SQUASHFS_INVALID_FRAG)
				break;
			err = sqfs_frag_block(fs, inode, &data_off, &data_size, &block);
			if (err)
				return err;
		} else {			
			if ((err = sqfs_blocklist_next(&bl)))
				return err;
			if (bl.pos + block_size <= start)
				continue;
			
			data_off = 0;
			if (bl.input_size == 0) { // Hole!
				data_size = file_size - bl.pos;
				if (data_size > block_size)
					data_size = block_size;
			} else {
				err = sqfs_data_cache(fs, &fs->data_cache, bl.block,
					bl.header, &block);
				if (err)
					return err;
				data_size = block->size;
			}
		}
		
		size_t take = data_size - read_off;
		if (take > *size)
			take = *size;
		if (block) {
			memcpy(buf, block->data + data_off + read_off, take);
			// BLOCK CACHED, DON'T DISPOSE
		} else {
			memset(buf, 0, take);
		}
		read_off = 0;
		*size -= take;
		buf += take;
		
		if (fragment)
			break;
	}
	
	*size = (char*)buf - buf_orig;
	return *size ? SQFS_OK : SQFS_ERR;
}

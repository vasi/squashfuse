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
#include "dir.h"

#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>

#include "squashfuse.h"

static sqfs_err sqfs_dir_md_read(sqfs_dir *dir, void *buf, size_t size) {
	dir->remain -= size;
	return sqfs_md_read(dir->fs, &dir->cur, buf, size);
}

sqfs_err sqfs_opendir(sqfs *fs, sqfs_inode *inode, sqfs_dir *dir) {
	if (!S_ISDIR(inode->base.mode))
		return SQFS_ERR;
	
	memset(dir, 0, sizeof(*dir));
	dir->fs = fs;
	dir->cur.block = inode->xtra.dir.start_block +
		fs->sb.directory_table_start;
	dir->cur.offset = inode->xtra.dir.offset;
	dir->remain = inode->xtra.dir.dir_size - 3;
	dir->entry.name = dir->name;
	
	return SQFS_OK;
}

sqfs_dir_entry *sqfs_readdir(sqfs_dir *dir, sqfs_err *err) {
	struct squashfs_dir_entry entry;
	
	while (dir->header.count == 0) {
		if (dir->remain <= 0) {
			*err = SQFS_OK;
			return NULL;
		}
		
		if ((*err = sqfs_dir_md_read(dir, &dir->header, sizeof(dir->header))))
			return NULL;
		sqfs_swapin_dir_header(&dir->header);
		++(dir->header.count);
	}
	
	if ((*err = sqfs_dir_md_read(dir, &entry, sizeof(entry))))
		return NULL;
	sqfs_swapin_dir_entry(&entry);
	--(dir->header.count);
	
	sqfs_dir_md_read(dir, &dir->name, entry.size + 1);
	dir->name[entry.size + 1] = '\0';
	
	dir->entry.inode = ((uint64_t)dir->header.start_block << 16) +
		entry.offset;
	/* entry.inode_number is signed */
	dir->entry.inode_number = dir->header.inode_number + (int16_t)entry.inode_number;
	dir->entry.type = entry.type;
	
	*err = SQFS_OK;
	return &dir->entry;
}

/* Internal versions use 'size' characters of 'name' to do the lookup */

static sqfs_err sqfs_dir_ff_sz(sqfs_dir *dir, sqfs_inode *inode,
		const char *name, size_t size) {
	size_t skipped = 0;
	sqfs_md_cursor cur = inode->next;
	size_t count = inode->xtra.dir.idx_count;
	
	if (count == 0)
		return SQFS_OK;
	
	while (count--) {
		char cmp[SQUASHFS_NAME_LEN + 1];
		struct squashfs_dir_index idx;
		sqfs_err err;
		
		if ((err = sqfs_md_read(dir->fs, &cur, &idx, sizeof(idx))))
			return err;
		sqfs_swapin_dir_index(&idx);
		
		if ((err = sqfs_md_read(dir->fs, &cur, cmp, idx.size + 1)))
			return err;
		cmp[idx.size + 1] = '\0';
		
		if (strncmp(cmp, name, size) > 0)
			break;
		
		skipped = idx.index;
		dir->cur.block = idx.start_block + dir->fs->sb.directory_table_start;
	}
	
	dir->remain -= skipped;
	dir->cur.offset = (dir->cur.offset + skipped) % SQUASHFS_METADATA_SIZE;
	return SQFS_OK;
}

static sqfs_err sqfs_lookup_dir_sz(sqfs_dir *dir,
		const char *name, size_t size, sqfs_dir_entry *entry) {
	sqfs_err err;
	sqfs_dir_entry *dentry;
	while ((dentry = sqfs_readdir(dir, &err))) {
		if (strncmp(dentry->name, name, size) == 0) {
			*entry = *dentry;
			entry->name = NULL;
			return SQFS_OK;
		}
	}
	return SQFS_ERR;
}

static sqfs_err sqfs_lookup_dir_fast_sz(sqfs_dir *dir,
		sqfs_inode *inode, const char *name, size_t size,
		sqfs_dir_entry *entry) {
	sqfs_err err;
	if ((err = sqfs_dir_ff_sz(dir, inode, name, size)))
		return err;
	return sqfs_lookup_dir_sz(dir, name, size, entry);
}

sqfs_err sqfs_dir_ff(sqfs_dir *dir, sqfs_inode *inode, const char *name) {
	return sqfs_dir_ff_sz(dir, inode, name, strlen(name));
}
sqfs_err sqfs_lookup_dir(sqfs_dir *dir, const char *name,
		sqfs_dir_entry *entry) {
	return sqfs_lookup_dir_sz(dir, name, strlen(name), entry);
}
sqfs_err sqfs_lookup_dir_fast(sqfs_dir *dir, sqfs_inode *inode,
		const char *name, sqfs_dir_entry *entry) {
	return sqfs_lookup_dir_fast_sz(dir, inode, name, strlen(name), entry);
}

sqfs_err sqfs_lookup_path(sqfs *fs, sqfs_inode *inode,
		const char *path) {
	sqfs_dir dir;
	sqfs_dir_entry entry;
	
	const char *name;
	size_t size;
	while (*path) {
		sqfs_err err = sqfs_opendir(fs, inode, &dir);
		if (err)
			return err;
		
		/* Find next path component */
		while (*path == '/') /* skip leading slashes */
			++path;
		
		name = path;
		while (*path && *path != '/')
			++path;
		size = path - name;
		if (size == 0) /* we're done */
			break;
		
		err = sqfs_lookup_dir_fast_sz(&dir, inode, name, size, &entry);
		if (err)
			return err;
		
		if ((err = sqfs_inode_get(fs, inode, entry.inode)))
			return err;
	}
	return SQFS_OK;
}

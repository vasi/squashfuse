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
#include "xattr.h"

#include "squashfuse.h"
#include "nonstd.h"

#include <stdio.h>

sqfs_err sqfs_xattr_init(sqfs *fs) {
	off_t start = fs->sb.xattr_id_table_start;
	if (start == SQUASHFS_INVALID_BLK)
		return SQFS_OK;
	
	ssize_t read = sqfs_pread(fs->fd, &fs->xattr_info, sizeof(fs->xattr_info),
		start);
	if (read != sizeof(fs->xattr_info))
		return SQFS_ERR;
	sqfs_swapin_xattr_id_table(&fs->xattr_info);
	
	return sqfs_table_init(&fs->xattr_table, fs->fd,
		start + sizeof(fs->xattr_info), sizeof(struct squashfs_xattr_id),
		fs->xattr_info.xattr_ids);
}

sqfs_err sqfs_xattr_test(sqfs *fs, sqfs_inode *inode) {
	if (fs->xattr_info.xattr_ids == 0) {
		fprintf(stderr, "xattr: Not supported\n");
		return SQFS_OK;
	}
	
	if (inode->xattr == SQUASHFS_INVALID_XATTR) {
		fprintf(stderr, "xattr: None found\n");
		return SQFS_OK;
	}
	
	struct squashfs_xattr_id xattr_id;
	fprintf(stderr, "xattr: inode idx %d\n", inode->xattr);
	sqfs_err err = sqfs_table_get(&fs->xattr_table, fs, inode->xattr,
		&xattr_id);
	if (err)
		return SQFS_ERR;
	sqfs_swapin_xattr_id(&xattr_id);
	fprintf(stderr, "xattr: count %d, size %d\n", xattr_id.count,
		xattr_id.size);
	
	char name[256]; // FIXME
	sqfs_md_cursor cur;
	sqfs_md_cursor_inode(&cur, xattr_id.xattr,
		fs->xattr_info.xattr_table_start);
	while (xattr_id.count--) {
		struct squashfs_xattr_entry entry;
		if ((err = sqfs_md_read(fs, &cur, &entry, sizeof(entry))))
			return err;
		sqfs_swapin_xattr_entry(&entry);
		
		if ((err = sqfs_md_read(fs, &cur, name, entry.size)))
			return err;
		name[entry.size] = '\0';
		fprintf(stderr, "xattr: type %d, name %s\n",
			entry.type & SQUASHFS_XATTR_PREFIX_MASK, name);
		
		uint32_t valsize;
		if ((err = sqfs_md_read(fs, &cur, &valsize, sizeof(valsize))))
			return err;
		sqfs_swapin32(&valsize);
		if ((err = sqfs_md_read(fs, &cur, NULL, valsize)))
			return err;
	}
	
	return SQFS_OK;
}

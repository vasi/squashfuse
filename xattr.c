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

sqfs_err sqfs_xattr_open(sqfs *fs, sqfs_inode *inode, sqfs_xattr *xattr) {
	xattr->remain = 0; // assume none exist	
	if (fs->xattr_info.xattr_ids == 0 || inode->xattr == SQUASHFS_INVALID_XATTR)
		return SQFS_OK;
	
	sqfs_err err = sqfs_table_get(&fs->xattr_table, fs, inode->xattr,
		&xattr->info);
	if (err)
		return SQFS_ERR;
	sqfs_swapin_xattr_id(&xattr->info);
	
	sqfs_md_cursor_inode(&xattr->cur, xattr->info.xattr,
		fs->xattr_info.xattr_table_start);
	
	xattr->fs = fs;
	xattr->remain = xattr->info.count;
	xattr->state = SQFS_XATTR_READ;
	return SQFS_OK;
}

static sqfs_err sqfs_xattr_forward(sqfs_xattr *xattr, sqfs_xattr_state want) {
	sqfs_err err = SQFS_OK;
	while (xattr->state != want) {
		switch (xattr->state) {
			case SQFS_XATTR_READ: err = sqfs_xattr_read(xattr); break;
			case SQFS_XATTR_NAME: err = sqfs_xattr_name(xattr, NULL); break;
			case SQFS_XATTR_VAL: err = sqfs_xattr_val(xattr, NULL, NULL); break;
		}
		if (err)
			return err;
	}
	return err;
}

sqfs_err sqfs_xattr_read(sqfs_xattr *xattr) {
	sqfs_err err;
	if ((err = sqfs_xattr_forward(xattr, SQFS_XATTR_READ)))
		return err;
	if (xattr->remain == 0)
		return SQFS_ERR;
	
	if ((err = sqfs_md_read(xattr->fs, &xattr->cur, &xattr->entry,
			sizeof(xattr->entry))))
		return err;
	sqfs_swapin_xattr_entry(&xattr->entry);
	
	--(xattr->remain);
	xattr->state = SQFS_XATTR_NAME;
	return err;
}

sqfs_err sqfs_xattr_name(sqfs_xattr *xattr, char *name) {
	sqfs_err err;
	if ((err = sqfs_xattr_forward(xattr, SQFS_XATTR_NAME)))
		return err;
	
	if ((err = sqfs_md_read(xattr->fs, &xattr->cur, name, xattr->entry.size)))
		return err;
	xattr->state = SQFS_XATTR_VAL;
	return err;
}

sqfs_err sqfs_xattr_val(sqfs_xattr *xattr, size_t *size, void *buf) {
	sqfs_err err;
	if ((err = sqfs_xattr_forward(xattr, SQFS_XATTR_VAL)))
		return err;
	
	bool ool = xattr->entry.type & SQUASHFS_XATTR_VALUE_OOL;
	sqfs *fs = xattr->fs;
	sqfs_md_cursor ool_cur, *cur = &xattr->cur;
	
	struct squashfs_xattr_val val;
	if ((err = sqfs_md_read(fs, cur, &val, sizeof(val))))
		return err;
	sqfs_swapin_xattr_val(&val);
	
	if (ool && (size || buf)) {
		uint64_t pos;
		if ((err = sqfs_md_read(fs, cur, &pos, sizeof(pos))))
			return err;
		sqfs_swapin64(&pos);
		
		cur = &ool_cur;
		sqfs_md_cursor_inode(cur, pos, fs->xattr_info.xattr_table_start);
		if ((err = sqfs_md_read(fs, cur, &val, sizeof(val))))
			return err;
		sqfs_swapin_xattr_val(&val);
	}
	
	if (size)
		*size = val.vsize;
	if ((err = sqfs_md_read(fs, cur, buf, val.vsize)))
		return err;	
	
	xattr->state = SQFS_XATTR_READ;
	return err;
}

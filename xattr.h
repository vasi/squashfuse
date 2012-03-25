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
#ifndef SQFS_XATTR_H
#define SQFS_XATTR_H

#include "common.h"

#include "squashfs_fs.h"

sqfs_err sqfs_xattr_init(sqfs *fs);

typedef enum {
	SQFS_XATTR_READ, SQFS_XATTR_NAME, SQFS_XATTR_VAL
} sqfs_xattr_state;

typedef struct {
	sqfs *fs;
	sqfs_md_cursor cur;
	sqfs_xattr_state state;
	
	size_t remain;
	struct squashfs_xattr_id info;
	struct squashfs_xattr_entry entry;
} sqfs_xattr;

sqfs_err sqfs_xattr_open(sqfs *fs, sqfs_inode *inode, sqfs_xattr *xattr);

// Call once per xattr, while xattr->remain > 0
sqfs_err sqfs_xattr_read(sqfs_xattr *xattr);

size_t sqfs_xattr_name_size(sqfs_xattr *xattr);

// May call one or more of these after sqfs_xattr_read, in order
// Out pointers may be NULL to just skip the data instead of reading it.
// Caller is responsible for ensuring enough room in buffers.
// Name is not null terminated, can use xattr->entry.size for length.
sqfs_err sqfs_xattr_name(sqfs_xattr *xattr, char *name);
sqfs_err sqfs_xattr_val(sqfs_xattr *xattr, size_t *size, void *buf);

#endif

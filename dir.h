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
#ifndef SQFS_DIR_H
#define SQFS_DIR_H

#include "common.h"

#include <stdbool.h>

#include "squashfs_fs.h"

typedef struct {
	sqfs_inode_id inode;
	sqfs_inode_num inode_number;
	int type;
	char *name;
	sq_off_t offset, next_offset;
} sqfs_dir_entry;

typedef struct {
	sqfs *fs;
	
	sqfs_md_cursor cur;
	size_t total, remain;
	struct squashfs_dir_header header;
	
	char name[SQUASHFS_NAME_LEN+1];
	sqfs_dir_entry entry;
} sqfs_dir;

sqfs_err sqfs_opendir(sqfs *fs, sqfs_inode *inode, sqfs_dir *dir);
sqfs_dir_entry *sqfs_readdir(sqfs_dir *dir, sqfs_err *err);

/* Fast forward in a directory to find a given file */
sqfs_err sqfs_dir_ff_offset(sqfs_dir *dir, sqfs_inode *inode, sq_off_t off,
	bool *found);

/* For lookup name functions, returned entry will have no name field */
sqfs_err sqfs_lookup_dir(sqfs_dir *dir, const char *name,
	sqfs_dir_entry *entry);
sqfs_err sqfs_lookup_dir_fast(sqfs_dir *dir, sqfs_inode *inode,
	const char *name, sqfs_dir_entry *entry);

/* 'path' will be modified, 'inode' replaced */
sqfs_err sqfs_lookup_path(sqfs *fs, sqfs_inode *inode, const char *path);

#endif

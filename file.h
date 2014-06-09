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
#ifndef SQFS_FILE_H
#define SQFS_FILE_H

#include "common.h"

/* Iterator through blocks of a file */
typedef uint32_t sqfs_blocklist_entry;
typedef struct {
  sqfs *fs;
  size_t remain;      /* How many blocks left in the file? */
  sqfs_md_cursor cur; /* Points to next blocksize in MD */
  bool started;

  uint64_t pos;
  
  uint64_t block;     /* Points to next data block location */
  sqfs_blocklist_entry header; /* Packed blocksize data */
  uint32_t input_size;         /* Extracted size of this block */
} sqfs_blocklist;

/* Count the number of blocks in a file */
size_t sqfs_blocklist_count(sqfs *fs, sqfs_inode *inode);

/* Setup a blocklist for a file */
void sqfs_blocklist_init(sqfs *fs, sqfs_inode *inode,
  sqfs_blocklist *bl);

/* Iterate along the blocklist */
sqfs_err sqfs_blocklist_next(sqfs_blocklist *bl);


/* Read a range of a file into a buffer */
sqfs_err sqfs_read_range(sqfs *fs, sqfs_inode *inode, sqfs_off_t start,
  sqfs_off_t *size, void *buf);

#endif

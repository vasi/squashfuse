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
#ifndef SQFS_LL_H
#define SQFS_LL_H

#include "squashfuse.h"

#include <fuse_lowlevel.h>

typedef struct sqfs_ll sqfs_ll;
struct sqfs_ll {
	sqfs fs;
	
	/* Converting inodes between squashfs and fuse */
	fuse_ino_t (*ino_fuse)(sqfs_ll *ll, sqfs_inode_id i);
	sqfs_inode_id (*ino_sqfs)(sqfs_ll *ll, fuse_ino_t i);
	
	/* Register a new inode, returning the fuse ID for it */
	fuse_ino_t (*ino_register)(sqfs_ll *ll, sqfs_dir_entry *e);
	void (*ino_forget)(sqfs_ll *ll, fuse_ino_t i, size_t refs);
	
	/* Like register, but don't actually remember it */
	fuse_ino_t (*ino_fuse_num)(sqfs_ll *ll, sqfs_dir_entry *e);
	
	/* Private data, and how to destroy it */
	void *ino_data;
	void (*ino_destroy)(sqfs_ll *ll);	
};

sqfs_err sqfs_ll_init(sqfs_ll *ll);
void sqfs_ll_destroy(sqfs_ll *ll);


/* Get an inode from an sqfs_ll */
sqfs_err sqfs_ll_inode(sqfs_ll *ll, sqfs_inode *inode, fuse_ino_t i);

/* Convenience function: Get both ll and inode, and handle errors */
#define SQFS_FUSE_INODE_NONE 0
typedef struct {
	sqfs_ll *ll;
	sqfs_inode inode;
} sqfs_ll_i;
sqfs_err sqfs_ll_iget(fuse_req_t req, sqfs_ll_i *lli, fuse_ino_t i);


int sqfs_ll_daemonize(int fg);

void sqfs_ll_op_getattr(fuse_req_t req, fuse_ino_t ino,
	struct fuse_file_info *fi);

void sqfs_ll_op_opendir(fuse_req_t req, fuse_ino_t ino,
	struct fuse_file_info *fi);

void sqfs_ll_op_create(fuse_req_t req, fuse_ino_t parent, const char *name,
	mode_t mode, struct fuse_file_info *fi);

void sqfs_ll_op_releasedir(fuse_req_t req, fuse_ino_t ino,
	struct fuse_file_info *fi);

size_t sqfs_ll_add_direntry(fuse_req_t req, char *buf, size_t bufsize,
	const char *name, const struct stat *st, off_t off);

void sqfs_ll_op_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
	off_t off, struct fuse_file_info *fi);

void sqfs_ll_op_lookup(fuse_req_t req, fuse_ino_t parent,
		const char *name);

void sqfs_ll_op_open(fuse_req_t req, fuse_ino_t ino,
		struct fuse_file_info *fi);

void sqfs_ll_op_release(fuse_req_t req, fuse_ino_t ino,
		struct fuse_file_info *fi);

void sqfs_ll_op_read(fuse_req_t req, fuse_ino_t ino,
		size_t size, off_t off, struct fuse_file_info *fi);

void sqfs_ll_op_readlink(fuse_req_t req, fuse_ino_t ino);

void sqfs_ll_op_listxattr(fuse_req_t req, fuse_ino_t ino, size_t size);

void sqfs_ll_op_getxattr(fuse_req_t req, fuse_ino_t ino,
		const char *name, size_t size
#ifdef FUSE_XATTR_POSITION
		, uint32_t position
#endif
		);

void sqfs_ll_op_forget(fuse_req_t req, fuse_ino_t ino,
		unsigned long nlookup);

void sqfs_ll_op_init(void *userdata, struct fuse_conn_info *conn);

void stfs_ll_op_statfs(fuse_req_t req, fuse_ino_t ino);


/* Helpers to abstract out FUSE 2.5 vs 3.0+ differences */

typedef struct {
	int fd;
	struct fuse_session *session;
#if FUSE_USE_VERSION < 30
	struct fuse_chan *ch;
#endif
} sqfs_ll_chan;

sqfs_err sqfs_ll_mount(
		sqfs_ll_chan *ch,
		const char *mountpoint,
		struct fuse_args *args,
        struct fuse_lowlevel_ops *ops,
        size_t ops_size,
        void *userdata);

void sqfs_ll_unmount(sqfs_ll_chan *ch, const char *mountpoint);

void alarm_tick(int sig);

void setup_idle_timeout(struct fuse_session *se, unsigned int timeout_secs);

void teardown_idle_timeout();

sqfs_ll *sqfs_ll_open_with_subdir(const char *path, size_t offset, const char *subdir);
sqfs_ll *sqfs_ll_open(const char *path, size_t offset);


#endif

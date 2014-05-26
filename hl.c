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
#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <fuse.h>
#include "fuseprivate.h"
#include "squashfuse.h"
#include "nonstd.h"

typedef struct sqfs_hl sqfs_hl;
struct sqfs_hl {
	sqfs fs;
	sqfs_inode root;
};

static sqfs_err sqfs_hl_lookup(sqfs **fs, sqfs_inode *inode,
		const char *path) {
	sqfs_hl *hl = fuse_get_context()->private_data;
	*fs = &hl->fs;
	if (inode)
		*inode = hl->root; /* copy */

	return path ? sqfs_lookup_path(*fs, inode, path) : SQFS_OK;
}


static void sqfs_hl_op_destroy(void *user_data) {
	sqfs_hl *hl = (sqfs_hl*)user_data;
	sqfs_destroy(&hl->fs);
	free(hl);
}

static void *sqfs_hl_op_init(struct fuse_conn_info *conn) {
	return fuse_get_context()->private_data;
}

static int sqfs_hl_op_getattr(const char *path, struct stat *st) {
	sqfs *fs;
	sqfs_inode inode;
	if (sqfs_hl_lookup(&fs, &inode, path))
		return -ENOENT;
	
	if (sqfs_stat(fs, &inode, st))
		return -ENOENT;
	
	return 0;
}

static int sqfs_hl_op_opendir(const char *path, struct fuse_file_info *fi) {
	sqfs *fs;
	sqfs_inode inode;
	sqfs_dir *dir;
	
	if (sqfs_hl_lookup(&fs, &inode, path))
		return -ENOENT;
	
	dir = malloc(sizeof(*dir));
	if (!dir)
		return -ENOMEM;
	if (sqfs_opendir(fs, &inode, dir)) {
		free(dir);
		return -ENOTDIR;
	}
	fi->fh = (intptr_t)dir;
	return 0;
}

static int sqfs_hl_op_releasedir(const char *path,
		struct fuse_file_info *fi) {
	free((sqfs_dir*)(intptr_t)fi->fh);
	fi->fh = 0;
	return 0;
}

// FIXME: Use offsets??
static int sqfs_hl_op_readdir(const char *path, void *buf,
		fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	sqfs_err err;
	sqfs_dir_entry *dentry;
	struct stat st;
	sqfs_dir *dir = (sqfs_dir*)(intptr_t)fi->fh;
	
	memset(&st, 0, sizeof(st));
	
	while ((dentry = sqfs_readdir(dir, &err))) {
		st.st_mode = sqfs_mode(dentry->type);
		filler(buf, dentry->name, &st, 0);
	}
	if (err)
		return -EIO;
	return 0;
}

static int sqfs_hl_op_open(const char *path, struct fuse_file_info *fi) {
	sqfs *fs;
	sqfs_inode *inode;
	
	if (fi->flags & (O_WRONLY | O_RDWR))
		return -EROFS;
	
	inode = malloc(sizeof(*inode));
	if (!inode)
		return -ENOMEM;
	
	if (sqfs_hl_lookup(&fs, inode, path)) {
		free(inode);
		return -ENOENT;
	}
	
	if (!S_ISREG(inode->base.mode)) {
		free(inode);
		return -EISDIR;
	}
	
	fi->fh = (intptr_t)inode;
	return 0;
}

static int sqfs_hl_op_release(const char *path, struct fuse_file_info *fi) {
	free((sqfs_inode*)(intptr_t)fi->fh);
	fi->fh = 0;
	return 0;
}

static int sqfs_hl_op_read(const char *path, char *buf, size_t size,
		off_t off, struct fuse_file_info *fi) {
	sqfs *fs;
	sqfs_hl_lookup(&fs, NULL, NULL);
	sqfs_inode *inode = (sqfs_inode*)(intptr_t)fi->fh;

	off_t osize = size;
	if (sqfs_read_range(fs, inode, off, &osize, buf))
		return -EIO;
	return osize;
}

// FIXME: Make sqfs_readlink take a size
static int sqfs_hl_op_readlink(const char *path, char *buf, size_t size) {
	char *tmp;
	sqfs *fs;
	sqfs_inode inode;
	
	if (sqfs_hl_lookup(&fs, &inode, path))
		return -ENOENT;
	
	if (!S_ISLNK(inode.base.mode)) {
		return -EINVAL;
	} else if (!(tmp = calloc(1, inode.xtra.symlink_size + 1))) {
		return -ENOMEM;
	} else if (sqfs_readlink(fs, &inode, tmp)) {
		free(tmp);
		return -EIO;
	}
	
	strncpy(buf, tmp, size);
	free(tmp);
	buf[size - 1] = '\0';
	return 0;
}

// FIXME: share
static int sqfs_hl_listxattr_real(sqfs_xattr *x, char *buf, size_t *size) {
	size_t count = 0;
	
	while (x->remain) {
		size_t n;
		if (sqfs_xattr_read(x))
			 return EIO;
		n = sqfs_xattr_name_size(x);
		count += n + 1;
		
		if (buf) {
			if (count > *size)
				return ERANGE;
			if (sqfs_xattr_name(x, buf, true))
				return EIO;
			buf += n;
			*buf++ = '\0';
		}
	}
	*size = count;
	return 0;
}

static int sqfs_hl_op_listxattr(const char *path, char *buf, size_t size) {
	sqfs *fs;
	sqfs_inode inode;
	sqfs_xattr x;
	int ferr;
	
	if (sqfs_hl_lookup(&fs, &inode, path))
		return -ENOENT;

	if (sqfs_xattr_open(fs, &inode, &x))
		return -EIO;
	
	ferr = sqfs_hl_listxattr_real(&x, buf, &size);
	if (ferr)
		return -ferr;
	return size;
}

static int sqfs_hl_op_getxattr(const char *path, const char *name,
		char *value, size_t size
#ifdef FUSE_XATTR_POSITION
		, uint32_t position
#endif
		) {
	sqfs *fs;
	sqfs_inode inode;
	sqfs_xattr x;
	bool found;
	size_t vsize;
	char *buf = NULL;

	if (sqfs_hl_lookup(&fs, &inode, path))
		return -ENOENT;
	
	if (sqfs_xattr_open(fs, &inode, &x))
		return -EIO;
#ifdef FUSE_XATTR_POSITION
	if (position != 0) /* We don't support resource forks */
		return -EINVAL;
#endif
	if (sqfs_xattr_find(&x, name, &found))
		return -EIO;
	if (!found)
		return -sqfs_enoattr();
	if (sqfs_xattr_value_size(&x, &vsize))
		return -EIO;
	if (!size)
		return vsize;
	if (!(buf = malloc(vsize)))
		return -ENOMEM;
	if (sqfs_xattr_value(&x, buf)) {
		free(buf);
		return -EIO;
	}
	
	if (size > vsize)
		size = vsize;
	memcpy(value, buf, size);
	free(buf);
	return size;
}

static void sqfs_hl_usage(char *name, bool fuse_usage) {
	fprintf(stderr, "%s (c) 2012 Dave Vasilevsky\n\n", PACKAGE_STRING);
	fprintf(stderr, "Usage: %s [options] ARCHIVE MOUNTPOINT\n",
		name ? name : PACKAGE_NAME);
	if (fuse_usage) {
		struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
		fuse_opt_add_arg(&args, ""); /* progname */
		fuse_opt_add_arg(&args, "-ho");
		fprintf(stderr, "\n");
		fuse_parse_cmdline(&args, NULL, NULL, NULL);
	}
	exit(-2);
}

typedef struct {
	char *progname;
	const char *image;
	int mountpoint;
} sqfs_hl_opts;

static int sqfs_hl_opt_proc(void *data, const char *arg, int key,
		struct fuse_args *outargs) {
	sqfs_hl_opts *opts = (sqfs_hl_opts*)data;
	if (key == FUSE_OPT_KEY_NONOPT) {
		if (opts->mountpoint) {
			return -1; /* Too many args */
		} else if (opts->image) {
			opts->mountpoint = 1;
			return 1;
		} else {
			opts->image = arg;
			return 0;
		}
	} else if (key == FUSE_OPT_KEY_OPT) {
		if (strncmp(arg, "-h", 2) == 0 || strncmp(arg, "--h", 3) == 0)
			sqfs_hl_usage(opts->progname, true);
	}
	return 1; /* Keep */
}

static sqfs_hl *sqfs_hl_open(const char *path) {
	sqfs_hl *hl;
	
	hl = malloc(sizeof(*hl));
	if (!hl) {
		perror("Can't allocate memory");
	} else {
		memset(hl, 0, sizeof(*hl));
	
		if (sqfs_open_image(&hl->fs, path) == SQFS_OK) {
			if (sqfs_inode_get(&hl->fs, &hl->root, hl->fs.sb.root_inode))
				fprintf(stderr, "Can't find the root of this filesystem!\n");
			else
				return hl;
			sqfs_destroy(&hl->fs);
		}
		
		free(hl);
	}
	return NULL;
}

int main(int argc, char *argv[]) {
	struct fuse_args args;
	sqfs_hl_opts opts;
	
	sqfs_hl *hl;
	
	struct fuse_operations sqfs_hl_ops;
	memset(&sqfs_hl_ops, 0, sizeof(sqfs_hl_ops));
	sqfs_hl_ops.init			= sqfs_hl_op_init;
	sqfs_hl_ops.destroy		= sqfs_hl_op_destroy;
	sqfs_hl_ops.getattr		= sqfs_hl_op_getattr;
	sqfs_hl_ops.opendir		= sqfs_hl_op_opendir;
	sqfs_hl_ops.releasedir	= sqfs_hl_op_releasedir;
	sqfs_hl_ops.readdir		= sqfs_hl_op_readdir;
	sqfs_hl_ops.open		= sqfs_hl_op_open;
	sqfs_hl_ops.release		= sqfs_hl_op_release;
	sqfs_hl_ops.read		= sqfs_hl_op_read;
	sqfs_hl_ops.readlink	= sqfs_hl_op_readlink;
	sqfs_hl_ops.listxattr	= sqfs_hl_op_listxattr;
	sqfs_hl_ops.getxattr	= sqfs_hl_op_getxattr;
   
	/* PARSE ARGS */
	args.argc = argc;
	args.argv = argv;
	args.allocated = 0;
	
	opts.progname = argv[0];
	opts.image = NULL;
	opts.mountpoint = 0;
	if (fuse_opt_parse(&args, &opts, NULL, sqfs_hl_opt_proc) == -1)
		sqfs_hl_usage(argv[0], true);
	
	hl = sqfs_hl_open(opts.image);
	if (!hl)
		return -1;
	return fuse_main(args.argc, args.argv, &sqfs_hl_ops, hl);
}

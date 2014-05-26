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

#include "ll.h"
#include "squashfuse.h"
#include "nonstd.h"
#include "fuseprivate.h"

static const double SQFS_TIMEOUT = DBL_MAX;

static void sqfs_ll_op_getattr(fuse_req_t req, fuse_ino_t ino,
		struct fuse_file_info *fi) {
	sqfs_ll_i lli;
	struct stat st;
	if (sqfs_ll_iget(req, &lli, ino))
		return;
	
	if (sqfs_stat(&lli.ll->fs, &lli.inode, &st)) {
		fuse_reply_err(req, ENOENT);
	} else {
		st.st_ino = ino;
		fuse_reply_attr(req, &st, SQFS_TIMEOUT);
	}
}

static void sqfs_ll_op_opendir(fuse_req_t req, fuse_ino_t ino,
		struct fuse_file_info *fi) {
	sqfs_ll_i lli;
	sqfs_dir *dir;
	
	fi->fh = (intptr_t)NULL;
	
	if (sqfs_ll_iget(req, &lli, ino))
		return;
	
	dir = malloc(sizeof(sqfs_dir));
	if (!dir) {
		fuse_reply_err(req, ENOMEM);
	} else {
		if (sqfs_opendir(&lli.ll->fs, &lli.inode, dir)) {
			fuse_reply_err(req, ENOTDIR);
		} else {
			fi->fh = (intptr_t)dir;
			fuse_reply_open(req, fi);
			return;
		}
		free(dir);
	}
}

static void sqfs_ll_op_releasedir(fuse_req_t req, fuse_ino_t ino,
		struct fuse_file_info *fi) {
	free((sqfs_dir*)(intptr_t)fi->fh);
	fuse_reply_err(req, 0); /* yes, this is necessary */
}

static void sqfs_ll_add_direntry(fuse_req_t req, const char *name,
		const struct stat *st, off_t off) {
	size_t bsize;
	char *buf;
	
	#if HAVE_DECL_FUSE_ADD_DIRENTRY
		bsize = fuse_add_direntry(req, NULL, 0, name, st, 0);
	#else
		bsize = fuse_dirent_size(strlen(name));
	#endif
	
	buf = malloc(bsize);
	if (!buf) {
		fuse_reply_err(req, ENOMEM);
	} else {
		#if HAVE_DECL_FUSE_ADD_DIRENTRY
			fuse_add_direntry(req, buf, bsize, name, st, off + bsize);
		#else
			fuse_add_dirent(buf, name, st, off + bsize);
		#endif
		fuse_reply_buf(req, buf, bsize);
		free(buf);
	}
}

/* TODO: More efficient to return multiple entries at a time? */
static void sqfs_ll_op_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
		off_t off, struct fuse_file_info *fi) {
	sqfs_ll_i lli;
	sqfs_dir *dir;
	sqfs_dir_entry *entry;
	sqfs_err err = SQFS_OK;
	
	sqfs_ll_iget(req, &lli, SQFS_FUSE_INODE_NONE);
	
	dir = (sqfs_dir*)(intptr_t)fi->fh;
	entry = sqfs_readdir(dir, &err);
	if (err) {
		fuse_reply_err(req, EIO);
	} else if (!entry) {
		fuse_reply_buf(req, NULL, 0);
	} else {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = lli.ll->ino_fuse_num(lli.ll, entry);
		st.st_mode = sqfs_mode(entry->type);
		sqfs_ll_add_direntry(req, entry->name, &st, off);
	}
}

static void sqfs_ll_op_lookup(fuse_req_t req, fuse_ino_t parent,
		const char *name) {
	sqfs_ll_i lli;
	sqfs_dir dir;
	sqfs_dir_entry entry;
	sqfs_inode inode;
	
	if (sqfs_ll_iget(req, &lli, parent))
		return;
	
	if (sqfs_opendir(&lli.ll->fs, &lli.inode, &dir)) {
		fuse_reply_err(req, ENOTDIR);
		return;
	}
	if (sqfs_lookup_dir_fast(&dir, &lli.inode, name, &entry)) {
		fuse_reply_err(req, ENOENT);
		return;
	}
	
	if (sqfs_inode_get(&lli.ll->fs, &inode, entry.inode)) {
		fuse_reply_err(req, ENOENT);
	} else {
		struct fuse_entry_param fentry;
		memset(&fentry, 0, sizeof(fentry));
		if (sqfs_stat(&lli.ll->fs, &inode, &fentry.attr)) {
			fuse_reply_err(req, EIO);
		} else {
			fentry.attr_timeout = fentry.entry_timeout = SQFS_TIMEOUT;
			fentry.ino = lli.ll->ino_register(lli.ll, &entry);
			fentry.attr.st_ino = fentry.ino;
			fuse_reply_entry(req, &fentry);
		}
	}
}

static void sqfs_ll_op_open(fuse_req_t req, fuse_ino_t ino,
		struct fuse_file_info *fi) {
	sqfs_inode *inode;
	sqfs_ll *ll;
	
	if (fi->flags & (O_WRONLY | O_RDWR)) {
		fuse_reply_err(req, EROFS);
		return;
	}
	
	inode = malloc(sizeof(sqfs_inode));
	if (!inode) {
		fuse_reply_err(req, ENOMEM);
		return;
	}
	
	ll = fuse_req_userdata(req);
	if (sqfs_ll_inode(ll, inode, ino)) {
		fuse_reply_err(req, ENOENT);
	} else if (!S_ISREG(inode->base.mode)) {
		fuse_reply_err(req, EISDIR);
	} else {
		fi->fh = (intptr_t)inode;
		fuse_reply_open(req, fi);
		return;
	}
	free(inode);
}

static void sqfs_ll_op_release(fuse_req_t req, fuse_ino_t ino,
		struct fuse_file_info *fi) {
	free((sqfs_inode*)(intptr_t)fi->fh);
	fi->fh = 0;
	fuse_reply_err(req, 0);
}

static void sqfs_ll_op_read(fuse_req_t req, fuse_ino_t ino,
		size_t size, off_t off, struct fuse_file_info *fi) {
	sqfs_ll *ll = fuse_req_userdata(req);
	sqfs_inode *inode = (sqfs_inode*)(intptr_t)fi->fh;
	sqfs_err err = SQFS_OK;
	
	off_t osize;
	char *buf = malloc(size);
	if (!buf) {
		fuse_reply_err(req, ENOMEM);
		return;
	}
	
	osize = size;
	err = sqfs_read_range(&ll->fs, inode, off, &osize, buf);
	if (err) {
		fuse_reply_err(req, EIO);
	} else if (osize == 0) { /* EOF */
		fuse_reply_buf(req, NULL, 0);
	} else {
		fuse_reply_buf(req, buf, osize);
	}
	free(buf);
}

static void sqfs_ll_op_readlink(fuse_req_t req, fuse_ino_t ino) {
	char *dst;
	size_t size;
	sqfs_ll_i lli;
	if (sqfs_ll_iget(req, &lli, ino))
		return;
	
	if (!S_ISLNK(lli.inode.base.mode)) {
		fuse_reply_err(req, EINVAL);
	} else if (sqfs_readlink(&lli.ll->fs, &lli.inode, NULL, &size)) {
		fuse_reply_err(req, EIO);
	} else if (!(dst = malloc(size + 1))) {
		fuse_reply_err(req, ENOMEM);
	} else if (sqfs_readlink(&lli.ll->fs, &lli.inode, dst, &size)) {
		fuse_reply_err(req, EIO);
		free(dst);
	} else {
		fuse_reply_readlink(req, dst);
		free(dst);
	}
}

static int sqfs_ll_listxattr_real(sqfs_xattr *x, char *buf, size_t *size) {
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

static void sqfs_ll_op_listxattr(fuse_req_t req, fuse_ino_t ino, size_t size) {
	sqfs_ll_i lli;
	sqfs_xattr x;
	char *buf;
	int ferr;
	
	if (sqfs_ll_iget(req, &lli, ino))
		return;

	if (sqfs_xattr_open(&lli.ll->fs, &lli.inode, &x)) {
		fuse_reply_err(req, EIO);
		return;
	}
	
	buf = NULL;
	if (size) {
		if (!(buf = malloc(size))) {
			fuse_reply_err(req, ENOMEM);
			return;
		}
	}
	
	ferr = sqfs_ll_listxattr_real(&x, buf, &size);
	if (ferr) {
		fuse_reply_err(req, ferr);
	} else if (buf) {
		fuse_reply_buf(req, buf, size);
	} else {
		fuse_reply_xattr(req, size);
	}
	free(buf);
}

static void sqfs_ll_op_getxattr(fuse_req_t req, fuse_ino_t ino,
		const char *name, size_t size
#ifdef FUSE_XATTR_POSITION
		, uint32_t position
#endif
		) {
	sqfs_xattr x;
	bool found;
	size_t vsize;
	char *buf = NULL;
	
	sqfs_ll_i lli;
	if (sqfs_ll_iget(req, &lli, ino))
		return;
	
	if (sqfs_xattr_open(&lli.ll->fs, &lli.inode, &x)) {
		fuse_reply_err(req, EIO);
#ifdef FUSE_XATTR_POSITION
	} else if (position != 0) { /* We don't support resource forks */
		fuse_reply_err(req, EINVAL);
#endif
	} else if (sqfs_xattr_find(&x, name, &found)) {
		fuse_reply_err(req, EIO);
	} else if (!found) {
		fuse_reply_err(req, sqfs_enoattr());
	} else if (sqfs_xattr_value_size(&x, &vsize)) {
		fuse_reply_err(req, EIO);
	} else if (!size) {
		fuse_reply_xattr(req, vsize);
	} else if (vsize > size) {
		fuse_reply_err(req, ERANGE);
	} else if (!(buf = malloc(vsize))) {
		fuse_reply_err(req, ENOMEM);
	} else if (sqfs_xattr_value(&x, buf)) {
		fuse_reply_err(req, EIO);
	} else {
		fuse_reply_buf(req, buf, vsize);
	}
	free(buf);
}

static void sqfs_ll_op_forget(fuse_req_t req, fuse_ino_t ino,
		unsigned long nlookup) {
	sqfs_ll_i lli;
	sqfs_ll_iget(req, &lli, SQFS_FUSE_INODE_NONE);
	lli.ll->ino_forget(lli.ll, ino, nlookup);
	fuse_reply_none(req);
}

static void sqfs_usage(char *name, bool fuse_usage) {
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
} sqfs_ll_opts;

static int sqfs_ll_opt_proc(void *data, const char *arg, int key,
		struct fuse_args *outargs) {
	sqfs_ll_opts *opts = (sqfs_ll_opts*)data;
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
			sqfs_usage(opts->progname, true);
	}
	return 1; /* Keep */
}


/* Helpers to abstract out FUSE 2.5 vs 2.6+ differences */

typedef struct {
	int fd;
	struct fuse_chan *ch;
} sqfs_ll_chan;

static sqfs_err sqfs_ll_mount(sqfs_ll_chan *ch, const char *mountpoint,
		struct fuse_args *args) {
	#ifdef HAVE_NEW_FUSE_UNMOUNT
		ch->ch = fuse_mount(mountpoint, args);
	#else
		ch->fd = fuse_mount(mountpoint, args);
		if (ch->fd == -1)
			return SQFS_ERR;
		ch->ch = fuse_kern_chan_new(ch->fd);
	#endif
	return ch->ch ? SQFS_OK : SQFS_ERR;
}

static void sqfs_ll_unmount(sqfs_ll_chan *ch, const char *mountpoint) {
	#ifdef HAVE_NEW_FUSE_UNMOUNT
		fuse_unmount(mountpoint, ch->ch);
	#else
		close(ch->fd);
		fuse_unmount(mountpoint);
	#endif
}

static sqfs_ll *sqfs_ll_open(const char *path) {
	sqfs_ll *ll;
	
	ll = malloc(sizeof(*ll));
	if (!ll) {
		perror("Can't allocate memory");
	} else {
		memset(ll, 0, sizeof(*ll));
	
		if (sqfs_open_image(&ll->fs, path) == SQFS_OK) {
			if (sqfs_ll_init(ll))
				fprintf(stderr, "Can't initialize this filesystem!\n");
			else
				return ll;
			sqfs_destroy(&ll->fs);
		}
		
		free(ll);
	}
	return NULL;
}

int main(int argc, char *argv[]) {
	struct fuse_args args;
	sqfs_ll_opts opts;
	
	char *mountpoint = NULL;
	int mt, fg;
	
	int err;
	sqfs_ll *ll;
	
	struct fuse_lowlevel_ops sqfs_ll_ops;
	memset(&sqfs_ll_ops, 0, sizeof(sqfs_ll_ops));
	sqfs_ll_ops.getattr		= sqfs_ll_op_getattr;
	sqfs_ll_ops.opendir		= sqfs_ll_op_opendir;
	sqfs_ll_ops.releasedir	= sqfs_ll_op_releasedir;
	sqfs_ll_ops.readdir		= sqfs_ll_op_readdir;
	sqfs_ll_ops.lookup		= sqfs_ll_op_lookup;
	sqfs_ll_ops.open		= sqfs_ll_op_open;
	sqfs_ll_ops.release		= sqfs_ll_op_release;
	sqfs_ll_ops.read		= sqfs_ll_op_read;
	sqfs_ll_ops.readlink	= sqfs_ll_op_readlink;
	sqfs_ll_ops.listxattr	= sqfs_ll_op_listxattr;
	sqfs_ll_ops.getxattr	= sqfs_ll_op_getxattr;
	sqfs_ll_ops.forget		= sqfs_ll_op_forget;
   
	/* PARSE ARGS */
	args.argc = argc;
	args.argv = argv;
	args.allocated = 0;
	
	opts.progname = argv[0];
	opts.image = NULL;
	opts.mountpoint = 0;
	if (fuse_opt_parse(&args, &opts, NULL, sqfs_ll_opt_proc) == -1)
		sqfs_usage(argv[0], true);

	if (fuse_parse_cmdline(&args, &mountpoint, &mt, &fg) == -1)
		sqfs_usage(argv[0], true);
	if (mountpoint == NULL)
		sqfs_usage(argv[0], true);
	
	/* OPEN FS */
	err = !(ll = sqfs_ll_open(opts.image));
	
	/* STARTUP FUSE */
	if (!err) {
		sqfs_ll_chan ch;
		err = -1;
		if (sqfs_ll_mount(&ch, mountpoint, &args) == SQFS_OK) {
			struct fuse_session *se = fuse_lowlevel_new(&args,
				&sqfs_ll_ops, sizeof(sqfs_ll_ops), ll);	
			if (se != NULL) {
				if (sqfs_ll_daemonize(fg) != -1) {
					if (fuse_set_signal_handlers(se) != -1) {
						fuse_session_add_chan(se, ch.ch);
						/* FIXME: multithreading */
						err = fuse_session_loop(se);
						fuse_remove_signal_handlers(se);
						#if HAVE_DECL_FUSE_SESSION_REMOVE_CHAN
							fuse_session_remove_chan(ch.ch);
						#endif
					}
				}
				fuse_session_destroy(se);
			}
			sqfs_ll_destroy(ll);
			sqfs_ll_unmount(&ch, mountpoint);
		}
	}
	fuse_opt_free_args(&args);
	free(mountpoint);
	
	return -err;
}

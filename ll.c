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
#include "ll.h"
#include "fuseprivate.h"
#include "stat.h"

#include "nonstd.h"

#include <errno.h>
#include <float.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

static const double SQFS_TIMEOUT = DBL_MAX;

/* See comment near alarm_tick for details of how idle timeouts are
   managed. */

/* timeout, in seconds, after which we will automatically unmount */
static unsigned int idle_timeout_secs = 0;
/* last access timestamp */
static time_t last_access = 0;
/* count of files and directories currently open.  drecement after
 * last_access for correctness. */
static sig_atomic_t open_refcount = 0;
/* same as lib/fuse_signals.c */
static struct fuse_session *fuse_instance = NULL;

static void update_access_time(void) {
#ifdef SQFS_MULTITHREADED
	/* We only need to track access time if we have an idle timeout,
	 * don't bother with expensive operations if idle_timeout is 0.
	 */
	if (idle_timeout_secs) {
		time_t now = time(NULL);
		__atomic_store_n(&last_access, now, __ATOMIC_RELEASE);
	}
#else
	last_access = time(NULL);
#endif
}

static void update_open_refcount(int delta) {
#ifdef SQFS_MULTITHREADED
	__atomic_fetch_add(&open_refcount, delta, __ATOMIC_RELEASE);
#else
	open_refcount += delta;
#endif
}

static inline time_t get_access_time(void) {
#ifdef SQFS_MULTITHREADED
	return __atomic_load_n(&last_access, __ATOMIC_ACQUIRE);
#else
	return last_access;
#endif
}

static inline sig_atomic_t get_open_refcount(void) {
#ifdef SQFS_MULTITHREADED
	return __atomic_load_n(&open_refcount, __ATOMIC_ACQUIRE);
#else
	return open_refcount;
#endif
}

void sqfs_ll_op_getattr(fuse_req_t req, fuse_ino_t ino,
		struct fuse_file_info *fi) {
	sqfs_ll_i lli;
	struct stat st;
	update_access_time();
	if (sqfs_ll_iget(req, &lli, ino))
		return;
	
	if (sqfs_stat(&lli.ll->fs, &lli.inode, &st)) {
		fuse_reply_err(req, ENOENT);
	} else {
		st.st_ino = ino;
		fuse_reply_attr(req, &st, SQFS_TIMEOUT);
	}
}

void sqfs_ll_op_opendir(fuse_req_t req, fuse_ino_t ino,
		struct fuse_file_info *fi) {
	sqfs_ll_i *lli;
	update_access_time();
	
	fi->fh = (intptr_t)NULL;
	
	lli = malloc(sizeof(*lli));
	if (!lli) {
		fuse_reply_err(req, ENOMEM);
		return;
	}
	
	if (sqfs_ll_iget(req, lli, ino) == SQFS_OK) {
		if (!S_ISDIR(lli->inode.base.mode)) {
			fuse_reply_err(req, ENOTDIR);
		} else {
			fi->fh = (intptr_t)lli;
			update_open_refcount(1);
			fuse_reply_open(req, fi);
			return;
		}
	}
	free(lli);
}

void sqfs_ll_op_create(fuse_req_t req, fuse_ino_t parent, const char *name,
			      mode_t mode, struct fuse_file_info *fi) {
	update_access_time();
	fuse_reply_err(req, EROFS);
}

void sqfs_ll_op_releasedir(fuse_req_t req, fuse_ino_t ino,
		struct fuse_file_info *fi) {
	update_access_time();
	update_open_refcount(-1);
	free((sqfs_ll_i*)(intptr_t)fi->fh);
	fuse_reply_err(req, 0); /* yes, this is necessary */
}

size_t sqfs_ll_add_direntry(fuse_req_t req, char *buf, size_t bufsize,
		const char *name, const struct stat *st, off_t off) {
	#if HAVE_DECL_FUSE_ADD_DIRENTRY
		return fuse_add_direntry(req, buf, bufsize, name, st, off);
	#else
		size_t esize = fuse_dirent_size(strlen(name));
		if (bufsize >= esize)
			fuse_add_dirent(buf, name, st, off);
		return esize;
	#endif
}

void sqfs_ll_op_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
		off_t off, struct fuse_file_info *fi) {
	sqfs_err sqerr;
	sqfs_dir dir;
	sqfs_name namebuf;
	sqfs_dir_entry entry;
	size_t esize;
	struct stat st;
	
	char *buf = NULL, *bufpos = NULL;
	sqfs_ll_i *lli = (sqfs_ll_i*)(intptr_t)fi->fh;
	int err = 0;
	
	update_access_time();
	if (sqfs_dir_open(&lli->ll->fs, &lli->inode, &dir, off))
		err = EINVAL;
	if (!err && !(bufpos = buf = malloc(size)))
		err = ENOMEM;
	
	if (!err) {
		memset(&st, 0, sizeof(st));
		sqfs_dentry_init(&entry, namebuf);
		while (sqfs_dir_next(&lli->ll->fs, &dir, &entry, &sqerr)) {
			st.st_ino = lli->ll->ino_fuse_num(lli->ll, &entry);
			st.st_mode = sqfs_dentry_mode(&entry);
		
			esize = sqfs_ll_add_direntry(req, bufpos, size, sqfs_dentry_name(&entry),
				&st, sqfs_dentry_next_offset(&entry));
			if (esize > size)
				break;
		
			bufpos += esize;
			size -= esize;
		}
		if (sqerr)
			err = EIO;
	}
	
	if (err)
		fuse_reply_err(req, err);
	else
		fuse_reply_buf(req, buf, bufpos - buf);
	free(buf);
}

void sqfs_ll_op_lookup(fuse_req_t req, fuse_ino_t parent,
		const char *name) {
	sqfs_ll_i lli;
	sqfs_err sqerr;
	sqfs_name namebuf;
	sqfs_dir_entry entry;
	bool found;
	sqfs_inode inode;
	
	update_access_time();
	if (sqfs_ll_iget(req, &lli, parent))
		return;
	
	if (!S_ISDIR(lli.inode.base.mode)) {
		fuse_reply_err(req, ENOTDIR);
		return;
	}
	
	sqfs_dentry_init(&entry, namebuf);
	sqerr = sqfs_dir_lookup(&lli.ll->fs, &lli.inode, name, strlen(name), &entry,
		&found);
	if (sqerr) {
		fuse_reply_err(req, EIO);
		return;
	}
	if (!found) {
		/* Returning with zero inode indicates not found with
		 * timeout, i.e. future lookups of this name will not generate
		 * fuse requests.
		 */
		struct fuse_entry_param fentry;
		memset(&fentry, 0, sizeof(fentry));
		fentry.attr_timeout = fentry.entry_timeout = SQFS_TIMEOUT;
		fentry.ino = 0;
		fuse_reply_entry(req, &fentry);
		return;
	}

	if (sqfs_inode_get(&lli.ll->fs, &inode, sqfs_dentry_inode(&entry))) {
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

void sqfs_ll_op_open(fuse_req_t req, fuse_ino_t ino,
		struct fuse_file_info *fi) {
	sqfs_inode *inode;
	sqfs_ll *ll;
	
	update_access_time();
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
		fi->keep_cache = 1;
		update_open_refcount(1);
		fuse_reply_open(req, fi);
		return;
	}
	free(inode);
}

void sqfs_ll_op_release(fuse_req_t req, fuse_ino_t ino,
		struct fuse_file_info *fi) {
	free((sqfs_inode*)(intptr_t)fi->fh);
	fi->fh = 0;
	update_access_time();
	update_open_refcount(-1);
	fuse_reply_err(req, 0);
}

void sqfs_ll_op_read(fuse_req_t req, fuse_ino_t ino,
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
	
	update_access_time();
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

void sqfs_ll_op_readlink(fuse_req_t req, fuse_ino_t ino) {
	char *dst;
	size_t size;
	sqfs_ll_i lli;
	update_access_time();
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

void sqfs_ll_op_listxattr(fuse_req_t req, fuse_ino_t ino, size_t size) {
	sqfs_ll_i lli;
	char *buf;
	int ferr;
	
	update_access_time();
	if (sqfs_ll_iget(req, &lli, ino))
		return;

	buf = NULL;
	if (size && !(buf = malloc(size))) {
		fuse_reply_err(req, ENOMEM);
		return;
	}
	
	ferr = sqfs_listxattr(&lli.ll->fs, &lli.inode, buf, &size);
	if (ferr) {
		fuse_reply_err(req, ferr);
	} else if (buf) {
		fuse_reply_buf(req, buf, size);
	} else {
		fuse_reply_xattr(req, size);
	}
	free(buf);
}

void sqfs_ll_op_getxattr(fuse_req_t req, fuse_ino_t ino,
		const char *name, size_t size
#ifdef FUSE_XATTR_POSITION
		, uint32_t position
#endif
		) {
	sqfs_ll_i lli;
	char *buf = NULL;
	size_t real = size;

#ifdef FUSE_XATTR_POSITION
	if (position != 0) { /* We don't support resource forks */
		fuse_reply_err(req, EINVAL);
		return;
	}
#endif
	
	update_access_time();
	if (sqfs_ll_iget(req, &lli, ino))
		return;
	
	if (!(buf = malloc(size)))
		fuse_reply_err(req, ENOMEM);
	else if (sqfs_xattr_lookup(&lli.ll->fs, &lli.inode, name, buf, &real))
		fuse_reply_err(req, EIO);
	else if (real == 0)
		fuse_reply_err(req, sqfs_enoattr());
	else if (size == 0)
		fuse_reply_xattr(req, real);
	else if (size < real)
		fuse_reply_err(req, ERANGE);
	else
		fuse_reply_buf(req, buf, real);
	free(buf);
}

void sqfs_ll_op_forget(fuse_req_t req, fuse_ino_t ino,
		unsigned long nlookup) {
	sqfs_ll_i lli;
	update_access_time();
	sqfs_ll_iget(req, &lli, SQFS_FUSE_INODE_NONE);
	lli.ll->ino_forget(lli.ll, ino, nlookup);
	fuse_reply_none(req);
}

void stfs_ll_op_statfs(fuse_req_t req, fuse_ino_t ino) {
	sqfs_ll *ll;
	struct statvfs st;
	int err;

	ll = fuse_req_userdata(req);
	err = sqfs_statfs(&ll->fs, &st);
	if (err == 0) {
		fuse_reply_statfs(req, &st);
	} else {
		fuse_reply_err(req, err);
	}
}

void sqfs_ll_op_init(void *userdata, struct fuse_conn_info *conn) {
	sqfs_ll *ll = userdata;

	notify_mount_ready_async(ll->fs.notify_pipe, NOTIFY_SUCCESS);
}

/* Helpers to abstract out FUSE 2.5 vs 3.0+ differences */

#if FUSE_USE_VERSION >= 30
sqfs_err sqfs_ll_mount(
		sqfs_ll_chan *ch,
		const char *mountpoint,
		struct fuse_args *args,
        struct fuse_lowlevel_ops *ops,
        size_t ops_size,
        void *userdata) {
	ch->session = fuse_session_new(args, ops, ops_size, userdata);
	if (!ch->session) {
		return SQFS_ERR;
	}
	if (fuse_session_mount(ch->session, mountpoint)) {
		fuse_session_destroy(ch->session);
		ch->session = NULL;
		return SQFS_ERR;
	}
	return SQFS_OK;
}

void sqfs_ll_unmount(sqfs_ll_chan *ch, const char *mountpoint) {
	fuse_session_unmount(ch->session);
	fuse_session_destroy(ch->session);
	ch->session = NULL;
}

#else /* FUSE_USE_VERSION >= 30 */

void sqfs_ll_unmount(sqfs_ll_chan *ch, const char *mountpoint) {
	if (ch->session) {
#if HAVE_DECL_FUSE_SESSION_REMOVE_CHAN
		fuse_session_remove_chan(ch->ch);
#endif
		fuse_session_destroy(ch->session);
	}
#ifdef HAVE_NEW_FUSE_UNMOUNT
	fuse_unmount(mountpoint, ch->ch);
#else
	close(ch->fd);
	fuse_unmount(mountpoint);
#endif
}

sqfs_err sqfs_ll_mount(
		sqfs_ll_chan *ch,
		const char *mountpoint,
		struct fuse_args *args,
		struct fuse_lowlevel_ops *ops,
		size_t ops_size,
		void *userdata) {
#ifdef HAVE_NEW_FUSE_UNMOUNT
	ch->ch = fuse_mount(mountpoint, args);
	if (!ch->ch) {
		return SQFS_ERR;
	}
#else
	ch->fd = fuse_mount(mountpoint, args);
	if (ch->fd == -1)
		return SQFS_ERR;
	ch->ch = fuse_kern_chan_new(ch->fd);
	if (!ch->ch) {
		close(ch->fd);
		return SQFS_ERR;
	}
#endif

	ch->session = fuse_lowlevel_new(args,
			ops, sizeof(*ops), userdata);
	if (!ch->session) {
		sqfs_ll_unmount(ch, mountpoint);
		return SQFS_ERR;
	}
	fuse_session_add_chan(ch->session, ch->ch);
	return SQFS_OK;
}

#endif /* FUSE_USE_VERSION >= 30 */

/* Idle unmount timeout management is based on signal handling from
   fuse (see set_one_signal_handler and exit_handler in libfuse's
   lib/fuse_signals.c.

   When an idle timeout is set, we use a one second alarm to check if
   no activity has taken place within the idle window, as tracked by
   last_access.  We also maintain an open/opendir refcount so that
   directories and files can be held open and unaccessed without
   triggering the idle timeout.
 */

void alarm_tick(int sig) {
	if (!fuse_instance || idle_timeout_secs == 0) {
		return;
	}

	if (get_open_refcount() == 0 &&
		time(NULL) - get_access_time() > idle_timeout_secs) {
		/* Safely shutting down fuse in a cross-platform way is a dark art!
		   But just about any platform should stop on SIGINT, so do that */
		kill(getpid(), SIGINT);
		return;
	}
	alarm(1);  /* always reset our alarm */
}

void setup_idle_timeout(struct fuse_session *se, unsigned int timeout_secs) {
	idle_timeout_secs = timeout_secs;
	update_access_time();

	struct sigaction sa;
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = alarm_tick;
	sigemptyset(&(sa.sa_mask));
	sa.sa_flags = 0;

	fuse_instance = se;
	if (sigaction(SIGALRM, &sa, NULL) == -1) {
		perror("fuse: cannot get old signal handler");
		return;
	}

	alarm(1);
}

void teardown_idle_timeout() {
	alarm(0);
	fuse_instance = NULL;
}

sqfs_ll *sqfs_ll_open_with_subdir(const char *path, size_t offset, const char *subdir) {
	sqfs_ll *ll;
	
	ll = malloc(sizeof(*ll));
	if (!ll) {
		perror("Can't allocate memory");
	} else {
		memset(ll, 0, sizeof(*ll));
		ll->fs.offset = offset;
		if (sqfs_open_image_with_subdir(&ll->fs, path, offset, subdir) == SQFS_OK) {
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

sqfs_ll *sqfs_ll_open(const char *path, size_t offset) {
	return sqfs_ll_open_with_subdir(path, offset, NULL);
}

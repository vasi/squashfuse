#include "ll.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/***** INODE CONVERSION FOR 64-BIT INODES ****
 *
 * sqfs(root) maps to FUSE_ROOT_ID == 1
 * sqfs(0) maps to 2
 *
 * Both 1 and 2 are guaranteed not to be used by sqfs, due to inode size
 */
static fuse_ino_t sqfs_ll_ino64_fuse(sqfs_ll *ll, sqfs_inode_id i) {
	if (i == ll->fs.sb.root_inode) {
		return FUSE_ROOT_ID;
	} else if (i == 0) {
		return 2;
	} else {
		return i;
	}
}
static sqfs_inode_id sqfs_ll_ino64_sqfs(sqfs_ll *ll, fuse_ino_t i) {
	if (i == FUSE_ROOT_ID) {
		return ll->fs.sb.root_inode;
	} else if (i == 2) {
		return 0;
	} else {
		return i;
	}
}
static fuse_ino_t sqfs_ll_ino64_register(sqfs_ll *ll, sqfs_dir_entry *e) {
	return sqfs_ll_ino64_fuse(ll, e->inode);
}


sqfs_err sqfs_ll_init(sqfs_ll *ll, int fd) {
	// FIXME: 32-bit inodes?
	if (sizeof(fuse_ino_t) < SQFS_INODE_ID_BYTES) {
		fprintf(stderr, "Need at least %d-bit inodes!\n",
			SQFS_INODE_ID_BYTES * 8);
		exit(-3);
	} else {
		ll->ino_fuse = sqfs_ll_ino64_fuse;
		ll->ino_sqfs = sqfs_ll_ino64_sqfs;
		ll->ino_register = sqfs_ll_ino64_register;
		ll->ino_data = NULL;
	}
	
	return sqfs_init(&ll->fs, fd);
}

void sqfs_ll_destroy(sqfs_ll *ll) {
	sqfs_destroy(&ll->fs);
}

sqfs_err sqfs_ll_inode(sqfs_ll *ll, sqfs_inode *inode, fuse_ino_t i) {
	return sqfs_inode_get(&ll->fs, inode, ll->ino_sqfs(ll, i));
}


sqfs_err sqfs_ll_iget(fuse_req_t req, sqfs_ll_i *lli, fuse_ino_t i) {
	lli->ll = fuse_req_userdata(req);
	sqfs_err err = SQFS_OK;
	if (i != SQFS_FUSE_INODE_NONE) {
		err = sqfs_ll_inode(lli->ll, &lli->inode, i);
		if (err)
			fuse_reply_err(req, ENOENT);
	}
	return err;
}

sqfs_err sqfs_ll_stat(sqfs_ll *ll, sqfs_inode *inode, struct stat *st) {
	memset(st, 0, sizeof(*st));
	st->st_mode = inode->base.mode | sqfs_mode(inode->base.inode_type);
	st->st_nlink = inode->nlink;
	// FIXME: uid, gid, rdev
	st->st_mtimespec.tv_sec = st->st_ctimespec.tv_sec =
		st->st_atimespec.tv_sec = inode->base.mtime;
	if (S_ISREG(st->st_mode)) {
		st->st_size = inode->xtra.reg.file_size;
		st->st_blocks = st->st_size / 512;
	} // FIXME: do symlinks, dirs, etc have a size?
	st->st_blksize = ll->fs.sb.block_size; // seriously?
	return SQFS_OK;
}

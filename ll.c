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



/***** INODE CONVERSION FOR 32-BIT INODES ****
 *
 * We maintain a table of sqfs_inode_num => sqfs_inode_id.
 * We go the other direction by fetching inodes.
 *
 * Mapping: sqfs_inode_num <=> fuse_ino_t
 *   Most num(N) maps to N + 1
 *   num(root) maps to FUSE_ROOT_ID == 1
 *   num(0) maps to num(root) + 1
 *
 * FIXME:
 * - Theoretically this could overflow if a filesystem uses all 2 ** 32 inodes,
 *   since fuse inode zero is unavailable.
 * - We only strictly need 48 bits for each table entry, not 64.
 * - If an export table is available, we can lookup inode_id's there, instead of
 *   keeping a table. Or maybe keep just a small cache?
 */
#define FUSE_INODE_NONE 0
#define SQFS_INODE_NONE 1

typedef struct {
	sqfs_inode_num root;
	sqfs_inode_id table[0];
} sqfs_ll_inode_map;

static fuse_ino_t sqfs_ll_ino32_num2fuse(sqfs_ll *ll, sqfs_inode_num n) {
	sqfs_ll_inode_map *map = ll->ino_data;
	if (n == map->root) {
		return FUSE_ROOT_ID;
	} else if (n == 0) {
		return map->root + 1;
	} else {
		return n + 1;
	} 
}

static fuse_ino_t sqfs_ll_ino32_fuse(sqfs_ll *ll, sqfs_inode_id i) {
	sqfs_inode inode;
	if (sqfs_inode_get(&ll->fs, &inode, i))
		return FUSE_INODE_NONE; // We shouldn't get here!
	return sqfs_ll_ino32_num2fuse(ll, inode.base.inode_number);
}

static sqfs_inode_id sqfs_ll_ino32_sqfs(sqfs_ll *ll, fuse_ino_t i) {
	sqfs_ll_inode_map *map = ll->ino_data;
	sqfs_inode_num n;
	if (i == FUSE_ROOT_ID) {
		n = map->root;
	} else if (i == map->root + 1) {
		n = 0;
	} else {
		n = i - 1;
	}
	
	return map->table[n];
}

static fuse_ino_t sqfs_ll_ino32_register(sqfs_ll *ll, sqfs_dir_entry *e) {
	sqfs_ll_inode_map *map = ll->ino_data;
	map->table[e->inode_number] = e->inode;
	return sqfs_ll_ino32_num2fuse(ll, e->inode_number);
}



sqfs_err sqfs_ll_init(sqfs_ll *ll, int fd) {
	sqfs_err err = sqfs_init(&ll->fs, fd);
	if (err)
		return err;
	
	ll->ino_data = NULL;
	if (sizeof(fuse_ino_t) >= SQFS_INODE_ID_BYTES) {
		ll->ino_fuse = sqfs_ll_ino64_fuse;
		ll->ino_sqfs = sqfs_ll_ino64_sqfs;
		ll->ino_register = sqfs_ll_ino64_register;
	} else {
		sqfs_inode inode;
		err = sqfs_inode_get(&ll->fs, &inode, ll->fs.sb.root_inode);
		if (err)
			return err;
		
		sqfs_ll_inode_map *map = malloc(sizeof(sqfs_ll_inode_map) +
			ll->fs.sb.inodes * sizeof(sqfs_inode_id));
		for (uint32_t i = 0; i < ll->fs.sb.inodes; ++i)
			map->table[i] = SQFS_INODE_NONE;
		map->root = inode.base.inode_number;
		map->table[map->root] = ll->fs.sb.root_inode;
		
		ll->ino_fuse = sqfs_ll_ino32_fuse;
		ll->ino_sqfs = sqfs_ll_ino32_sqfs;
		ll->ino_register = sqfs_ll_ino32_register;
		ll->ino_data = map;
	}
	
	return err; 
}

void sqfs_ll_destroy(sqfs_ll *ll) {
	sqfs_destroy(&ll->fs);
	if (ll->ino_data)
		free(ll->ino_data);
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
	st->st_mtimespec.tv_sec = st->st_ctimespec.tv_sec =
		st->st_atimespec.tv_sec = inode->base.mtime;
	
	if (S_ISREG(st->st_mode)) {
		// FIXME: do symlinks, dirs, etc have a size?
		st->st_size = inode->xtra.reg.file_size;
		st->st_blocks = st->st_size / 512;
	} else if (S_ISBLK(st->st_mode) || S_ISCHR(st->st_mode)) {
		st->st_rdev = inode->xtra.dev;
	}
	
	st->st_blksize = ll->fs.sb.block_size; // seriously?
	
	sqfs_err err = sqfs_id_get(&ll->fs, inode->base.uid, &st->st_uid);
	if (err)
		return err;
	err = sqfs_id_get(&ll->fs, inode->base.guid, &st->st_gid);
	if (err)
		return err;
	
	return SQFS_OK;
}

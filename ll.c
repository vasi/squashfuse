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
#include "ll.h"
#include "hash.h"

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

static fuse_ino_t sqfs_ll_ino64_fuse_num(sqfs_ll *ll, sqfs_dir_entry *e) {
	return sqfs_ll_ino64_fuse(ll, e->inode);
}

static sqfs_err sqfs_ll_ino64_init(sqfs_ll *ll) {
	ll->ino_fuse = sqfs_ll_ino64_fuse;
	ll->ino_sqfs = sqfs_ll_ino64_sqfs;
	ll->ino_fuse_num = sqfs_ll_ino64_fuse_num;
	return SQFS_OK;
}



/***** INODE CONVERSION FOR 32-BIT INODES ****
 *
 * We maintain a cache of sqfs_inode_num => sqfs_inode_id.
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
#define SQFS_ICACHE_INITIAL 65536

#define FUSE_INODE_NONE 0
#define SQFS_INODE_NONE 1

typedef struct {
	sqfs_inode_num root;
	sqfs_hash icache;
} sqfs_ll_inode_map;

typedef struct {
	size_t refcount;
	sqfs_inode_id inode;
} sqfs_ll_inode_entry;

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

static fuse_ino_t sqfs_ll_ino32_fuse2num(sqfs_ll *ll, fuse_ino_t i) {
	sqfs_ll_inode_map *map = ll->ino_data;
	if (i == FUSE_ROOT_ID) {
		return map->root;
	} else if (i == map->root + 1) {
		return 0;
	} else {
		return i - 1;
	}
}

static fuse_ino_t sqfs_ll_ino32_fuse(sqfs_ll *ll, sqfs_inode_id i) {
	sqfs_inode inode;
	if (sqfs_inode_get(&ll->fs, &inode, i))
		return FUSE_INODE_NONE; // We shouldn't get here!
	return sqfs_ll_ino32_num2fuse(ll, inode.base.inode_number);
}

static sqfs_inode_id sqfs_ll_ino32_sqfs(sqfs_ll *ll, fuse_ino_t i) {
	if (i == FUSE_ROOT_ID)
		return ll->fs.sb.root_inode;
		
	sqfs_ll_inode_map *map = ll->ino_data;
	sqfs_inode_num n = sqfs_ll_ino32_fuse2num(ll, i);
	
	sqfs_ll_inode_entry *ie = sqfs_hash_get(&map->icache, n);
	return ie ? ie->inode : SQFS_INODE_NONE;
}

static fuse_ino_t sqfs_ll_ino32_fuse_num(sqfs_ll *ll, sqfs_dir_entry *e) {
	return sqfs_ll_ino32_num2fuse(ll, e->inode_number);
}

static fuse_ino_t sqfs_ll_ino32_register(sqfs_ll *ll, sqfs_dir_entry *e) {
	sqfs_ll_inode_map *map = ll->ino_data;
	
	sqfs_ll_inode_entry *ie = sqfs_hash_get(&map->icache, e->inode_number);
	if (ie) {
		++ie->refcount;
	} else {
		ie = malloc(sizeof(*ie));
		if (!ie)
			return FUSE_INODE_NONE;
		ie->inode = e->inode;
		ie->refcount = 1;
		sqfs_hash_add(&map->icache, e->inode_number, ie);
	}
	
	return sqfs_ll_ino32_fuse_num(ll, e);
}

static void sqfs_ll_ino32_forget(sqfs_ll *ll, fuse_ino_t i, size_t refs) {
	sqfs_ll_inode_map *map = ll->ino_data;
	sqfs_inode_num n = sqfs_ll_ino32_fuse2num(ll, i);
	
	sqfs_ll_inode_entry *ie = sqfs_hash_get(&map->icache, n);
	if (!ie)
		return;
	
	if (ie->refcount > refs) {
		ie->refcount -= refs;
	} else {
		sqfs_hash_remove(&map->icache, n);
	}
}

static void sqfs_ll_ino32_destroy(sqfs_ll *ll) {
	sqfs_ll_inode_map *map = ll->ino_data;
	sqfs_hash_destroy(&map->icache);
	free(map);
}

static sqfs_err sqfs_ll_ino32_init(sqfs_ll *ll) {
	sqfs_inode inode;
	sqfs_err err = sqfs_inode_get(&ll->fs, &inode, ll->fs.sb.root_inode);
	if (err)
		return err;
		
	sqfs_ll_inode_map *map = malloc(sizeof(sqfs_ll_inode_map));
	map->root = inode.base.inode_number;
	sqfs_hash_init(&map->icache, SQFS_ICACHE_INITIAL);
		
	ll->ino_fuse = sqfs_ll_ino32_fuse;
	ll->ino_sqfs = sqfs_ll_ino32_sqfs;
	ll->ino_fuse_num = sqfs_ll_ino32_fuse_num;
	ll->ino_register = sqfs_ll_ino32_register;
	ll->ino_forget = sqfs_ll_ino32_forget;
	ll->ino_destroy = sqfs_ll_ino32_destroy;
	ll->ino_data = map;
	
	return err; 
}



static void sqfs_ll_null_forget(sqfs_ll *ll, fuse_ino_t i, size_t refs) {
	// pass
}

sqfs_err sqfs_ll_init(sqfs_ll *ll, int fd) {
	memset(ll, 0, sizeof(*ll));
	sqfs_err err = sqfs_init(&ll->fs, fd);
	if (err)
		return err;
	
	if (0 && sizeof(fuse_ino_t) >= SQFS_INODE_ID_BYTES) {
		err = sqfs_ll_ino64_init(ll);
	} else {
		err = sqfs_ll_ino32_init(ll);
	}
	if (!ll->ino_register)
		ll->ino_register = ll->ino_fuse_num;
	if (!ll->ino_forget)
		ll->ino_forget = sqfs_ll_null_forget;
	
	return err;
}

void sqfs_ll_destroy(sqfs_ll *ll) {
	sqfs_destroy(&ll->fs);
	if (ll->ino_destroy)
		ll->ino_destroy(ll);
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
	st->st_mtime = st->st_ctime = st->st_atime = inode->base.mtime;
	
	if (S_ISREG(st->st_mode)) {
		// FIXME: do symlinks, dirs, etc have a size?
		st->st_size = inode->xtra.reg.file_size;
		st->st_blocks = st->st_size / 512;
	} else if (S_ISBLK(st->st_mode) || S_ISCHR(st->st_mode)) {
		st->st_rdev = inode->xtra.dev;
	}
	
	st->st_blksize = ll->fs.sb.block_size; // seriously?
	
	uid_t id;
	sqfs_err err = sqfs_id_get(&ll->fs, inode->base.uid, &id);
	if (err)
		return err;
	st->st_uid = id;
	err = sqfs_id_get(&ll->fs, inode->base.guid, &id);
	st->st_gid = id;
	if (err)
		return err;
	
	return SQFS_OK;
}

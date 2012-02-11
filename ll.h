#ifndef SQFS_LL_H
#define SQFS_LL_H

#define FUSE_USE_VERSION 26
#include <fuse/fuse_lowlevel.h>

#include "squashfuse.h"

typedef struct sqfs_ll sqfs_ll;
struct sqfs_ll {
	sqfs fs;
	
	// Converting inodes between squashfs and fuse
	fuse_ino_t (*ino_fuse)(sqfs_ll *ll, sqfs_inode_id i);
	fuse_ino_t (*ino_sqfs)(sqfs_ll *ll, sqfs_inode_id i);
	void *ino_data;
};

sqfs_err sqfs_ll_init(sqfs_ll *ll, int fd);
void sqfs_ll_destroy(sqfs_ll *ll);

#endif

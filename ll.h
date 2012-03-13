#ifndef SQFS_LL_H
#define SQFS_LL_H

#define FUSE_USE_VERSION 26
#include <fuse_lowlevel.h>

#include "squashfuse.h"

typedef struct sqfs_ll sqfs_ll;
struct sqfs_ll {
	sqfs fs;
	
	// Converting inodes between squashfs and fuse
	fuse_ino_t (*ino_fuse)(sqfs_ll *ll, sqfs_inode_id i);
	sqfs_inode_id (*ino_sqfs)(sqfs_ll *ll, fuse_ino_t i);
	
	// Register a new inode, returning the fuse ID for it
	fuse_ino_t (*ino_register)(sqfs_ll *ll, sqfs_dir_entry *e);
	void *ino_data;
};

sqfs_err sqfs_ll_init(sqfs_ll *ll, int fd);
void sqfs_ll_destroy(sqfs_ll *ll);


// Get an inode from an sqfs_ll
sqfs_err sqfs_ll_inode(sqfs_ll *ll, sqfs_inode *inode, fuse_ino_t i);

// Convenience function: Get both ll and inode, and handle errors
#define SQFS_FUSE_INODE_NONE 0
typedef struct {
	sqfs_ll *ll;
	sqfs_inode inode;
} sqfs_ll_i;
sqfs_err sqfs_ll_iget(fuse_req_t req, sqfs_ll_i *lli, fuse_ino_t i);


// Fill in a stat structure. Does not set st_ino
sqfs_err sqfs_ll_stat(sqfs_ll *ll, sqfs_inode *inode, struct stat *st);

#endif

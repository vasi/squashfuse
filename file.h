#ifndef SQFS_FILE_H
#define SQFS_FILE_H

#include "common.h"
#include "squashfs_fs.h"

sqfs_err sqfs_frag_entry(sqfs *fs, struct squashfs_fragment_entry *frag,
	uint32_t idx);

sqfs_err sqfs_frag_block(sqfs *fs, sqfs_inode *inode,
	size_t *offset, size_t *size, sqfs_block **block);

#endif

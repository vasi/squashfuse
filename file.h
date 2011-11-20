#ifndef SQFS_FILE_H
#define SQFS_FILE_H

#include <stdbool.h>

#include "common.h"
#include "squashfs_fs.h"

sqfs_err sqfs_frag_entry(sqfs *fs, struct squashfs_fragment_entry *frag,
	uint32_t idx);

sqfs_err sqfs_frag_block(sqfs *fs, sqfs_inode *inode,
	size_t *offset, size_t *size, sqfs_block **block);


typedef struct {
	sqfs *fs;
	size_t remain;
	sqfs_md_cursor cur;
	bool started;

	uint64_t pos;
	
	uint64_t block;
	uint32_t header;
	uint32_t input_size;
} sqfs_blocklist;

size_t sqfs_blocklist_count(sqfs *fs, sqfs_inode *inode);

void sqfs_blocklist_init(sqfs *fs, sqfs_inode *inode, sqfs_blocklist *bl);
sqfs_err sqfs_blocklist_next(sqfs_blocklist *bl);


sqfs_err sqfs_read_range(sqfs *fs, sqfs_inode *inode, off_t start,
	off_t *size, void *buf);

#endif

#ifndef SQUFS_SWAPFUSE_H
#define SQFS_SWAPFUSE_H

#include "squashfs_fs.h"
#include "swap.h"

typedef enum {
	SQFS_OK,
	SQFS_ERR,
	SQFS_FORMAT,
} sqfs_err;

typedef uint64_t sqfs_inode_num;

typedef struct {
	int fd;
	struct squashfs_super_block sb;
} sqfs;

typedef struct {
	size_t size;
	void *data;
} sqfs_block;

typedef struct {
	off_t block;
	size_t offset;
} sqfs_md_cursor;

sqfs_err sqfs_init(sqfs *fs, int fd);

sqfs_err sqfs_md_block_read(sqfs *fs, off_t *pos, sqfs_block **block);
void sqfs_block_dispose(sqfs_block *block);

void sqfs_md_cursor_inum(sqfs_md_cursor *cur, sqfs_inode_num num, off_t base);

sqfs_err sqfs_md_read(sqfs *fs, sqfs_md_cursor *cur, void *buf, size_t size);

#endif

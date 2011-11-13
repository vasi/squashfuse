#ifndef SQUFS_SWAPFUSE_H
#define SQFS_SWAPFUSE_H

#include "squashfs_fs.h"
#include "swap.h"

typedef enum {
	SQFS_OK,
	SQFS_ERR,
	SQFS_FORMAT,
} sqfs_err;

struct sqfs {
	int fd;
	struct squashfs_super_block sb;
};

struct sqfs_block {
	size_t size;
	char *data;
};

sqfs_err sqfs_init(struct sqfs *fs, int fd);

sqfs_err sqfs_read_md_block(struct sqfs *fs, off_t pos,
	struct sqfs_block *block);
void sqfs_dispose_block(struct sqfs_block *block);

#endif

#ifndef SQFS_SWAPFUSE_H
#define SQFS_SWAPFUSE_H

#include "common.h"
#include "squashfs_fs.h"
#include "swap.h"
#include "table.h"

struct sqfs {
	int fd;
	struct squashfs_super_block sb;
	sqfs_table id_table;
};

typedef struct {
	size_t size;
	void *data;
} sqfs_block;

typedef struct {
	off_t block;
	size_t offset;
} sqfs_md_cursor;


// Number of groups of size 'group' required to hold size 'total'
size_t sqfs_divceil(size_t total, size_t group);

sqfs_err sqfs_init(sqfs *fs, int fd);
void sqfs_destroy(sqfs *fs);

sqfs_err sqfs_md_block_read(sqfs *fs, off_t *pos, sqfs_block **block);
void sqfs_block_dispose(sqfs_block *block);

void sqfs_md_cursor_inum(sqfs_md_cursor *cur, sqfs_inode_num num, off_t base);

sqfs_err sqfs_md_read(sqfs *fs, sqfs_md_cursor *cur, void *buf, size_t size);

typedef uint16_t sqfs_id_idx;
typedef uint32_t sqfs_id;
sqfs_err sqfs_lookup_id(sqfs *fs, sqfs_id_idx idx, sqfs_id *id);

#endif

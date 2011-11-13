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

typedef uint32_t sqfs_xattr_idx;
typedef struct {
	struct squashfs_base_inode base;
	int nlink;
	sqfs_xattr_idx xattr;
	
	sqfs_md_cursor next;
	
	union {
		uint32_t dev;
		size_t symlink_size;
		struct {
			uint64_t start_block;
			uint64_t file_size;
			uint32_t frag_block;
			uint32_t frag_off;
		} reg;
		struct {
			uint32_t start_block;
			uint32_t dir_size;
			uint16_t idx_count;
			uint32_t parent_inode;
		} dir;
	} xtra;
} sqfs_inode;


// Number of groups of size 'group' required to hold size 'total'
size_t sqfs_divceil(size_t total, size_t group);

sqfs_err sqfs_init(sqfs *fs, int fd);
void sqfs_destroy(sqfs *fs);

sqfs_err sqfs_md_block_read(sqfs *fs, off_t *pos, sqfs_block **block);
void sqfs_block_dispose(sqfs_block *block);

void sqfs_md_cursor_inum(sqfs_md_cursor *cur, sqfs_inode_num num, off_t base);

sqfs_err sqfs_md_read(sqfs *fs, sqfs_md_cursor *cur, void *buf, size_t size);

mode_t sqfs_mode(int inode_type);
sqfs_err sqfs_inode_get(sqfs *fs, sqfs_inode *inode, sqfs_inode_num num);

sqfs_err sqfs_id_get(sqfs *fs, uint16_t idx, uid_t *id);

#endif

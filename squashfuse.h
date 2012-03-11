#ifndef SQFS_SWAPFUSE_H
#define SQFS_SWAPFUSE_H

#include <stdbool.h>

#include "common.h"
#include "dir.h"
#include "file.h"
#include "squashfs_fs.h"
#include "swap.h"
#include "table.h"

struct sqfs {
	int fd;
	struct squashfs_super_block sb;
	sqfs_table id_table;
	sqfs_table frag_table;
};

typedef uint32_t sqfs_xattr_idx;
struct sqfs_inode {
	struct squashfs_base_inode base;
	int nlink;
	sqfs_xattr_idx xattr;
	
	sqfs_md_cursor next;
	
	union {
		dev_t dev;
		size_t symlink_size;
		struct {
			uint64_t start_block;
			uint64_t file_size;
			uint32_t frag_idx;
			uint32_t frag_off;
		} reg;
		struct {
			uint32_t start_block;
			uint16_t offset;
			uint32_t dir_size;
			uint16_t idx_count;
			uint32_t parent_inode;
		} dir;
	} xtra;
};


// Number of groups of size 'group' required to hold size 'total'
size_t sqfs_divceil(size_t total, size_t group);


sqfs_err sqfs_init(sqfs *fs, int fd);
void sqfs_destroy(sqfs *fs);


void sqfs_md_header(uint16_t hdr, bool *compressed, uint16_t *size);
void sqfs_data_header(uint32_t hdr, bool *compressed, uint32_t *size);

sqfs_err sqfs_block_read(sqfs *fs, off_t pos, bool compressed, uint32_t size,
	size_t outsize, sqfs_block **block);
void sqfs_block_dispose(sqfs_block *block);

sqfs_err sqfs_md_block_read(sqfs *fs, off_t *pos, sqfs_block **block);
sqfs_err sqfs_data_block_read(sqfs *fs, off_t pos, uint32_t hdr,
	sqfs_block **block);


void sqfs_md_cursor_inode(sqfs_md_cursor *cur, sqfs_inode_id id, off_t base);

sqfs_err sqfs_md_read(sqfs *fs, sqfs_md_cursor *cur, void *buf, size_t size);


sqfs_err sqfs_inode_get(sqfs *fs, sqfs_inode *inode, sqfs_inode_id id);

mode_t sqfs_mode(int inode_type);
sqfs_err sqfs_id_get(sqfs *fs, uint16_t idx, uid_t *id);

sqfs_err sqfs_readlink(sqfs *fs, sqfs_inode *inode, char *buf);

#endif

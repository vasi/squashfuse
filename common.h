#ifndef SQFS_COMMON_H
#define SQFS_COMMON_H

#include <stdint.h>
#include <sys/types.h>

typedef enum {
	SQFS_OK,
	SQFS_ERR,
	SQFS_FORMAT,
} sqfs_err;

typedef uint64_t sqfs_inode_id;

typedef struct sqfs sqfs;
typedef struct sqfs_inode sqfs_inode;

typedef struct {
	size_t size;
	void *data;
} sqfs_block;

typedef struct {
	off_t block;
	size_t offset;
} sqfs_md_cursor;

#endif

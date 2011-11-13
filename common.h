#ifndef SQFS_COMMON_H
#define SQFS_COMMON_H

#include <stdint.h>

typedef enum {
	SQFS_OK,
	SQFS_ERR,
	SQFS_FORMAT,
} sqfs_err;

typedef uint64_t sqfs_inode_num;

typedef struct sqfs sqfs;

#endif

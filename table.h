#ifndef SQFS_TABLE_H
#define SQFS_TABLE_H

#include "common.h"

#include <sys/types.h>

typedef struct {
	size_t each;
	uint64_t *blocks;
} sqfs_table;

sqfs_err sqfs_table_init(sqfs_table *table, int fd, off_t start, size_t each,
	size_t count);
void sqfs_table_destroy(sqfs_table *table);

sqfs_err sqfs_table_get(sqfs_table *table, sqfs *fs, size_t idx, void *buf);

#endif

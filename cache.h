#ifndef SQFS_CACHE_H
#define SQFS_CACHE_H

#include "common.h"

// Really simplistic block cache
//  - Linear search
//  - Linear eviction
//  - No thread safety
//  - Misses are caller's responsibility

typedef struct {
	off_t pos;
	size_t data_size;
	sqfs_block *block;
} sqfs_block_cache_entry;

typedef struct {
	sqfs_block_cache_entry *entries;
	size_t size;
	size_t next; // next block to evict
} sqfs_block_cache;

sqfs_err sqfs_cache_init(sqfs_block_cache *cache, size_t size);
void sqfs_cache_destroy(sqfs_block_cache *cache);

// Return NULL if not found
sqfs_block *sqfs_cache_get(sqfs_block_cache *cache, off_t pos,
	size_t *data_size);
void sqfs_cache_set(sqfs_block_cache *cache, off_t pos, sqfs_block *block,
	size_t data_size);

#endif

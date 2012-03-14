#include "cache.h"

#include "squashfuse.h"

#include <stdlib.h>

sqfs_err sqfs_cache_init(sqfs_block_cache *cache, size_t size) {
	cache->size = size;
	cache->next = 0;
	cache->entries = calloc(size, sizeof(sqfs_block_cache_entry));
	if (!cache->entries)
		return SQFS_ERR;
	return SQFS_OK;
}

void sqfs_cache_destroy(sqfs_block_cache *cache) {
	for (int i = 0; i < cache->size; ++i) {
		sqfs_block *block = cache->entries[i].block;
		if (block)
			sqfs_block_dispose(block);
	}
	free(cache->entries);
}

sqfs_block *sqfs_cache_get(sqfs_block_cache *cache, off_t pos,
		size_t *data_size) {
	for (int i = 0; i < cache->size; ++i) {
		sqfs_block_cache_entry *entry = &cache->entries[i];
		if (entry->pos == pos && entry->block) {
			if (data_size)
				*data_size = entry->data_size;
			return entry->block;
		}
	}
	return NULL;
}

void sqfs_cache_set(sqfs_block_cache *cache, off_t pos, sqfs_block *block,
		size_t data_size) {
	sqfs_block_cache_entry *entry = &cache->entries[cache->next];
	if (entry->block)
		sqfs_block_dispose(entry->block);
	
	cache->next += 1;
	cache->next %= cache->size;
	
	entry->pos = pos;
	entry->data_size = data_size;
	entry->block = block;
}

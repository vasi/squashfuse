/*
 * Copyright (c) 2012 Dave Vasilevsky <dave@vasilevsky.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
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

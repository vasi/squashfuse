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
#ifndef SQFS_CACHE_H
#define SQFS_CACHE_H

#include "common.h"

/* Cache data, given a key. Used to cache data and metadata blocks, and for
   the blockidx.

- Linear search.
- Thread-safe, if threads are available.
- Tracks which items are in-use, so only unused ones can be evicted.
- Round-robin eviction.
- If multiple threads try to create the same entry, coalesces the requests.
*/

typedef uint64_t sqfs_cache_key;
typedef void *sqfs_cache_value;

/* Function to cleanup an evicted item */
typedef void (*sqfs_cache_dispose)(sqfs_cache_value);

/* Initialize a cache:
   value_size		The size of each value item.
   initial			The number of items to be cached, initially. If all items are
							  in use, this may grow up to 'capacity'.
	 capacity			The maximum number of items to cache.
	 dispose			A function to cleanup evicted items. */
sqfs_err sqfs_cache_init(sqfs_cache *cache, size_t value_size, size_t initial,
	size_t capacity, sqfs_cache_dispose dispose);

/* Cleanup the cache. */
void sqfs_cache_destroy(sqfs_cache *cache);

/* Get an item from the cache.
	 
If an item for 'key' already exists and is initialized, it will be returned.
sqfs_cache_entry_is_initialized() will return true. The item may only be read,
not changed.

If an item exists but is not yet initialized, will wait until initialization
is complete. Then the item will be returned, as above.

If no item exists, a new item will be created and returned.
sqfs_cache_entry_is_initialized() will return false. The item must be
initialized, and then sqfs_cache_entry_ready() must be called to indicate
that it is ready for use.

In any case, sqfs_cache_entry_release() must be called after the caller is
done with the item.

This function may cause unused entries to be evicted, or may cause the
cache to grow if there are no unused items. If a new item is needed, but all
items are used and the cache is at capacity, will wait until there is room. */
sqfs_err sqfs_cache_get(sqfs_cache *cache, sqfs_cache_key key,
	sqfs_cache_entry **entry);

/* Get the data stored in this cache entry. The caller must not change this
	 data if sqfs_cache_entry_is_initialized() returns true. */
sqfs_cache_value sqfs_cache_entry_value(sqfs_cache_entry *entry);

/* Notify this cache that the caller doesn't need this entry anymore, and it
   is available for re-use. */
void sqfs_cache_entry_release(sqfs_cache_entry *entry);

/* Mark this entry as fully initialized. After calling this, the entry must
   not be changed, only read. */
void sqfs_cache_entry_ready(sqfs_cache_entry *entry);

/* Check if the entry is initialized or not. If it is not, this thread must
   perform initialization. */
bool sqfs_cache_entry_is_initialized(sqfs_cache_entry *entry);






// #define SQFS_CACHE_IDX_INVALID 0
// 
// typedef uint64_t sqfs_cache_idx;
// typedef void (*sqfs_cache_dispose)(void* data);
// 
// typedef struct {
// 	sqfs_cache_idx *idxs;
// 	uint8_t *buf;
// 	
// 	sqfs_cache_dispose dispose;
// 	
// 	size_t size, count;
// 	size_t next; /* next block to evict */
// } sqfs_cache;
// 
// sqfs_err sqfs_cache_init(sqfs_cache *cache, size_t size, size_t count,
// 	sqfs_cache_dispose dispose);
// void sqfs_cache_destroy(sqfs_cache *cache);
// 
// void *sqfs_cache_get(sqfs_cache *cache, sqfs_cache_idx idx);
// void *sqfs_cache_add(sqfs_cache *cache, sqfs_cache_idx idx);
// 
// 
// typedef struct {
// 	sqfs_block *block;
// 	size_t data_size;
// } sqfs_block_cache_entry;

/* FIXME: move to FS */
sqfs_err sqfs_block_cache_init(sqfs_cache *cache, size_t count);

#endif

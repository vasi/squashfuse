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

#include "config.h"

#ifndef SQFS_MULTITHREADED

#include "cache.h"

#include "fs.h"

#include <assert.h>
#include <stdlib.h>

typedef struct sqfs_cache_internal {
	uint8_t *buf;

	sqfs_cache_dispose dispose;

	size_t size, count;
	size_t next; /* next block to evict */
} sqfs_cache_internal;

typedef struct {
	int valid;
	sqfs_cache_idx idx;
} sqfs_cache_entry_hdr;

sqfs_err sqfs_cache_init(sqfs_cache *cache, size_t size, size_t count,
			 sqfs_cache_dispose dispose) {

	sqfs_cache_internal *c = malloc(sizeof(sqfs_cache_internal));
	if (!c) {
		return SQFS_ERR;
	}

	c->size = size + sizeof(sqfs_cache_entry_hdr);
	c->count = count;
	c->dispose = dispose;
	c->next = 0;

	c->buf = calloc(count, c->size);

	if (c->buf) {
		*cache = c;
		return SQFS_OK;
	}

	sqfs_cache_destroy(&c);
	return SQFS_ERR;
}

static sqfs_cache_entry_hdr *sqfs_cache_entry_header(
						     sqfs_cache_internal* cache,
						     size_t i) {
	return (sqfs_cache_entry_hdr *)(cache->buf + i * cache->size);
}

static void* sqfs_cache_entry(sqfs_cache_internal* cache, size_t i) {
	return (void *)(sqfs_cache_entry_header(cache, i) + 1);
}

void sqfs_cache_destroy(sqfs_cache *cache) {
	if (cache && *cache) {
		sqfs_cache_internal *c = *cache;
		if (c->buf) {
			size_t i;
			for (i = 0; i < c->count; ++i) {
				sqfs_cache_entry_hdr *hdr =
					sqfs_cache_entry_header(c, i);
				if (hdr->valid) {
					c->dispose((void *)(hdr + 1));
				}
			}
		}
		free(c->buf);
		free(c);
		*cache = NULL;
	}
}

void *sqfs_cache_get(sqfs_cache *cache, sqfs_cache_idx idx) {
	size_t i;
	sqfs_cache_internal *c = *cache;
	sqfs_cache_entry_hdr *hdr;

	for (i = 0; i < c->count; ++i) {
		hdr = sqfs_cache_entry_header(c, i);
		if (hdr->idx == idx) {
			assert(hdr->valid);
			return sqfs_cache_entry(c, i);
		}
	}

	/* No existing entry; free one if necessary, allocate a new one. */
	i = (c->next++);
	c->next %= c->count;

	hdr = sqfs_cache_entry_header(c, i);
	if (hdr->valid) {
		/* evict */
		c->dispose((void *)(hdr + 1));
		hdr->valid = 0;
	}

	hdr->idx = idx;
	return (void *)(hdr + 1);
}

int sqfs_cache_entry_valid(const sqfs_cache *cache, const void *e) {
	sqfs_cache_entry_hdr *hdr = ((sqfs_cache_entry_hdr *)e) - 1;
	return hdr->valid;
}

void sqfs_cache_entry_mark_valid(sqfs_cache *cache, void *e) {
	sqfs_cache_entry_hdr *hdr = ((sqfs_cache_entry_hdr *)e) - 1;
	assert(hdr->valid == 0);
	hdr->valid = 1;
}

void sqfs_cache_put(const sqfs_cache *cache, const void *e) {
	// nada, we have no locking in single-threaded implementation.
}
#endif /* SQFS_MULTITHREADED */

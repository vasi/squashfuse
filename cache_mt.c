#include "config.h"

#ifdef SQFS_MULTITHREADED

/* Thread-safe cache implementation.
 *
 * Simple implementation: basic hash table, each individual entry is
 * protected by a mutex, any collision is handled by eviction.
 */

#include "cache.h"
#include "fs.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

typedef struct sqfs_cache_internal {
    uint8_t *buf;
    sqfs_cache_dispose dispose;
    size_t entry_size, count;
} sqfs_cache_internal;

typedef struct {
    enum { EMPTY, FULL } state;
    sqfs_cache_idx idx;
    pthread_mutex_t lock;
} sqfs_cache_entry_hdr;

// MurmurHash64A performance-optimized for hash of uint64_t keys
const static uint64_t kMurmur2Seed = 4193360111ul;
static uint64_t MurmurRehash64A(uint64_t key) {
    const uint64_t m = 0xc6a4a7935bd1e995;
    const int r = 47;

    uint64_t h = (uint64_t)kMurmur2Seed ^ (sizeof(uint64_t) * m);

    key *= m;
    key ^= key >> r;
    key *= m;

    h ^= key;
    h *= m;

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}

static sqfs_cache_entry_hdr *sqfs_cache_entry_header(
                             sqfs_cache_internal* cache,
                             size_t i) {
    assert(i < cache->count);
    return (sqfs_cache_entry_hdr *)(cache->buf + i * cache->entry_size);
}

sqfs_err sqfs_cache_init(sqfs_cache *cache, size_t entry_size, size_t count,
             sqfs_cache_dispose dispose) {
    size_t i;
    pthread_mutexattr_t attr;
    sqfs_cache_internal *c = malloc(sizeof(sqfs_cache_internal));

    if (!c) {
        return SQFS_ERR;
    }

    c->entry_size = entry_size + sizeof(sqfs_cache_entry_hdr);
    c->count = count;
    c->dispose = dispose;

    pthread_mutexattr_init(&attr);
#if defined(_GNU_SOURCE) && !defined(NDEBUG)
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
#endif

    c->buf = calloc(c->count, c->entry_size);
    if (!c->buf) {
        goto err_out;
    }

    for (i = 0; i < c->count; ++i) {
        sqfs_cache_entry_hdr *hdr = sqfs_cache_entry_header(c, i);
        hdr->state = EMPTY;
        if (pthread_mutex_init(&hdr->lock, &attr)) {
            goto err_out;
        }
    }

    pthread_mutexattr_destroy(&attr);

    *cache = c;
    return SQFS_OK;

err_out:
    sqfs_cache_destroy(&c);
    return SQFS_ERR;
}

void sqfs_cache_destroy(sqfs_cache *cache) {
    if (cache && *cache) {
        sqfs_cache_internal *c = *cache;
        if (c->buf) {
            size_t i;
            for (i = 0; i < c->count; ++i) {
                sqfs_cache_entry_hdr *hdr =
                    sqfs_cache_entry_header(c, i);
                if (hdr->state == FULL) {
                    c->dispose((void *)(hdr + 1));
                }
                if (pthread_mutex_destroy(&hdr->lock)) {
                    assert(0);
                }
            }
        }
        free(c->buf);
        free(c);
        *cache = NULL;
    }
}

void *sqfs_cache_get(sqfs_cache *cache, sqfs_cache_idx idx) {
    sqfs_cache_internal *c = *cache;
    sqfs_cache_entry_hdr *hdr;
    void *entry;

    uint64_t key = MurmurRehash64A(idx) % c->count;

    hdr = sqfs_cache_entry_header(c, key);
    if (pthread_mutex_lock(&hdr->lock)) { assert(0); }
    /* matching unlock is in sqfs_cache_put() */
    entry = (void *)(hdr + 1);

    if (hdr->state == EMPTY) {
        hdr->idx = idx;
        return entry;
    }

    /* There's a valid entry: it's either a cache hit or a collision. */
    assert(hdr->state == FULL);
    if (hdr->idx == idx) {
        return entry;
    }

    /* Collision. */
    c->dispose((void *)(hdr + 1));
    hdr->state = EMPTY;
    hdr->idx = idx;
    return entry;
}

int sqfs_cache_entry_valid(const sqfs_cache *cache, const void *e) {
    sqfs_cache_entry_hdr *hdr = ((sqfs_cache_entry_hdr *)e) - 1;
    return hdr->state == FULL;
}

void sqfs_cache_entry_mark_valid(sqfs_cache *cache, void *e) {
    sqfs_cache_entry_hdr *hdr = ((sqfs_cache_entry_hdr *)e) - 1;
    assert(hdr->state == EMPTY);
    hdr->state = FULL;
}

void sqfs_cache_put(const sqfs_cache *cache, const void *e) {
    sqfs_cache_entry_hdr *hdr = ((sqfs_cache_entry_hdr *)e) - 1;
    if (pthread_mutex_unlock(&hdr->lock)) { assert(0); }
}

#endif /* SQFS_MULTITHREADED */

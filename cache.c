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

#include <stdlib.h>

static sqfs_err sqfs_cache_dispose_null(sqfs_cache_value SQFS_UNUSED(v)) {
  return SQFS_OK;
}

sqfs_err sqfs_cache_init(sqfs_cache *cache, size_t value_size, size_t initial,
    size_t capacity, sqfs_cache_dispose dispose) {
  sqfs_err err;
  size_t i;
  
  /* Setup mutex, and lock it for safety */
  if ((err = sqfs_mutex_init(&cache->mutex)))
    return err;
  if ((err = sqfs_mutex_lock(&cache->mutex)))
    goto error_mutex;
  
  /* Initialize everything */
  cache->entries = NULL;
  cache->value_size = value_size;
  cache->initial = initial;
  cache->capacity = capacity;
  cache->avail = initial;
  cache->allocated = 0;
  cache->evict = 0;
  cache->waiters = 0;
  if (!dispose)
    dispose = sqfs_cache_dispose_null;
  cache->dispose = dispose;
  
  if ((err = sqfs_cond_init(&cache->cv)))
    goto error_lock;
  
  cache->entries = (sqfs_cache_entry*)malloc(
    capacity * sizeof(sqfs_cache_entry));
  if (!cache->entries) {
    err = SQFS_ERR;
    goto error_cond;
  }
  
  for (i = 0; i < capacity; ++i)
    cache->entries[i].value = NULL;
  
  /* Cleanup */
  if ((err = sqfs_mutex_unlock(&cache->mutex)))
    goto error_alloc;
  
  return SQFS_OK;

error_alloc:
  free(cache->entries);
error_cond:
  sqfs_cond_destroy(&cache->cv);
error_lock:
  sqfs_mutex_unlock(&cache->mutex);
error_mutex:
  sqfs_mutex_destroy(&cache->mutex);
  return err;
}

sqfs_err sqfs_cache_destroy(sqfs_cache *cache) {
  sqfs_err err, ret;
  size_t capacity;
  sqfs_cache_entry *entries, *e;
  sqfs_cache_dispose dispose;
  sqfs_cache_value v;
  size_t i;
  
  if (!cache)
    return SQFS_OK;
  ret = SQFS_OK;
  
  if ((err = sqfs_mutex_lock(&cache->mutex)) && !ret)
    ret = err;
  
  /* Empty out the cache */
  capacity = cache->capacity;
  entries = cache->entries;
  dispose = cache->dispose;
  cache->initial = cache->capacity = cache->avail = cache->allocated = 0;
  cache->entries = NULL;
  if ((err = sqfs_cond_destroy(&cache->cv)) && !ret)
    ret = err;
  
  /* Clean up */
  if ((err = sqfs_mutex_unlock(&cache->mutex)) && !ret)
    ret = err;
  if ((err = sqfs_mutex_destroy(&cache->mutex)) && !ret)
    ret = err;
  
  for (i = 0; i < capacity; ++i) {
    e = entries + i;
    if (!e->value)
      continue;
    
    v = e->value;
    e->value = NULL;
    if ((err = dispose(v)) && !ret)
      ret = err;
    free(v);
    
    if ((err = sqfs_cond_destroy(&e->cv)) && !ret)
      ret = err;
  }
  
  free(entries);
  return ret;
}

sqfs_cache_value sqfs_cache_entry_value(sqfs_cache_entry *entry) {
  return entry->value;
}

bool sqfs_cache_entry_is_initialized(const sqfs_cache_entry *entry) {
  return entry->ready;
}

sqfs_err sqfs_cache_entry_release(sqfs_cache_entry *entry) {
  sqfs_err err, ret;
  sqfs_cache *cache;
  ret = SQFS_OK;
  
  if (!entry->cache || !entry->cache->entries)
    return SQFS_ERR;
  cache = entry->cache;
  
  if ((err = sqfs_mutex_lock(&cache->mutex)))
    return err;
  
  if (--entry->refcount == 0) {
    ++cache->avail;
    if (cache->waiters) {
      if ((err = sqfs_cond_signal(&cache->cv)) && !ret)
        ret = err;
    }
  }
  
  if ((err = sqfs_mutex_unlock(&cache->mutex)) && !ret)
    ret = err;
  
  return ret;
}

sqfs_err sqfs_cache_entry_ready(sqfs_cache_entry *entry) {
  sqfs_err err, ret;
  sqfs_cache *cache;
  ret = SQFS_OK;
  
  if (!entry->cache || !entry->cache->entries)
    return SQFS_ERR;
  cache = entry->cache;
  
  if ((err = sqfs_mutex_lock(&cache->mutex)))
    return err;
  
  entry->ready = true;
  if (entry->waiting) {
    if ((err = sqfs_cond_broadcast(&entry->cv)) && !ret)
      ret = err;
  }
  
  if ((err = sqfs_mutex_unlock(&cache->mutex)) && !ret)
    ret = err;
  
  return ret;
}

/* Setup an unallocated entry */
static sqfs_err sqfs_cache_entry_allocate(sqfs_cache *cache,
    sqfs_cache_entry **entry) {
  sqfs_cache_entry *e;
  sqfs_err err = SQFS_OK;
  
  e = cache->entries + cache->allocated;
  
  if (!(e->value = malloc(cache->value_size)))
    return SQFS_ERR;
  e->cache = cache;
  
  if ((err = sqfs_cond_init(&e->cv))) {
    free(e->value);
    e->value = NULL;
    return err;
  }
  
  ++cache->allocated;
  *entry = e;
  return SQFS_OK;
}

/* Find an entry we can (re)use. Yield NULL if none can be found.
   Assumes the cache is locked. */
static sqfs_err sqfs_cache_find_free_entry(sqfs_cache *cache,
    sqfs_cache_key key, sqfs_cache_entry **entry) {
  sqfs_err err;
  sqfs_cache_entry *e;
  size_t i;
  
  *entry = NULL;
  
  /* First, prefer a completely free cache entry */
  if (cache->avail && cache->allocated < cache->initial) {
    if ((err = sqfs_cache_entry_allocate(cache, &e)))
      return err;
    --cache->avail;
    goto found;
  }
  
  /* Second, try to evict an unused entry */
  for (i = 0; i < cache->allocated; ++i) {
    size_t j = (cache->evict + i) % cache->allocated;
    e = cache->entries + j;
    if (e->refcount == 0) {
      /* Got one! */
      if ((err = cache->dispose(e->value)))
        return err;
      --cache->avail;
      cache->evict = (j + 1) % cache->allocated;
      goto found;
    }
  }
  
  /* Third, try a spare entry */
  if (cache->allocated < cache->capacity) {
    if ((err = sqfs_cache_entry_allocate(cache, &e)))
      return err;
    /* No change to avail, this was unavailable before! */
    goto found;
  }
  
  /* Nothing's free, return failure */
  *entry = NULL;
  return SQFS_OK;
  
found:
  e->key = key;
  e->ready = false;
  e->refcount = 1;
  e->waiting = false;
  *entry = e;
  return SQFS_OK;
}

sqfs_err sqfs_cache_get(sqfs_cache *cache, sqfs_cache_key key,
    sqfs_cache_entry **entry) {
  sqfs_err err;
  sqfs_cache_entry *e;
  size_t i;
  
  *entry = NULL;
  if (!cache->entries)
    return SQFS_ERR;
  err = SQFS_OK;
  
  if ((err = sqfs_mutex_lock(&cache->mutex)))
    return err;
  
  while (true) {
    /* Check if we already have the entry */
    for (i = 0; i < cache->allocated; ++i) {
      e = cache->entries + i;
      if (e->key == key) {
        /* Found it */
        if (e->refcount++ == 0)
          --cache->avail;
      
        /* Wait until it's ready */
        while (!e->ready) {
          e->waiting = true;
          if ((err = sqfs_cond_wait(&e->cv, &cache->mutex)))
            goto unlock;
        }
        *entry = e;
        goto unlock;
      }
    }
    
    /* We don't already have it, maybe we can create it */
    if ((err = sqfs_cache_find_free_entry(cache, key, &e)))
      goto unlock;
    if (e != NULL) {
      *entry = e;
      goto unlock;
    }
    
    /* No free entries, wait until we have one */
    ++cache->waiters;
    err = sqfs_cond_wait(&cache->cv, &cache->mutex);
    --cache->waiters;
    if (err)
      goto unlock;
  }

unlock:
  sqfs_mutex_unlock(&cache->mutex);
  return err;
}



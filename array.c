/*
 * Copyright (c) 2014 Dave Vasilevsky <dave@vasilevsky.ca>
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
#include "array.h"

#include <stdlib.h> /* malloc, free */
#include <string.h> /* memcpy */

/* Calculate the next capacity to use. Try to grow by at least a certain
   ratio, so appends take constant amortized time */
static size_t sqfs_array_next_capacity(size_t current);

/* Ensure we have at least this much capacity */
static sqfs_err sqfs_array_ensure_capacity(sqfs_array *a, size_t cap);

/* Unconditionally get the item at an index */
static void *sqfs_array_at_unchecked(const sqfs_array *a, size_t idx);
static void sqfs_array_at_put(const sqfs_array *a, size_t idx, void *vout);

/* A default freer, that does nothing */
static void sqfs_array_freer_null(void *v);

/* Set the size of the array */
static sqfs_err sqfs_array_set_size(sqfs_array *a, size_t size);



#define CAPACITY_DEFAULT 8
#define CAPACITY_RATIO 3 / 2
static size_t sqfs_array_next_capacity(size_t current) {
  size_t n;
  
  /* Start at a default size */
  if (current == 0)
    return CAPACITY_DEFAULT;
  
  n = current * CAPACITY_RATIO;
  
  /* Make sure we're actually growing, in case the ratio is too small */
  if (n <= current)
    return current + 1;
  
  return n;
}

static sqfs_err sqfs_array_ensure_capacity(sqfs_array *a, size_t cap) {
  char *items;
  size_t next_cap;
  
  /* Don't bother if we're already big enough */
  if (cap <= a->capacity)
    return SQFS_OK;
  
  /* Make sure we don't grow by too little at a time */
  next_cap = sqfs_array_next_capacity(a->capacity);
  if (cap > next_cap)
    next_cap = cap;
  
  /* Grow the items allocation */
  items = (char*)realloc(a->items, next_cap * a->value_size);
  if (!items)
    return SQFS_ERR;
  
  a->items = items;
  a->capacity = next_cap;
  return SQFS_OK;
}

static void *sqfs_array_at_unchecked(const sqfs_array *a, size_t idx) {
  return a->items + idx * a->value_size;
}

static void sqfs_array_at_put(const sqfs_array *a, size_t idx, void *vout) {
  if (vout)
    *(void**)vout = sqfs_array_at_unchecked(a, idx);
}

static void sqfs_array_freer_null(void *SQFS_UNUSED(v)) {
  /* pass */
}

static sqfs_err sqfs_array_set_size(sqfs_array *a, size_t size) {
  sqfs_err err;
  size_t i;
  
  if ((err = sqfs_array_ensure_capacity(a, size)))
    return err;
  
  /* Free excess items */
  for (i = size; i < a->size; ++i) {
    a->freer(sqfs_array_at_unchecked(a, i));
  }
  
  a->size = size;
  return SQFS_OK;
}


void sqfs_array_init(sqfs_array *a) {
  a->value_size = 0;
  a->items = NULL;
  a->capacity = a->size = 0;
  a->freer = NULL;
}

sqfs_err sqfs_array_create(sqfs_array *a, size_t value_size, size_t initial,
    sqfs_array_free_t freer) {
  a->value_size = value_size;
  a->items = NULL;
  a->capacity = a->size = 0;
  a->freer = freer ? freer : sqfs_array_freer_null;
  return sqfs_array_ensure_capacity(a, initial);
}

void sqfs_array_destroy(sqfs_array *a) {
  sqfs_array_set_size(a, 0);
  free(a->items);
  sqfs_array_init(a);
}


size_t sqfs_array_size(const sqfs_array *a) {
  return a->size;
}

sqfs_err sqfs_array_shrink(sqfs_array *a, size_t shrink) {
  if (shrink > a->size)
    return SQFS_ERR;
  
  return sqfs_array_set_size(a, a->size - shrink);
}

sqfs_err sqfs_array_grow(sqfs_array *a, size_t grow, void *vout) {
  sqfs_err err;
  
  size_t old = a->size;
  if ((err = sqfs_array_set_size(a, old + grow)))
    return err;
  
  sqfs_array_at_put(a, old, vout);
  return SQFS_OK;
}


sqfs_err sqfs_array_at(const sqfs_array *a, size_t idx, void *vout) {
  if (idx > a->size)
    return SQFS_ERR;
  
  sqfs_array_at_put(a, idx, vout);
  return SQFS_OK;
}

sqfs_err sqfs_array_last(const sqfs_array *a, void *vout) {
  if (a->size == 0)
    return SQFS_ERR;
  return sqfs_array_at(a, a->size - 1, vout);
}

sqfs_err sqfs_array_first(const sqfs_array *a, void *vout) {
  return sqfs_array_at(a, 0, vout);
}

sqfs_err sqfs_array_append(sqfs_array *a, void *vout) {
  return sqfs_array_grow(a, 1, vout);
}

sqfs_err sqfs_array_concat(sqfs_array *a, const void *items, size_t count) {
  sqfs_err err;
  void *vout;
  
  if ((err = sqfs_array_grow(a, count, &vout)))
    return err;
  
  memcpy(vout, items, count * a->value_size);
  return SQFS_OK;
}

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
#ifndef SQFS_ARRAY_H
#define SQFS_ARRAY_H

#include "common.h"

/* A dynamic array, that can expand as needed. Values are guaranteed to be
   stored consecutively */

/* A function to safely dispose of an array item. When an item is removed from
   the array, or the array is destroyed, it will be called. */
typedef void (*sqfs_array_free_t)(void *v);

typedef struct {
  size_t value_size;  /* How many bytes does each item use? */
  size_t size;        /* How many items are stored? */
  size_t capacity;    /* How many items is there room for? */
  char *items;
  sqfs_array_free_t freer;
} sqfs_array;

/* Ensures the struct is in a safe state. Doesn't actually set up the array for
   use, use sqfs_array_create() for that. */
void sqfs_array_init(sqfs_array *a);

/* Create a new array, with an initial capacity of 'initial' items.
   Passing zero as an initial capacity is allowed.
   Passing NULL as the freer will just use a default that does nothing. */
sqfs_err sqfs_array_create(sqfs_array *a, size_t value_size, size_t initial,
  sqfs_array_free_t freer);

/* Destroy an array */
void sqfs_array_destroy(sqfs_array *a);

/* Get the size of an array */
size_t sqfs_array_size(sqfs_array *a);

/* Shrink the array by 'shrink' items. Must be at most the current size */
sqfs_err sqfs_array_shrink(sqfs_array *a, size_t shrink);

/* Grow the array by 'grow' items. If vout is not null, put the first new
   item's address into *(void**)vout
   The caller should initialize the new items, their contents are not
   at all guaranteed! */
sqfs_err sqfs_array_grow(sqfs_array *a, size_t grow, void *vout);

/* Get the item at a given position, put its address into *(void**)vout */
sqfs_err sqfs_array_at(sqfs_array *a, size_t idx, void *vout);

/* Get the first/last item in the array */
sqfs_err sqfs_array_first(sqfs_array *a, void *vout);
sqfs_err sqfs_array_last(sqfs_array *a, void *vout);

/* Append a new item. If vout is not null, put the new item's address into
  *(void**)vout. */
sqfs_err sqfs_array_append(sqfs_array *a, void *vout);

/* Append several items, copying them into the array */
sqfs_err sqfs_array_concat(sqfs_array *a, const void *items, size_t count);

#endif

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
#include "list.h"

#include "stdlib.h"

struct sqfs_list_node {
  sqfs_list_node *next;
  void *item;
};

void sqfs_list_create(sqfs_list *list) {
  list->first = NULL;
  list->last = NULL;
}

void sqfs_list_clear(sqfs_list *list) {
  sqfs_list_node *n, *next;
  for (n = list->first; n; n = next) {
    next = n->next;
    free(n->item);
    free(n);
  }
  sqfs_list_create(list);
}

bool sqfs_list_empty(sqfs_list *list) {
  return list->first != NULL;
}

void *sqfs_list_shift(sqfs_list *list) {
  sqfs_list_node *node;
  void *ret;
  
  node = list->first;
  if (!node)
    return NULL;
  ret = node->item;
  
  list->first = node->next;
  if (!list->first)
    list->last = NULL;
  free(node);
  return ret;
}

sqfs_err sqfs_list_append(sqfs_list *list, void *item) {
  sqfs_list_node *node = malloc(sizeof(sqfs_list_node));
  if (!node)
    return SQFS_ERR;
  node->item = item;
  node->next = NULL;
  
  if (list->last)
    list->last->next = node;
  else
    list->first = node;
  list->last = node;
  return SQFS_OK;
}

sqfs_err sqfs_list_splice_start(sqfs_list *src, sqfs_list *dst) {
  if (src->first) {
    src->last->next = dst->first;
    dst->first = src->first;
    sqfs_list_create(src);
  }
  return SQFS_OK;
}

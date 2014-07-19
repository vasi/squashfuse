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
#ifndef SQFS_RESOLVE_H
#define SQFS_RESOLVE_H

#include "common.h"

#include "fs.h"
#include "list.h"
#include "stack.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Resolve paths including symbolic links */
typedef struct {
  sqfs *fs;
  sqfs_inode root;
  sqfs_stack levels;     /* inodes */
  sqfs_list components;  /* strings */
} sqfs_resolver;

void sqfs_resolver_init(sqfs_resolver *res);
sqfs_err sqfs_resolver_create(sqfs_resolver *res, sqfs *fs);
void sqfs_resolver_destroy(sqfs_resolver *res);

/* Reset to initial state */
sqfs_err sqfs_resolver_reset(sqfs_resolver *res);

/* Add a path component to end of current path */
sqfs_err sqfs_resolver_append_name(sqfs_resolver *res, const char *name);

/* Add a complete path to beginning of current path */
sqfs_err sqfs_resolver_prepend_path(sqfs_resolver *res, const char *path);

/* Perform a resolution operation */
sqfs_err sqfs_resolver_resolve(sqfs_resolver *res, sqfs_inode *inode,
  bool *found);

/* Copy the current resolver state */
sqfs_err sqfs_resolver_copy(const sqfs_resolver *src, sqfs_resolver *dst);

#ifdef __cplusplus
}
#endif

#endif

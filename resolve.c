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
#include "resolve.h"

#include "dir.h"
#include "dynstring.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define SQFS_RESOLVE_MAX_DEPTH 256

/* Find a pointer to the current inode */
static sqfs_err sqfs_resolver_current(sqfs_resolver *res, sqfs_inode **inode) {
  sqfs_err err;
  if (sqfs_stack_size(&res->levels)) {
    if ((err = sqfs_stack_top(&res->levels, inode)))
      return err;
  } else {
    *inode = &res->root;
  }
  return SQFS_OK;
}


void sqfs_resolver_init(sqfs_resolver *res) {
  res->fs = NULL;
  sqfs_stack_init(&res->levels);
  sqfs_list_create(&res->components);
}

sqfs_err sqfs_resolver_create(sqfs_resolver *res, sqfs *fs) {
  sqfs_err err = sqfs_inode_get(fs, &res->root, sqfs_inode_root(fs));
  if (err)
    return err;
  
  err = sqfs_stack_create(&res->levels, sizeof(sqfs_inode), 0, NULL);
  if (err)
    return err;
  res->fs = fs;
  return SQFS_OK;
}

void sqfs_resolver_destroy(sqfs_resolver *res) {
  sqfs_stack_destroy(&res->levels);
  sqfs_list_clear(&res->components);
  res->fs = NULL;
}

sqfs_err sqfs_resolver_reset(sqfs_resolver *res) {
  sqfs_stack_clear(&res->levels);
  sqfs_list_clear(&res->components);
  return SQFS_OK;
}

sqfs_err sqfs_resolver_append_name(sqfs_resolver *res, const char *name) {
  char *n = sqfs_strdup(name);
  sqfs_err err = sqfs_list_append(&res->components, n);
  if (err)
    free(n);
  return err;
}

sqfs_err sqfs_resolver_prepend_path(sqfs_resolver *res, const char *path) {
  sqfs_err err;
  sqfs_list names;
  sqfs_list_create(&names);
  
  while (true) {
    size_t len;
    char *dup;
    const char *name = sqfs_path_next(&path, &len);
    if (!name)
      break;
    
    dup = sqfs_strndup(name, len);
    if ((err = sqfs_list_append(&names, dup))) {
      free(dup);
      sqfs_list_clear(&names);
      return err;
    }
  }
  
  err = sqfs_list_splice_start(&names, &res->components);
  if (err)
    sqfs_list_clear(&names);
  return err;
}

sqfs_err sqfs_resolver_resolve(sqfs_resolver *res, sqfs_inode *inode,
    bool *found) {
  sqfs_err err;
  sqfs_inode *parent, *pinode;
  sqfs_dir_entry entry;
  sqfs_name namebuf;
  char *name;
  size_t depth = 0;

  *found = true;
  sqfs_dentry_init(&entry, namebuf);    
  while (!sqfs_list_empty(&res->components)) {
    name = sqfs_list_first(&res->components);
    
    /* Handle special names. Don't allow anything higher than the FS root */
    if (!*name) { /* root */
      sqfs_stack_clear(&res->levels);
    } else if (strcmp(name, "..") == 0) {
      sqfs_stack_pop(&res->levels);
    } else if (strcmp(name, ".") != 0) {
      /* Regular name. Find the parent inode */
      if ((err = sqfs_resolver_current(res, &parent))) {
        free(name);
        return err;
      }
    
      /* Lookup the path component */
      err = sqfs_dir_lookup(res->fs, parent, name, strlen(name), &entry,
        found);
      if (err)
        return err;
      if (!*found)
        return SQFS_OK;
    
      if ((err = sqfs_inode_get(res->fs, inode, sqfs_dentry_inode(&entry))))
        return err;
    
      if (S_ISLNK(inode->base.mode)) {
        /* Symlink: add target to front of components */
        size_t symlen;
        char *target;
        if (++depth > SQFS_RESOLVE_MAX_DEPTH)
          return SQFS_ERR;
        
        if ((err = sqfs_readlink(res->fs, inode, NULL, &symlen)))
          return err;
        if (!(target = malloc(symlen)))
          return SQFS_ERR;
        if ((err = sqfs_readlink(res->fs, inode, target, &symlen))) {
          free(target);
          return err;
        }
        
        err = sqfs_resolver_prepend_path(res, target);
        free(target);
        if (err)
          return err;
      } else {
        /* File or dir: add inode to stack */
        if ((err = sqfs_stack_push(&res->levels, &pinode)))
          return err;
        *pinode = *inode;
      }
    } /* end regular name */ 
    free(sqfs_list_shift(&res->components)); /* free the name we just used */
  }
  
  /* Out of path components, success! */
  if ((err = sqfs_resolver_current(res, &pinode)))
    return err;
  *inode = *pinode;
  return SQFS_OK;
}

sqfs_err sqfs_resolver_copy(const sqfs_resolver *src, sqfs_resolver *dst) {
  sqfs_err err;
  sqfs_resolver_init(dst);
  if ((err = sqfs_stack_copy(&src->levels, &dst->levels)))
    return err;
  dst->root = src->root;
  dst->fs = src->fs;
  return SQFS_OK;
}

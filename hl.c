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
#include "squashfuse.h"
#include "fuseprivate.h"

#include "nonstd.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct sqfs_hl sqfs_hl;
struct sqfs_hl {
  sqfs fs;
  sqfs_inode root;
};

/* On ancient FUSE, off_t is 32-bit, but there's no good way to detect it */
#if !HAVE_STRUCT_FUSE_OPERATIONS_RELEASE
  #define FUSE_32BIT 1
#endif

/* Get the appropriate flags argument for our FUSE version */
#if HAVE_STRUCT_FUSE_FILE_INFO
  typedef struct fuse_file_info *sqfs_file_info;
#else
  typedef int sqfs_file_info;
#endif

/* Global user-data, for broken FUSE implementations */
#if !HAVE_STRUCT_FUSE_OPERATIONS_INIT && !SQFS_CONTEXT_BROKEN
  #define SQFS_CONTEXT_BROKEN 1
#endif
#if SQFS_CONTEXT_BROKEN || !HAVE_FUSE_INIT_USER_DATA
  static sqfs_hl *gHL;
#endif

/* Get the user-data, whether or not we're broken */
static sqfs_hl *sqfs_user_data(void *data) {
#if SQFS_CONTEXT_BROKEN
  (void)data;
  return gHL;
#else
  if (data)
    return data;
  return fuse_get_context()->private_data;
#endif
}

/* Lookup the fs and inode for a path */
static sqfs_err sqfs_hl_lookup(sqfs **fs, sqfs_inode *inode,
    const char *path) {
  bool found;
  sqfs_err err;
  
  sqfs_hl *hl = sqfs_user_data(NULL);
  *fs = &hl->fs;
  *inode = hl->root; /* copy */

  err = sqfs_lookup_path(*fs, inode, path, &found);
  if (err)
    return err;
  if (!found)
    return SQFS_ERR;
  
  return SQFS_OK;
}

/* Get the filehandle that should be in the fileinfo */
static sqfs_err sqfs_hl_fh(sqfs **fs, sqfs_inode **inode, const char *path,
    sqfs_file_info fi) {
#if SQFS_CONTEXT_BROKEN || !HAVE_STRUCT_FUSE_FILE_INFO
  (void)fi;
  return sqfs_hl_lookup(fs, *inode, path);
#else
  (void)path;
  *fs = &sqfs_user_data(NULL)->fs;
  *inode = (sqfs_inode*)(intptr_t)fi->fh;
  return SQFS_OK;
#endif
}

/* Abstract out things that use fuse_file_info */
static void sqfs_hl_set_fh(sqfs_file_info fi, void *fh) {
  #if HAVE_STRUCT_FUSE_FILE_INFO
    fi->fh = (intptr_t)fh;
  #else
    free(fh);
  #endif
}
#if HAVE_STRUCT_FUSE_OPERATIONS_READDIR || HAVE_STRUCT_FUSE_OPERATIONS_RELEASE
static void sqfs_hl_free_fh(sqfs_file_info fi) {
  #if HAVE_STRUCT_FUSE_FILE_INFO
    free((void*)(intptr_t)fi->fh);
    fi->fh = 0;
  #else
    (void)fi;
  #endif
}
#endif
static int sqfs_hl_flags(sqfs_file_info fi) {
  #if HAVE_STRUCT_FUSE_FILE_INFO
    return fi->flags;
  #else
    return fi;
  #endif
}


#if HAVE_STRUCT_FUSE_OPERATIONS_INIT

static void sqfs_hl_op_destroy(void *user_data) {
  sqfs_hl *hl = sqfs_user_data(user_data);
  sqfs_destroy(&hl->fs, true);
  free(hl);
}

static void *sqfs_hl_op_init(
#if HAVE_FUSE_INIT_USER_DATA
    struct fuse_conn_info *SQFS_UNUSED(conn)
#endif
    ) {
#if SQFS_CONTEXT_BROKEN || !HAVE_FUSE_INIT_USER_DATA
  return gHL;
#else
  return fuse_get_context()->private_data;
#endif
}

#endif /* INIT */


static int sqfs_hl_op_getattr(const char *path, struct stat *st) {
  sqfs *fs;
  sqfs_inode inode;
  if (sqfs_hl_lookup(&fs, &inode, path))
    return -ENOENT;
  
  if (sqfs_stat(fs, &inode, st))
    return -ENOENT;
  
  /* Minix needs this */
  st->st_ino = inode.base.inode_number;

  return 0;
}


typedef bool (*sqfs_filler)(sqfs_dir_entry *entry, void *subfiller, void *buf,
  void *data);

static int sqfs_hl_dir_common(const char *path, sqfs_file_info fi,
    off_t offset, sqfs_filler filler, void *subfiller, void *buf, void *data) {
  sqfs_err err;
  sqfs *fs;
  sqfs_dir dir;
  sqfs_name namebuf;
  sqfs_dir_entry entry;
  sqfs_inode inode, *inodep = &inode;

  if (sqfs_hl_fh(&fs, &inodep, path, fi))
    return -ENOENT;

  if (sqfs_dir_open(fs, inodep, &dir, offset))
    return -EINVAL;

  sqfs_dentry_init(&entry, namebuf);
  while (sqfs_dir_next(fs, &dir, &entry, &err)) {
    if (filler(&entry, subfiller, buf, data))
      return 0;
  }
  if (err)
    return -EIO;
  return 0;
}


#if HAVE_STRUCT_FUSE_OPERATIONS_READDIR

static int sqfs_hl_op_opendir(const char *path, sqfs_file_info fi) {
  sqfs *fs;
  sqfs_inode *inode;
  
  inode = (sqfs_inode*)malloc(sizeof(*inode));
  if (!inode)
    return -ENOMEM;
  
  if (sqfs_hl_lookup(&fs, inode, path)) {
    free(inode);
    return -ENOENT;
  }
    
  if (!S_ISDIR(inode->base.mode)) {
    free(inode);
    return -ENOTDIR;
  }
  
  sqfs_hl_set_fh(fi, inode);
  return 0;
}

static int sqfs_hl_op_releasedir(const char *SQFS_UNUSED(path),
    sqfs_file_info fi) {
  sqfs_hl_free_fh(fi);
  return 0;
}

static bool sqfs_hl_readdir_filler(sqfs_dir_entry *entry, void *filler,
    void *buf, void *data) {
  struct stat *stp = (struct stat*)data;
  sqfs_off_t doff = 0;

  #if !SQFS_READDIR_NO_OFFSET
    doff = sqfs_dentry_next_offset(entry);
    stp->st_mode = sqfs_dentry_mode(entry);
  #endif

  return ((fuse_fill_dir_t)filler)(buf, sqfs_dentry_name(entry), stp, doff);
}

static int sqfs_hl_op_readdir(const char *path, void *buf,
    fuse_fill_dir_t filler, off_t offset, sqfs_file_info fi) {
  struct stat st, *stp = &st;
  memset(&st, 0, sizeof(st));
  #if SQFS_READDIR_NO_OFFSET
    stp = NULL;
    offset = 0;
  #endif
  
  return sqfs_hl_dir_common(path, fi, offset, sqfs_hl_readdir_filler,
    (void*)filler, buf, stp);
}

#else /* !READDIR */

static bool sqfs_hl_getdir_filler(sqfs_dir_entry *entry, void *filler,
    void *buf, void *data) {
  return ((fuse_dirfil_t)filler)(buf, sqfs_dentry_name(entry), 0
#if HAVE_FUSE_DIRFIL_T_INODE
      , 0
#endif
    );
}

static int sqfs_hl_op_getdir(const char *path, fuse_dirh_t dh,
    fuse_dirfil_t filler) {
  return sqfs_hl_dir_common(path, 0, 0, sqfs_hl_getdir_filler, (void*)filler,
    dh, NULL);
}

#endif /* !READDIR */


static int sqfs_hl_op_open(const char *path, sqfs_file_info fi) {
  sqfs *fs;
  sqfs_inode *inode;
  
#if !SQFS_OPEN_BAD_FLAGS
  if ((sqfs_hl_flags(fi) & O_ACCMODE) != O_RDONLY)
    return -EROFS;
#endif
  
  inode = (sqfs_inode*)malloc(sizeof(*inode));
  if (!inode)
    return -ENOMEM;
  
  if (sqfs_hl_lookup(&fs, inode, path)) {
    free(inode);
    return -ENOENT;
  }
  
  if (!S_ISREG(inode->base.mode)) {
    free(inode);
    return -EISDIR;
  }
  
  sqfs_hl_set_fh(fi, inode);
  return 0;
}


#if HAVE_STRUCT_FUSE_OPERATIONS_RELEASE

static int sqfs_hl_op_release(const char *SQFS_UNUSED(path),
    sqfs_file_info fi) {
  sqfs_hl_free_fh(fi);
  return 0;
}

#endif


static int sqfs_hl_op_read(const char *path, char *buf, size_t size, off_t off
#if HAVE_STRUCT_FUSE_FILE_INFO
  , sqfs_file_info fi
#endif
) {
  sqfs *fs;
  sqfs_inode inode, *inodep = &inode;
  off_t osize;
  
  #if !HAVE_STRUCT_FUSE_FILE_INFO
    sqfs_file_info fi = 0;
  #endif
  #if FUSE_32BIT
    off = off & UINT32_MAX;
  #endif
  
  if (sqfs_hl_fh(&fs, &inodep, path, fi))
    return -ENOENT;

  osize = size;
  if (sqfs_read_range(fs, inodep, off, &osize, buf))
    return -EIO;
  return osize;
}

static int sqfs_hl_op_readlink(const char *path, char *buf, size_t size) {
  sqfs *fs;
  sqfs_inode inode;
  if (sqfs_hl_lookup(&fs, &inode, path))
    return -ENOENT;
  
  if (!S_ISLNK(inode.base.mode)) {
    return -EINVAL;
  } else if (sqfs_readlink(fs, &inode, buf, &size)) {
    return -EIO;
  }
  return 0;
}


#if HAVE_STRUCT_FUSE_OPERATIONS_LISTXATTR

static int sqfs_hl_op_listxattr(const char *path, char *buf, size_t size) {
  sqfs *fs;
  sqfs_inode inode;
  int ferr;
  
  if (sqfs_hl_lookup(&fs, &inode, path))
    return -ENOENT;

  ferr = sqfs_listxattr(fs, &inode, buf, &size);
  if (ferr)
    return -ferr;
  return size;
}

static int sqfs_hl_op_getxattr(const char *path, const char *name,
    char *value, size_t size
#if FUSE_XATTR_POSITION
    , uint32_t position
#endif
    ) {
  sqfs *fs;
  sqfs_inode inode;
  size_t real = size;

#if FUSE_XATTR_POSITION
  if (position != 0) /* We don't support resource forks */
    return -EINVAL;
#endif

  if (sqfs_hl_lookup(&fs, &inode, path))
    return -ENOENT;
  
  if ((sqfs_xattr_lookup(fs, &inode, name, value, &real)))
    return -EIO;
  if (real == 0)
    return -sqfs_enoattr();
  if (size != 0 && size < real)
    return -ERANGE;
  return real;
}

#endif /* LISTXATTR */


static sqfs_hl *sqfs_hl_open(const char *path) {
  sqfs_hl *hl;
  
  hl = (sqfs_hl*)malloc(sizeof(*hl));
  if (!hl) {
    perror("Can't allocate memory");
  } else {
    memset(hl, 0, sizeof(*hl));
  
    if (sqfs_open_image(&hl->fs, path) == SQFS_OK) {
      if (sqfs_inode_get(&hl->fs, &hl->root, sqfs_inode_root(&hl->fs)))
        fprintf(stderr, "Can't find the root of this filesystem!\n");
      else
        return hl;
      sqfs_destroy(&hl->fs, true);
    }
    
    free(hl);
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  struct fuse_operations sqfs_hl_ops;
  struct fuse_args args;
  sqfs_opts opts;
  sqfs_hl *hl;
  int ret = 0;
  
  memset(&sqfs_hl_ops, 0, sizeof(sqfs_hl_ops));
#if HAVE_STRUCT_FUSE_OPERATIONS_INIT
  sqfs_hl_ops.init        = sqfs_hl_op_init;
  sqfs_hl_ops.destroy     = sqfs_hl_op_destroy;
#endif
  sqfs_hl_ops.getattr     = sqfs_hl_op_getattr;
#if HAVE_STRUCT_FUSE_OPERATIONS_READDIR
  sqfs_hl_ops.opendir     = sqfs_hl_op_opendir;
  sqfs_hl_ops.releasedir  = sqfs_hl_op_releasedir;
  sqfs_hl_ops.readdir     = sqfs_hl_op_readdir;
#else
  sqfs_hl_ops.getdir      = sqfs_hl_op_getdir;
#endif
  sqfs_hl_ops.open        = sqfs_hl_op_open;
#if HAVE_STRUCT_FUSE_OPERATIONS_RELEASE
  sqfs_hl_ops.release     = sqfs_hl_op_release;
#endif
  sqfs_hl_ops.read        = sqfs_hl_op_read;
  sqfs_hl_ops.readlink    = sqfs_hl_op_readlink;
#if HAVE_STRUCT_FUSE_OPERATIONS_LISTXATTR
  sqfs_hl_ops.listxattr   = sqfs_hl_op_listxattr;
  sqfs_hl_ops.getxattr    = sqfs_hl_op_getxattr;
#endif
  
  if (sqfs_opt_parse(&args, argc, argv, &opts))
    sqfs_usage(argv[0], true);
  if (!opts.image)
    sqfs_usage(argv[0], true);

  if (!sqfs_threads_available())
    sqfs_opt_single_threaded(&args);
  
  hl = sqfs_hl_open(opts.image);
  free(opts.image);
  if (!hl)
    return -1;
  
#if SQFS_CONTEXT_BROKEN || !HAVE_FUSE_INIT_USER_DATA
  gHL = hl;
#endif
#if HAVE_FUSE_INIT_USER_DATA || HAVE_FUSE_MAIN_RETURN
  ret =
#endif
#if HAVE_FUSE_INIT_USER_DATA
  fuse_main(args.argc, args.argv, &sqfs_hl_ops, hl);
#else
  fuse_main(args.argc, args.argv, &sqfs_hl_ops);
#endif
  
  sqfs_opt_free(&args);
  return ret;
}

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
#include "fuseprivate.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nonstd.h"

sqfs_err sqfs_stat(sqfs *fs, sqfs_inode *inode, struct stat *st) {
  sqfs_err err = SQFS_OK;
  uid_t id;
  
  memset(st, 0, sizeof(*st));
  st->st_mode = inode->base.mode;
  st->st_nlink = inode->nlink;
  st->st_mtime = st->st_ctime = st->st_atime = inode->base.mtime;
  
  if (S_ISREG(st->st_mode)) {
    /* FIXME: do symlinks, dirs, etc have a size? */
    st->st_size = inode->xtra.reg.file_size;
    st->st_blocks = st->st_size / 512;
  } else if (S_ISBLK(st->st_mode) || S_ISCHR(st->st_mode)) {
    st->st_rdev = sqfs_makedev(inode->xtra.dev.major,
      inode->xtra.dev.minor);
  }
  
  st->st_blksize = fs->sb.block_size; /* seriously? */
  
  err = sqfs_id_get(fs, inode->base.uid, &id);
  if (err)
    return err;
  st->st_uid = id;
  err = sqfs_id_get(fs, inode->base.guid, &id);
  st->st_gid = id;
  if (err)
    return err;
  
  return SQFS_OK;
}

int sqfs_listxattr(sqfs *fs, sqfs_inode *inode, char *buf, size_t *size) {
  sqfs_xattr x;
  size_t count = 0;
  
  if (sqfs_xattr_open(fs, inode, &x))
    return -EIO;
  
  while (x.remain) {
    size_t n;
    if (sqfs_xattr_read(&x))
       return EIO;
    n = sqfs_xattr_name_size(&x);
    count += n + 1;
    
    if (buf) {
      if (count > *size)
        return ERANGE;
      if (sqfs_xattr_name(&x, buf, true))
        return EIO;
      buf += n;
      *buf++ = '\0';
    }
  }
  *size = count;
  return 0;
}

void sqfs_usage(char *progname, bool fuse_usage) {
  fprintf(stderr, "%s (c) 2012 Dave Vasilevsky\n\n", PACKAGE_STRING);
  fprintf(stderr, "Usage: %s [options] ARCHIVE MOUNTPOINT\n",
    progname ? progname : PACKAGE_NAME);
#if FUSE_PARSE_CMDLINE
  if (fuse_usage) {
    struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
    fuse_opt_add_arg(&args, ""); /* progname */
    fuse_opt_add_arg(&args, "-ho");
    fprintf(stderr, "\n");
    fuse_parse_cmdline(&args, NULL, NULL, NULL);
  }
#endif
  exit(-2);
}


#if CONTEXT_BROKEN
  static sqfs_opts *gOpts;
#endif


/* Scan for a "-o image=foo.squashfs" option. Minix actually needs this,
   there's terrible opt parsing. */
static char *sqfs_opt_scan_image(struct fuse_args *args) {
#ifdef __minix
  int i;
  bool in_opt = false;
  
  for (i = 0; i < args->argc; ++i) {
    char *a = args->argv[i];
    if (strcmp(a, "--") == 0) /* End of args marker */
      break;
    
    /* Are we starting a new option group? */
    if (!in_opt && strcmp(a, "-o") == 0) {
      in_opt = true; /* Minix doesn't allow '-ofoo=bar', needs '-o foo=bar' */
      continue;
    }
    if (!in_opt) /* Don't care about non-options */
      continue;
    
    while (true) {
      char *sep = a;
      char *comma = a;
      
      while (*comma && *comma != ',') /* Find a comma */
        ++comma;
      
      while (sep < comma && *sep && *sep != '=')
        ++sep; /* Find an equals separator, Minix doesn't allow spaces */
      
      if (sep < comma && strncmp(a, "image", sep - a) == 0) {
        /* Found our option! */
        ++sep;
        return strndup(sep, comma - sep);
      }
      
      if (!*comma)
        break;
      a = comma + 1;
    }
    
    in_opt = false; /* No longer in an option group */
  }
  
#endif
  return NULL;
}

static int sqfs_opt_proc(void *data, const char *arg, int key,
    struct fuse_args *SQFS_UNUSED(outargs)) {
  sqfs_opts *opts;
#if CONTEXT_BROKEN
  opts = gOpts;
#else
  opts = (sqfs_opts*)data;
#endif
  
  if (key == FUSE_OPT_KEY_NONOPT) {
    if (opts->mountpoint) {
      return -1; /* Too many args */
    } else if (opts->image) {
      opts->mountpoint = 1;
      return 1;
    } else {
      if (!(opts->image = malloc(strlen(arg) + 1)))
        return -1;
      strcpy(opts->image, arg);
      return 0;
    }
  } else if (key == FUSE_OPT_KEY_OPT) {
    if (strncmp(arg, "-h", 2) == 0 || strncmp(arg, "--h", 3) == 0)
      sqfs_usage(opts->progname, true);
  }
  return 1; /* Keep */
}

sqfs_err sqfs_opt_single_threaded(struct fuse_args *args) {
#if HAVE_FUSE_OPT_PARSE
  if (fuse_opt_add_arg(args, "-s") == -1)
    return SQFS_ERR;
  return SQFS_OK;
#else
  #error TODO
#endif
}

void sqfs_opt_free(struct fuse_args *args) {
#if HAVE_FUSE_OPT_PARSE
  if (args->allocated)
    fuse_opt_free_args(args);
#else
  #error TODO
#endif
}

sqfs_err sqfs_opt_parse(struct fuse_args *outargs, int argc, char **argv,
    sqfs_opts *opts) {
#if HAVE_FUSE_OPT_PARSE
  char *scan_image = NULL;
  struct fuse_opt specs[] = { FUSE_OPT_END };
  
  outargs->argc = argc;
  outargs->argv = argv;
  outargs->allocated = 0;
  
  opts->progname = argv[0];
  opts->image = NULL;
  opts->mountpoint = 0;
#if CONTEXT_BROKEN
  gOpts = opts;
#endif
  
  scan_image = sqfs_opt_scan_image(outargs);
  if (fuse_opt_parse(outargs, opts, specs, sqfs_opt_proc) == -1)
    return SQFS_ERR;

  if (scan_image) {
    free(opts->image);
    opts->image = scan_image;
  }
    
  return SQFS_OK;
#else
  #error TODO
#endif
}

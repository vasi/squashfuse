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

/* fuse_opt_add_arg() equivalent */
static int sqfs_opt_add_arg(struct fuse_args *args, const char *arg);

/* Option-processing callback */
static int sqfs_opt_proc(void *data, const char *arg, int key,
    struct fuse_args *outargs);


#if __minix || !HAVE_FUSE_OPT_PARSE
  #define NEED_OPT_SCAN 1
#endif
#if NEED_OPT_SCAN
/* Very simple opt parsing. We only handle:
   - a literal '--'
   - positional arguments
   - arguments of the form '-f' or '--foo', without argument
   - arguments like '-o foo,bar'. We don't split foo from bar.
*/
#define OPT_TYPE_OPTARG     1   /* -o foo,bar */
#define OPT_TYPE_OPTION     2   /* -f or --foo or -- */
#define OPT_TYPE_POSITIONAL 3
typedef sqfs_err (*sqfs_opt_scan_proc)(void *ctx, int type, const char *arg);
static sqfs_err sqfs_opt_scan(void *ctx, sqfs_opt_scan_proc proc, int argc,
  char **argv);
#endif

/* Scan for -o image=something arguments, if necessary,
   without fuse_opt_parse() */
#ifdef __minix
static char *sqfs_opt_scan_image(int argc, char **argv);
#endif

#if !HAVE_FUSE_OPT_PARSE
/* Ersatz replacement for fuse_opt_parse() */
static sqfs_err sqfs_opt_parse_ersatz(struct fuse_args *outargs, int argc,
  char **argv, sqfs_opts *opts);
#endif


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
  } else if (S_ISLNK(st->st_mode)) {
    size_t size;
    if (!sqfs_readlink(fs, inode, NULL, &size))
      st->st_size = size;
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


#if NEED_OPT_SCAN
static sqfs_err sqfs_opt_scan(void *ctx, sqfs_opt_scan_proc proc, int argc,
    char **argv) {
  size_t i;
  bool in_opt = false, /* are we at an option argument? */
    no_opts = false; /* are we done with options? */
  sqfs_err err = SQFS_OK;
  
  for (i = 1; i < argc; ++i) {
    char *a = argv[i];
    if (err)
      return err;
    
    if (no_opts) {
      proc(ctx, OPT_TYPE_POSITIONAL, a);
      continue;
    }
    
    if (!in_opt) {
      if (a[0] != '-') {
        err = proc(ctx, OPT_TYPE_POSITIONAL, a);
        continue;
      } else if (a[1] == 'o') {
        in_opt = true;
        if (!a[2])
          continue; /* '-o whatever' as two args */
        a += 2;
      } else {
        err = proc(ctx, OPT_TYPE_OPTION, a);
        if (a[1] == '-' && !a[2])
          no_opts = true; /* found -- */
        continue;
      }
    }
    
    err = proc(ctx, OPT_TYPE_OPTARG, a);
    in_opt = false;
  }
  
  return err;
}
#endif


#ifdef __minix
/* This is only needed on Minix, where opt-parsing in FUSE is terrible. We
   can't provide a non-block-device as an argument, so we need to use
   -o image=foo.squashfs . And fuse_opt_proc() doesn't behave. Wheeeee! */
static sqfs_err sqfs_opt_scan_image_proc(void *ctx, int type,
    const char *arg) {
  char **image = (char**)ctx;
  
  if (*image || type != OPT_TYPE_OPTARG)
    return SQFS_OK;
  
  /* Go over each item in the foo=bar,baz=blah group */
  while (true) {
    const char *sep = arg;
    const char *comma = arg;
    
    while (*comma && *comma != ',') /* Find a comma */
      ++comma;
    
    while (sep < comma && *sep && *sep != '=')
      ++sep; /* Find an equals separator, Minix doesn't allow spaces */
    
    if (sep < comma && strncmp(arg, "image", sep - arg) == 0) {
      /* Found our option! */
      size_t len;
      ++sep;
      len = comma - sep;
      if (!(*image = malloc(len + 1)))
        return SQFS_ERR;
      strncpy(*image, sep, len + 1);
      (*image)[len] = '\0';
      return SQFS_OK;
    }
    
    if (!*comma)
      break;
    arg = comma + 1;
  }
  
  return SQFS_OK;
}
static char *sqfs_opt_scan_image(int argc, char **argv) {
  char *ret = NULL;
  sqfs_err err = sqfs_opt_scan(&ret, sqfs_opt_scan_image_proc, argc, argv);
  return err ? NULL : ret;
}
#endif


#if !HAVE_FUSE_OPT_PARSE
typedef struct {
  struct fuse_args *outargs;
  sqfs_opts *opts;
} sqfs_opt_parse_ersatz_ctx;
static sqfs_err sqfs_opt_parse_ersatz_proc(void *ctx, int type,
    const char *arg) {
  sqfs_opt_parse_ersatz_ctx *c = (sqfs_opt_parse_ersatz_ctx*)ctx;
  
  /* NOTE: This relies on the current behaviour of sqfs_opt_proc! */
  int key = (type == OPT_TYPE_POSITIONAL ? FUSE_OPT_KEY_NONOPT
      : FUSE_OPT_KEY_OPT);
  int keep = sqfs_opt_proc(c->opts, arg, key, NULL);
  
  if (keep == -1)
    return SQFS_ERR;
  if (keep) {
    sqfs_err err = SQFS_OK;
    if (type == OPT_TYPE_OPTARG)
      err = sqfs_opt_add_arg(c->outargs, "-o");
    if (!err)
      err = sqfs_opt_add_arg(c->outargs, arg);
    return err;
  }
  return SQFS_OK;
}
static sqfs_err sqfs_opt_parse_ersatz(struct fuse_args *outargs, int argc,
    char **argv, sqfs_opts *opts) {
  sqfs_opt_parse_ersatz_ctx ctx;
  ctx.outargs = outargs;
  ctx.opts = opts;
  
  outargs->argc = 0;
  outargs->argv = NULL;
  outargs->allocated = 0;
  
  sqfs_opt_add_arg(outargs, argv[0]); /* progname */
  return sqfs_opt_scan(&ctx, sqfs_opt_parse_ersatz_proc, argc, argv);
}
#endif


static int sqfs_opt_add_arg(struct fuse_args *args, const char *arg) {
#if HAVE_FUSE_OPT_PARSE
  return fuse_opt_add_arg(args, arg) == -1 ? SQFS_ERR : SQFS_OK;
#else
  char *new_arg, **new_argv;
  size_t new_argc, new_size;
  
  if (!(new_arg = malloc(strlen(arg) + 1)))
    return -1;
  strcpy(new_arg, arg);
    
  new_argc = args->argc + 1;
  new_size = new_argc + 1; /* NULL at end */
  new_argv = realloc(args->argv, new_size * sizeof(char*));
  if (!new_argv) {
    free(new_arg);
    return -1;
  }
  
  new_argv[new_argc - 1] = new_arg;
  new_argv[new_argc] = NULL;
  
  args->argc = new_argc;
  args->argv = new_argv;
  args->allocated = 1;
  
  return 0;
#endif
}

sqfs_err sqfs_opt_single_threaded(struct fuse_args *args) {
  if (sqfs_opt_add_arg(args, "-s") == -1)
    return SQFS_ERR;
  return SQFS_OK;
}

void sqfs_opt_free(struct fuse_args *args) {
  if (!args)
    return;
  
#if HAVE_FUSE_OPT_PARSE
  if (args->allocated) /* Some systems don't check this */
    fuse_opt_free_args(args);
#else
  if (args->allocated) {
    if (args->argv) {
      size_t i;
      for (i = 0; i < args->argc; ++i) {
        if (args->argv[i])
          free(args->argv[i]);
      }
      free(args->argv);
    }
    args->argc = 0;
    args->argv = NULL;
    args->allocated = 0;
  }
#endif
}


#if SQFS_CONTEXT_BROKEN
  static sqfs_opts *gOpts;
#endif

static int sqfs_opt_proc(void *data, const char *arg, int key,
    struct fuse_args *SQFS_UNUSED(outargs)) {
  sqfs_opts *opts;
#if SQFS_CONTEXT_BROKEN
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

sqfs_err sqfs_opt_parse(struct fuse_args *outargs, int argc, char **argv,
    sqfs_opts *opts) {
  opts->progname = argv[0];
  opts->image = NULL;
  opts->mountpoint = 0;
#if SQFS_CONTEXT_BROKEN
  gOpts = opts;
#endif

#if HAVE_FUSE_OPT_PARSE
  {
    struct fuse_opt specs[] = { FUSE_OPT_END };
  
    outargs->argc = argc;
    outargs->argv = argv;
    outargs->allocated = 0;
  
    if (fuse_opt_parse(outargs, opts, specs, sqfs_opt_proc) == -1)
      return SQFS_ERR;

    #ifdef __minix
    {
      char *scan_image = sqfs_opt_scan_image(argc, argv);
      if (scan_image) {
        free(opts->image);
        opts->image = scan_image;
      }
    }
    #endif
    
    return SQFS_OK;
  }
#else
  /* Ersatz fuse_opt_parse, just enough for our needs */
  return sqfs_opt_parse_ersatz(outargs, argc, argv, opts);
#endif
}

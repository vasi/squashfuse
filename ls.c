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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#define PROGNAME "squashfuse_ls"

#define ERR_MISC  (-1)
#define ERR_USAGE (-2)
#define ERR_OPEN  (-3)

static void usage() {
  sqfs_print(stderr, PROGNAME);
  sqfs_print(stderr, " (c) 2013 Dave Vasilevsky\n\n");
  sqfs_print(stderr, "Usage: ");
  sqfs_print(stderr, PROGNAME);
  sqfs_print(stderr, " ARCHIVE\n");
  exit(ERR_USAGE);
}

static void die(const char *msg) {
  sqfs_print(stderr, msg);
  sqfs_print(stderr, "\n");
  exit(ERR_MISC);
}

SQFS_MAIN {
  sqfs_err err = SQFS_OK;
  sqfs_traverse trv;
  sqfs fs;
  sqfs_host_path image;

  sqfs_print_init();

  if (argc > 2)
    usage();
  image = argc == 2 ? argv[1] : NULL;

  if ((err = sqfs_open_image(&fs, image)))
    exit(ERR_OPEN);
  
  if ((err = sqfs_traverse_open(&trv, &fs, sqfs_inode_root(&fs))))
    die("sqfs_traverse_open error");
  while (sqfs_traverse_next(&trv, &err)) {
    if (!trv.dir_end) {
      sqfs_print(stdout, sqfs_traverse_path(&trv));
      sqfs_print(stdout, "\n");
    }
  }
  if (err)
    die("sqfs_traverse_next error");
  sqfs_traverse_close(&trv);
  
  sqfs_destroy(&fs, true);
  return 0;
}

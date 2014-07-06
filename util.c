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
#include "util.h"

#include "dynstring.h"
#include "fs.h"
#include "input.h"

#include <stdio.h>
#include <stdlib.h>

/* TODO: i18n of error messages */
char *sqfs_open_error(sqfs *fs, sqfs_err err) {
  #define SQFMT(...) do { \
    if (sqfs_dynstring_format(&str, __VA_ARGS__)) \
      goto error; \
    } while (0)
  
  sqfs_dynstring str;
  
  if (err == SQFS_OK)
    return NULL;
  
  sqfs_dynstring_init(&str);
  sqfs_dynstring_create(&str, 0);
  
  switch (err) {
    case SQFS_BADFORMAT:
      SQFMT("Not a squashfs image");
      break;
    case SQFS_BADVERSION: {
      int major, minor, mj1, mn1, mj2, mn2;
      sqfs_version(fs, &major, &minor);
      sqfs_version_supported(&mj1, &mn1, &mj2, &mn2);
      SQFMT("Image version %d.%d detected, only version",
        major, minor);
      if (mj1 == mj2 && mn1 == mn2)
        SQFMT(" %d.%d", mj1, mn1);
      else
        SQFMT("s %d.%d through %d.%d", mj1, mn1, mj2, mn2);
      SQFMT(" supported");
      break;
    }
    case SQFS_BADCOMP: {
      bool first = true;
      int i;
      sqfs_compression_type sup[SQFS_COMP_MAX],
        comp = sqfs_compression(fs);
      sqfs_compression_supported(sup);
      SQFMT("Image uses %s compression, we only support ",
        sqfs_compression_name(comp));
      for (i = 0; i < SQFS_COMP_MAX; ++i) {
        if (sup[i] == SQFS_COMP_UNKNOWN)
          continue;
        if (!first)
          SQFMT(", ");
        SQFMT("%s", sqfs_compression_name(sup[i]));
        first = false;
      }
      break;
    }
    case SQFS_UNSEEKABLE:
      SQFMT("Image is not seekable");
      break;
    default:
      SQFMT("Unknown error");
  }
  
  return sqfs_dynstring_detach(&str);

error:
  sqfs_dynstring_destroy(&str);
  return NULL; /* Nothing we can do */
#undef SQFMT
}

sqfs_err sqfs_open_image(sqfs *fs, sqfs_host_path image) {
  sqfs_err err;
  sqfs_input *in;
  char *msg;
  
  if (!(in = (sqfs_input*)malloc(sizeof(sqfs_input))))
    return SQFS_ERR;
  
#if SQ_EMBED_DATA
  err = sqfs_input_memory_create(in, sqfs_embed, sqfs_embed_len);
#else
  if (image)
    err = sqfs_input_open(in, image);
  else
    err = sqfs_input_open_stdin(in);
#endif
  
  if (err) {
    msg = in->i_error(in);
    fprintf(stderr, "Can't open file: %s\n", msg);
    free(msg);
    return err;
  }
  
  if ((err = sqfs_init(fs, in))) {
    msg = sqfs_open_error(fs, err);
    fprintf(stderr, "Error opening image: %s\n", msg);
    free(msg);
    in->i_close(in);
    return err;
  }
  
  return SQFS_OK;
}

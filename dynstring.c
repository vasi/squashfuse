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
#include "dynstring.h"

#include "nonstd.h"

#include <stdlib.h>
#include <string.h>

/* Hack to replace va_copy */
#if __VMS
  #define va_copy(_dst, _src) ((_dst) = (_src))
#endif

char *sqfs_strdup(const char *s) {
  size_t asz = strlen(s) + 1;
  char *ret = (char*)malloc(asz);
  if (ret)
    strncpy(ret, s, asz);
  return ret;
}

char *sqfs_asprintf(const char *fmt, ...) {
  va_list ap;
  sqfs_err err;
  sqfs_dynstring str;
  
  sqfs_dynstring_init(&str);
  sqfs_dynstring_create(&str, 0);
  va_start(ap, fmt);
  err = sqfs_dynstring_vformat(&str, fmt, ap);
  va_end(ap);
  
  if (err) {
    sqfs_dynstring_destroy(&str);
    return NULL;
  }
  return sqfs_dynstring_detach(&str);
}


void sqfs_dynstring_init(sqfs_dynstring *s) {
  sqfs_array_init(s);
}

sqfs_err sqfs_dynstring_create(sqfs_dynstring *s, size_t initial) {
  sqfs_err err;
  if ((err = sqfs_array_create(s, sizeof(char), initial, NULL)))
    return err;
  return sqfs_array_append(s, NULL); /* room for terminator */
}

void sqfs_dynstring_destroy(sqfs_dynstring *s) {
  sqfs_array_destroy(s);
}

size_t sqfs_dynstring_size(sqfs_dynstring *s) {
  return sqfs_array_size(s) - 1;
}

char *sqfs_dynstring_string(sqfs_dynstring *s) {
  char *c;
  sqfs_array_first(s, &c); /* should always succeed */
  c[sqfs_dynstring_size(s)] = '\0'; /* ensure nul-termination */
  return c;
}

sqfs_err sqfs_dynstring_shrink(sqfs_dynstring *s, size_t shrink) {
  if (shrink > sqfs_dynstring_size(s))
    return SQFS_ERR; /* don't remove nul-terminator space! */
  
  return sqfs_array_shrink(s, shrink);
}

sqfs_err sqfs_dynstring_concat_size(sqfs_dynstring *s, const char *cat,
    size_t size) {
  sqfs_err err;
  char *last;
  
  if ((err = sqfs_array_grow(s, size, &last)))
    return err;
  
  memcpy(last - 1, cat, size);
  return SQFS_OK;
}

sqfs_err sqfs_dynstring_concat(sqfs_dynstring *s, const char *cat) {
  return sqfs_dynstring_concat_size(s, cat, strlen(cat));
}

sqfs_err sqfs_dynstring_format(sqfs_dynstring *s, const char *fmt, ...) {
  va_list ap;
  sqfs_err err;
  
  va_start(ap, fmt);
  err = sqfs_dynstring_vformat(s, fmt, ap);
  va_end(ap);
  return err;
}

sqfs_err sqfs_dynstring_vformat(sqfs_dynstring *s, const char *fmt,
    va_list ap) {
  va_list ap2;
  int printed;
  size_t size;
  char *start;
  sqfs_err err = SQFS_OK;
    
  /* Find out the size */
  va_copy(ap2, ap);
  size = sqfs_dynstring_size(s);
  sqfs_array_at(s, size, &start);
  printed = sqfs_vsnprintf(start, 1, fmt, ap);
  if (printed == -1) {
    err = SQFS_ERR;
    goto done;
  }
  
  /* Actually print it */
  if ((err = sqfs_array_grow(s, printed, NULL)))
    goto done;
  sqfs_array_at(s, size, &start);
  printed = sqfs_vsnprintf(start, printed + 1, fmt, ap2);
  if (printed == -1) {
    err = SQFS_ERR;
    goto done;
  }

done:
  va_end(ap2);
  return err;
}

char *sqfs_dynstring_detach(sqfs_dynstring *s) {
  char *ret = sqfs_dynstring_string(s);
  s->items = NULL;
  sqfs_array_init(s);
  return ret;
}

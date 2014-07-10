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
#ifndef SQFS_DYNSTRING_H
#define SQFS_DYNSTRING_H

#include "array.h"

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Utility function */
char *sqfs_strdup(const char *s);
char *sqfs_asprintf(const char *fmt, ...);

/* A dynamically expanding string wrapper */
typedef sqfs_array sqfs_dynstring;

void sqfs_dynstring_init(sqfs_dynstring *s);

sqfs_err sqfs_dynstring_create(sqfs_dynstring *s, size_t initial);
void sqfs_dynstring_destroy(sqfs_dynstring *s);

size_t sqfs_dynstring_size(const sqfs_dynstring *s);

/* Get the contents as a C-string */
char *sqfs_dynstring_string(sqfs_dynstring *s);

sqfs_err sqfs_dynstring_shrink(sqfs_dynstring *s, size_t shrink);
sqfs_err sqfs_dynstring_concat(sqfs_dynstring *s, const char *cat);
sqfs_err sqfs_dynstring_concat_size(sqfs_dynstring *s, const char *cat,
  size_t size);

/* Print into the dynstring */
sqfs_err sqfs_dynstring_format(sqfs_dynstring *s, const char *fmt, ...);
sqfs_err sqfs_dynstring_vformat(sqfs_dynstring *s, const char *fmt,
  va_list ap);

/* Return the contents as a char* and destroy the dynstring. The return
   value must be deallocated by the caller */
char *sqfs_dynstring_detach(sqfs_dynstring *s);

#ifdef __cplusplus
}
#endif

#endif

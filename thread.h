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
#ifndef SQFS_THREAD_H
#define SQFS_THREAD_H

/* Wrapper around threading primitives, for portability */

#include "common.h"


#ifdef HAVE_PTHREAD
	#include <pthread.h>

	typedef struct {
		pthread_mutex_t mutex;
	} sqfs_mutex;
	typedef struct {
		pthread_cond_t cond;
	} sqfs_cond_var;
	
#else /* No pthreads */
	typedef struct {
		char dummy;
	} sqfs_mutex;
	typedef struct {
		char dummy;
	} sqfs_cond_var;
#endif

sqfs_err sqfs_mutex_init(sqfs_mutex *m);
sqfs_err sqfs_mutex_destroy(sqfs_mutex *m);
sqfs_err sqfs_mutex_lock(sqfs_mutex *m);
sqfs_err sqfs_mutex_unlock(sqfs_mutex *m);

sqfs_err sqfs_cond_init(sqfs_cond_var *cv);
sqfs_err sqfs_cond_destroy(sqfs_cond_var *cv);
sqfs_err sqfs_cond_wait(sqfs_cond_var *cv, sqfs_mutex *m);
sqfs_err sqfs_cond_signal(sqfs_cond_var *cv);
sqfs_err sqfs_cond_broadcast(sqfs_cond_var *cv);

#endif

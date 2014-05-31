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
#include "traverse.h"

#include "squashfuse.h"

#include <stdlib.h>
#include <string.h>


#define PATH_SEPARATOR "/"

/* Default initial capacity of trv.path */
#define DEFAULT_PATH_CAP 32

/* The struct stored in trv.stack */
typedef struct {
	sqfs_dir dir;
	size_t name_size;
} sqfs_traverse_level;


/* Path manipulation functions */
static sqfs_err sqfs_traverse_path_add(sqfs_traverse *trv,
		const char *str, size_t size) {
	size_t need = trv->path_size + size;
	if (need > trv->path_cap) {
		char *next_path;
		size_t next_cap = trv->path_cap;
		while (need > next_cap)
			next_cap *= 2;
		
		if (!(next_path = realloc(trv->path, next_cap)))
			return SQFS_ERR;
		
		trv->path = next_path;
		trv->path_cap = next_cap;
	}
	
	strncat(trv->path + trv->path_size - 1, str, size);
	trv->path_size = need;
	return SQFS_OK;
}

static void sqfs_traverse_path_remove(sqfs_traverse *trv, size_t size) {
	trv->path_size -= size;
	trv->path[trv->path_size - 1] = '\0';
}


/* Helpers to grow and shrink the dir stack */
static sqfs_err sqfs_traverse_descend_inode(sqfs_traverse *trv,
		sqfs_inode *inode) {
	sqfs_err err;
	sqfs_traverse_level *level;
	
	if ((err = sqfs_stack_push(&trv->stack, &level)))
		return err;
	
	if ((err = sqfs_dir_open(trv->fs, inode, &level->dir, 0)))
		return err;
	level->name_size = trv->path_last_size;
	
	if (trv->path_last_size) {
		if ((err = sqfs_traverse_path_add(trv, PATH_SEPARATOR, 1)))
		return err;
	}
	trv->path_last_size = 0;
	
	trv->descend = false;
	return err;
}

static sqfs_err sqfs_traverse_descend(sqfs_traverse *trv, sqfs_inode_id iid) {
	sqfs_err err;
	sqfs_inode inode;
	
	if ((err = sqfs_inode_get(trv->fs, &inode, iid)))
		return err;
	
	return sqfs_traverse_descend_inode(trv, &inode);
}

static sqfs_err sqfs_traverse_ascend(sqfs_traverse *trv) {
	sqfs_err err;
	sqfs_traverse_level *level;
	
	if ((err = sqfs_stack_top(&trv->stack, &level)))
		return err;
	
	sqfs_traverse_path_remove(trv, trv->path_last_size);
	if (level->name_size > 0)
		sqfs_traverse_path_remove(trv, 1); /* separator */
	
	trv->path_last_size = level->name_size;
		
	sqfs_stack_pop(&trv->stack);
	return SQFS_OK;
}


sqfs_err sqfs_traverse_open_inode(sqfs_traverse *trv, sqfs *fs,
		sqfs_inode *inode) {
	sqfs_err err;
	
	sqfs_stack_init(&trv->stack);
	trv->path = NULL;
	trv->fs = fs;
	
	trv->path_cap = DEFAULT_PATH_CAP;
	if (!(trv->path = malloc(trv->path_cap))) {
		err = SQFS_ERR;
		goto error;
	}
	trv->path[0] = '\0';
	trv->path_size = 1;
	trv->path_last_size = 0;
	
	sqfs_dentry_init(&trv->entry, trv->namebuf);
	err = sqfs_stack_create(&trv->stack, sizeof(sqfs_traverse_level), 0, NULL);
	if (err)
		goto error;
	
	if ((err = sqfs_traverse_descend_inode(trv, inode)))
		goto error;
	
	return SQFS_OK;
	
error:
	sqfs_traverse_close(trv);
	return err;
}

sqfs_err sqfs_traverse_open(sqfs_traverse *trv, sqfs *fs, sqfs_inode_id iid) {
	sqfs_err err;
	sqfs_inode inode;
	
	if ((err = sqfs_inode_get(fs, &inode, iid)))
		return err;
	
	return sqfs_traverse_open_inode(trv, fs, &inode);
}

void sqfs_traverse_close(sqfs_traverse *trv) {
	sqfs_stack_destroy(&trv->stack);
	free(trv->path);
}

bool sqfs_traverse_next(sqfs_traverse *trv, sqfs_err *err) {
	sqfs_traverse_level *level;
	
	if (sqfs_stack_size(&trv->stack) == 0)
		return false;
	
	/* Descend into a directory, if we must */
	if (trv->descend) {
		sqfs_inode_id iid = sqfs_dentry_inode(&trv->entry);
		if ((*err = sqfs_traverse_descend(trv, iid)))
			return false;
	}
	
	/* Check if there's another dir entry */
	if ((*err = sqfs_stack_top(&trv->stack, &level)))
		return false;
	if (!sqfs_dir_next(trv->fs, &level->dir, &trv->entry, err)) {
		if (*err)
			return false;
		
		/* We're done with this directory */
		if ((*err = sqfs_traverse_ascend(trv)))
			return false;
		trv->dir_end = true;
		return true;
	}
	
	/* We have a valid entry */
	sqfs_traverse_path_remove(trv, trv->path_last_size);
	trv->path_last_size = sqfs_dentry_name_size(&trv->entry);
	*err = sqfs_traverse_path_add(trv, sqfs_dentry_name(&trv->entry),
		trv->path_last_size);
	if (*err)
		return false;
	
	trv->dir_end = false;
	if (sqfs_dentry_is_dir(&trv->entry))
		trv->descend = true;
	return true;
}

sqfs_err sqfs_traverse_skip(sqfs_traverse *trv) {
	return sqfs_traverse_ascend(trv);
}


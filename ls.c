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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "squashfuse.h"
#include "dir.h"

#define PROGNAME "squashfuse_ls"

static void usage() {
	fprintf(stderr, "%s (c) 2013 Dave Vasilevsky\n\n", PROGNAME);
	fprintf(stderr, "Usage: %s ARCHIVE\n", PROGNAME);
	exit(-2);
}

static void die(const char *msg) {
	fprintf(stderr, "%s\n", msg);
	exit(-1);
}

// FIXME: Simple traversal API?
typedef struct sqfs_ls_dir sqfs_ls_dir;
struct sqfs_ls_dir {
	char *name;
	sqfs_dir dir;
};

typedef struct sqfs_ls_path sqfs_ls_path;
struct sqfs_ls_path {
	sqfs *fs;
	size_t capacity, size;
	sqfs_ls_dir *dirs;
};


static void sqfs_ls_path_init(sqfs_ls_path *path, sqfs *fs) {
	path->fs = fs;
	path->size = path->capacity = 0;
	path->dirs = NULL;
}

static sqfs_ls_dir *sqfs_ls_path_top(sqfs_ls_path *path) {
	return &path->dirs[path->size - 1];
}

static void sqfs_ls_path_pop(sqfs_ls_path *path) {
	if (path->size == 0)
		return;

	free(sqfs_ls_path_top(path)->name);
	path->size--;
}

static void sqfs_ls_path_destroy(sqfs_ls_path *path) {
	while (path->size)
		sqfs_ls_path_pop(path);
	free(path->dirs);
	path->dirs = NULL;
}

static sqfs_ls_dir *sqfs_ls_path_expand(sqfs_ls_path *path) {
	if (path->size == path->capacity) {
		path->capacity = path->size ? (path->size * 3 / 2) : 8;
		path->dirs = realloc(path->dirs, path->capacity * sizeof(path->dirs[0]));
		if (!path->dirs)
			die("Out of memory");
	}
	path->size++;
	return sqfs_ls_path_top(path);
}

static void sqfs_ls_path_push(sqfs_ls_path *path, sqfs_inode_id inode_id,
		char *name) {
	sqfs_inode inode;
	sqfs_ls_dir *dir;

	if (sqfs_inode_get(path->fs, &inode, inode_id))
		die("sqfs_inode_get error");

	dir = sqfs_ls_path_expand(path);
	if (sqfs_opendir(path->fs, &inode, &dir->dir))
		die("sqfs_opendir error");
	
	dir->name = NULL;
	if (name && !(dir->name = strdup(name)))
		die("Out of memory");
}

// FIXME: Character encoding? Wide chars on Windows?
void sqfs_ls_path_print(sqfs_ls_path *path, char sep, char *name) {
	size_t i;
	for (i = 0; i < path->size; ++i) {
		sqfs_ls_dir *dir = &path->dirs[i];
		if (dir->name)
			printf("%s%c", dir->name, sep);
	}
	printf("%s\n", name);
}

int main(int argc, char *argv[]) {
	sqfs_err err = SQFS_OK;
	sqfs_ls_path path;
	sqfs fs;

	sqfs_fd_t file;
	char *image;

	if (argc != 2)
		usage();
	image = argv[1];

	// FIXME: Use sqfs_open_image()...
	// FIXME: WIN32 API
	file = open(image, O_RDONLY);
	if (file == -1)
		die("open error");

	err = sqfs_init(&fs, file);
	if (err) {
		fprintf(stderr, "squashfuse init error: %d\n", err);
		exit(-1);
	}

	sqfs_ls_path_init(&path, &fs);
	sqfs_ls_path_push(&path, fs.sb.root_inode, NULL);

	while (path.size > 0) {
		sqfs_dir_entry *dentry;
		sqfs_ls_dir *dir = sqfs_ls_path_top(&path);
		dentry = sqfs_readdir(&dir->dir, &err);
		if (err)
			die("sqfs_readdir error");

		if (dentry) {
			sqfs_ls_path_print(&path, '/', dentry->name); // FIXME: separator
			if (S_ISDIR(sqfs_mode(dentry->type)))
				sqfs_ls_path_push(&path, dentry->inode, dentry->name);
		} else {
			sqfs_ls_path_pop(&path);
		}
	}

	sqfs_ls_path_destroy(&path);
	close(file);

	return 0;
}

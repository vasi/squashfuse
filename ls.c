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

#include "traverse.h"
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


// FIXME: Character encoding? Wide chars on Windows?
int main(int argc, char *argv[]) {
	sqfs_err err = SQFS_OK;
	sqfs_traverse trv;
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
	
	if ((err = sqfs_traverse_open(&trv, &fs, sqfs_inode_root(&fs))))
		die("sqfs_traverse_open error");
	while (sqfs_traverse_next(&trv, &err)) {
		if (!trv.dir_end) {
			printf("%s\n", trv.path);
			if (strcmp(trv.path, "usr/share/man") == 0)
				sqfs_traverse_prune(&trv);
		}
	}
	if (err)
		die("sqfs_traverse_next error");
	sqfs_traverse_close(&trv);
	
	close(file);
	return 0;
}

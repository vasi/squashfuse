#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "squashfuse.h"

#define CHUNKSIZE 4096

static void die(char *msg) {
	fprintf(stderr, "%s\n", msg);
	exit(-2);
}

int main(int argc, char *argv[]) {
	if (argc != 3)
		die("Need two arguments");
	int fd = open(argv[1], O_RDONLY);
	if (fd == -1) {
		perror("Can't open squashfs file");
		exit(-2);
	}
	
	sqfs fs;
	sqfs_err err = sqfs_init(&fs, fd);
	if (err == SQFS_BADFORMAT)
		die("Not a squashfs filesystem");
	if (err)
		die("Can't initialize squashfs filesystem");
	
	sqfs_inode inode;
	if (sqfs_inode_get(&fs, &inode, fs.sb.root_inode))
		die("Can't get root inode");
	
	if (sqfs_lookup_path(&fs, &inode, argv[2]))
		die("Can't find path");
	
	char buf[CHUNKSIZE];
	for (off_t start = 0; true; start += CHUNKSIZE) {
		off_t size = CHUNKSIZE;
		if (sqfs_read_range(&fs, &inode, start, &size, buf))
			die("Error reading range");
		if (size)
			fwrite(buf, size, 1, stdout);
		if (size < CHUNKSIZE)
			break;
	}
	
	return 0;
}

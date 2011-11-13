#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "squashfuse.h"

static void die(char *msg) {
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s IMAGE\n", argv[0]);
		return -1;
	}
	
	char *file = argv[1];
	int fd = open(file, O_RDONLY);
	if (fd == -1)
		die("error opening image");
	
	sqfs fs;
	if (sqfs_init(&fs, fd))
		die("error initializing fs");
	
	sqfs_inode inode;
	if (sqfs_inode_get(&fs, &inode, fs.sb.root_inode))
		die("error reading inode");
		
	char path[] = "squashfs/Makefile";
	if (sqfs_lookup_path(&fs, &inode, path))
		die("error looking up path");
	
	size_t offset, size;
	sqfs_block *block;
	if (sqfs_frag_block(&fs, &inode, &offset, &size, &block))
		die("error reading fragment");
	
	fwrite(block->data + offset, size, 1, stdout);
		
	sqfs_destroy(&fs);
	close(fd);
	return 0;
}

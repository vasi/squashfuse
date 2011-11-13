#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
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
	
	sqfs_md_cursor cur;
	sqfs_md_cursor_inum(&cur, fs.sb.root_inode, fs.sb.inode_table_start);
	
	struct squashfs_base_inode base;
	if (sqfs_md_read(&fs, &cur, &base, sizeof(base)))
		die("error reading inode");
	sqfs_swapin_base_inode(&base);
	
	time_t mtime = base.mtime;
	printf("Root mtime: %s", ctime(&mtime));
	
	close(fd);
	return 0;
}

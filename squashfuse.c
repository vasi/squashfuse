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
	
	struct sqfs fs;
	if (sqfs_init(&fs, fd))
		die("error initializing fs");
		
	uint32_t root_block = (fs.sb.root_inode >> 16) + fs.sb.inode_table_start;
	uint16_t root_offset = fs.sb.root_inode & 0xffff;
	
	struct sqfs_block block;
	if (sqfs_read_md_block(&fs, root_block, &block))
		die("error reading md block");
	
	struct squashfs_base_inode *base;
	if (root_offset + sizeof(*base) > block.size) {
		fprintf(stderr, "FIXME: Need multiple blocks for inode\n");
		return 0;
	}
	base = (struct squashfs_base_inode*)(block.data + root_offset);
	sqfs_swapin_base_inode(base);
	
	time_t mtime = base->mtime;
	printf("Root mtime: %s", ctime(&mtime));
	
	close(fd);
	return 0;
}

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "squashfs_fs.h"
#include "swap.h"

int main(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s IMAGE\n", argv[0]);
		return -1;
	}
	
	int err = 1;
	char *file = argv[1];
	int fd = open(file, O_RDONLY);
	if (fd == -1) {
		perror("Opening image failed");
		goto cleanup;
	}
	
	struct squashfs_super_block sb;
	if (read(fd, &sb, sizeof(sb)) != sizeof(sb)) {
		perror("Reading superblock");
		goto cleanup;
	}
	sqfs_swapin_super_block(&sb);
	if (sb.s_magic != SQUASHFS_MAGIC) {
		fprintf(stderr, "Not a squashfs image\n");
		goto cleanup;
	}
	
	printf("Inodes: %d\n", sb.inodes);
	err = 0;
	
cleanup:
	close(fd);
	return err;
}

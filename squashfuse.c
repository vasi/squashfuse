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
	if (argc < 2) {
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
	
	
	if (argc != 5)
		die("bad args");
	char *endptr, *path = argv[2];
	off_t start = strtoll(argv[3], &endptr, 0);
	if (!*argv[3] || *endptr)
		die("bad len");
	off_t size = strtoll(argv[4], &endptr, 0);
	if (!*argv[4] || *endptr)
		die("bad len");
	
	if (sqfs_lookup_path(&fs, &inode, path))
		die("error looking up path");
	
	char *buf = malloc(size);
	if (!buf)
		die("malloc");
	
	off_t read = size;
	if (sqfs_read_range(&fs, &inode, start, &read, buf))
		die("read range");
	
	fprintf(stderr, "Read: %jd\n", (intmax_t)read);
	if (fwrite(buf, read, 1, stdout) != 1)
		die("fwrite");
	
	free(buf);
	sqfs_destroy(&fs);
	close(fd);
	return 0;
}

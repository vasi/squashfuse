#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
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
	
	time_t mtime = inode.base.mtime;
	printf("Root mtime: %s", ctime(&mtime));
	
	uid_t id;
	if (sqfs_id_get(&fs, inode.base.uid, &id))
		die("error getting uid");
	printf("UID: %d\n", id);
	
	if (S_ISDIR(inode.base.mode)) {
		sqfs_dir dir;
		if (sqfs_opendir(&fs, &inode, &dir))
			die("error opening dir");
		
		sqfs_err err;
		sqfs_dir_entry *entry;
		printf("\n");
		while ((entry = sqfs_readdir(&dir, &err))) {
			printf("%s\n", entry->name);
		}
		if (err)
			die("error reading dir");
	}
	
	sqfs_destroy(&fs);
	close(fd);
	return 0;
}

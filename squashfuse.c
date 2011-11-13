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

static void scandir(sqfs *fs, sqfs_inode_id id, char *path) {
	char *end = path + strlen(path);
	*end++ = '/';
	
	sqfs_inode inode;
	if (sqfs_inode_get(fs, &inode, id))
		die("error reading inode");
	
	sqfs_dir dir;
	if (sqfs_opendir(fs, &inode, &dir))
		die("error opening dir");
	
	sqfs_err err;
	sqfs_dir_entry *entry;
	while ((entry = sqfs_readdir(&dir, &err))) {
		strcpy(end, entry->name);
		printf("%s\n", path);
		
		if (S_ISDIR(sqfs_mode(entry->type)))
			scandir(fs, entry->inode, path);
	}
	if (err)
		die("error reading dir");
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
	
	if (0) {
		char path[PATH_MAX+1];
		path[0] = '\0';
		scandir(&fs, fs.sb.root_inode, path);
	} else if (1) {
		sqfs_inode inode;
		if (sqfs_inode_get(&fs, &inode, fs.sb.root_inode))
			die("error reading inode");
		
		char path[] = "etc/X11/fonts/Type1/xfonts-mathml.scale";
		if (sqfs_lookup_path(&fs, &inode, path))
			die("error looking up path");
		
		time_t mtime = inode.base.mtime;
		printf("%s", ctime(&mtime));
	}
		
	sqfs_destroy(&fs);
	close(fd);
	return 0;
}

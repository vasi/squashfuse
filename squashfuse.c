#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define FUSE_USE_VERSION 26
#include <fuse/fuse_lowlevel.h>

#include "squashfuse.h"

static void sqfs_ll_getattr(fuse_req_t req, fuse_ino_t ino,
		struct fuse_file_info *fi) {
	fuse_reply_err(req, ENOENT);
}

static void sqfs_ll_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
		off_t off, struct fuse_file_info *fi) {
	fuse_reply_err(req, ENOENT);
}

static struct fuse_lowlevel_ops sqfs_ll = {
	.getattr	= sqfs_ll_getattr,
	.readdir	= sqfs_ll_readdir,
};

typedef struct {
	const char *image;
	int mountpoint;
} sqfs_ll_opts;

static int sqfs_ll_opt_proc(void *data, const char *arg, int key,
		struct fuse_args *outargs) {
	sqfs_ll_opts *opts = (sqfs_ll_opts*)data;
	if (key == FUSE_OPT_KEY_NONOPT) {
		if (opts->mountpoint) {
			return -1; // Too many args
		} else if (opts->image) {
			opts->mountpoint = 1;
			return 1;
		} else {
			opts->image = arg;
			return 0;
		}
	}
	return 1; // Keep
}

int main(int argc, char *argv[]) {
	// FIXME: 32-bit inodes?
	if (sizeof(fuse_ino_t) < 6) {
		fprintf(stderr, "Need at least 48-bit inodes!\n");
		exit(-3);
	}
	
	// PARSE ARGS
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	sqfs_ll_opts opts = { .image = NULL, .mountpoint = 0 };
	int usage = 0;
	if (fuse_opt_parse(&args, &opts, NULL, sqfs_ll_opt_proc) == -1)
		usage = 1;

	char *mountpoint = NULL;
	int mt, fg;
	if (usage || fuse_parse_cmdline(&args, &mountpoint, &mt, &fg) == -1 ||
			mountpoint == NULL) {
		fprintf(stderr, "Usage: %s [OPTIONS] IMAGE MOUNTPOINT\n", argv[0]);
		exit(-2);
	}
	
	
	// OPEN FS
	int err = 0;
	int fd = open(opts.image, O_RDONLY);
	if (fd == -1) {
		perror("Can't open squashfs image");
		err = 1;
	}

	sqfs fs;
	sqfs_err serr = sqfs_init(&fs, fd);
	if (serr) {
		fprintf(stderr, "Can't open image: %d\n", serr);
		err = 1;
	}
	
	
	// STARTUP FUSE
	if (!err) {
		err = -1;
		struct fuse_chan *ch = fuse_mount(mountpoint, &args);
		if (ch) {
			struct fuse_session *se = fuse_lowlevel_new(&args,
				&sqfs_ll, sizeof(sqfs_ll), &fs);	
			if (se != NULL) {
				if (fuse_daemonize(fg) != -1) {
					if (fuse_set_signal_handlers(se) != -1) {
						fuse_session_add_chan(se, ch);
						// FIXME: multithreading
						err = fuse_session_loop(se);
						fuse_remove_signal_handlers(se);
						fuse_session_remove_chan(ch);
					}
				}
				fuse_session_destroy(se);
			}
			fuse_unmount(mountpoint, ch);
		}
	}
	fuse_opt_free_args(&args);
	free(mountpoint);

	return -err;
}

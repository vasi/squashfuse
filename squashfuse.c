#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define FUSE_USE_VERSION 26
#include <fuse/fuse_lowlevel.h>

#include "squashfuse.h"


static const double SQFS_TIMEOUT = DBL_MAX;

// FUSE wants the root to have inode 1. Convert back and forth
static fuse_ino_t sqfs_ll_ino_fuse(sqfs *fs, sqfs_inode_id i);
static sqfs_inode_id sqfs_ll_ino_sqfs(sqfs *fs, fuse_ino_t i);


typedef struct {
	const char *image;
	int mountpoint;
} sqfs_ll_opts;

static int sqfs_ll_opt_proc(void *data, const char *arg, int key,
	struct fuse_args *outargs);


static void sqfs_ll_getattr(fuse_req_t req, fuse_ino_t ino,
	struct fuse_file_info *fi);
static void sqfs_ll_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
	off_t off, struct fuse_file_info *fi);



static fuse_ino_t sqfs_ll_ino_fuse(sqfs *fs, sqfs_inode_id i) {
       return (i == fs->sb.root_inode) ? FUSE_ROOT_ID : i;
}
static sqfs_inode_id sqfs_ll_ino_sqfs(sqfs *fs, fuse_ino_t i) {
       return (i == FUSE_ROOT_ID) ? fs->sb.root_inode : i;
}


static void sqfs_ll_getattr(fuse_req_t req, fuse_ino_t ino,
		struct fuse_file_info *fi) {
	sqfs *fs = fuse_req_userdata(req);
	sqfs_inode_id inode_id = sqfs_ll_ino_sqfs(fs, ino);
	sqfs_inode inode;
	if (sqfs_inode_get(fs, &inode, inode_id)) {
		fuse_reply_err(req, ENOENT);
		return;
	}
	
	struct stat st;
	memset(&st, 0, sizeof(st));
	st.st_mode = inode.base.mode | sqfs_mode(inode.base.inode_type);
	st.st_nlink = inode.nlink;
	st.st_ino = ino;
	// FIXME: uid, gid, rdev
	st.st_mtimespec.tv_sec = st.st_ctimespec.tv_sec =
		st.st_atimespec.tv_sec = inode.base.mtime;
	if (S_ISREG(st.st_mode)) {
		st.st_size = inode.xtra.reg.file_size; // symlink?
		st.st_blocks = st.st_size / 512;
	}
	st.st_blksize = fs->sb.block_size; // seriously?
	fuse_reply_attr(req, &st, SQFS_TIMEOUT);
}

static void sqfs_ll_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
		off_t off, struct fuse_file_info *fi) {
	fuse_reply_err(req, ENOENT);
}

static struct fuse_lowlevel_ops sqfs_ll = {
	.getattr	= sqfs_ll_getattr,
	.readdir	= sqfs_ll_readdir,
};


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
	if (sizeof(fuse_ino_t) < SQFS_INODE_ID_BYTES) {
		fprintf(stderr, "Need at least %d-bit inodes!\n",
			SQFS_INODE_ID_BYTES * 8);
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

#include "config.h"

#ifdef NONSTD__DARWIN_C_SOURCE
	#define _DARWIN_C_SOURCE
#endif
#ifdef NONSTD__XOPEN_SOURCE
	#define _XOPEN_SOURCE 500
#endif
#ifdef NONSTD__BSD_SOURCE
	#define _BSD_SOURCE
#endif
#ifdef NONSTD__GNU_SOURCE
	#define _GNU_SOURCE
#endif
#ifdef NONSTD__POSIX_C_SOURCE
	#undef _POSIX_C_SOURCE
#endif

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "squashfs_fs.h"

dev_t sqfs_xmakedev(int maj, int min) {
	return makedev(maj, min);
}

ssize_t sqfs_xpread(int fd, void *buf, size_t count, off_t off) {
	return pread(fd, buf, count, off);
}

// S_IF* are not standard
mode_t sqfs_mode(int inode_type) {
	switch (inode_type) {
		case SQUASHFS_DIR_TYPE:
		case SQUASHFS_LDIR_TYPE:
			return S_IFDIR;
		case SQUASHFS_REG_TYPE:
		case SQUASHFS_LREG_TYPE:
			return S_IFREG;
		case SQUASHFS_SYMLINK_TYPE:
		case SQUASHFS_LSYMLINK_TYPE:
			return S_IFLNK;
		case SQUASHFS_BLKDEV_TYPE:
		case SQUASHFS_LBLKDEV_TYPE:
			return S_IFBLK;
		case SQUASHFS_CHRDEV_TYPE:
		case SQUASHFS_LCHRDEV_TYPE:
			return S_IFCHR;
		case SQUASHFS_FIFO_TYPE:
		case SQUASHFS_LFIFO_TYPE:
			return S_IFIFO;
		case SQUASHFS_SOCKET_TYPE:
		case SQUASHFS_LSOCKET_TYPE:
			return S_IFSOCK;
	}
	return 0;
}


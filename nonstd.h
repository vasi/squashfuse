#ifndef SQFS_STD_H
#define SQFS_STD_H

// Non-standard functions that we need

dev_t sqfs_makedev(int maj, int min);

ssize_t sqfs_pread(int fd, void *buf, size_t count, off_t off);

#endif

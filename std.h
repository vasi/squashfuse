#ifndef SQFS_STD_H
#define SQFS_STD_H

// Non-standard functions that we need

dev_t xmakedev(int maj, int min);

ssize_t xpread(int fd, void *buf, size_t count, off_t off);

#endif

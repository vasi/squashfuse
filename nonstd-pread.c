#include "config.h"

#define SQFEATURE NONSTD_PREAD_DEF
#include "nonstd-internal.h"

#include <unistd.h>

ssize_t sqfs_pread(int fd, void *buf, size_t count, off_t off) {
	return pread(fd, buf, count, off);
}

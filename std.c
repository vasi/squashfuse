// Linux
#define _XOPEN_SOURCE 500 // pread
#define _BSD_SOURCE

// Mac
#define _DARWIN_C_SOURCE

#include <sys/types.h>
#include <unistd.h>

dev_t xmakedev(int maj, int min) {
	return makedev(maj, min);
}

ssize_t xpread(int fd, void *buf, size_t count, off_t off) {
	return pread(fd, buf, count, off);
}

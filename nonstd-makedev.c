#include "config.h"

#define SQFEATURE NONSTD_MAKEDEV_DEF
#include "nonstd-internal.h"

#include <sys/types.h>

dev_t sqfs_makedev(int maj, int min) {
	return makedev(maj, min);
}

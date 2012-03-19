#include "config.h"

#define SQFEATURE NONSTD_DAEMON_DEF
#include "nonstd-internal.h"

#include <unistd.h>
#include "ll.h"

int sqfs_ll_daemonize(int fg) {
	#if HAVE_DECL_FUSE_DAEMONIZE
		return fuse_daemonize(fg);
	#else
		return daemon(0,0);
	#endif
}


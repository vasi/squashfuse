/*
 * Copyright (c) 2012 Dave Vasilevsky <dave@vasilevsky.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "ll.h"
#include "fuseprivate.h"
#include "stat.h"

#include "nonstd.h"

#include <errno.h>
#include <float.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	struct fuse_args args;
	sqfs_opts opts;

#if FUSE_USE_VERSION >= 30
	struct fuse_cmdline_opts fuse_cmdline_opts;
#else
	struct {
		char *mountpoint;
		int mt, foreground;
	} fuse_cmdline_opts;
#endif
	
	int err;
	sqfs_ll *ll;
	struct fuse_opt fuse_opts[] = {
		{"offset=%zu", offsetof(sqfs_opts, offset), 0},
		{"timeout=%u", offsetof(sqfs_opts, idle_timeout_secs), 0},
		FUSE_OPT_END
	};
	
	struct fuse_lowlevel_ops sqfs_ll_ops;
	memset(&sqfs_ll_ops, 0, sizeof(sqfs_ll_ops));
	sqfs_ll_ops.getattr		= sqfs_ll_op_getattr;
	sqfs_ll_ops.opendir		= sqfs_ll_op_opendir;
	sqfs_ll_ops.releasedir	= sqfs_ll_op_releasedir;
	sqfs_ll_ops.readdir		= sqfs_ll_op_readdir;
	sqfs_ll_ops.lookup		= sqfs_ll_op_lookup;
	sqfs_ll_ops.open		= sqfs_ll_op_open;
	sqfs_ll_ops.create		= sqfs_ll_op_create;
	sqfs_ll_ops.release		= sqfs_ll_op_release;
	sqfs_ll_ops.read		= sqfs_ll_op_read;
	sqfs_ll_ops.readlink	= sqfs_ll_op_readlink;
	sqfs_ll_ops.listxattr	= sqfs_ll_op_listxattr;
	sqfs_ll_ops.getxattr	= sqfs_ll_op_getxattr;
	sqfs_ll_ops.forget		= sqfs_ll_op_forget;
	sqfs_ll_ops.statfs    = stfs_ll_op_statfs;
   
	/* PARSE ARGS */
	args.argc = argc;
	args.argv = argv;
	args.allocated = 0;
	
	opts.progname = argv[0];
	opts.image = NULL;
	opts.mountpoint = 0;
	opts.offset = 0;
	opts.idle_timeout_secs = 0;
	if (fuse_opt_parse(&args, &opts, fuse_opts, sqfs_opt_proc) == -1)
		sqfs_usage(argv[0], true);

#if FUSE_USE_VERSION >= 30
	if (fuse_parse_cmdline(&args, &fuse_cmdline_opts) != 0)
#else
	if (fuse_parse_cmdline(&args,
                           &fuse_cmdline_opts.mountpoint,
                           &fuse_cmdline_opts.mt,
                           &fuse_cmdline_opts.foreground) == -1)
#endif
		sqfs_usage(argv[0], true);
	if (fuse_cmdline_opts.mountpoint == NULL)
		sqfs_usage(argv[0], true);

	/* fuse_daemonize() will unconditionally clobber fds 0-2.
	 *
	 * If we get one of these file descriptors in sqfs_ll_open,
	 * we're going to have a bad time. Just make sure that all
	 * these fds are open before opening the image file, that way
	 * we must get a different fd.
	 */
	while (true) {
	    int fd = open("/dev/null", O_RDONLY);
	    if (fd == -1) {
		/* Can't open /dev/null, how bizarre! However,
		 * fuse_deamonize won't clobber fds if it can't
		 * open /dev/null either, so we ought to be OK.
		 */
		break;
	    }
	    if (fd > 2) {
		/* fds 0-2 are now guaranteed to be open. */
		close(fd);
		break;
	    }
	}

	/* OPEN FS */
	err = !(ll = sqfs_ll_open(opts.image, opts.offset));
	
	/* STARTUP FUSE */
	if (!err) {
		sqfs_ll_chan ch;
		err = -1;
		if (sqfs_ll_mount(
                        &ch,
                        fuse_cmdline_opts.mountpoint,
                        &args,
                        &sqfs_ll_ops,
                        sizeof(sqfs_ll_ops),
                        ll) == SQFS_OK) {
			if (sqfs_ll_daemonize(fuse_cmdline_opts.foreground) != -1) {
				if (fuse_set_signal_handlers(ch.session) != -1) {
					if (opts.idle_timeout_secs) {
						setup_idle_timeout(ch.session, opts.idle_timeout_secs);
					}
					/* FIXME: multithreading */
					err = fuse_session_loop(ch.session);
					teardown_idle_timeout();
					fuse_remove_signal_handlers(ch.session);
				}
			}
			sqfs_ll_destroy(ll);
			sqfs_ll_unmount(&ch, fuse_cmdline_opts.mountpoint);
		}
	}
	fuse_opt_free_args(&args);
	free(ll);
	free(fuse_cmdline_opts.mountpoint);
	
	return -err;
}
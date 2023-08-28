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


#if defined(SQFS_SIGTERM_HANDLER)
#include <sys/utsname.h>
#include <linux/version.h>
static bool kernel_version_at_least(unsigned required_major,
                                    unsigned required_minor,
                                    unsigned required_micro) {
    struct utsname info;

    if (uname(&info) >= 0) {
        unsigned major, minor, micro;

        if (sscanf(info.release, "%u.%u.%u", &major, &minor, &micro) == 3) {
            return KERNEL_VERSION(major, minor, micro) >=
                KERNEL_VERSION(required_major, required_minor, required_micro);
        }
    }
    return false;
}

/* libfuse's default SIGTERM handler (set up in fuse_set_signal_handlers())
 * immediately calls fuse_session_exit(), which shuts down the filesystem
 * even if there are active users. This leads to nastiness if other processes
 * still depend on the filesystem.
 *
 * So: we respond to SIGTERM by starting a lazy unmount. This is done
 * by exec'ing fusermount3, which works properly for unpriviledged
 * users (we cannot use umount2() syscall because it is not signal safe;
 * fork() and exec(), amazingly, are).
 *
 * If we fail to start the lazy umount, we signal ourself with SIGINT,
 * which falls back to the old behavior of exiting ASAP.
 */
static const char *g_mount_point = NULL;
static void sigterm_handler(int signum) {
	/* Unfortunately, lazy umount of in-use fuse filesystem triggers
	 * kernel bug on kernels < 5.2, Fixed by kernel commit
	 * e8f3bd773d22f488724dffb886a1618da85c2966 in 5.2.
	 */
	if (g_mount_point && kernel_version_at_least(5,2,0)) {
        int pid = fork();
        if (pid == 0) {
            /* child process: disassociate ourself from parent so
             * we do not become zombie (as parent does not waitpid()).
             */
            pid_t parent = getppid();
            setsid();
            execl("/bin/fusermount3", "fusermount3",
                  "-u", "-q", "-z", "--", g_mount_point, NULL);
            execlp("fusermount3", "fusermount3",
                   "-u", "-q", "-z", "--", g_mount_point, NULL);
            /* if we get here, we can't run fusermount,
             * kill the original process with a harshness.
             */
            kill(parent, SIGINT);
            _exit(0);
        } else if (pid > 0) {
            /* parent process: nothing to do, murderous child will do us
             * in one way or another.
             */
            return;
        }
    }
    /* If we get here, we have failed to lazy unmount for whatever reason,
     * kill ourself more brutally.
     */
    kill(getpid(), SIGINT);
}

static void set_sigterm_handler(const char *mountpoint) {
    struct sigaction sa;

    g_mount_point = mountpoint;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigterm_handler;
    sigemptyset(&(sa.sa_mask));
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction(SIGTERM)");
    }
}
#endif /* SQFS_SIGTERM_HANDLER */

int main(int argc, char *argv[]) {
	struct fuse_args args;
	sqfs_opts opts;

#if FUSE_USE_VERSION >= 30
	struct fuse_cmdline_opts fuse_cmdline_opts = {};
#else
	struct {
		char *mountpoint;
		int mt, foreground;
	} fuse_cmdline_opts = {};
#endif
	
	int err;
	int sqfs_err;
	sqfs_ll *ll = NULL;
	struct fuse_opt fuse_opts[] = {
		{"offset=%zu", offsetof(sqfs_opts, offset), 0},
		{"timeout=%u", offsetof(sqfs_opts, idle_timeout_secs), 0},
		{"uid=%d", offsetof(sqfs_opts, uid), 0},
		{"gid=%d", offsetof(sqfs_opts, gid), 0},
		{"subdir=%s", offsetof(sqfs_opts, subdir), 0},
		{"notify_pipe=%s", offsetof(sqfs_opts, notify_pipe), 0},
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
	sqfs_ll_ops.init    = sqfs_ll_op_init;
   
	/* PARSE ARGS */
	args.argc = argc;
	args.argv = argv;
	args.allocated = 0;
	
	opts.progname = argv[0];
	opts.image = NULL;
	opts.mountpoint = 0;
	opts.offset = 0;
	opts.idle_timeout_secs = 0;
	opts.uid = 0;
	opts.gid = 0;
	opts.subdir = NULL;
	opts.notify_pipe = NULL;
	if (fuse_opt_parse(&args, &opts, fuse_opts, sqfs_opt_proc) == -1) {
		err = sqfs_usage(argv[0], true, true);
		goto out;
	}

#if FUSE_USE_VERSION >= 30
	if (fuse_parse_cmdline(&args, &fuse_cmdline_opts) != 0) {
#else
	if (fuse_parse_cmdline(&args,
                           &fuse_cmdline_opts.mountpoint,
                           &fuse_cmdline_opts.mt,
                           &fuse_cmdline_opts.foreground) == -1) {
#endif
		err = sqfs_usage(argv[0], true, true);
		goto out;
	}
	if (fuse_cmdline_opts.mountpoint == NULL) {
		err = sqfs_usage(argv[0], true, true);
		goto out;
	}

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
	err = !(ll = sqfs_ll_open_with_subdir(opts.image, opts.offset, opts.subdir));
	
	/* STARTUP FUSE */
	if (!err) {
		ll->fs.uid = opts.uid;
		ll->fs.gid = opts.gid;
		ll->fs.notify_pipe = opts.notify_pipe;

		sqfs_ll_chan ch;
		err = -1;
		sqfs_err = sqfs_ll_mount(
				&ch,
				fuse_cmdline_opts.mountpoint,
				&args,
				&sqfs_ll_ops,
				sizeof(sqfs_ll_ops),
				ll);
		if (sqfs_err != SQFS_OK) {
			goto out;
		}
		if (sqfs_ll_daemonize(fuse_cmdline_opts.foreground) != -1) {
			if (fuse_set_signal_handlers(ch.session) != -1) {
#if defined(SQFS_SIGTERM_HANDLER)
				set_sigterm_handler(fuse_cmdline_opts.mountpoint);
#endif
				if (opts.idle_timeout_secs) {
					setup_idle_timeout(ch.session, opts.idle_timeout_secs);
				}
#ifdef SQFS_MULTITHREADED
# if FUSE_USE_VERSION >= 30
				if (!fuse_cmdline_opts.singlethread) {
					struct fuse_loop_config config;
					config.clone_fd = 1;
					config.max_idle_threads = 10;
					err = fuse_session_loop_mt(ch.session, &config);
				}
# else /* FUSE_USE_VERSION < 30 */
				if (fuse_cmdline_opts.mt) {
					err = fuse_session_loop_mt(ch.session);
				}
# endif /* FUSE_USE_VERSION */
				else
#endif
					err = fuse_session_loop(ch.session);
				teardown_idle_timeout();
				fuse_remove_signal_handlers(ch.session);
			}
		}
		sqfs_ll_destroy(ll);
		sqfs_ll_unmount(&ch, fuse_cmdline_opts.mountpoint);
	}

out:
	if (err) {
		if (opts.notify_pipe) {
			notify_mount_ready(opts.notify_pipe, NOTIFY_FAILURE);
		}
	}
	fuse_opt_free_args(&args);
	free(ll);
	free(fuse_cmdline_opts.mountpoint);
	
	return -err;
}

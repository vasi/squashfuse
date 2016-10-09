#include "squashfuse.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROGNAME "squashfuse_extract"

#define ERR_MISC	(-1)
#define ERR_USAGE	(-2)
#define ERR_OPEN	(-3)

static void usage() {
    fprintf(stderr, "Usage: %s ARCHIVE PATH_TO_EXTRACT\n", PROGNAME);
    exit(ERR_USAGE);
}

static void die(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(ERR_MISC);
}

int main(int argc, char *argv[]) {
    sqfs_err err = SQFS_OK;
    sqfs_traverse trv;
    sqfs fs;
    char *image;
    char *path_to_extract;
    
    if (argc != 3)
        usage();
    image = argv[1];
    path_to_extract = argv[2];
    
    if ((err = sqfs_open_image(&fs, image, 0)))
        exit(ERR_OPEN);
    
    if ((err = sqfs_traverse_open(&trv, &fs, sqfs_inode_root(&fs))))
        die("sqfs_traverse_open error");
    while (sqfs_traverse_next(&trv, &err)) {
        if (!trv.dir_end) {
            if(strcmp(trv.path, path_to_extract) == 0){
                fprintf(stderr, "Extracting %s\n", trv.path);
                fprintf(stderr, "sqfs_inode_id: %lu\n", trv.entry.inode);
                sqfs_inode inode;
                if (sqfs_inode_get(&fs, &inode, trv.entry.inode))
                    die("sqfs_inode_get error");
                fprintf(stderr, "file_size: %lu\n", inode.xtra.reg.file_size);
                char *buf = malloc(inode.xtra.reg.file_size);
                if (sqfs_read_range(&fs, &inode, 0, &inode.xtra.reg.file_size, buf))
                    die("sqfs_read_range error");
                fwrite (buf, 1, inode.xtra.reg.file_size, stdout);
                free(buf);
            }
        }
    }
    if (err)
        die("sqfs_traverse_next error");
    sqfs_traverse_close(&trv);
    
    sqfs_fd_close(fs.fd);
    return 0;
}

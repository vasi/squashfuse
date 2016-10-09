#include "squashfuse.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "squashfs_fs.h"

#define PROGNAME "squashfuse_extract"

#define ERR_MISC	(-1)
#define ERR_USAGE	(-2)
#define ERR_OPEN	(-3)

static void usage() {
    fprintf(stderr, "Usage: %s ARCHIVE PATH_TO_EXTRACT\n", PROGNAME);
    fprintf(stderr, "       %s -a\n", PROGNAME);
    exit(ERR_USAGE);
}

static void die(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(ERR_MISC);
}


bool startsWith(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre),
    lenstr = strlen(str);
    return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
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
            if((startsWith(path_to_extract, trv.path) != 0) || (strcmp("-a", path_to_extract) == 0)){
                fprintf(stderr, "trv.path: %s\n", trv.path);
                fprintf(stderr, "sqfs_inode_id: %lu\n", trv.entry.inode);
                sqfs_inode inode;
                if (sqfs_inode_get(&fs, &inode, trv.entry.inode))
                    die("sqfs_inode_get error");
                fprintf(stderr, "inode.base.inode_type: %lu\n", inode.base.inode_type);
                fprintf(stderr, "inode.xtra.reg.file_size: %lu\n", inode.xtra.reg.file_size);
                if(inode.base.inode_type == SQUASHFS_DIR_TYPE){
                    fprintf(stderr, "inode.xtra.dir.parent_inode: %lu\n", inode.xtra.dir.parent_inode);
                    fprintf(stderr, "mkdir: %s/\n", trv.path);
                    if(access(trv.path, F_OK ) == -1 ) {
                        if (mkdir(trv.path, 0777) == -1) {
                            perror("mkdir error");
                            exit(EXIT_FAILURE);
                        }
                    }
                } else if(inode.base.inode_type == SQUASHFS_REG_TYPE){
                    fprintf(stderr, "Extract to: %s\n", trv.path);
                    // Read the file in chunks
                    off_t bytes_already_read = 0;
                    size_t bytes_at_a_time = 1024; 
                    FILE * f;
                    f = fopen (trv.path, "w+");
                    if (f == NULL)
                        die("fopen error");
                    while (bytes_already_read < inode.xtra.reg.file_size)
                    {
                        char *buf = malloc(bytes_at_a_time);
                        if (sqfs_read_range(&fs, &inode, bytes_already_read, &bytes_at_a_time, buf))
                            die("sqfs_read_range error");
                        // fwrite(buf, 1, bytes_at_a_time, stdout);
                        fwrite(buf, 1, bytes_at_a_time, f);
                        free(buf);                    
                        bytes_already_read = bytes_already_read + bytes_at_a_time;
                    }
                    fclose(f);
                } else if(inode.base.inode_type == SQUASHFS_SYMLINK_TYPE){
                    fprintf(stderr, "TODO: Implement symlink: ./%s\n", trv.path);
                } else {
                    fprintf(stderr, "TODO: Implement inode.base.inode_type %i\n", inode.base.inode_type);
                }
                
                fprintf(stderr, "\n");
            }
        }
    }
    if (err)
        die("sqfs_traverse_next error");
    sqfs_traverse_close(&trv);
    
    sqfs_fd_close(fs.fd);
    return 0;
}

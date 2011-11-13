#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <zlib.h>

#include "squashfs_fs.h"
#include "swap.h"

int main(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s IMAGE\n", argv[0]);
		return -1;
	}
	
	int err = 1;
	char *file = argv[1];
	int fd = open(file, O_RDONLY);
	if (fd == -1) {
		perror("Opening image failed");
		goto cleanup;
	}
	
	struct squashfs_super_block sb;
	if (read(fd, &sb, sizeof(sb)) != sizeof(sb)) {
		perror("Reading superblock");
		goto cleanup;
	}
	sqfs_swapin_super_block(&sb);
	if (sb.s_magic != SQUASHFS_MAGIC) {
		fprintf(stderr, "Not a squashfs image\n");
		// TODO: Check version, compression
		goto cleanup;
	}	
	printf("Inodes: %d\n", sb.inodes);
	
	uint32_t root_block = (sb.root_inode >> 16) + sb.inode_table_start;
	uint16_t root_offset = sb.root_inode & 0xffff;
	
	off_t pos = root_block;
	uint16_t hdr;
	if (pread(fd, &hdr, sizeof(hdr), pos) != sizeof(hdr)) {
		perror("Reading block header");
		goto cleanup;
	}
	pos += sizeof(hdr);
	hdr = sqfs_swapin16(hdr);
	bool compressed = !(hdr & SQUASHFS_COMPRESSED_BIT);
	size_t len = hdr & ~SQUASHFS_COMPRESSED_BIT;
	if (!len)
		len = SQUASHFS_COMPRESSED_BIT;
	
	char *block = malloc(len);
	if (!block) {
		fprintf(stderr, "Failed to allocate block data\n");
		goto cleanup;
	}
	if (pread(fd, block, len, pos) != len) {
		perror("Reading block");
		goto cleanup_block;
	}
	
	size_t olen = len;
	if (compressed) {
		size_t decomp_size = SQUASHFS_METADATA_SIZE;
		char *decomp = malloc(decomp_size);
		if (!decomp) {
			fprintf(stderr, "Failed to allocate decompression output\n");
			goto cleanup_block;
		}
		int zerr = uncompress((Bytef*)decomp, &decomp_size,
			(Bytef*)block, len);
		if (zerr != Z_OK) {
			fprintf(stderr, "zlib error %d\n", zerr);
			free(decomp);
			goto cleanup_block;
		}
		free(block);
		block = decomp;
		olen = decomp_size;
	}
	
	struct squashfs_base_inode *base;
	if (root_offset + sizeof(*base) > olen) {
		fprintf(stderr, "FIXME: Need multiple blocks for inode\n");
		err = 0;
		goto cleanup_block;
	}
	base = (struct squashfs_base_inode*)(block + root_offset);
	sqfs_swapin_base_inode(base);
	
	time_t mtime = base->mtime;
	printf("Root mtime: %s", ctime(&mtime));
	
	err = 0;
cleanup_block:
	free(block);
cleanup:
	close(fd);
	return err;
}

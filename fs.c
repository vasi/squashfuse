#include "squashfuse.h"

#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include <zlib.h>

sqfs_err sqfs_init(struct sqfs *fs, int fd) {
	fs->fd = fd;
	if (pread(fd, &fs->sb, sizeof(fs->sb), 0) != sizeof(fs->sb))
		return SQFS_ERR;
	sqfs_swapin_super_block(&fs->sb);
	
	if (fs->sb.s_magic != SQUASHFS_MAGIC)
		return SQFS_FORMAT;
	if (fs->sb.s_major != SQUASHFS_MAJOR || fs->sb.s_minor > SQUASHFS_MINOR)
		return SQFS_ERR;
	if (fs->sb.compression != ZLIB_COMPRESSION) // FIXME
		return SQFS_ERR;
	
	return SQFS_OK;
}

sqfs_err sqfs_read_md_block(struct sqfs *fs, off_t pos,
		struct sqfs_block *block) {
	block->data = NULL;
	
	uint16_t hdr;
	if (pread(fs->fd, &hdr, sizeof(hdr), pos) != sizeof(hdr))
		return SQFS_ERR;
	pos += sizeof(hdr);
	hdr = sqfs_swapin16(hdr);
	
	bool compressed = !(hdr & SQUASHFS_COMPRESSED_BIT);
	size_t len = hdr & ~SQUASHFS_COMPRESSED_BIT;
	if (!len)
		len = SQUASHFS_COMPRESSED_BIT;
	
	if (!(block->data = malloc(len)))
		return SQFS_ERR;
	if (pread(fs->fd, block->data, len, pos) != len)
		goto err;
	
	if (compressed) {
		size_t osize = SQUASHFS_METADATA_SIZE;
		char *decomp = malloc(osize);
		if (!decomp)
			goto err;
		
		int zerr = uncompress((Bytef*)decomp, &osize,
			(Bytef*)block->data, len);
		if (zerr != Z_OK) {
			free(decomp);
			goto err;
		}
		free(block->data);
		block->data = decomp;
		block->size = osize;
	} else {
		block->size = len;
	}
	
	return SQFS_OK;

err:
	free(block->data);
	block->data = NULL;
	return SQFS_ERR;
}

void sqfs_dispose_block(struct sqfs_block *block) {
	free(block->data);
}

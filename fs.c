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
#include "squashfuse.h"

#include "nonstd.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define DATA_CACHED_BLKS 1
#define FRAG_CACHED_BLKS 3

sqfs_err sqfs_init(sqfs *fs, int fd) {
	memset(fs, 0, sizeof(*fs));
	
	fs->fd = fd;
	if (sqfs_pread(fd, &fs->sb, sizeof(fs->sb), 0) != sizeof(fs->sb))
		return SQFS_ERR;
	sqfs_swapin_super_block(&fs->sb);
	
	if (fs->sb.s_magic != SQUASHFS_MAGIC)
		return SQFS_FORMAT;
	if (fs->sb.s_major != SQUASHFS_MAJOR || fs->sb.s_minor > SQUASHFS_MINOR)
		return SQFS_ERR;
	
	if (!(fs->decompressor = sqfs_decompressor_get(fs->sb.compression)))
		return SQFS_ERR;
	
	sqfs_err err = sqfs_table_init(&fs->id_table, fd, fs->sb.id_table_start,
		sizeof(uint32_t), fs->sb.no_ids);
	err |= sqfs_table_init(&fs->frag_table, fd, fs->sb.fragment_table_start,
		sizeof(struct squashfs_fragment_entry), fs->sb.fragments);
	err |= sqfs_xattr_init(fs);
	err |= sqfs_cache_init(&fs->md_cache, SQUASHFS_CACHED_BLKS);
	err |= sqfs_cache_init(&fs->data_cache, DATA_CACHED_BLKS);
	err |= sqfs_cache_init(&fs->frag_cache, FRAG_CACHED_BLKS);
	if (err) {
		sqfs_destroy(fs);
		return SQFS_ERR;
	}
	
	return SQFS_OK;
}

void sqfs_destroy(sqfs *fs) {
	sqfs_table_destroy(&fs->id_table);
	sqfs_table_destroy(&fs->frag_table);
	sqfs_cache_destroy(&fs->md_cache);
	sqfs_cache_destroy(&fs->data_cache);
	sqfs_cache_destroy(&fs->frag_cache);
}

void sqfs_md_header(uint16_t hdr, bool *compressed, uint16_t *size) {
	*compressed = !(hdr & SQUASHFS_COMPRESSED_BIT);
	*size = hdr & ~SQUASHFS_COMPRESSED_BIT;
	if (!*size)
		*size = SQUASHFS_COMPRESSED_BIT;
}

void sqfs_data_header(uint32_t hdr, bool *compressed, uint32_t *size) {
	*compressed = !(hdr & SQUASHFS_COMPRESSED_BIT_BLOCK);
	*size = hdr & ~SQUASHFS_COMPRESSED_BIT_BLOCK;
}

sqfs_err sqfs_block_read(sqfs *fs, off_t pos, bool compressed,
		uint32_t size, size_t outsize, sqfs_block **block) {
	sqfs_err err = SQFS_ERR;
	if (!(*block = malloc(sizeof(**block))))
		return SQFS_ERR;
	if (!((*block)->data = malloc(size)))
		goto error;
	
	if (sqfs_pread(fs->fd, (*block)->data, size, pos) != size)
		goto error;

	if (compressed) {
		char *decomp = malloc(outsize);
		if (!decomp)
			goto error;
		
		err = fs->decompressor((*block)->data, size, decomp, &outsize);
		if (err) {
			free(decomp);
			goto error;
		}
		free((*block)->data);
		(*block)->data = decomp;
		(*block)->size = outsize;
	} else {
		(*block)->size = size;
	}

	return SQFS_OK;

error:
	sqfs_block_dispose(*block);
	*block = NULL;
	return err;
}

sqfs_err sqfs_md_block_read(sqfs *fs, off_t pos, size_t *data_size,
		sqfs_block **block) {
	*data_size = 0;
	
	uint16_t hdr;
	if (sqfs_pread(fs->fd, &hdr, sizeof(hdr), pos) != sizeof(hdr))
		return SQFS_ERR;
	pos += sizeof(hdr);
	*data_size += sizeof(hdr);
	sqfs_swapin16(&hdr);
	
	bool compressed;
	uint16_t size;
	sqfs_md_header(hdr, &compressed, &size);
	
	sqfs_err err = sqfs_block_read(fs, pos, compressed, size,
		SQUASHFS_METADATA_SIZE, block);
	*data_size += size;
	return err;
}

sqfs_err sqfs_data_block_read(sqfs *fs, off_t pos, uint32_t hdr,
		sqfs_block **block) {
	bool compressed;
	uint32_t size;
	sqfs_data_header(hdr, &compressed, &size);
	return sqfs_block_read(fs, pos, compressed, size,
		fs->sb.block_size, block);
}

sqfs_err sqfs_md_cache(sqfs *fs, off_t *pos, sqfs_block **block) {
	size_t data_size;
	*block = sqfs_cache_get(&fs->md_cache, *pos, &data_size);
	if (!*block) {
		//fprintf(stderr, "MD BLOCK: %12llx\n", *pos);
		sqfs_err err = sqfs_md_block_read(fs, *pos, &data_size, block);
		if (err)
			return err;
		sqfs_cache_set(&fs->md_cache, *pos, *block, data_size);
	}
	*pos += data_size;
	return SQFS_OK;
}

sqfs_err sqfs_data_cache(sqfs *fs, sqfs_block_cache *cache, off_t pos,
		uint32_t hdr, sqfs_block **block) {
	*block = sqfs_cache_get(cache, pos, NULL);
	if (!*block) {
		sqfs_err err = sqfs_data_block_read(fs, pos, hdr, block);
		if (err)
			return err;
		sqfs_cache_set(cache, pos, *block, 0);
	}
	return SQFS_OK;
}

void sqfs_block_dispose(sqfs_block *block) {
	free(block->data);
	free(block);
}

void sqfs_md_cursor_inode(sqfs_md_cursor *cur, sqfs_inode_id id, off_t base) {
	cur->block = (id >> 16) + base;
	cur->offset = id & 0xffff;
}

sqfs_err sqfs_md_read(sqfs *fs, sqfs_md_cursor *cur, void *buf, size_t size) {
	off_t pos = cur->block;
	while (size > 0) {
		sqfs_block *block;
		sqfs_err err = sqfs_md_cache(fs, &pos, &block);
		if (err)
			return err;
		
		size_t take = block->size - cur->offset;
		if (take > size)
			take = size;		
		if (buf)
			memcpy(buf, (char*)block->data + cur->offset, take);
		// BLOCK CACHED, DON'T DISPOSE
		
		if (buf)
			buf = (char*)buf + take;
		size -= take;
		if (size) {
			cur->block = pos;
			cur->offset = 0;
		} else {
			cur->offset += take;
		}		
	}
	return SQFS_OK;
}

size_t sqfs_divceil(size_t total, size_t group) {
	size_t q = total / group;
	if (total % group)
		q += 1;
	return q;
}

sqfs_err sqfs_id_get(sqfs *fs, uint16_t idx, uid_t *id) {
	uint32_t rid;
	sqfs_err err = sqfs_table_get(&fs->id_table, fs, idx, &rid);
	if (err)
		return err;
	sqfs_swapin32(&rid);
	*id = (uid_t)rid;
	return SQFS_OK;
}

sqfs_err sqfs_readlink(sqfs *fs, sqfs_inode *inode, char *buf) {
	if (!S_ISLNK(sqfs_mode(inode->base.inode_type)))
		return SQFS_ERR;
	sqfs_md_cursor cur = inode->next;
	sqfs_err err = sqfs_md_read(fs, &cur, buf, inode->xtra.symlink_size);
	buf[inode->xtra.symlink_size] = '\0';
	return err;
}

// Turn the internal format of a device number to our system's dev_t
// It looks like rdev is just what the Linux kernel uses: 20 bit minor,
// split in two around a 12 bit major
static dev_t sqfs_decode_dev(uint32_t rdev) {
	return sqfs_makedev((rdev >> 8) & 0xfff,
		(rdev & 0xff) | ((rdev >> 12) & 0xfff00));
}

#define INODE_TYPE(_type) \
	struct squashfs_##_type##_inode x; \
	err = sqfs_md_read(fs, &inode->next, &x, sizeof(x)); \
	if (err) return err; \
	sqfs_swapin_##_type##_inode(&x)

sqfs_err sqfs_inode_get(sqfs *fs, sqfs_inode *inode, sqfs_inode_id id) {
	memset(inode, 0, sizeof(*inode));
	inode->xattr = SQUASHFS_INVALID_XATTR;
	
	sqfs_md_cursor cur;
	sqfs_md_cursor_inode(&cur, id, fs->sb.inode_table_start);
	inode->next = cur;
	
	sqfs_err err = sqfs_md_read(fs, &cur, &inode->base, sizeof(inode->base));
	if (err)
		return err;
	sqfs_swapin_base_inode(&inode->base);
	
	inode->base.mode |= sqfs_mode(inode->base.inode_type);
	switch (inode->base.inode_type) {
		case SQUASHFS_REG_TYPE: {
			INODE_TYPE(reg);
			inode->nlink = 1;
			inode->xtra.reg.start_block = x.start_block;
			inode->xtra.reg.file_size = x.file_size;
			inode->xtra.reg.frag_idx = x.fragment;
			inode->xtra.reg.frag_off = x.offset;
			break;
		}
		case SQUASHFS_LREG_TYPE: {
			INODE_TYPE(lreg);
			inode->nlink = x.nlink;
			inode->xtra.reg.start_block = x.start_block;
			inode->xtra.reg.file_size = x.file_size;
			inode->xtra.reg.frag_idx = x.fragment;
			inode->xtra.reg.frag_off = x.offset;
			inode->xattr = x.xattr;
			break;
		}
		case SQUASHFS_DIR_TYPE: {
			INODE_TYPE(dir);
			inode->nlink = x.nlink;
			inode->xtra.dir.start_block = x.start_block;
			inode->xtra.dir.offset = x.offset;
			inode->xtra.dir.dir_size = x.file_size;
			inode->xtra.dir.idx_count = 0;
			inode->xtra.dir.parent_inode = x.parent_inode;
			break;
		}
		case SQUASHFS_LDIR_TYPE: {
			INODE_TYPE(ldir);
			inode->nlink = x.nlink;
			inode->xtra.dir.start_block = x.start_block;
			inode->xtra.dir.offset = x.offset;
			inode->xtra.dir.dir_size = x.file_size;
			inode->xtra.dir.idx_count = x.i_count;
			inode->xtra.dir.parent_inode = x.parent_inode;
			inode->xattr = x.xattr;
			break;
		}
		case SQUASHFS_SYMLINK_TYPE:
		case SQUASHFS_LSYMLINK_TYPE: {
			INODE_TYPE(symlink);
			inode->nlink = x.nlink;
			inode->xtra.symlink_size = x.symlink_size;
			
			if (inode->base.inode_type == SQUASHFS_LSYMLINK_TYPE) {
				// skip symlink target
				cur = inode->next;
				err = sqfs_md_read(fs, &cur, NULL, inode->xtra.symlink_size);
				if (err)
					return err;
				err = sqfs_md_read(fs, &cur, &inode->xattr, sizeof(inode->xattr));
				if (err)
					return err;
				sqfs_swapin32(&inode->xattr);
			}
			break;
		}
		case SQUASHFS_BLKDEV_TYPE:
		case SQUASHFS_CHRDEV_TYPE: {
			INODE_TYPE(dev);
			inode->nlink = x.nlink;
			inode->xtra.dev = sqfs_decode_dev(x.rdev);
			break;
		}
		case SQUASHFS_LBLKDEV_TYPE:
		case SQUASHFS_LCHRDEV_TYPE: {
			INODE_TYPE(ldev);
			inode->nlink = x.nlink;
			inode->xtra.dev = sqfs_decode_dev(x.rdev);
			inode->xattr = x.xattr;
			break;
		}
		case SQUASHFS_SOCKET_TYPE:
		case SQUASHFS_FIFO_TYPE: {
			INODE_TYPE(ipc);
			inode->nlink = x.nlink;
			break;
		}
		case SQUASHFS_LSOCKET_TYPE:
		case SQUASHFS_LFIFO_TYPE: {
			INODE_TYPE(lipc);
			inode->nlink = x.nlink;
			inode->xattr = x.xattr;
			break;
		}
		
		default: return SQFS_ERR;
	}
	
	return SQFS_OK;
}
#undef INODE_TYPE

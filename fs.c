#include "squashfuse.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <zlib.h>

sqfs_err sqfs_init(sqfs *fs, int fd) {
	memset(fs, 0, sizeof(*fs));
	
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
	
	sqfs_err err = sqfs_table_init(&fs->id_table, fd, fs->sb.id_table_start,
		sizeof(uint32_t), fs->sb.no_ids);
	if (err)
		return err;
	
	err = sqfs_table_init(&fs->frag_table, fd, fs->sb.fragment_table_start,
		sizeof(struct squashfs_fragment_entry), fs->sb.fragments);
	if (err)
		return err;
	
	return SQFS_OK;
}

void sqfs_destroy(sqfs *fs) {
	sqfs_table_destroy(&fs->id_table);
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
	if (!(*block = malloc(sizeof(**block))))
		return SQFS_ERR;
	if (!((*block)->data = malloc(size)))
		goto err;
	
	if (pread(fs->fd, (*block)->data, size, pos) != size)
		goto err;

	if (compressed) {
		char *decomp = malloc(outsize);
		if (!decomp)
			goto err;

		int zerr = uncompress((Bytef*)decomp, &outsize,
			(Bytef*)(*block)->data, size);
		if (zerr != Z_OK) {
			free(decomp);
			goto err;
		}
		free((*block)->data);
		(*block)->data = decomp;
		(*block)->size = outsize;
	} else {
		(*block)->size = size;
	}

	return SQFS_OK;

err:
	sqfs_block_dispose(*block);
	*block = NULL;
	return SQFS_ERR;
}

sqfs_err sqfs_md_block_read(sqfs *fs, off_t *pos, sqfs_block **block) {
	uint16_t hdr;
	if (pread(fs->fd, &hdr, sizeof(hdr), *pos) != sizeof(hdr))
		return SQFS_ERR;
	*pos += sizeof(hdr);
	sqfs_swapin16(&hdr);
	
	bool compressed;
	uint16_t size;
	sqfs_md_header(hdr, &compressed, &size);
	
	sqfs_err err = sqfs_block_read(fs, *pos, compressed, size,
		SQUASHFS_METADATA_SIZE, block);
	*pos += size;
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
		sqfs_err err = sqfs_md_block_read(fs, &pos, &block);
		if (err)
			return err;
		
		size_t take = block->size - cur->offset;
		if (take > size)
			take = size;		
		if (buf)
			memcpy(buf, (char*)block->data + cur->offset, take);
		sqfs_block_dispose(block);
		
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

mode_t sqfs_mode(int inode_type) {
	switch (inode_type) {
		case SQUASHFS_DIR_TYPE:
		case SQUASHFS_LDIR_TYPE:
			return S_IFDIR;
		case SQUASHFS_REG_TYPE:
		case SQUASHFS_LREG_TYPE:
			return S_IFREG;
		case SQUASHFS_SYMLINK_TYPE:
		case SQUASHFS_LSYMLINK_TYPE:
			return S_IFLNK;
		case SQUASHFS_BLKDEV_TYPE:
		case SQUASHFS_LBLKDEV_TYPE:
			return S_IFBLK;
		case SQUASHFS_CHRDEV_TYPE:
		case SQUASHFS_LCHRDEV_TYPE:
			return S_IFCHR;
		case SQUASHFS_FIFO_TYPE:
		case SQUASHFS_LFIFO_TYPE:
			return S_IFIFO;
		case SQUASHFS_SOCKET_TYPE:
		case SQUASHFS_LSOCKET_TYPE:
			return S_IFSOCK;
	}
	return 0;
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
	return makedev((rdev >> 8) & 0xfff,
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
			// FIXME: sparse ok?
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
		
		// FIXME: more types
		default: return SQFS_ERR;
	}
	// FIXME: xattr
	
	return SQFS_OK;
}
#undef INODE_TYPE

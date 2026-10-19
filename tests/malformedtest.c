/* Regression tests for malformed on-disk offsets and block geometry. */
#include "fs.h"
#include "file.h"
#include "swap.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define INODE_TABLE_START 96
#define FRAGMENT_DATA_START 384
#define FRAGMENT_METADATA_START 400
#define FRAGMENT_TABLE_START 512

static int write_at(FILE *file, long pos, const void *data, size_t size) {
	return fseek(file, pos, SEEK_SET) == 0 &&
		fwrite(data, 1, size, file) == size;
}

static FILE *make_image(uint32_t block_size, uint16_t block_log) {
	FILE *file = tmpfile();
	struct squashfs_super_block sb;
	struct squashfs_reg_inode inode;
	struct squashfs_fragment_entry frag;
	uint16_t header;
	uint64_t fragment_metadata = FRAGMENT_METADATA_START;
	static const char data[] = "abcdefgh";

	if (!file)
		return NULL;
	memset(&sb, 0, sizeof(sb));
	sb.s_magic = SQUASHFS_MAGIC;
	sb.s_major = SQUASHFS_MAJOR;
	sb.s_minor = SQUASHFS_MINOR;
	sb.compression = ZLIB_COMPRESSION;
	sb.block_size = block_size;
	sb.block_log = block_log;
	sb.fragments = 1;
	sb.root_inode = 0;
	sb.inode_table_start = INODE_TABLE_START;
	sb.fragment_table_start = FRAGMENT_TABLE_START;
	sb.lookup_table_start = SQUASHFS_INVALID_BLK;
	sb.xattr_id_table_start = SQUASHFS_INVALID_BLK;
	sqfs_swapin_super_block(&sb);
	if (!write_at(file, 0, &sb, sizeof(sb)))
		goto error;

	memset(&inode, 0, sizeof(inode));
	inode.inode_type = SQUASHFS_REG_TYPE;
	inode.mode = 0644;
	inode.fragment = 0;
	inode.file_size = 4;
	sqfs_swapin_reg_inode(&inode);
	header = SQUASHFS_COMPRESSED_BIT | sizeof(inode);
	sqfs_swapin16(&header);
	if (!write_at(file, INODE_TABLE_START, &header, sizeof(header)) ||
		!write_at(file, INODE_TABLE_START + sizeof(header), &inode, sizeof(inode)) ||
		!write_at(file, FRAGMENT_DATA_START, data, sizeof(data) - 1))
		goto error;

	memset(&frag, 0, sizeof(frag));
	frag.start_block = FRAGMENT_DATA_START;
	frag.size = SQUASHFS_COMPRESSED_BIT_BLOCK | (sizeof(data) - 1);
	sqfs_swapin_fragment_entry(&frag);
	header = SQUASHFS_COMPRESSED_BIT | sizeof(frag);
	sqfs_swapin16(&header);
	sqfs_swapin64(&fragment_metadata);
	if (!write_at(file, FRAGMENT_METADATA_START, &header, sizeof(header)) ||
		!write_at(file, FRAGMENT_METADATA_START + sizeof(header), &frag, sizeof(frag)) ||
		!write_at(file, FRAGMENT_TABLE_START, &fragment_metadata,
			sizeof(fragment_metadata)) || fflush(file) != 0)
		goto error;
	return file;

error:
	fclose(file);
	return NULL;
}

static int test_block_size(uint32_t size, uint16_t log, sqfs_err expected) {
	FILE *file = make_image(size, log);
	sqfs fs;
	sqfs_err err;
	if (!file)
		return 0;
	err = sqfs_init(&fs, fileno(file), 0);
	if (err == SQFS_OK)
		sqfs_destroy(&fs);
	fclose(file);
	return err == expected;
}

static int test_metadata_offset(void) {
	FILE *file = make_image(SQUASHFS_FILE_SIZE, SQUASHFS_FILE_LOG);
	sqfs fs;
	sqfs_md_cursor cur;
	struct squashfs_base_inode base;
	int ok = 0;
	if (!file)
		return 0;
	if (sqfs_init(&fs, fileno(file), 0) != SQFS_OK)
		goto done;
	sqfs_md_cursor_inode(&cur, 0, fs.sb.inode_table_start);
	if (sqfs_md_read(&fs, &cur, &base, sizeof(base)) != SQFS_OK)
		goto destroy;
	sqfs_md_cursor_inode(&cur, sizeof(struct squashfs_reg_inode),
		fs.sb.inode_table_start);
	if (sqfs_md_read(&fs, &cur, &base, sizeof(base)) != SQFS_ERR)
		goto destroy;
	sqfs_md_cursor_inode(&cur, sizeof(struct squashfs_reg_inode) + 1,
		fs.sb.inode_table_start);
	if (sqfs_md_read(&fs, &cur, &base, sizeof(base)) != SQFS_ERR)
		goto destroy;
	sqfs_md_cursor_inode(&cur, UINT16_MAX, fs.sb.inode_table_start);
	ok = sqfs_md_read(&fs, &cur, &base, sizeof(base)) == SQFS_ERR;
destroy:
	sqfs_destroy(&fs);
done:
	fclose(file);
	return ok;
}

static int test_fragment_offset(void) {
	FILE *file = make_image(SQUASHFS_FILE_SIZE, SQUASHFS_FILE_LOG);
	sqfs fs;
	sqfs_inode inode;
	sqfs_off_t size;
	char buf[8];
	int ok = 0;
	if (!file)
		return 0;
	if (sqfs_init(&fs, fileno(file), 0) != SQFS_OK)
		goto done;
	if (sqfs_inode_get(&fs, &inode, sqfs_inode_root(&fs)) != SQFS_OK)
		goto destroy;
	inode.xtra.reg.frag_off = 2;
	size = 4;
	if (sqfs_read_range(&fs, &inode, 0, &size, buf) != SQFS_OK ||
		size != 4 || memcmp(buf, "cdef", 4) != 0)
		goto destroy;
	inode.xtra.reg.frag_off = 4;
	size = 4;
	if (sqfs_read_range(&fs, &inode, 0, &size, buf) != SQFS_OK ||
		size != 4 || memcmp(buf, "efgh", 4) != 0)
		goto destroy;
	inode.xtra.reg.frag_off = 5;
	memset(buf, 0x55, sizeof(buf));
	size = 4;
	if (sqfs_read_range(&fs, &inode, 0, &size, buf) != SQFS_ERR ||
		buf[0] != 0x55)
		goto destroy;
	inode.xtra.reg.frag_off = UINT32_MAX;
	size = 4;
	ok = sqfs_read_range(&fs, &inode, 0, &size, buf) == SQFS_ERR;
destroy:
	sqfs_destroy(&fs);
done:
	fclose(file);
	return ok;
}

int main(void) {
	if (!test_block_size(SQUASHFS_FILE_SIZE, SQUASHFS_FILE_LOG, SQFS_OK) ||
		!test_block_size(SQUASHFS_FILE_MAX_SIZE, SQUASHFS_FILE_MAX_LOG, SQFS_OK) ||
		!test_block_size(0, SQUASHFS_FILE_LOG, SQFS_BADFORMAT) ||
		!test_block_size(3, SQUASHFS_FILE_LOG, SQFS_BADFORMAT) ||
		!test_block_size(SQUASHFS_FILE_SIZE, 16, SQFS_BADFORMAT) ||
		!test_block_size(SQUASHFS_FILE_SIZE, UINT16_MAX, SQFS_BADFORMAT) ||
		!test_block_size(SQUASHFS_FILE_MAX_SIZE * 2U, 21, SQFS_BADFORMAT) ||
		!test_metadata_offset() || !test_fragment_offset()) {
		fprintf(stderr, "malformed image regression test failed\n");
		return 1;
	}
	return 0;
}

#include "dir.h"

#include <string.h>
#include <sys/stat.h>

#include "squashfuse.h"

static sqfs_err sqfs_dir_md_read(sqfs_dir *dir, void *buf, size_t size) {
	dir->remain -= size;
	return sqfs_md_read(dir->fs, &dir->cur, buf, size);
}

sqfs_err sqfs_opendir(sqfs *fs, sqfs_inode *inode, sqfs_dir *dir) {
	if (!S_ISDIR(inode->base.mode))
		return SQFS_ERR;
	
	memset(dir, 0, sizeof(*dir));
	dir->fs = fs;
	dir->cur.block = inode->xtra.dir.start_block +
		fs->sb.directory_table_start;
	dir->cur.offset = inode->xtra.dir.offset;
	dir->remain = inode->xtra.dir.dir_size - 3;
	dir->entry.name = dir->name;
	
	return SQFS_OK;
}

sqfs_dir_entry *sqfs_readdir(sqfs_dir *dir, sqfs_err *err) {
	while (dir->remain && dir->header.count == 0) {
		if ((*err = sqfs_dir_md_read(dir, &dir->header, sizeof(dir->header))))
			return NULL;
		sqfs_swapin_dir_header(&dir->header);
		++(dir->header.count);
	}
	if (!dir->remain) {
		*err = SQFS_OK;
		return NULL;
	}
	
	struct squashfs_dir_entry entry;
	if ((*err = sqfs_dir_md_read(dir, &entry, sizeof(entry))))
		return NULL;
	sqfs_swapin_dir_entry(&entry);
	--(dir->header.count);
	
	sqfs_dir_md_read(dir, &dir->name, entry.size + 1);
	dir->name[entry.size + 1] = '\0';
	
	dir->entry.inode = ((uint64_t)dir->header.start_block << 16) +
		entry.offset;
	dir->entry.inode_number = dir->header.inode_number + entry.inode_number;
	dir->entry.type = entry.type;
	
	*err = SQFS_OK;
	return &dir->entry;
}

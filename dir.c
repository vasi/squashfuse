#include "dir.h"

#include <string.h>
#include <sys/param.h>
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
	while (dir->header.count == 0) {
		if (dir->remain <= 0) {
			*err = SQFS_OK;
			return NULL;
		}
		
		if ((*err = sqfs_dir_md_read(dir, &dir->header, sizeof(dir->header))))
			return NULL;
		sqfs_swapin_dir_header(&dir->header);
		++(dir->header.count);
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
	// entry.inode_number is signed
	dir->entry.inode_number = dir->header.inode_number + (int16_t)entry.inode_number;
	dir->entry.type = entry.type;
	
	*err = SQFS_OK;
	return &dir->entry;
}

sqfs_err sqfs_lookup_dir(sqfs_dir *dir, const char *name,
		sqfs_dir_entry *entry) {
	sqfs_err err;
	sqfs_dir_entry *dentry;
	while ((dentry = sqfs_readdir(dir, &err))) {
		if (strcmp(dentry->name, name) == 0) {
			*entry = *dentry;
			entry->name = NULL;
			return SQFS_OK;
		}
	}
	return SQFS_ERR;
}

sqfs_err sqfs_lookup_path(sqfs *fs, sqfs_inode *inode, char *path) {
	char buf[MAXPATHLEN + 1];
	strncpy(buf, path, sizeof(buf) - 1);
	path = buf;
	
	sqfs_dir dir;
	sqfs_dir_entry entry;
	while (*path) {
		sqfs_err err = sqfs_opendir(fs, inode, &dir);
		if (err)
			return err;
		
		// Find next path component
		char *name = path;
		while (*path && *path != '/')
			++path;
		if (*path == '/')
			*path++ = '\0';
		
		err = sqfs_lookup_dir(&dir, name, &entry);
		if (err)
			return err;
		
		if ((err = sqfs_inode_get(fs, inode, entry.inode)))
			return err;
	}
	return SQFS_OK;
}

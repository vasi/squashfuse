#include "file.h"

#include <sys/stat.h>

#include "squashfuse.h"

sqfs_err sqfs_frag_entry(sqfs *fs, struct squashfs_fragment_entry *frag,
		uint32_t idx) {
	if (idx == SQUASHFS_INVALID_FRAG)
		return SQFS_ERR;
	
	sqfs_err err = sqfs_table_get(&fs->frag_table, fs, idx, frag);
	sqfs_swapin_fragment_entry(frag);
	return err;
}

sqfs_err sqfs_frag_block(sqfs *fs, sqfs_inode *inode,
		size_t *offset, size_t *size, sqfs_block **block) {
	if (!S_ISREG(inode->base.mode))
		return SQFS_ERR;
	
	struct squashfs_fragment_entry frag;
	sqfs_err err = sqfs_frag_entry(fs, &frag, inode->xtra.reg.frag_idx);
	if (err)
		return err;
	
	err = sqfs_data_block_read(fs, frag.start_block, frag.size, block);
	if (err)
		return SQFS_ERR;
	
	*offset = inode->xtra.reg.frag_off;
	*size = inode->xtra.reg.file_size % fs->sb.block_size;
	return SQFS_OK;
}

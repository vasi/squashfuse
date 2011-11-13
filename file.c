#include "file.h"

#include "squashfuse.h"

sqfs_err sqfs_frag_entry(sqfs *fs, struct squashfs_fragment_entry *frag,
		uint32_t idx) {
	if (idx == SQUASHFS_INVALID_FRAG)
		return SQFS_ERR;
	
	sqfs_err err = sqfs_table_get(&fs->frag_table, fs, idx, frag);
	sqfs_swapin_fragment_entry(frag);
	return err;
}

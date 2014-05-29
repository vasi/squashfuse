void sqfs_swapin_super_block(struct squashfs_super_block *s){
sqfs_swapin32(&s->s_magic);
sqfs_swapin32(&s->inodes);
sqfs_swapin32(&s->mkfs_time);
sqfs_swapin32(&s->block_size);
sqfs_swapin32(&s->fragments);
sqfs_swapin16(&s->compression);
sqfs_swapin16(&s->block_log);
sqfs_swapin16(&s->flags);
sqfs_swapin16(&s->no_ids);
sqfs_swapin16(&s->s_major);
sqfs_swapin16(&s->s_minor);
sqfs_swapin64(&s->root_inode);
sqfs_swapin64(&s->bytes_used);
sqfs_swapin64(&s->id_table_start);
sqfs_swapin64(&s->xattr_id_table_start);
sqfs_swapin64(&s->inode_table_start);
sqfs_swapin64(&s->directory_table_start);
sqfs_swapin64(&s->fragment_table_start);
sqfs_swapin64(&s->lookup_table_start);
}
void sqfs_swapin_dir_index(struct squashfs_dir_index *s){
sqfs_swapin32(&s->index);
sqfs_swapin32(&s->start_block);
sqfs_swapin32(&s->size);
}
void sqfs_swapin_base_inode(struct squashfs_base_inode *s){
sqfs_swapin16(&s->inode_type);
sqfs_swapin16(&s->mode);
sqfs_swapin16(&s->uid);
sqfs_swapin16(&s->guid);
sqfs_swapin32(&s->mtime);
sqfs_swapin32(&s->inode_number);
}
void sqfs_swapin_ipc_inode(struct squashfs_ipc_inode *s){
sqfs_swapin16(&s->inode_type);
sqfs_swapin16(&s->mode);
sqfs_swapin16(&s->uid);
sqfs_swapin16(&s->guid);
sqfs_swapin32(&s->mtime);
sqfs_swapin32(&s->inode_number);
sqfs_swapin32(&s->nlink);
}
void sqfs_swapin_lipc_inode(struct squashfs_lipc_inode *s){
sqfs_swapin16(&s->inode_type);
sqfs_swapin16(&s->mode);
sqfs_swapin16(&s->uid);
sqfs_swapin16(&s->guid);
sqfs_swapin32(&s->mtime);
sqfs_swapin32(&s->inode_number);
sqfs_swapin32(&s->nlink);
sqfs_swapin32(&s->xattr);
}
void sqfs_swapin_dev_inode(struct squashfs_dev_inode *s){
sqfs_swapin16(&s->inode_type);
sqfs_swapin16(&s->mode);
sqfs_swapin16(&s->uid);
sqfs_swapin16(&s->guid);
sqfs_swapin32(&s->mtime);
sqfs_swapin32(&s->inode_number);
sqfs_swapin32(&s->nlink);
sqfs_swapin32(&s->rdev);
}
void sqfs_swapin_ldev_inode(struct squashfs_ldev_inode *s){
sqfs_swapin16(&s->inode_type);
sqfs_swapin16(&s->mode);
sqfs_swapin16(&s->uid);
sqfs_swapin16(&s->guid);
sqfs_swapin32(&s->mtime);
sqfs_swapin32(&s->inode_number);
sqfs_swapin32(&s->nlink);
sqfs_swapin32(&s->rdev);
sqfs_swapin32(&s->xattr);
}
void sqfs_swapin_symlink_inode(struct squashfs_symlink_inode *s){
sqfs_swapin16(&s->inode_type);
sqfs_swapin16(&s->mode);
sqfs_swapin16(&s->uid);
sqfs_swapin16(&s->guid);
sqfs_swapin32(&s->mtime);
sqfs_swapin32(&s->inode_number);
sqfs_swapin32(&s->nlink);
sqfs_swapin32(&s->symlink_size);
}
void sqfs_swapin_reg_inode(struct squashfs_reg_inode *s){
sqfs_swapin16(&s->inode_type);
sqfs_swapin16(&s->mode);
sqfs_swapin16(&s->uid);
sqfs_swapin16(&s->guid);
sqfs_swapin32(&s->mtime);
sqfs_swapin32(&s->inode_number);
sqfs_swapin32(&s->start_block);
sqfs_swapin32(&s->fragment);
sqfs_swapin32(&s->offset);
sqfs_swapin32(&s->file_size);
}
void sqfs_swapin_lreg_inode(struct squashfs_lreg_inode *s){
sqfs_swapin16(&s->inode_type);
sqfs_swapin16(&s->mode);
sqfs_swapin16(&s->uid);
sqfs_swapin16(&s->guid);
sqfs_swapin32(&s->mtime);
sqfs_swapin32(&s->inode_number);
sqfs_swapin64(&s->start_block);
sqfs_swapin64(&s->file_size);
sqfs_swapin64(&s->sparse);
sqfs_swapin32(&s->nlink);
sqfs_swapin32(&s->fragment);
sqfs_swapin32(&s->offset);
sqfs_swapin32(&s->xattr);
}
void sqfs_swapin_dir_inode(struct squashfs_dir_inode *s){
sqfs_swapin16(&s->inode_type);
sqfs_swapin16(&s->mode);
sqfs_swapin16(&s->uid);
sqfs_swapin16(&s->guid);
sqfs_swapin32(&s->mtime);
sqfs_swapin32(&s->inode_number);
sqfs_swapin32(&s->start_block);
sqfs_swapin32(&s->nlink);
sqfs_swapin16(&s->file_size);
sqfs_swapin16(&s->offset);
sqfs_swapin32(&s->parent_inode);
}
void sqfs_swapin_ldir_inode(struct squashfs_ldir_inode *s){
sqfs_swapin16(&s->inode_type);
sqfs_swapin16(&s->mode);
sqfs_swapin16(&s->uid);
sqfs_swapin16(&s->guid);
sqfs_swapin32(&s->mtime);
sqfs_swapin32(&s->inode_number);
sqfs_swapin32(&s->nlink);
sqfs_swapin32(&s->file_size);
sqfs_swapin32(&s->start_block);
sqfs_swapin32(&s->parent_inode);
sqfs_swapin16(&s->i_count);
sqfs_swapin16(&s->offset);
sqfs_swapin32(&s->xattr);
}
void sqfs_swapin_dir_entry(struct squashfs_dir_entry *s){
sqfs_swapin16(&s->offset);
sqfs_swapin16(&s->inode_number);
sqfs_swapin16(&s->type);
sqfs_swapin16(&s->size);
}
void sqfs_swapin_dir_header(struct squashfs_dir_header *s){
sqfs_swapin32(&s->count);
sqfs_swapin32(&s->start_block);
sqfs_swapin32(&s->inode_number);
}
void sqfs_swapin_fragment_entry(struct squashfs_fragment_entry *s){
sqfs_swapin64(&s->start_block);
sqfs_swapin32(&s->size);
}
void sqfs_swapin_xattr_entry(struct squashfs_xattr_entry *s){
sqfs_swapin16(&s->type);
sqfs_swapin16(&s->size);
}
void sqfs_swapin_xattr_val(struct squashfs_xattr_val *s){
sqfs_swapin32(&s->vsize);
}
void sqfs_swapin_xattr_id(struct squashfs_xattr_id *s){
sqfs_swapin64(&s->xattr);
sqfs_swapin32(&s->count);
sqfs_swapin32(&s->size);
}
void sqfs_swapin_xattr_id_table(struct squashfs_xattr_id_table *s){
sqfs_swapin64(&s->xattr_table_start);
sqfs_swapin32(&s->xattr_ids);
sqfs_swapin32(&s->unused);
}

// TODO
// - internals
//		- caches
//		- threading
//		- swap

typedef enum {
	SQFS_NOERR,
	SQFS_ERR_FORMAT,
	...
} sqfs_err;

typedef enum {
	SQFS_COMPRESS_ZLIB,
	SQFS_COMPRESS_XZ,
	SQFS_COMPRESS_LZO,
} sqfs_compression;

typedef enum {
	SQFS_FLAG_COMPRESS_INODES = 1 << 0,
	SQFS_FLAG_COMPRESS_DIRECTORIES = 1 << 1,
	...
} sqfs_flag;

typedef enum {
	SQFS_INODE_REG,
	SQFS_INODE_DIR,
	...
} sqfs_inode_type;


typedef uint64_t sqfs_inode_id;
typedef uint32_t sqfs_inode_number;

typedef struct sqfs sqfs;
typedef struct sqfs_inode sqfs_inode;

typedef struct sqfs_dir sqfs_dir;
typedef struct sqfs_xattr sqfs_xattr;


void sqfs_version_current(uint16_t *major, uint16_t *minor);
bool sqfs_version_supported(uint16_t major, uint16_t minor);
char *sqfs_strerror(sqfs_err err);


sqfs_err sqfs_fs_new(sqfs **fs, int fd);
void sqfs_fs_free(sqfs *fs);

// These are ok to call even if ERR_VERSION or ERR_UNSUPPORTED_COMPRESSION
void sqfs_fs_version(sqfs *fs, uint16_t *major, uint16_t *minor);
sqfs_compression sqfs_fs_compression(sqfs *fs);

uint32_t sqfs_fs_creation_time(sqfs *fs);
uint32_t sqfs_fs_block_size(sqfs *fs);
bool sqfs_fs_flag(sqfs *fs, sqfs_flag flag);
sqfs_inode_id sqfs_fs_root_id(sqfs *fs);
sqfs_err sqfs_fs_root_inode(sqfs *fs, sqfs_inode **inode);
uint64_t sqfs_fs_size(sqfs *fs);

sqfs_err sqfs_fs_inode_number_to_id(sqfs *fs, sqfs_inode_number num,
	sqfs_inode_id *iid);


sqfs_err sqfs_inode_new(sqfs_inode **inode, sqfs *fs, sqfs_inode_id iid);
void sqfs_inode_free(sqfs_inode *inode);

sqfs_inode_id sqfs_inode_id_get(sqfs_inode *inode);
sqfs_inode_number sqfs_inode_number_get(sqfs_inode *inode);
sqfs_inode_type sqfs_inode_type_get(sqfs_inode *inode);
mode_t sqfs_inode_mode(sqfs_inode *inode);
sqfs_err sqfs_inode_uid(sqfs_inode *inode, uid_t *uid);
sqfs_err sqfs_inode_gid(sqfs_inode *inode, gid_t *gid);
uint32_t sqfs_inode_mtime(sqfs_inode *inode);
uint32_t sqfs_inode_nlink(sqfs_inode *inode);

sqfs_err sqfs_inode_file_size(sqfs_inode *inode, uint64_t *size);
sqfs_err sqfs_inode_file_read(sqfs_inode *inode, off_t start, off_t *size,
	void *buf);
sqfs_err sqfs_inode_dir_parent(sqfs_inode *inode, sqfs_inode_number *parent);
sqfs_err sqfs_inode_dir_size(sqfs_inode *inode, uint32_t *size);
sqfs_err sqfs_inode_dev_device(sqfs_inode *inode, dev_t *device);
sqfs_err sqfs_inode_link_readlink(sqfs_inode *inode, char **target);


sqfs_err sqfs_dir_new(sqfs_dir **dir, sqfs_inode *inode);
void sqfs_dir_free(sqfs_dir *dir);

sqfs_err sqfs_dir_next(sqfs_dir *dir);
sqfs_err sqfs_dir_find(sqfs_dir *dir, const char *name);

bool sqfs_dir_entry_valid(sqfs_dir *dir);
uint32_t sqfs_dir_entry_position(sqfs_dir *dir);
sqfs_err sqfs_dir_entry_id(sqfs_dir *dir, sqfs_inode_id *iid);
sqfs_err sqfs_dir_entry_number(sqfs_dir *dir, sqfs_inode_number *num);
sqfs_err sqfs_dir_entry_type(sqfs_dir *dir, sqfs_inode_type *type);
sqfs_err sqfs_dir_entry_name(sqfs_dir *dir, char **name);
sqfs_err sqfs_dir_entry_inode(sqfs_dir *dir, sqfs_inode **inode);


size_t sqfs_compression_supported(sqfs_compression **types);
char *sqfs_compression_name(sqfs_compression type);


sqfs_err sqfs_xattr_new(sqfs_xattr **xattr, sqfs_inode *inode);
void sqfs_xattr_free(sqfs_xattr *xattr);

sqfs_err sqfs_xattr_next(sqfs_xattr *xattr);
sqfs_err sqfs_xattr_find(sqfs_xattr *xattr, const char *name);

bool sqfs_xattr_entry_valid(sqfs_xattr *xattr);
sqfs_err sqfs_xattr_entry_name(sqfs_xattr *xattr, char **name);
sqfs_err sqfs_xattr_entry_value_size(sqfs_xattr *xattr, uint32_t *size);
sqfs_err sqfs_xattr_entry_value(sqfs_xattr *xattr, char **value);


// Only resolves simple paths (no symlinks, parent dirs)
sqfs_err sqfs_inode_path(sqfs_inode *cwd, const char *path,
	sqfs_inode **result);

sqfs_err sqfs_inode_stat(sqfs_inode *inode, struct stat *st);
sqfs_err sqfs_inode_extract(sqfs_inode *inode, const char *path);

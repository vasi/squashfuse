#include <dokan.h>
#include "squashfuse.h"

#include <shlwapi.h>

static sqfs g_fs;
static sqfs_inode g_root;
static wchar_t *g_root_name;

// Convert path from Win to squashfs internal
static std::string sqfs_dk_sqfs_path(LPCWSTR path) {
  // Convert to UTF8
  size_t size = WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0, NULL,
    NULL);
  char *buf = new char[size];
  WideCharToMultiByte(CP_UTF8, 0, path, -1, buf, size, NULL, NULL);
  std::string ret = buf;
  delete[] buf;

  // Convert separators
  for (auto it = ret.begin(); it != ret.end(); ++it) {
    if (*it == '\\')
      *it = '/';
  }

  return ret;
}

// Convert filename from squashfs to Win. Return empty on failure
// FIXME: Escape illegal characters?
static const char *ILLEGAL_CHARS = "<>:\"/\\|?*";
static std::wstring sqfs_dk_win_name(const char *name) {
  // Check for illegal characters
  const char *found = strpbrk(name, ILLEGAL_CHARS);
  if (found)
    return std::wstring();

  // Convert to unicode
  size_t size = MultiByteToWideChar(CP_UTF8, 0, name, -1, NULL, 0);
  wchar_t *buf = new wchar_t[size];
  MultiByteToWideChar(CP_UTF8, 0, name, -1, buf, size);
  std::wstring ret = buf;
  delete[] buf;
  return ret;
}

// Lookup a file
static NTSTATUS sqfs_dk_lookup(LPCWSTR path, sqfs_inode &inode) {
  std::string spath = sqfs_dk_sqfs_path(path);
  inode = g_root;
  bool found = false;
  sqfs_err err = sqfs_lookup_path(&g_fs, &inode, spath.c_str(), &found);
  if (err)
    return STATUS_INTERNAL_ERROR;
  if (!found)
    return STATUS_OBJECT_PATH_NOT_FOUND;
  return STATUS_SUCCESS;
}

// Generate a serial number for this fs
static DWORD sqfs_dk_serial_number(sqfs &fs) {
  return fs.sb.mkfs_time ^ fs.sb.inodes ^ (DWORD)fs.sb.inode_table_start;
}

// Convert a time_t to a FILETIME
static void sqfs_dk_filetime(time_t t, LPFILETIME pft) {
  LONGLONG ll = Int32x32To64(t, 10000000) + 116444736000000000;
  pft->dwLowDateTime = (DWORD)ll;
  pft->dwHighDateTime = ll >> 32;
}

// Fill a file information structure from an inode
template <typename T>
static void sqfs_dk_file_info(T *info, sqfs_inode &inode) {
  info->dwFileAttributes = 0;
  if (S_ISDIR(inode.base.mode))
    info->dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
  info->dwFileAttributes |= FILE_ATTRIBUTE_READONLY;
  sqfs_dk_filetime(inode.base.mtime, &info->ftLastWriteTime);
  info->ftCreationTime = info->ftLastWriteTime;
  info->ftLastAccessTime = info->ftLastWriteTime;
  if (S_ISREG(inode.base.mode)) {
    info->nFileSizeHigh = inode.xtra.reg.file_size >> 32;
    info->nFileSizeLow = (DWORD)inode.xtra.reg.file_size;
  }
}

static NTSTATUS sqfs_dk_op_create_file(LPCWSTR path, DWORD access,
    DWORD sharemode, DWORD create, DWORD flags, PDOKAN_FILE_INFO fi) {
  sqfs_inode *inode = new sqfs_inode();
  NTSTATUS nterr = sqfs_dk_lookup(path, *inode);
  if (nterr) {
    delete inode;
    return nterr;
  }

  // FIXME: Check if it's a regular file?

  fi->Context = (ULONG64)inode;
  return STATUS_SUCCESS;
}

static NTSTATUS sqfs_dk_op_open_directory(LPCWSTR path, PDOKAN_FILE_INFO fi) {
  sqfs_inode inode;
  NTSTATUS nterr = sqfs_dk_lookup(path, inode);
  if (nterr)
    return nterr;

  if (!S_ISDIR(inode.base.mode))
    return STATUS_NOT_A_DIRECTORY;
  return STATUS_SUCCESS;
}

static void sqfs_dk_op_close_file(LPCWSTR path, PDOKAN_FILE_INFO fi) {
  delete (sqfs_inode*)fi->Context;
}

static NTSTATUS sqfs_dk_op_read_file(LPCWSTR path, LPVOID buf, DWORD bufsize,
    LPDWORD size, LONGLONG offset, PDOKAN_FILE_INFO fi) {
  sqfs_inode *inode = (sqfs_inode*)fi->Context;
  sqfs_off_t osize = bufsize;
  if (sqfs_read_range(&g_fs, inode, offset, &osize, buf))
    return STATUS_INTERNAL_ERROR;
  if (size)
    *size = (DWORD)osize;
  return STATUS_SUCCESS;
}

static NTSTATUS sqfs_dk_op_get_file_information(LPCWSTR path,
    LPBY_HANDLE_FILE_INFORMATION info, PDOKAN_FILE_INFO fi) {
  sqfs_inode inode;
  NTSTATUS nterr = sqfs_dk_lookup(path, inode);
  if (nterr)
    return nterr;

  ZeroMemory(info, sizeof(*info));
  sqfs_dk_file_info(info, inode);
  info->nNumberOfLinks = inode.nlink;
  info->nFileIndexLow = inode.base.inode_number;
  info->dwVolumeSerialNumber = sqfs_dk_serial_number(g_fs);
  return STATUS_SUCCESS;
}

static NTSTATUS sqfs_dk_op_find_files(LPCWSTR path, PFillFindData filler,
    PDOKAN_FILE_INFO fi) {
  sqfs_inode inode;
  NTSTATUS nterr = sqfs_dk_lookup(path, inode);
  if (nterr)
    return nterr;

  if (!S_ISDIR(inode.base.mode))
    return STATUS_NOT_A_DIRECTORY;

  sqfs_dir dir;
  if (sqfs_dir_open(&g_fs, &inode, &dir, 0))
    return STATUS_INTERNAL_ERROR;

  sqfs_dir_entry entry;
  sqfs_name namebuf;
  sqfs_dentry_init(&entry, namebuf);

  WIN32_FIND_DATAW find;
  find.dwReserved0 = 0;
  find.dwReserved1 = 0;
  find.cAlternateFileName[0] = L'\0';

  sqfs_err err;
  sqfs_inode child;
  while (sqfs_dir_next(&g_fs, &dir, &entry, &err)) {
    std::wstring name = sqfs_dk_win_name(sqfs_dentry_name(&entry));
    if (name.empty())
      continue; // Ignore illegal names

    if (sqfs_inode_get(&g_fs, &child, sqfs_dentry_inode(&entry)))
      return STATUS_INTERNAL_ERROR;

    sqfs_dk_file_info(&find, child);
    wcscpy_s(find.cFileName, name.c_str());

    filler(&find, fi);
  }
  return err ? STATUS_INTERNAL_ERROR : STATUS_SUCCESS;
}

static NTSTATUS sqfs_dk_op_get_volume_information(
    LPWSTR volname, DWORD volnamelen,
    LPDWORD serial, LPDWORD maxpath, LPDWORD flags,
    LPWSTR fsname, DWORD fsnamelen, PDOKAN_FILE_INFO fi) {
  wcscpy_s(volname, volnamelen / sizeof(WCHAR), g_root_name);
  *serial = sqfs_dk_serial_number(g_fs);
  *maxpath = SQUASHFS_NAME_LEN;
  *flags = FILE_CASE_SENSITIVE_SEARCH | FILE_CASE_PRESERVED_NAMES | 
    FILE_UNICODE_ON_DISK | FILE_PERSISTENT_ACLS | FILE_READ_ONLY_VOLUME;
  wcscpy_s(fsname, fsnamelen / sizeof(WCHAR), L"squashfuse");

  return STATUS_SUCCESS;
}

static NTSTATUS sqfs_dk_op_unmount(PDOKAN_FILE_INFO fi) {
  return STATUS_SUCCESS;
}

static void die(const char *msg) {
  fprintf(stderr, "%s\n", msg);
  exit(EXIT_FAILURE);
}

static void usage() {
  // FIXME!
  die("usage!");
}

int wmain(int argc, wchar_t *argv[]) {
  DOKAN_OPERATIONS dokan_ops;
  ZeroMemory(&dokan_ops, sizeof(dokan_ops));
  dokan_ops.CreateFile = sqfs_dk_op_create_file;
  dokan_ops.OpenDirectory = sqfs_dk_op_open_directory;
  dokan_ops.CloseFile = sqfs_dk_op_close_file;
  dokan_ops.ReadFile = sqfs_dk_op_read_file;
  dokan_ops.GetFileInformation = sqfs_dk_op_get_file_information;
  dokan_ops.FindFiles = sqfs_dk_op_find_files;
  dokan_ops.GetVolumeInformation = sqfs_dk_op_get_volume_information;
  dokan_ops.Unmount = sqfs_dk_op_unmount;

  DOKAN_OPTIONS opts;
  ZeroMemory(&opts, sizeof(opts));
  opts.Version = DOKAN_VERSION;
  opts.ThreadCount = 0; // default
  opts.Options = DOKAN_OPTION_KEEP_ALIVE;

  // Parse arguments
  sqfs_host_path image = NULL;
  for (int i = 1; i < argc; ++i) {
    wchar_t *arg = argv[i];
    if (arg[0] == L'/') {
      switch (arg[1]) {
        case L'd':
          opts.Options |= DOKAN_OPTION_DEBUG;
          break;
        default:
          usage();
      }
    } else if (!image) {
      image = arg;
    } else if (!opts.MountPoint) {
      opts.MountPoint = arg;
    } else {
      usage();
    }
  }
  if (!opts.MountPoint)
    usage();

  // Open the image
  sqfs_err err = sqfs_open_image(&g_fs, image);
  if (err)
    return EXIT_FAILURE;
  if ((err = sqfs_inode_get(&g_fs, &g_root, sqfs_inode_root(&g_fs))))
    return EXIT_FAILURE;

  // Get the name of the volume, based on squashfs archive name
  g_root_name = _wcsdup(image);
  PathStripPath(g_root_name);
  PathRemoveExtension(g_root_name);

  // Mount and handle errors
  int status = DokanMain(&opts, &dokan_ops);
  switch (status) {
    case DOKAN_SUCCESS: break;
    case DOKAN_ERROR: die("Unknown error");
    case DOKAN_DRIVE_LETTER_ERROR: die("Bad drive letter");
    case DOKAN_DRIVER_INSTALL_ERROR: die("Can't install driver");
    case DOKAN_START_ERROR: die("Unknown driver error");
    case DOKAN_MOUNT_ERROR: die("Can't assign drive letter");
    case DOKAN_MOUNT_POINT_ERROR: die("Mount point error");
    default:
      fprintf(stderr, "Unknown error %d\n", status);
      exit(EXIT_FAILURE);
  }
  
  return 0;
}

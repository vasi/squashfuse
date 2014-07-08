#include <dokan.h>
#include "squashfuse.h"

#include <shlwapi.h>

#define SQCONTEXT ((sqfs_dokan*)fi->DokanOptions->GlobalContext)

struct sqfs_dokan {
  sqfs fs;
  sqfs_inode root;
  std::wstring volname;
  bool escape; // Escape illegal characters?
};

static const wchar_t *ILLEGAL_CHARS = L"<>:\"/\\|?*";
static const wchar_t ESCAPE = L'%';

static wchar_t sqfs_hexchar(int v) {
  if (v < 10)
    return L'0' + v;
  return L'A' + (v - 10);
}
static int sqfs_hexdigit(wchar_t h) {
  if (h >= L'A' && h <= L'F')
    return h - L'A' + 10;
  if (h >= L'a' && h <= L'f')
    return h - L'a' + 10;
  if (h >= L'0' && h <= L'9')
    return h - L'0';
  return -1;
}

static std::wstring sqfs_escape(const std::wstring &s, bool escape) {
  std::wstring ret;
  for (auto it = s.begin(); it != s.end(); ++it) {
    bool illegal = false;
    for (const wchar_t *c = ILLEGAL_CHARS; *c; ++c) {
      if (*it == *c) {
        illegal  = true;
        break;
      }
    }

    if (illegal && !escape)
      return std::wstring(); // Reject unescaped illegal chars
    if (illegal || (escape && *it == ESCAPE)) {
      ret.push_back(ESCAPE);
      ret.push_back(sqfs_hexchar(*it / 16));
      ret.push_back(sqfs_hexchar(*it & 0xf));
    } else {
      ret.push_back(*it);
    }
  }
  return ret;
}
static std::wstring sqfs_unescape(const std::wstring &s) {
  std::wstring ret;
  for (auto it = s.begin(); it != s.end(); ++it) {
    if (*it == ESCAPE) {
      int a = -1, b = -1;
      if (it++ != s.end())
        a = sqfs_hexdigit(*it);
      if (it++ != s.end())
        b = sqfs_hexdigit(*it);
      if (a != -1 && b != -1)
        ret.push_back(a * 16 + b);
    } else {
      ret.push_back(*it);
    }
  }
  return ret;
}

// Convert filename from squashfs to Win. Return empty on failure


// Convert path from Win to squashfs internal
static std::string sqfs_dk_sqfs_path(LPCWSTR path, bool escape) {
  // Unescape characters
  std::wstring wname = path;
  if (escape)
    wname = sqfs_unescape(wname);

  // Convert to UTF8
  char *buf = sqfs_str_utf8(wname.c_str());
  std::string ret = buf;
  free(buf);

  // Convert separators
  for (auto it = ret.begin(); it != ret.end(); ++it) {
    if (*it == '\\')
      *it = '/';
  }

  return ret;
}

static std::wstring sqfs_dk_win_name(const char *name, bool escape) {
  // Convert to unicode
  wchar_t *buf = sqfs_str_wide(name);
  std::wstring wname = buf;
  free(buf);

  return sqfs_escape(wname, escape);
}

// Lookup a file
static NTSTATUS sqfs_dk_lookup(sqfs_dokan *ctx, LPCWSTR path,
    sqfs_inode &inode) {
  std::string spath = sqfs_dk_sqfs_path(path, ctx->escape);
  inode = ctx->root;
  bool found = false;
  sqfs_err err = sqfs_lookup_path(&ctx->fs, &inode, spath.c_str(), &found);
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
  NTSTATUS nterr = sqfs_dk_lookup(SQCONTEXT, path, *inode);
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
  NTSTATUS nterr = sqfs_dk_lookup(SQCONTEXT, path, inode);
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
  if (sqfs_read_range(&SQCONTEXT->fs, inode, offset, &osize, buf))
    return STATUS_INTERNAL_ERROR;
  if (size)
    *size = (DWORD)osize;
  return STATUS_SUCCESS;
}

static NTSTATUS sqfs_dk_op_get_file_information(LPCWSTR path,
    LPBY_HANDLE_FILE_INFORMATION info, PDOKAN_FILE_INFO fi) {
  sqfs_inode inode;
  NTSTATUS nterr = sqfs_dk_lookup(SQCONTEXT, path, inode);
  if (nterr)
    return nterr;

  ZeroMemory(info, sizeof(*info));
  sqfs_dk_file_info(info, inode);
  info->nNumberOfLinks = inode.nlink;
  info->nFileIndexLow = inode.base.inode_number;
  info->dwVolumeSerialNumber = sqfs_dk_serial_number(SQCONTEXT->fs);
  return STATUS_SUCCESS;
}

static NTSTATUS sqfs_dk_op_find_files(LPCWSTR path, PFillFindData filler,
    PDOKAN_FILE_INFO fi) {
  sqfs *fs = &SQCONTEXT->fs;
  sqfs_inode inode;
  NTSTATUS nterr = sqfs_dk_lookup(SQCONTEXT, path, inode);
  if (nterr)
    return nterr;

  if (!S_ISDIR(inode.base.mode))
    return STATUS_NOT_A_DIRECTORY;

  sqfs_dir dir;
  if (sqfs_dir_open(fs, &inode, &dir, 0))
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
  while (sqfs_dir_next(fs, &dir, &entry, &err)) {
    std::wstring name = sqfs_dk_win_name(sqfs_dentry_name(&entry),
      SQCONTEXT->escape);
    if (name.empty())
      continue; // Ignore illegal names

    if (sqfs_inode_get(fs, &child, sqfs_dentry_inode(&entry)))
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
  wcscpy_s(volname, volnamelen / sizeof(WCHAR),
    SQCONTEXT->volname.c_str());
  *serial = sqfs_dk_serial_number(SQCONTEXT->fs);
  *maxpath = SQUASHFS_NAME_LEN;
  *flags = FILE_CASE_SENSITIVE_SEARCH | FILE_CASE_PRESERVED_NAMES | 
    FILE_UNICODE_ON_DISK | FILE_PERSISTENT_ACLS | FILE_READ_ONLY_VOLUME;
  wcscpy_s(fsname, fsnamelen / sizeof(WCHAR), L"squashfuse");

  return STATUS_SUCCESS;
}

static NTSTATUS sqfs_dk_op_unmount(PDOKAN_FILE_INFO fi) {
  return STATUS_SUCCESS;
}


// Figure out a good volume name for this image
static std::wstring sqfs_dk_volname(const wchar_t *image) {
  wchar_t *name, *buf;
  name = buf = _wcsdup(image);
  PathStripPath(name);
  PathRemoveExtension(name);
  std::wstring ret = name;
  free(buf);
  return ret;
}

static void sqfs_dk_die(const wchar_t *msg) {
  if (msg)
    fwprintf(stderr, L"%s\n", msg);
  exit(EXIT_FAILURE);
}

static void sqfs_dk_usage(wchar_t *progname) {
  PathStripPath(progname);
  fwprintf(stderr, L"squashfuse (c) 2012 Dave Vasilevsky\n\n");
  fwprintf(stderr, L"Usage: %s [options] ARCHIVE MOUNPOINT\n\n", progname);
  fwprintf(stderr, L"Options:\n");
  fwprintf(stderr, L"  /d   Print debug messages\n");
  fwprintf(stderr, L"  /e   Escape illegal characters in filenames\n");
  sqfs_dk_die(NULL);
}

static bool sqfs_dk_parse_args(int argc, wchar_t *argv[],
    sqfs_host_path &image, DOKAN_OPTIONS &opts) {
  sqfs_dokan *ctx = (sqfs_dokan*)opts.GlobalContext;
  for (int i = 1; i < argc; ++i) {
    wchar_t *arg = argv[i];
    if (arg[0] == L'/') {
      switch (arg[1]) {
      case L'd':
        opts.Options |= DOKAN_OPTION_DEBUG;
        break;
      case L'e':
        ctx->escape = true;
        break;
      default:
        return false; // Unknown option
      }
    } else if (!image) {
      image = arg;
    } else if (!opts.MountPoint) {
      opts.MountPoint = arg;
    } else {
      return false; // Too many args
    }
  }
  if (!opts.MountPoint)
    return false; // Missing args
  return true;
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
  // Need Removable to be able to run executables!
  opts.Options = DOKAN_OPTION_KEEP_ALIVE | DOKAN_OPTION_REMOVABLE;

  sqfs_dokan *ctx = new sqfs_dokan();
  ctx->escape = false;
  opts.GlobalContext = (ULONG64)ctx;

  // Parse arguments
  sqfs_host_path image = NULL;
  if (!sqfs_dk_parse_args(argc, argv, image, opts))
    sqfs_dk_usage(argv[0]);

  // Open the image
  sqfs_err err = sqfs_open_image(&ctx->fs, image);
  if (err)
    return EXIT_FAILURE;
  if ((err = sqfs_inode_get(&ctx->fs, &ctx->root, sqfs_inode_root(&ctx->fs))))
    return EXIT_FAILURE;
  ctx->volname = sqfs_dk_volname(image);

  // Mount and handle errors
  int status = DokanMain(&opts, &dokan_ops);
  switch (status) {
    case DOKAN_SUCCESS: break;
    case DOKAN_ERROR: sqfs_dk_die(L"Unknown error");
    case DOKAN_DRIVE_LETTER_ERROR: sqfs_dk_die(L"Bad drive letter");
    case DOKAN_DRIVER_INSTALL_ERROR: sqfs_dk_die(L"Can't install driver");
    case DOKAN_START_ERROR: sqfs_dk_die(L"Unknown driver error");
    case DOKAN_MOUNT_ERROR: sqfs_dk_die(L"Can't assign drive letter");
    case DOKAN_MOUNT_POINT_ERROR: sqfs_dk_die(L"Mount point error");
    default:
      fwprintf(stderr, L"Unknown error %d\n", status);
      exit(EXIT_FAILURE);
  }
  
  return 0;
}

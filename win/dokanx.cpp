#include <windows.h>
#include <winbase.h>
#include <shlwapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <dokan.h>
#include <dokanx/fileinfo.h>
#include "squashfuse.h"

static sqfs g_fs;
static sqfs_inode g_root;
static wchar_t *g_root_name;

// Convert path from Win to squashfs internal
static std::string sqfs_dk_sqfs_path(LPCWSTR path) {
  // Convert to UTF8
  size_t size = WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0, NULL, NULL);
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



static WCHAR g_RootDirectory[MAX_PATH] = L"C:";
static WCHAR g_MountPoint[MAX_PATH] = L"M:";

NTSTATUS MirrorCreateFile(
    LPCWSTR					FileName,
    DWORD					DesiredAccess,
    DWORD					ShareMode,
    DWORD					CreationDisposition,
    DWORD					FlagsAndAttributes,
    PDOKAN_FILE_INFO		DokanFileInfo)
{
  sqfs_inode *inode = new sqfs_inode();
  NTSTATUS nterr = sqfs_dk_lookup(FileName, *inode);
  if (nterr) {
    delete inode;
    return nterr;
  }

  // FIXME: Check if it's a regular file?

  DokanFileInfo->Context = (ULONG64)inode;
  return STATUS_SUCCESS;
}

NTSTATUS MirrorCreateDirectory(
    LPCWSTR					FileName,
    PDOKAN_FILE_INFO		DokanFileInfo)
{
  return STATUS_MEDIA_WRITE_PROTECTED;
}


NTSTATUS MirrorOpenDirectory(
    LPCWSTR					FileName,
    PDOKAN_FILE_INFO		DokanFileInfo)
{
  sqfs_inode inode;
  NTSTATUS nterr = sqfs_dk_lookup(FileName, inode);
  if (nterr)
    return nterr;

  if (!S_ISDIR(inode.base.mode))
    return STATUS_NOT_A_DIRECTORY;
  return STATUS_SUCCESS;
}


void MirrorCloseFile(
    LPCWSTR					FileName,
    PDOKAN_FILE_INFO		DokanFileInfo)
{
  delete (sqfs_inode*)DokanFileInfo->Context;
}

void MirrorCleanup(
    LPCWSTR					FileName,
    PDOKAN_FILE_INFO		DokanFileInfo)
{
}

NTSTATUS MirrorReadFile(
    LPCWSTR				FileName,
    LPVOID				Buffer,
    DWORD				BufferLength,
    LPDWORD				ReadLength,
    LONGLONG			Offset,
    PDOKAN_FILE_INFO	DokanFileInfo)
{
  sqfs_inode *inode = (sqfs_inode*)DokanFileInfo->Context;
  sqfs_off_t osize = BufferLength;
  if (sqfs_read_range(&g_fs, inode, Offset, &osize, Buffer))
    return STATUS_INTERNAL_ERROR;
  if (ReadLength)
    *ReadLength = (DWORD)osize;
  return STATUS_SUCCESS;
}

NTSTATUS MirrorWriteFile(
    LPCWSTR		FileName,
    LPCVOID		Buffer,
    DWORD		NumberOfBytesToWrite,
    LPDWORD		NumberOfBytesWritten,
    LONGLONG			Offset,
    PDOKAN_FILE_INFO	DokanFileInfo)
{
  return STATUS_MEDIA_WRITE_PROTECTED;
}

NTSTATUS MirrorFlushFileBuffers(
    LPCWSTR		FileName,
    PDOKAN_FILE_INFO	DokanFileInfo)
{
  return STATUS_MEDIA_WRITE_PROTECTED;
}

NTSTATUS MirrorGetFileInformation(
    LPCWSTR							FileName,
    LPBY_HANDLE_FILE_INFORMATION	HandleFileInformation,
    PDOKAN_FILE_INFO				DokanFileInfo)
{
  sqfs_inode inode;
  NTSTATUS nterr = sqfs_dk_lookup(FileName, inode);
  if (nterr)
    return nterr;

  ZeroMemory(HandleFileInformation, sizeof(*HandleFileInformation));
  sqfs_dk_file_info(HandleFileInformation, inode);
  HandleFileInformation->nNumberOfLinks = inode.nlink;
  HandleFileInformation->nFileIndexLow = inode.base.inode_number;
  HandleFileInformation->dwVolumeSerialNumber = sqfs_dk_serial_number(g_fs);
  return STATUS_SUCCESS;
}

NTSTATUS MirrorFindFiles(
    LPCWSTR				FileName,
    PFillFindData		FillFindData, // function pointer
    PDOKAN_FILE_INFO	DokanFileInfo)
{
  sqfs_inode inode;
  NTSTATUS nterr = sqfs_dk_lookup(FileName, inode);
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

    FillFindData(&find, DokanFileInfo);
  }
  return err ? STATUS_INTERNAL_ERROR : STATUS_SUCCESS;
}

NTSTATUS MirrorDeleteFile(
    LPCWSTR				FileName,
    PDOKAN_FILE_INFO	DokanFileInfo)
{
  return STATUS_MEDIA_WRITE_PROTECTED;
}

NTSTATUS MirrorDeleteDirectory(
    LPCWSTR				FileName,
    PDOKAN_FILE_INFO	DokanFileInfo)
{
  return STATUS_MEDIA_WRITE_PROTECTED;
}


NTSTATUS MirrorMoveFile(
    LPCWSTR				FileName, // existing file name
    LPCWSTR				NewFileName,
    BOOL				ReplaceIfExisting,
    PDOKAN_FILE_INFO	DokanFileInfo)
{
  return STATUS_MEDIA_WRITE_PROTECTED;
}

NTSTATUS MirrorLockFile(
    LPCWSTR				FileName,
    LONGLONG			ByteOffset,
    LONGLONG			Length,
    PDOKAN_FILE_INFO	DokanFileInfo)
{
  return STATUS_MEDIA_WRITE_PROTECTED;
}

NTSTATUS MirrorSetEndOfFile(
    LPCWSTR				FileName,
    LONGLONG			ByteOffset,
    PDOKAN_FILE_INFO	DokanFileInfo)
{
  return STATUS_MEDIA_WRITE_PROTECTED;
}


NTSTATUS MirrorSetAllocationSize(
    LPCWSTR				FileName,
    LONGLONG			AllocSize,
    PDOKAN_FILE_INFO	DokanFileInfo)
{
  return STATUS_MEDIA_WRITE_PROTECTED;
}

NTSTATUS MirrorSetFileAttributes(
    LPCWSTR				FileName,
    DWORD				FileAttributes,
    PDOKAN_FILE_INFO	DokanFileInfo)
{
  return STATUS_MEDIA_WRITE_PROTECTED;
}

NTSTATUS MirrorSetFileTime(
    LPCWSTR				FileName,
    CONST FILETIME*		CreationTime,
    CONST FILETIME*		LastAccessTime,
    CONST FILETIME*		LastWriteTime,
    PDOKAN_FILE_INFO	DokanFileInfo)
{
  return STATUS_MEDIA_WRITE_PROTECTED;
}

NTSTATUS MirrorUnlockFile(
    LPCWSTR				FileName,
    LONGLONG			ByteOffset,
    LONGLONG			Length,
    PDOKAN_FILE_INFO	DokanFileInfo)
{
  return STATUS_MEDIA_WRITE_PROTECTED;
}

NTSTATUS MirrorGetVolumeInformation(
    LPWSTR		VolumeNameBuffer,
    DWORD		VolumeNameSize,
    LPDWORD		VolumeSerialNumber,
    LPDWORD		MaximumComponentLength,
    LPDWORD		FileSystemFlags,
    LPWSTR		FileSystemNameBuffer,
    DWORD		FileSystemNameSize,
    PDOKAN_FILE_INFO	/*DokanFileInfo*/)
{
  wcscpy_s(VolumeNameBuffer, VolumeNameSize / sizeof(WCHAR), g_root_name);
  *VolumeSerialNumber = sqfs_dk_serial_number(g_fs);
  *MaximumComponentLength = SQUASHFS_NAME_LEN;
  *FileSystemFlags = FILE_CASE_SENSITIVE_SEARCH | 
                      FILE_CASE_PRESERVED_NAMES | 
                      FILE_UNICODE_ON_DISK |
                      FILE_PERSISTENT_ACLS |
                      FILE_READ_ONLY_VOLUME;

  wcscpy_s(FileSystemNameBuffer, FileSystemNameSize / sizeof(WCHAR), L"squashfuse");

  return STATUS_SUCCESS;
}

NTSTATUS MirrorUnmount(
    PDOKAN_FILE_INFO	DokanFileInfo)
{
    return STATUS_SUCCESS;
}

int wmain(int argc, wchar_t* argv[])
{
    int status;
    int command;
    PDOKAN_OPERATIONS dokanOperations = (PDOKAN_OPERATIONS)malloc(sizeof(DOKAN_OPERATIONS));
    if (dokanOperations == nullptr)
    {
        return EXIT_FAILURE;
    }

    PDOKAN_OPTIONS dokanOptions = (PDOKAN_OPTIONS)malloc(sizeof(DOKAN_OPTIONS));
    if (dokanOperations == nullptr)
    {
        free(dokanOperations);
        return EXIT_FAILURE;
    }

    if (argc < 5) {
        fprintf(stderr, "mirror.exe\n"
            "  /r RootDirectory (ex. /r c:\\test)\n"
            "  /l DriveLetter (ex. /l m)\n"
            "  /t ThreadCount (ex. /t 5)\n"
            "  /d (enable debug output)\n"
            "  /s (use stderr for output)\n"
            "  /n (use network drive)\n"
            "  /m (use removable drive)");
        return EXIT_FAILURE;
    }

    ZeroMemory(dokanOptions, sizeof(DOKAN_OPTIONS));
    dokanOptions->Version = DOKAN_VERSION;
    dokanOptions->ThreadCount = 0; // use default

    sqfs_host_path image = NULL;
    for (command = 1; command < argc; command++) {
        switch (towlower(argv[command][1])) {
        case L'r':
            command++;
            wcscpy_s(g_RootDirectory, _countof(g_RootDirectory), argv[command]);
            break;
        case L'l':
            command++;
            wcscpy_s(g_MountPoint, _countof(g_MountPoint), argv[command]);
            dokanOptions->MountPoint = g_MountPoint;
            break;
        case L't':
            command++;
            dokanOptions->ThreadCount = (USHORT)_wtoi(argv[command]);
            break;
        default:
            image = argv[command];
        }
    }

    sqfs_err err = sqfs_open_image(&g_fs, image);
    if (err)
      return EXIT_FAILURE;
    if ((err = sqfs_inode_get(&g_fs, &g_root, sqfs_inode_root(&g_fs))))
      return EXIT_FAILURE;

    g_root_name = _wcsdup(image);
    PathStripPath(g_root_name);
    PathRemoveExtension(g_root_name);

    dokanOptions->Options |= DOKAN_OPTION_KEEP_ALIVE;

    ZeroMemory(dokanOperations, sizeof(DOKAN_OPERATIONS));
    dokanOperations->CreateFile = MirrorCreateFile;
    dokanOperations->OpenDirectory = MirrorOpenDirectory;
    dokanOperations->CreateDirectory = MirrorCreateDirectory;
    dokanOperations->Cleanup = MirrorCleanup;
    dokanOperations->CloseFile = MirrorCloseFile;
    dokanOperations->ReadFile = MirrorReadFile;
    dokanOperations->WriteFile = MirrorWriteFile;
    dokanOperations->FlushFileBuffers = MirrorFlushFileBuffers;
    dokanOperations->GetFileInformation = MirrorGetFileInformation;
    dokanOperations->FindFiles = MirrorFindFiles;
    dokanOperations->SetFileAttributes = MirrorSetFileAttributes;
    dokanOperations->SetFileTime = MirrorSetFileTime;
    dokanOperations->DeleteFile = MirrorDeleteFile;
    dokanOperations->DeleteDirectory = MirrorDeleteDirectory;
    dokanOperations->MoveFile = MirrorMoveFile;
    dokanOperations->SetEndOfFile = MirrorSetEndOfFile;
    dokanOperations->SetAllocationSize = MirrorSetAllocationSize;
    dokanOperations->LockFile = MirrorLockFile;
    dokanOperations->UnlockFile = MirrorUnlockFile;
    dokanOperations->GetVolumeInformation = MirrorGetVolumeInformation;
    dokanOperations->Unmount = MirrorUnmount;

    status = DokanMain(dokanOptions, dokanOperations);
    switch (status) {
    case DOKAN_SUCCESS:
        fprintf(stderr, "Success");
        break;
    case DOKAN_ERROR:
        fprintf(stderr, "Error");
        break;
    case DOKAN_DRIVE_LETTER_ERROR:
        fprintf(stderr, "Bad Drive letter");
        break;
    case DOKAN_DRIVER_INSTALL_ERROR:
        fprintf(stderr, "Can't install driver");
        break;
    case DOKAN_START_ERROR:
        fprintf(stderr, "Driver something wrong");
        break;
    case DOKAN_MOUNT_ERROR:
        fprintf(stderr, "Can't assign a drive letter");
        break;
    case DOKAN_MOUNT_POINT_ERROR:
        fprintf(stderr, "Mount point error");
        break;
    default:
        fprintf(stderr, "Unknown error: %d", status);
        break;
    }

    free(dokanOptions);
    free(dokanOperations);

    return 0;
}
#include <windows.h>
#include <winbase.h>
#include <Shlwapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <dokan.h>
#include <dokanx/fileinfo.h>
#include "squashfuse.h"

static sqfs g_fs;
static sqfs_inode g_root;
static wchar_t *g_root_name;

static LPCWSTR ILLEGAL_CHARS = L"<>:\"/\\|?*";

// Convert path
std::string sqfs_host2sqfs(LPCWSTR path) {
  char *buf;
  std::string ret;
  
  // Convert to UTF8
  size_t size = WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0, NULL, NULL);
  buf = new char[size];
  WideCharToMultiByte(CP_UTF8, 0, path, -1, buf, size, NULL, NULL);
  ret = buf;
  delete[] buf;

  // Convert separators
  for (auto it = ret.begin(); it != ret.end(); ++it) {
    if (*it == '\\')
      *it = '/';
  }

  return ret;
}

DWORD sqfs_serial_number(sqfs *fs) {
  return fs->sb.mkfs_time ^ fs->sb.inodes ^ (DWORD)fs->sb.inode_table_start;
}


static WCHAR g_RootDirectory[MAX_PATH] = L"C:";
static WCHAR g_MountPoint[MAX_PATH] = L"M:";

std::wstring AppendPathSeperatorIfNotExist(
  __in const std::wstring& path,
  __in WCHAR pathSeperator /*= '/'*/
  )
{
  if (path.empty() || path[path.size() - 1] != pathSeperator)
  {
    return path + pathSeperator;
  }
  return path;
}

NTSTATUS ToNtStatus(DWORD dwError)
{
    switch (dwError)
    {
    case ERROR_FILE_NOT_FOUND:
        return STATUS_OBJECT_NAME_NOT_FOUND;
    case ERROR_PATH_NOT_FOUND:
        return STATUS_OBJECT_PATH_NOT_FOUND;
    case ERROR_INVALID_PARAMETER:
        return STATUS_INVALID_PARAMETER;
    default:
        return STATUS_ACCESS_DENIED;
    }
}

std::wstring GetFilePath(
    __in const std::wstring& fileName
    )
{
    return std::wstring(g_RootDirectory) + fileName;
}

NTSTATUS MirrorCreateFile(
    LPCWSTR					FileName,
    DWORD					DesiredAccess,
    DWORD					ShareMode,
    DWORD					CreationDisposition,
    DWORD					FlagsAndAttributes,
    PDOKAN_FILE_INFO		DokanFileInfo)
{
    std::wstring filePath;
    HANDLE handle;
    DWORD fileAttr;

    filePath = GetFilePath(FileName);

    // When filePath is a directory, needs to change the flag so that the file can be opened.
    fileAttr = GetFileAttributes(filePath.c_str());
    if (fileAttr && fileAttr & FILE_ATTRIBUTE_DIRECTORY) {
        FlagsAndAttributes |= FILE_FLAG_BACKUP_SEMANTICS;
        //AccessMode = 0;
    }

    handle = CreateFile(
        filePath.c_str(),
        DesiredAccess,//GENERIC_READ|GENERIC_WRITE|GENERIC_EXECUTE,
        ShareMode,
        NULL, // security attribute
        OPEN_EXISTING,
        FlagsAndAttributes,// |FILE_FLAG_NO_BUFFERING,
        NULL); // template file handle

    if (handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND)
        {
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }
        else
        {
            return STATUS_ACCESS_DENIED;
        }
    }

    // save the file handle in Context
    DokanFileInfo->Context = (ULONG64)handle;
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
    HANDLE handle;
    DWORD attr;
    std::wstring filePath = GetFilePath(FileName);

    attr = GetFileAttributes(filePath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        return ToNtStatus(error);
    }
    if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return STATUS_NOT_A_DIRECTORY;
    }

    handle = CreateFile(
        filePath.c_str(),
        0,
        FILE_SHARE_READ|FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL);

    if (handle == INVALID_HANDLE_VALUE) {
        DWORD dwError = GetLastError();
        return ToNtStatus(dwError);
    }

    DokanFileInfo->Context = (ULONG64)handle;

    return STATUS_SUCCESS;
}


void MirrorCloseFile(
    LPCWSTR					FileName,
    PDOKAN_FILE_INFO		DokanFileInfo)
{
    std::wstring filePath = GetFilePath(FileName);

    if (DokanFileInfo->Context) {
        CloseHandle((HANDLE)DokanFileInfo->Context);
        DokanFileInfo->Context = 0;
    }
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
    HANDLE	handle = (HANDLE)DokanFileInfo->Context;
    ULONG	offset = (ULONG)Offset;
    BOOL	opened = FALSE;
    std::wstring filePath = GetFilePath(FileName);

    if (!handle || handle == INVALID_HANDLE_VALUE) {
        handle = CreateFile(
            filePath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);
        if (handle == INVALID_HANDLE_VALUE) {
            DWORD dwError = GetLastError();
            return ToNtStatus(dwError);
        }
        opened = TRUE;
    }
    
    if (SetFilePointer(handle, offset, NULL, FILE_BEGIN) == 0xFFFFFFFF) {
        DWORD dwError = GetLastError();
        if (opened)
            CloseHandle(handle);
        
        return ToNtStatus(dwError);
    }
        
    if (!ReadFile(handle, Buffer, BufferLength, ReadLength,NULL)) {
        DWORD dwError = GetLastError();
        if (opened)
            CloseHandle(handle);
        
        return ToNtStatus(dwError);
    }

    if (opened)
        CloseHandle(handle);

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
    HANDLE	handle = (HANDLE)DokanFileInfo->Context;
    BOOL	opened = FALSE;

    std::wstring filePath = GetFilePath(FileName);

    if (!handle || handle == INVALID_HANDLE_VALUE) {
        // If CreateDirectory returned FILE_ALREADY_EXISTS and 
        // it is called with FILE_OPEN_IF, that handle must be opened.
        handle = CreateFile(filePath.c_str(), 0, FILE_SHARE_READ, NULL, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS, NULL);
        if (handle == INVALID_HANDLE_VALUE)
        {
            DWORD dwError = GetLastError();
            return ToNtStatus(dwError);
        }
        opened = TRUE;
    }

    if (!GetFileInformationByHandle(handle,HandleFileInformation)) {
        // FileName is a root directory
        // in this case, FindFirstFile can't get directory information
        if (wcslen(FileName) == 1) {
            HandleFileInformation->dwFileAttributes = GetFileAttributes(filePath.c_str());

        } else {
            WIN32_FIND_DATAW find;
            ZeroMemory(&find, sizeof(WIN32_FIND_DATAW));
            handle = FindFirstFile(filePath.c_str(), &find);
            if (handle == INVALID_HANDLE_VALUE) {
                DWORD dwError = GetLastError();
                return ToNtStatus(dwError);
            }
            HandleFileInformation->dwFileAttributes = find.dwFileAttributes;
            HandleFileInformation->ftCreationTime = find.ftCreationTime;
            HandleFileInformation->ftLastAccessTime = find.ftLastAccessTime;
            HandleFileInformation->ftLastWriteTime = find.ftLastWriteTime;
            HandleFileInformation->nFileSizeHigh = find.nFileSizeHigh;
            HandleFileInformation->nFileSizeLow = find.nFileSizeLow;
            FindClose(handle);
        }
    }

    if (opened) {
        CloseHandle(handle);
    }

    return STATUS_SUCCESS;
}

NTSTATUS MirrorFindFiles(
    LPCWSTR				FileName,
    PFillFindData		FillFindData, // function pointer
    PDOKAN_FILE_INFO	DokanFileInfo)
{
    HANDLE				hFind;
    WIN32_FIND_DATAW	findData;
    DWORD				error;
    PWCHAR				yenStar = L"\\*";
    int count = 0;

    std::wstring filePath = GetFilePath(FileName);
    filePath = filePath + yenStar;

    hFind = FindFirstFile(filePath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        DWORD dwError = GetLastError();
        return ToNtStatus(dwError);
    }

    FillFindData(&findData, DokanFileInfo);
    count++;

    while (FindNextFile(hFind, &findData) != 0) {
        FillFindData(&findData, DokanFileInfo);
        count++;
    }
    
    error = GetLastError();
    FindClose(hFind);

    if (error != ERROR_NO_MORE_FILES) {
        return ToNtStatus(error);
    }

    return STATUS_SUCCESS;
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
  *VolumeSerialNumber = sqfs_serial_number(&g_fs);
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
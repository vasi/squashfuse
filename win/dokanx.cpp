#include <windows.h>
#include <winbase.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <dokan.h>
#include <dokanx/fileinfo.h>
#include "squashfuse.h"

BOOL g_UseStdErr;
BOOL g_DebugMode;

static WCHAR g_RootDirectory[MAX_PATH] = L"C:";
static WCHAR g_MountPoint[MAX_PATH] = L"M:";

#define logw(fmt, ...) \
	__noop(fmt, __VA_ARGS__);

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

static void
PrintUserName(PDOKAN_FILE_INFO	DokanFileInfo)
{
    HANDLE	handle;
    UCHAR buffer[1024];
    DWORD returnLength;
    WCHAR accountName[256];
    WCHAR domainName[256];
    DWORD accountLength = _countof(accountName);
    DWORD domainLength = _countof(domainName);
    PTOKEN_USER tokenUser;
    SID_NAME_USE snu;

    handle = DokanOpenRequestorToken(DokanFileInfo);
    if (handle == INVALID_HANDLE_VALUE) {
        logw(L"  DokanOpenRequestorToken failed");
        return;
    }

    if (!GetTokenInformation(handle, TokenUser, buffer, sizeof(buffer), &returnLength)) {
        logw(L"  GetTokenInformaiton failed: %d", GetLastError());
        CloseHandle(handle);
        return;
    }

    CloseHandle(handle);

    tokenUser = (PTOKEN_USER)buffer;
    if (!LookupAccountSid(NULL, tokenUser->User.Sid, accountName,
            &accountLength, domainName, &domainLength, &snu)) {
        logw(L"  LookupAccountSid failed: %d", GetLastError());
        return;
    }

    logw(L"  AccountName: %s, DomainName: %s", accountName, domainName);
}

#define MirrorCheckFlag(val, flag) if (val&flag) { logw(L#flag); }


void CheckFileAttributeFlags(DWORD FlagsAndAttributes)
{
    MirrorCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_ARCHIVE);
    MirrorCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_ENCRYPTED);
    MirrorCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_HIDDEN);
    MirrorCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_NORMAL);
    MirrorCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_NOT_CONTENT_INDEXED);
    MirrorCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_OFFLINE);
    MirrorCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_READONLY);
    MirrorCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_SYSTEM);
    MirrorCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_TEMPORARY);
    MirrorCheckFlag(FlagsAndAttributes, FILE_FLAG_WRITE_THROUGH);
    MirrorCheckFlag(FlagsAndAttributes, FILE_FLAG_OVERLAPPED);
    MirrorCheckFlag(FlagsAndAttributes, FILE_FLAG_NO_BUFFERING);
    MirrorCheckFlag(FlagsAndAttributes, FILE_FLAG_RANDOM_ACCESS);
    MirrorCheckFlag(FlagsAndAttributes, FILE_FLAG_SEQUENTIAL_SCAN);
    MirrorCheckFlag(FlagsAndAttributes, FILE_FLAG_DELETE_ON_CLOSE);
    MirrorCheckFlag(FlagsAndAttributes, FILE_FLAG_BACKUP_SEMANTICS);
    MirrorCheckFlag(FlagsAndAttributes, FILE_FLAG_POSIX_SEMANTICS);
    MirrorCheckFlag(FlagsAndAttributes, FILE_FLAG_OPEN_REPARSE_POINT);
    MirrorCheckFlag(FlagsAndAttributes, FILE_FLAG_OPEN_NO_RECALL);
    MirrorCheckFlag(FlagsAndAttributes, SECURITY_ANONYMOUS);
    MirrorCheckFlag(FlagsAndAttributes, SECURITY_IDENTIFICATION);
    MirrorCheckFlag(FlagsAndAttributes, SECURITY_IMPERSONATION);
    MirrorCheckFlag(FlagsAndAttributes, SECURITY_DELEGATION);
    MirrorCheckFlag(FlagsAndAttributes, SECURITY_CONTEXT_TRACKING);
    MirrorCheckFlag(FlagsAndAttributes, SECURITY_EFFECTIVE_ONLY);
    MirrorCheckFlag(FlagsAndAttributes, SECURITY_SQOS_PRESENT);
}

void CheckDesiredAccessFlags(DWORD DesiredAccess)
{
    MirrorCheckFlag(DesiredAccess, GENERIC_READ);
    MirrorCheckFlag(DesiredAccess, GENERIC_WRITE);
    MirrorCheckFlag(DesiredAccess, GENERIC_EXECUTE);

    MirrorCheckFlag(DesiredAccess, DELETE);
    MirrorCheckFlag(DesiredAccess, FILE_READ_DATA);
    MirrorCheckFlag(DesiredAccess, FILE_READ_ATTRIBUTES);
    MirrorCheckFlag(DesiredAccess, FILE_READ_EA);
    MirrorCheckFlag(DesiredAccess, READ_CONTROL);
    MirrorCheckFlag(DesiredAccess, FILE_WRITE_DATA);
    MirrorCheckFlag(DesiredAccess, FILE_WRITE_ATTRIBUTES);
    MirrorCheckFlag(DesiredAccess, FILE_WRITE_EA);
    MirrorCheckFlag(DesiredAccess, FILE_APPEND_DATA);
    MirrorCheckFlag(DesiredAccess, WRITE_DAC);
    MirrorCheckFlag(DesiredAccess, WRITE_OWNER);
    MirrorCheckFlag(DesiredAccess, SYNCHRONIZE);
    MirrorCheckFlag(DesiredAccess, FILE_EXECUTE);
    MirrorCheckFlag(DesiredAccess, STANDARD_RIGHTS_READ);
    MirrorCheckFlag(DesiredAccess, STANDARD_RIGHTS_WRITE);
    MirrorCheckFlag(DesiredAccess, STANDARD_RIGHTS_EXECUTE);
}

void CheckShareModeFlags(DWORD ShareMode)
{
    MirrorCheckFlag(ShareMode, FILE_SHARE_READ);
    MirrorCheckFlag(ShareMode, FILE_SHARE_WRITE);
    MirrorCheckFlag(ShareMode, FILE_SHARE_DELETE);
}

NTSTATUS MirrorCreateFile(
    LPCWSTR					FileName,
    DWORD					DesiredAccess,
    DWORD					ShareMode,
    DWORD					CreationDisposition,
    DWORD					FlagsAndAttributes,
    PDOKAN_FILE_INFO		DokanFileInfo)
{
    logw(L"Start<%s>", FileName);
    std::wstring filePath;
    HANDLE handle;
    DWORD fileAttr;

    filePath = GetFilePath(FileName);

    logw(L"CreateFile : %s", filePath.c_str());

    PrintUserName(DokanFileInfo);

    if (CreationDisposition == CREATE_NEW)
        logw(L"CREATE_NEW");
    if (CreationDisposition == OPEN_ALWAYS)
        logw(L"OPEN_ALWAYS");
    if (CreationDisposition == CREATE_ALWAYS)
        logw(L"CREATE_ALWAYS");
    if (CreationDisposition == OPEN_EXISTING)
        logw(L"OPEN_EXISTING");
    if (CreationDisposition == TRUNCATE_EXISTING)
        logw(L"TRUNCATE_EXISTING");

    logw(L"ShareMode = 0x%x", ShareMode);

    CheckShareModeFlags(ShareMode);

    logw(L"AccessMode = 0x%x", DesiredAccess);

    CheckDesiredAccessFlags(DesiredAccess);

    // When filePath is a directory, needs to change the flag so that the file can be opened.
    fileAttr = GetFileAttributes(filePath.c_str());
    if (fileAttr && fileAttr & FILE_ATTRIBUTE_DIRECTORY) {
        FlagsAndAttributes |= FILE_FLAG_BACKUP_SEMANTICS;
        //AccessMode = 0;
    }
    logw(L"FlagsAndAttributes = 0x%08X", FlagsAndAttributes);

    CheckFileAttributeFlags(FlagsAndAttributes);

    handle = CreateFile(
        filePath.c_str(),
        DesiredAccess,//GENERIC_READ|GENERIC_WRITE|GENERIC_EXECUTE,
        ShareMode,
        NULL, // security attribute
        CreationDisposition,
        FlagsAndAttributes,// |FILE_FLAG_NO_BUFFERING,
        NULL); // template file handle

    if (handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        logw(L"error code = %d", error);
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
    logw(L"Start<%s>", FileName);
    std::wstring filePath = GetFilePath(FileName);

    logw(L"CreateDirectory : %s", filePath.c_str());
    if (!CreateDirectory(filePath.c_str(), NULL)) {
        DWORD error = GetLastError();
        logw(L"failed(%d)", error);
        return ToNtStatus(error);
    }
    return STATUS_SUCCESS;
}


NTSTATUS MirrorOpenDirectory(
    LPCWSTR					FileName,
    PDOKAN_FILE_INFO		DokanFileInfo)
{
    logw(L"Start<%s>", FileName);
    HANDLE handle;
    DWORD attr;
    std::wstring filePath = GetFilePath(FileName);

    logw(L"OpenDirectory : %s", filePath.c_str());

    attr = GetFileAttributes(filePath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        logw(L"failed(%d)", error);
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
        logw(L"failed(%d)", dwError);
        return ToNtStatus(dwError);
    }

    logw(L"");

    DokanFileInfo->Context = (ULONG64)handle;

    return STATUS_SUCCESS;
}


void MirrorCloseFile(
    LPCWSTR					FileName,
    PDOKAN_FILE_INFO		DokanFileInfo)
{
    std::wstring filePath = GetFilePath(FileName);

    if (DokanFileInfo->Context) {
        logw(L"CloseFile: %s", filePath.c_str());
        logw(L"error : not cleanuped file");
        CloseHandle((HANDLE)DokanFileInfo->Context);
        DokanFileInfo->Context = 0;
    } else {
        //DbgPrint(L"Close: %s\ninvalid handle", filePath.c_str());
        logw(L"Close: %s", filePath.c_str());
    }

    //DbgPrint(L"");
}

void MirrorCleanup(
    LPCWSTR					FileName,
    PDOKAN_FILE_INFO		DokanFileInfo)
{
    std::wstring filePath = GetFilePath(FileName);

    if (DokanFileInfo->Context) {
        logw(L"Cleanup: %s", filePath.c_str());
        CloseHandle((HANDLE)DokanFileInfo->Context);
        DokanFileInfo->Context = 0;

        if (DokanFileInfo->DeleteOnClose) {
            logw(L"DeleteOnClose");
            if (DokanFileInfo->IsDirectory) {
                logw(L"  DeleteDirectory ");
                if (!RemoveDirectory(filePath.c_str())) {
                    logw(L"error code = %d", GetLastError());
                } else {
                    logw(L"success");
                }
            } else {
                logw(L"  DeleteFile ");
                if (DeleteFile(filePath.c_str()) == 0) {
                    logw(L" error code = %d", GetLastError());
                } else {
                    logw(L"success");
                }
            }
        }

    } else {
        logw(L"Cleanup: %s\ninvalid handle", filePath.c_str());
    }
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

    logw(L"ReadFile : %s", filePath.c_str());

    if (!handle || handle == INVALID_HANDLE_VALUE) {
        logw(L"invalid handle, cleanuped?");
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
            logw(L"failed(%d)", dwError);
            return ToNtStatus(dwError);
        }
        opened = TRUE;
    }
    
    if (SetFilePointer(handle, offset, NULL, FILE_BEGIN) == 0xFFFFFFFF) {
        DWORD dwError = GetLastError();
        logw(L"seek error, offset = %d", offset);
        if (opened)
            CloseHandle(handle);
        
        logw(L"failed(%d)", dwError);
        return ToNtStatus(dwError);
    }
        
    if (!ReadFile(handle, Buffer, BufferLength, ReadLength,NULL)) {
        DWORD dwError = GetLastError();
        logw(L"read error = %u, buffer length = %d, read length = %d",
            dwError, BufferLength, *ReadLength);
        if (opened)
            CloseHandle(handle);
        
        logw(L"failed(%d)", dwError);
        return ToNtStatus(dwError);

    } else {
        logw(L"read %d, offset %d", *ReadLength, offset);
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
    HANDLE	handle = (HANDLE)DokanFileInfo->Context;
    ULONG	offset = (ULONG)Offset;
    BOOL	opened = FALSE;
    std::wstring filePath = GetFilePath(FileName);

    logw(L"WriteFile : %s, offset %I64d, length %d", filePath.c_str(), Offset, NumberOfBytesToWrite);

    // reopen the file
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        logw(L"invalid handle, cleanuped?");
        handle = CreateFile(
            filePath.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);
        if (handle == INVALID_HANDLE_VALUE) {
            DWORD dwError = GetLastError();
            logw(L"failed(%d)", dwError);
            return ToNtStatus(dwError);
        }
        opened = TRUE;
    }

    if (DokanFileInfo->WriteToEndOfFile) {
        if (SetFilePointer(handle, 0, NULL, FILE_END) == INVALID_SET_FILE_POINTER) {
            DWORD dwError = GetLastError();
            
            logw(L"seek error, offset = EOF, error = %d", GetLastError());
            logw(L"failed(%d)", dwError);
            return ToNtStatus(dwError);
        }
    } else if (SetFilePointer(handle, offset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
        DWORD dwError = GetLastError();
        logw(L"seek error, offset = %d, error = %d", offset, GetLastError());
        logw(L"failed(%d)", dwError);
        return ToNtStatus(dwError);
    }
        
    if (!WriteFile(handle, Buffer, NumberOfBytesToWrite, NumberOfBytesWritten, NULL)) {
        DWORD dwError = GetLastError();
        logw(L"write error = %u, buffer length = %d, write length = %d",
            GetLastError(), NumberOfBytesToWrite, *NumberOfBytesWritten);
        logw(L"failed(%d)", dwError);
        return ToNtStatus(dwError);

    } else {
        logw(L"write %d, offset %d", *NumberOfBytesWritten, offset);
    }

    // close the file when it is reopened
    if (opened)
        CloseHandle(handle);

    return STATUS_SUCCESS;
}

NTSTATUS MirrorFlushFileBuffers(
    LPCWSTR		FileName,
    PDOKAN_FILE_INFO	DokanFileInfo)
{
    HANDLE	handle = (HANDLE)DokanFileInfo->Context;
    std::wstring filePath = GetFilePath(FileName);

    logw(L"FlushFileBuffers : %s", filePath.c_str());

    if (!handle || handle == INVALID_HANDLE_VALUE) {
        logw(L"invalid handle, but return success");
        return STATUS_SUCCESS;
    }

    if (FlushFileBuffers(handle)) {
        return STATUS_SUCCESS;
    } else {
        DWORD dwError = GetLastError();
        logw(L"FlushFileBuffers failed(%d)", dwError);
        return ToNtStatus(dwError);
    }
}


NTSTATUS MirrorGetFileInformation(
    LPCWSTR							FileName,
    LPBY_HANDLE_FILE_INFORMATION	HandleFileInformation,
    PDOKAN_FILE_INFO				DokanFileInfo)
{
    HANDLE	handle = (HANDLE)DokanFileInfo->Context;
    BOOL	opened = FALSE;

    std::wstring filePath = GetFilePath(FileName);

    logw(L"GetFileInfo : %s", filePath.c_str());

    if (!handle || handle == INVALID_HANDLE_VALUE) {
        logw(L"invalid handle");

        // If CreateDirectory returned FILE_ALREADY_EXISTS and 
        // it is called with FILE_OPEN_IF, that handle must be opened.
        handle = CreateFile(filePath.c_str(), 0, FILE_SHARE_READ, NULL, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS, NULL);
        if (handle == INVALID_HANDLE_VALUE)
        {
            DWORD dwError = GetLastError();
            logw(L"CreateFile failed(%d)", dwError);
            return ToNtStatus(dwError);
        }
        opened = TRUE;
    }

    if (!GetFileInformationByHandle(handle,HandleFileInformation)) {
        logw(L"error code = %d", GetLastError());

        // FileName is a root directory
        // in this case, FindFirstFile can't get directory information
        if (wcslen(FileName) == 1) {
            logw(L"  root dir");
            HandleFileInformation->dwFileAttributes = GetFileAttributes(filePath.c_str());

        } else {
            WIN32_FIND_DATAW find;
            ZeroMemory(&find, sizeof(WIN32_FIND_DATAW));
            handle = FindFirstFile(filePath.c_str(), &find);
            if (handle == INVALID_HANDLE_VALUE) {
                DWORD dwError = GetLastError();
                logw(L"FindFirstFile failed(%d)", dwError);
                return ToNtStatus(dwError);
            }
            HandleFileInformation->dwFileAttributes = find.dwFileAttributes;
            HandleFileInformation->ftCreationTime = find.ftCreationTime;
            HandleFileInformation->ftLastAccessTime = find.ftLastAccessTime;
            HandleFileInformation->ftLastWriteTime = find.ftLastWriteTime;
            HandleFileInformation->nFileSizeHigh = find.nFileSizeHigh;
            HandleFileInformation->nFileSizeLow = find.nFileSizeLow;
            logw(L"FindFiles OK, file size = %d", find.nFileSizeLow);
            FindClose(handle);
        }
    } else {
        logw(L"GetFileInformationByHandle success, file size = %d",
            HandleFileInformation->nFileSizeLow);
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
    logw(L"FindFiles :%s", filePath.c_str());

    hFind = FindFirstFile(filePath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        DWORD dwError = GetLastError();
        logw(L"FindFirstFile failed(%d)", dwError);
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
        logw(L"FindFirstFile failed not ERROR_NO_MORE_FILES(%d)", error);
        return ToNtStatus(error);
    }

    logw(L"FindFiles return %d entries in %s", count, filePath.c_str());

    return STATUS_SUCCESS;
}


NTSTATUS MirrorDeleteFile(
    LPCWSTR				FileName,
    PDOKAN_FILE_INFO	DokanFileInfo)
{
    HANDLE	handle = (HANDLE)DokanFileInfo->Context;

    std::wstring filePath = GetFilePath(FileName);

    logw(L"DeleteFile %s", filePath.c_str());

    return STATUS_SUCCESS;
}

NTSTATUS MirrorDeleteDirectory(
    LPCWSTR				FileName,
    PDOKAN_FILE_INFO	DokanFileInfo)
{
    logw(L"Start<%s>", FileName);
    HANDLE	handle = (HANDLE)DokanFileInfo->Context;
    HANDLE	hFind;
    WIN32_FIND_DATAW findData;
    ULONG	cchFilePath;

    std::wstring filePath = GetFilePath(FileName);

    logw(L"DeleteDirectory %s", filePath.c_str());

    filePath = AppendPathSeperatorIfNotExist(filePath, L'\\');
    filePath = filePath + L"*";

    hFind = FindFirstFile(filePath.c_str(), &findData);
    while (hFind != INVALID_HANDLE_VALUE) {
        if (wcscmp(findData.cFileName, L"..") != 0 &&
            wcscmp(findData.cFileName, L".") != 0) {
            FindClose(hFind);
            logw(L"  Directory is not empty: %s", findData.cFileName);
            return -(int)ERROR_DIR_NOT_EMPTY;
        }
        if (!FindNextFile(hFind, &findData)) {
            break;
        }
    }
    FindClose(hFind);

    DWORD dwError = GetLastError();
    if (dwError == ERROR_NO_MORE_FILES) {
        return STATUS_SUCCESS;
    } else {
        logw(L"FindFirstFile failed(%d)", dwError);
        return ToNtStatus(dwError);
    }
}


NTSTATUS MirrorMoveFile(
    LPCWSTR				FileName, // existing file name
    LPCWSTR				NewFileName,
    BOOL				ReplaceIfExisting,
    PDOKAN_FILE_INFO	DokanFileInfo)
{
    logw(L"Start. Origin<%s> Target<%s>", FileName, NewFileName);
    BOOL status;

    std::wstring filePath = GetFilePath(FileName);
    std::wstring newFilePath = GetFilePath(NewFileName);

    logw(L"MoveFile %s -> %s", filePath.c_str(), newFilePath.c_str());

    if (DokanFileInfo->Context) {
        // should close? or rename at closing?
        CloseHandle((HANDLE)DokanFileInfo->Context);
        DokanFileInfo->Context = 0;
    }

    if (ReplaceIfExisting)
        status = MoveFileEx(filePath.c_str(), newFilePath.c_str(), MOVEFILE_REPLACE_EXISTING);
    else
        status = MoveFile(filePath.c_str(), newFilePath.c_str());

    if (status == FALSE) {
        DWORD error = GetLastError();
        logw(L"MoveFile failed code = %d", error);
        return ToNtStatus(error);
    } else {
        return STATUS_SUCCESS;
    }
}

NTSTATUS MirrorLockFile(
    LPCWSTR				FileName,
    LONGLONG			ByteOffset,
    LONGLONG			Length,
    PDOKAN_FILE_INFO	DokanFileInfo)
{
    HANDLE	handle;
    LARGE_INTEGER offset;
    LARGE_INTEGER length;

    std::wstring filePath = GetFilePath(FileName);

    logw(L"LockFile %s", filePath.c_str());

    handle = (HANDLE)DokanFileInfo->Context;
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        return STATUS_INVALID_HANDLE;
    }

    length.QuadPart = Length;
    offset.QuadPart = ByteOffset;

    if (LockFile(handle, offset.HighPart, offset.LowPart, length.HighPart, length.LowPart)) {
        logw(L"success");
        return STATUS_SUCCESS;
    } else {
        DWORD dwError = GetLastError();
        logw(L"failed(%d)", dwError);
        return ToNtStatus(dwError);
    }
}

NTSTATUS MirrorSetEndOfFile(
    LPCWSTR				FileName,
    LONGLONG			ByteOffset,
    PDOKAN_FILE_INFO	DokanFileInfo)
{
    HANDLE			handle;
    LARGE_INTEGER	offset;

    std::wstring filePath = GetFilePath(FileName);

    logw(L"SetEndOfFile %s, %I64d", filePath.c_str(), ByteOffset);

    handle = (HANDLE)DokanFileInfo->Context;
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        return STATUS_INVALID_HANDLE;
    }

    offset.QuadPart = ByteOffset;
    if (!SetFilePointerEx(handle, offset, NULL, FILE_BEGIN)) {
        DWORD dwError = GetLastError();
        logw(L"SetFilePointerEx failed(%d)", dwError);
        return ToNtStatus(dwError);
    }

    if (!SetEndOfFile(handle)) {
        DWORD dwError = GetLastError();
        logw(L"SetEndOfFile failed(%d)", dwError);
        return ToNtStatus(dwError);
    }

    return STATUS_SUCCESS;
}


NTSTATUS MirrorSetAllocationSize(
    LPCWSTR				FileName,
    LONGLONG			AllocSize,
    PDOKAN_FILE_INFO	DokanFileInfo)
{
    HANDLE			handle;
    LARGE_INTEGER	fileSize;

    std::wstring filePath = GetFilePath(FileName);

    logw(L"SetAllocationSize %s, %I64d", filePath.c_str(), AllocSize);

    handle = (HANDLE)DokanFileInfo->Context;
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        return STATUS_INVALID_HANDLE;
    }

    if (GetFileSizeEx(handle, &fileSize)) {
        if (AllocSize < fileSize.QuadPart) {
            fileSize.QuadPart = AllocSize;
            if (!SetFilePointerEx(handle, fileSize, NULL, FILE_BEGIN))
            {    
                DWORD dwError = GetLastError();
                logw(L"SetAllocationSize: SetFilePointer failed(%d), offset = %I64d", dwError, AllocSize);
                return ToNtStatus(dwError);
            }
            if (!SetEndOfFile(handle)) {
                DWORD dwError = GetLastError();
                logw(L"SetEndOfFile failed(%d)", dwError);
                return ToNtStatus(dwError);
            }
        }
    } else {
        DWORD error = GetLastError();
        logw(L"error code = %d", error);
        return ToNtStatus(error);
    }
    return STATUS_SUCCESS;
}

NTSTATUS MirrorSetFileAttributes(
    LPCWSTR				FileName,
    DWORD				FileAttributes,
    PDOKAN_FILE_INFO	DokanFileInfo)
{
    std::wstring filePath = GetFilePath(FileName);

    logw(L"SetFileAttributes %s", filePath.c_str());

    if (!SetFileAttributes(filePath.c_str(), FileAttributes)) {
        DWORD error = GetLastError();
        logw(L"error code = %d", error);
        return ToNtStatus(error);
    }

    logw(L"");
    return STATUS_SUCCESS;
}

NTSTATUS MirrorSetFileTime(
    LPCWSTR				FileName,
    CONST FILETIME*		CreationTime,
    CONST FILETIME*		LastAccessTime,
    CONST FILETIME*		LastWriteTime,
    PDOKAN_FILE_INFO	DokanFileInfo)
{
    HANDLE	handle;

    std::wstring filePath = GetFilePath(FileName);

    logw(L"SetFileTime %s", filePath.c_str());

    handle = (HANDLE)DokanFileInfo->Context;

    if (!handle || handle == INVALID_HANDLE_VALUE) {
        return STATUS_INVALID_HANDLE;
    }

    if (!SetFileTime(handle, CreationTime, LastAccessTime, LastWriteTime)) {
        DWORD error = GetLastError();
        logw(L"error code = %d", error);
        return ToNtStatus(error);
    }

    logw(L"");
    return STATUS_SUCCESS;
}

NTSTATUS MirrorUnlockFile(
    LPCWSTR				FileName,
    LONGLONG			ByteOffset,
    LONGLONG			Length,
    PDOKAN_FILE_INFO	DokanFileInfo)
{
    HANDLE	handle;
    LARGE_INTEGER	length;
    LARGE_INTEGER	offset;

    std::wstring filePath = GetFilePath(FileName);

    logw(L"UnlockFile %s", filePath.c_str());

    handle = (HANDLE)DokanFileInfo->Context;
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        return STATUS_INVALID_HANDLE;
    }

    length.QuadPart = Length;
    offset.QuadPart = ByteOffset;

    if (UnlockFile(handle, offset.HighPart, offset.LowPart, length.HighPart, length.LowPart)) {
        logw(L"success");
        return STATUS_SUCCESS;
    } else {
        DWORD error = GetLastError();
        logw(L"error code = %d", error);
        return ToNtStatus(error);
    }
}

NTSTATUS MirrorGetFileSecurity(
    LPCWSTR					FileName,
    PSECURITY_INFORMATION	SecurityInformation,
    PSECURITY_DESCRIPTOR	SecurityDescriptor,
    ULONG				BufferLength,
    PULONG				LengthNeeded,
    PDOKAN_FILE_INFO	DokanFileInfo)
{
    HANDLE	handle;
    std::wstring filePath = GetFilePath(FileName);

    logw(L"GetFileSecurity %s", filePath.c_str());

    handle = (HANDLE)DokanFileInfo->Context;
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        return STATUS_INVALID_HANDLE;
    }

    if (!GetUserObjectSecurity(handle, SecurityInformation, SecurityDescriptor,
            BufferLength, LengthNeeded)) {
        int error = GetLastError();
        if (error == ERROR_INSUFFICIENT_BUFFER) {
            logw(L"  GetUserObjectSecurity failed: ERROR_INSUFFICIENT_BUFFER");
            return STATUS_BUFFER_OVERFLOW;
        } else {
            logw(L"  GetUserObjectSecurity failed: %d", error);
            return ToNtStatus(error);
        }
    }
    return STATUS_SUCCESS;
}

NTSTATUS MirrorSetFileSecurity(
    LPCWSTR					FileName,
    PSECURITY_INFORMATION	SecurityInformation,
    PSECURITY_DESCRIPTOR	SecurityDescriptor,
    ULONG				/*SecurityDescriptorLength*/,
    PDOKAN_FILE_INFO	DokanFileInfo)
{
    HANDLE	handle;
    std::wstring filePath = GetFilePath(FileName);

    logw(L"SetFileSecurity %s", filePath.c_str());

    handle = (HANDLE)DokanFileInfo->Context;
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        logw(L"invalid handle");
        return STATUS_INVALID_HANDLE;
    }

    if (!SetUserObjectSecurity(handle, SecurityInformation, SecurityDescriptor)) {
        int error = GetLastError();
        logw(L"  SetUserObjectSecurity failed: %d", error);
        return ToNtStatus(error);
    }
    return STATUS_SUCCESS;
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
    wcscpy_s(VolumeNameBuffer, VolumeNameSize / sizeof(WCHAR), L"DOKAN");
    *VolumeSerialNumber = 0x19831116;
    *MaximumComponentLength = 256;
    *FileSystemFlags = FILE_CASE_SENSITIVE_SEARCH | 
                        FILE_CASE_PRESERVED_NAMES | 
                        FILE_SUPPORTS_REMOTE_STORAGE |
                        FILE_UNICODE_ON_DISK |
                        FILE_PERSISTENT_ACLS;

    wcscpy_s(FileSystemNameBuffer, FileSystemNameSize / sizeof(WCHAR), L"Dokan");

    return STATUS_SUCCESS;
}

NTSTATUS MirrorUnmount(
    PDOKAN_FILE_INFO	DokanFileInfo)
{
    logw(L"Unmount");
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

    g_DebugMode = FALSE;
    g_UseStdErr = FALSE;

    ZeroMemory(dokanOptions, sizeof(DOKAN_OPTIONS));
    dokanOptions->Version = DOKAN_VERSION;
    dokanOptions->ThreadCount = 0; // use default

    for (command = 1; command < argc; command++) {
        switch (towlower(argv[command][1])) {
        case L'r':
            command++;
            wcscpy_s(g_RootDirectory, _countof(g_RootDirectory), argv[command]);
            logw(L"RootDirectory: %ls", g_RootDirectory);
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
        case L'd':
            g_DebugMode = TRUE;
            break;
        case L's':
            g_UseStdErr = TRUE;
            break;
        case L'n':
            dokanOptions->Options |= DOKAN_OPTION_NETWORK;
            break;
        case L'm':
            dokanOptions->Options |= DOKAN_OPTION_REMOVABLE;
            break;
        default:
            fwprintf(stderr, L"unknown command: %s", argv[command]);
            free(dokanOperations);
            free(dokanOptions);
            return EXIT_FAILURE;
        }
    }

    if (g_DebugMode) {
        dokanOptions->Options |= DOKAN_OPTION_DEBUG;
    }
    if (g_UseStdErr) {
        dokanOptions->Options |= DOKAN_OPTION_STDERR;
    }

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
    dokanOperations->FindFilesWithPattern = nullptr;
    dokanOperations->SetFileAttributes = MirrorSetFileAttributes;
    dokanOperations->SetFileTime = MirrorSetFileTime;
    dokanOperations->DeleteFile = MirrorDeleteFile;
    dokanOperations->DeleteDirectory = MirrorDeleteDirectory;
    dokanOperations->MoveFile = MirrorMoveFile;
    dokanOperations->SetEndOfFile = MirrorSetEndOfFile;
    dokanOperations->SetAllocationSize = MirrorSetAllocationSize;
    dokanOperations->LockFile = MirrorLockFile;
    dokanOperations->UnlockFile = MirrorUnlockFile;
    dokanOperations->GetFileSecurity = MirrorGetFileSecurity;
    dokanOperations->SetFileSecurity = MirrorSetFileSecurity;
    dokanOperations->GetDiskFreeSpace = nullptr;
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
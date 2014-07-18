#include "squashfuse.h"

#include <shlwapi.h>

#include <map>

#include <pfmapi.h>
#include <pfmmarshaller.h>

#define SQFS_PFM_NAME "squashfuse_pfm"
#define SQFS_PFM_WNAME L"squashfuse_pfm"

// Create sync, unidirectional pipe. Return true on success
static int sqfs_pfm_pipe(HANDLE* read, HANDLE* write);

// Mount a filesystem
static int sqfs_pfm_mount(PfmReadOnlyFormatterOps *ops, wchar_t *mountpoint);

// Convert times
static int64_t sqfs_pfm_time(time_t t);

// Fill attribute structure
static void sqfs_pfm_attribs(const sqfs_inode &inode, PfmAttribs *att);


static const wchar_t helloFileName[] = L"readme.txt";
static const char helloData[] = "Hello world.\r\n";
static const size_t helloDataSize = sizeof(helloData)-sizeof(helloData[0]);


typedef int64_t open_id_t;
typedef int64_t list_id_t;
typedef int64_t seq_num_t;
struct open_file {
  sqfs_inode inode;
  seq_num_t seq;
  std::map<list_id_t,sqfs_dir> lists;

  open_file(const sqfs_inode &ino) : inode(ino), seq(0) { }
  open_file() { }

  void fill_open(PfmOpenAttribs *att) {
    att->openSequence = seq;
    att->accessLevel = pfmAccessLevelReadData;
    sqfs_pfm_attribs(inode, &att->attribs);
  }
};

struct sqfs_pfm_ops : PfmReadOnlyFormatterOps {
  int64_t folderOpenId;
  int64_t fileOpenId;

  sqfs fs;
  sqfs_inode root;
  std::wstring volname;

  // open file tracking
  std::map<open_id_t, open_file> open_files;
  // reverse index
  std::map<sqfs_inode_num, open_id_t> open_inodes;
  
  
  sqfs_pfm_ops() : folderOpenId(0), fileOpenId(0) { }
  sqfs_err init(const wchar_t *image);

  // Lookup a name
  sqfs_err lookup(sqfs_inode &inode, const PfmNamePart *names, size_t count,
    bool &found);

  void __cdecl ReleaseName(wchar_t *name);
  int __cdecl Open(const PfmNamePart *nameParts, size_t namePartCount,
    int8_t accessLevel, int64_t newOpenId, PfmOpenAttribs *openAttribs,
    wchar_t **endName);
  int __cdecl Close(int64_t openId, int64_t openSequence);
  int __cdecl List(int64_t openId, int64_t listId,
    PfmMarshallerListResult *listResult);
  int __cdecl ListEnd(int64_t openId, int64_t listId);
  int __cdecl Read(int64_t openId, uint64_t fileOffset, void *data,
    size_t requestedSize, size_t *outActualSize);
  int __cdecl Capacity(uint64_t *totalCapacity);
  int __cdecl FlushMedia(uint8_t *mediaClean);
  int __cdecl Control(int64_t openId, int8_t accessLevel, int controlCode,
    const void *input, size_t inputSize, void *output, size_t maxOutputSize,
    size_t *outputSize);
  int __cdecl MediaInfo(int64_t openId, PfmMediaInfo *mediaInfo,
    wchar_t **mediaLabel);
  int __cdecl Access(int64_t openId, int8_t accessLevel,
    PfmOpenAttribs *openAttribs);  
};

sqfs_err sqfs_pfm_ops::lookup(sqfs_inode &inode, const PfmNamePart *names,
    size_t count, bool &found) {
  // FIXME: symlinks?
  sqfs_dir_entry entry;
  sqfs_name namebuf;
  sqfs_dentry_init(&entry, namebuf);

  sqfs_err err;
  inode = root;
  for (size_t i = 0; i < count; ++i) {
    const PfmNamePart *part = names + i;
    err = sqfs_dir_lookup(&fs, &root, part->name8, part->len8, &entry,
      &found);
    if (err)
      return err;
    if (!found)
      return SQFS_OK;

    if ((err = sqfs_inode_get(&fs, &inode, sqfs_dentry_inode(&entry))))
      return err;
  }
  return SQFS_OK;
}

void __cdecl sqfs_pfm_ops::ReleaseName(wchar_t *name) {
  free(name);
}

int __cdecl sqfs_pfm_ops::Open(const PfmNamePart *nameParts,
    size_t namePartCount, int8_t accessLevel, int64_t newOpenId,
    PfmOpenAttribs *openAttribs, wchar_t **endName) {
  // Find it
  sqfs_err err;
  bool found;
  sqfs_inode inode;
  if ((err = lookup(inode, nameParts, namePartCount, found)))
    return pfmErrorFailed;
  if (!found)
    return pfmErrorNotFound;

  // check if we have it
  sqfs_inode_num num = inode.base.inode_number;
  open_id_t oid = newOpenId;
  if (open_inodes.count(num)) {
    oid = open_inodes[num];
  } else {
    open_inodes[num] = oid;
    open_files[oid] = open_file(inode);
  }
  open_file &of = open_files[oid];
  of.seq++;

  // Fill openAttribs
  openAttribs->openId = oid;
  of.fill_open(openAttribs);
  
  return pfmErrorSuccess;
}

int __cdecl sqfs_pfm_ops::Close(int64_t openId, int64_t openSequence) {
  if (!open_files.count(openId))
    return pfmErrorSuccess;

  open_file &of = open_files[openId];
  if (openSequence < of.seq)
    return pfmErrorSuccess;

  open_inodes.erase(of.inode.base.inode_number);
  open_files.erase(openId);
  return pfmErrorSuccess;
}

int __cdecl sqfs_pfm_ops::List(int64_t openId, int64_t listId,
    PfmMarshallerListResult *listResult) {
  if (!open_files.count(openId))
    return pfmErrorNotFound;
  open_file &of = open_files[openId];
  if (!S_ISDIR(of.inode.base.mode))
    return pfmErrorNotAFolder;
  
  // Find if it's an open listId
  if (!of.lists.count(listId)) {
    sqfs_dir dir;
    if (sqfs_dir_open(&fs, &of.inode, &dir, 0))
      return pfmErrorFailed;
    of.lists[listId] = dir;
  }
  sqfs_dir &dir = of.lists[listId];

  sqfs_err err;
  sqfs_inode inode;
  sqfs_dir_entry entry;
  sqfs_name namebuf;
  sqfs_dentry_init(&entry, namebuf);
  PfmAttribs att;
  while (sqfs_dir_next(&fs, &dir, &entry, &err)) {
    sqfs_dir save_dir = dir;
    if (sqfs_inode_get(&fs, &inode, sqfs_dentry_inode(&entry)))
      return pfmErrorFailed;
    
    memset(&att, 0, sizeof(att));
    sqfs_pfm_attribs(inode, &att);

    uint8_t need_more;
    uint8_t added = listResult->Add8(&att, sqfs_dentry_name(&entry),
      &need_more);
    if (!added) {
      dir = save_dir; // use this file next time
      return pfmErrorSuccess;
    }
    if (!need_more)
      return pfmErrorSuccess;
  }
  if (err)
    return pfmErrorFailed;
  listResult->NoMore();
  return pfmErrorSuccess;
}

int __cdecl sqfs_pfm_ops::ListEnd(int64_t openId, int64_t listId) {
  if (!open_files.count(openId))
    return pfmErrorSuccess;
  open_files[openId].lists.erase(listId);
  return pfmErrorSuccess;
}

int __cdecl sqfs_pfm_ops::Read(int64_t openId, uint64_t fileOffset,
    void *data, size_t requestedSize, size_t *outActualSize) {
  if (!open_files.count(openId))
    return pfmErrorNotFound;
  open_file &of = open_files[openId];
  if (!S_ISREG(of.inode.base.mode))
    return pfmErrorNotAFile;

  if (fileOffset > of.inode.xtra.reg.file_size) {
    *outActualSize = 0;
    return pfmErrorSuccess;
  }

  sqfs_off_t osize = requestedSize;
  if (sqfs_read_range(&fs, &of.inode, fileOffset, &osize, data))
    return pfmErrorFailed;
  *outActualSize = (size_t)osize;
  return pfmErrorSuccess;
}

int __cdecl sqfs_pfm_ops::Capacity(uint64_t *totalCapacity) {
  *totalCapacity = fs.sb.bytes_used; // not super useful
  return 0;
}

int __cdecl sqfs_pfm_ops::FlushMedia(uint8_t *mediaClean) {
  *mediaClean = true;
  return 0;
}

int __cdecl sqfs_pfm_ops::Control(int64_t openId, int8_t accessLevel,
    int controlCode, const void *input, size_t inputSize, void *output,
    size_t maxOutputSize, size_t *outputSize) {
   return pfmErrorInvalid;
}

int __cdecl sqfs_pfm_ops::MediaInfo(int64_t openId,
    PfmMediaInfo *mediaInfo, wchar_t **mediaLabel) {
  mediaInfo->createTime = sqfs_pfm_time(fs.sb.mkfs_time);
  *mediaLabel = wcsdup(volname.c_str());
  return 0;
}

int __cdecl sqfs_pfm_ops::Access(int64_t openId, int8_t accessLevel,
    PfmOpenAttribs *openAttribs) {
  if (!open_files.count(openId))
    return pfmErrorNotFound;
  open_files[openId].fill_open(openAttribs);
  return pfmErrorSuccess;
}

sqfs_err sqfs_pfm_ops::init(const wchar_t *image) {
  sqfs_err err;
  if ((err = sqfs_open_image(&fs, image)))
    return err;
  if ((err = sqfs_inode_get(&fs, &root, sqfs_inode_root(&fs))))
    return err;

  wchar_t *w = wcsdup(image);
  PathStripPath(w);
  PathRemoveExtension(w);
  volname = w;
  free(w);

  return SQFS_OK;
}

static int64_t sqfs_pfm_time(time_t t) {
  return Int32x32To64(t, 10000000) + 116444736000000000;
}

static void sqfs_pfm_attribs(const sqfs_inode &inode, PfmAttribs *att) {
  att->fileType = S_ISDIR(inode.base.mode) ? pfmFileTypeFolder
    : pfmFileTypeFile;
  att->fileFlags = pfmFileFlagReadOnly;
  att->fileId = inode.base.inode_number;
  att->fileSize = 0;
  if (S_ISREG(inode.base.mode))
    att->fileSize = inode.xtra.reg.file_size;

  att->writeTime = sqfs_pfm_time(inode.base.mtime);
  att->accessTime = att->createTime = att->changeTime = att->writeTime;
}

static int sqfs_pfm_pipe(HANDLE* read, HANDLE* write) {
  UUID uuid;
  UuidCreate(&uuid);
  RPC_WSTR pipe_name;
  UuidToStringW(&uuid, &pipe_name);
  wchar_t pipe_path[MAX_PATH];
  swprintf_s(pipe_path, L"\\\\.\\pipe\\%s", pipe_name);
  RpcStringFree(&pipe_name);

  DWORD timeout = 30 * 1000;
  *read = CreateNamedPipe(pipe_path,
    PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
    PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 0, 0, timeout, NULL);
  if (*read == INVALID_HANDLE_VALUE)
    return GetLastError();

  *write = CreateFile(pipe_path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
    FILE_FLAG_OVERLAPPED, NULL);
  if (*write == INVALID_HANDLE_VALUE) {
    int err = GetLastError();
    CloseHandle(*read);
    *read = INVALID_HANDLE_VALUE;
    return err;
  }

  return ERROR_SUCCESS;
}

static int sqfs_pfm_mount(PfmReadOnlyFormatterOps *ops, wchar_t *mountpoint) {
  PfmApi *pfm = NULL;
  PfmMarshaller *marshaller = NULL;
  PfmAlerter *alerter = NULL;
  PfmMount *mount = NULL;
  HANDLE rpipe = INVALID_HANDLE_VALUE, wpipe = INVALID_HANDLE_VALUE;

  PfmMountCreateParams params;
  PfmMountCreateParams_Init(&params);
  // FIXME: canonicalize? drive letter?
  params.mountFileName = mountpoint;

  int err = 0;
  if ((err = PfmApiFactory(&pfm)))
    fwprintf(stderr, L"Can't open PFM api: Error %d\n", err);
  if (!err && (err = PfmMarshallerFactory(&marshaller)))
    fwprintf(stderr, L"Can't create marshaller: Error %d\n", err);
  if (!err && (err = sqfs_pfm_pipe(&rpipe, &params.toFormatterWrite)))
    fwprintf(stderr, L"Can't create read pipe: Error %d\n", err);
  if (!err && (err = sqfs_pfm_pipe(&params.fromFormatterRead, &wpipe)))
    fwprintf(stderr, L"Can't create write pipe: Error %d\n", err);
  if (!err && (err = pfm->Alerter(params.ownerId, &alerter)))
    fwprintf(stderr, L"Can't create alerter: Error %d\n", err);
  if (!err && (err = pfm->MountCreate(&params, &mount)))
    fwprintf(stderr, L"Can't create mount: Error %d\n", err);

  if (params.toFormatterWrite != INVALID_HANDLE_VALUE)
    CloseHandle(params.toFormatterWrite);
  if (params.fromFormatterRead != INVALID_HANDLE_VALUE)
    CloseHandle(params.fromFormatterRead);

  if (!err) {
    marshaller->SetTrace(SQFS_PFM_WNAME);
    //marshaller->SetStatus(GetStdHandle(STD_OUTPUT_HANDLE));
  }

  if (!err) {
    err = marshaller->ServeReadOnly(ops, pfmVolumeFlagReadOnly,
      SQFS_PFM_NAME, rpipe, wpipe);
    if (err)
      fwprintf(stderr, L"Can't serve mount: Error %d\n", err);
  }
  
  if (mount)
    mount->Release();
  if (alerter)
    alerter->Release();
  if (wpipe != INVALID_HANDLE_VALUE)
    CloseHandle(wpipe);
  if (rpipe != INVALID_HANDLE_VALUE)
    CloseHandle(rpipe);
  if (marshaller)
    marshaller->Release();
  if (pfm)
    pfm->Release();
  PfmApiUnload();
  return err;
}

int wmain(int argc, wchar_t* argv[]) {
  // FIXME: parse args
  wchar_t *image = argv[1];
  sqfs_pfm_ops ops;
  if (ops.init(image))
    return EXIT_FAILURE;

  wchar_t *mountpoint = argv[2];
  return sqfs_pfm_mount(&ops, mountpoint);
}

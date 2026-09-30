#include "../internal/xrt_vfs.h"

#include <stdlib.h>



#if defined(XRT_FEATURE_VFS)

#define XRT_VFS_DIR_MAX_ENTRIES 8192u
#define XRT_VFS_DIR_MAX_NAME 4096u
#define XRT_VFS_DIR_MAX_NAME_BYTES (8u * 1024u * 1024u)
#define XRT_VFS_DIR_BUCKET_NONE SIZE_MAX

typedef struct xrt_vfs_generation xrt_vfs_generation;
typedef struct xrt_vfs_snapshot xrt_vfs_snapshot;

struct xrt_vfs_generation {
	volatile int32 RefCount;
	uint64 MountId;
	int32 Priority;
	xvfscase CaseMode;
	str Prefix;
	size_t PrefixSize;
	xvfsprovider_v1 Provider;
	ptr Context;
};

struct xrt_vfs_snapshot {
	volatile int32 RefCount;
	size_t Count;
	xrt_vfs_generation* Items[];
};

struct xvfs_impl {
	volatile int32 RefCount;
	xmutex Lock;
	xrt_vfs_snapshot* Snapshot;
	uint64 NextMountId;
};

struct xvfs_mount_impl {
	volatile int32 RefCount;
	xvfs Vfs;
	xrt_vfs_generation* Generation;
	bool Active;
};

typedef struct xrt_vfs_file_state {
	xrt_file_backend_ops Backend;
	xvfsfileops_v1 Provider;
	ptr State;
	xrt_vfs_generation* Generation;
	xmutex CursorLock;
	uint64 Cursor;
} xrt_vfs_file_state;

typedef struct xrt_vfs_dir_item {
	xdirentry Entry;
	str Name;
	xvfscase CaseMode;
	uint64 FoldHash;
	size_t NextHash;
} xrt_vfs_dir_item;

typedef struct xrt_vfs_dir_state {
	xrt_vfs_dir_item* Items;
	size_t Count;
	size_t Capacity;
	size_t NameBytes;
	size_t Position;
	size_t* Buckets;
	size_t BucketCount;
	xrt_vfs_generation** Generations;
	size_t GenerationCount;
	size_t GenerationCapacity;
} xrt_vfs_dir_state;

static void __xrtVfsGenerationRelease(xrt_vfs_generation* pGeneration);



/* 设置 xrt.vfs 域的稳定错误。 */
void __xrtVfsError(xerrkind Kind, xvfserror Code,
	cstr sOperation, cstr sMessage)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.vfs";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) __xrtErrorSetOwned(pError);
}



/* 拒绝非规范路径，避免 provider 再次解释平台路径语义。 */
bool __xrtVfsPath(cstr sPath, xstrview* pPath)
{
	xstrview Path;
	size_t iStart;

	if ( (sPath == NULL) || (pPath == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Path = xrtStrView(sPath);
	if ( (Path.Size == 0u) || (Path.Size > XRT_VFS_PATH_MAX) ||
		 (Path.Data[0] != '/') ||
		 ((Path.Size > 1u) && (Path.Data[Path.Size - 1u] == '/')) ||
		 !xrtUtf8Valid(Path, NULL) ) {
		__xrtVfsError(XERR_ARGUMENT, XVFS_ERROR_PATH,
			"path", "the virtual path is not canonical UTF-8");
		return false;
	}
	if ( Path.Size == 1u ) {
		*pPath = Path;
		return true;
	}
	iStart = 1u;
	for ( size_t i = 1u; i <= Path.Size; i++ ) {
		unsigned char iByte = (i == Path.Size) ? '/' :
			(unsigned char)Path.Data[i];

		if ( (i < Path.Size) &&
			 ((iByte == 0u) || (iByte == (unsigned char)'\\')) ) {
			__xrtVfsError(XERR_ARGUMENT, XVFS_ERROR_PATH,
				"path", "the virtual path contains a forbidden byte");
			return false;
		}
		if ( iByte != (unsigned char)'/' ) continue;
		if ( (i == iStart) ||
			 ((i - iStart == 1u) && (Path.Data[iStart] == '.')) ||
			 ((i - iStart == 2u) && (Path.Data[iStart] == '.') &&
			  (Path.Data[iStart + 1u] == '.')) ) {
			__xrtVfsError(XERR_ARGUMENT, XVFS_ERROR_PATH,
				"path", "the virtual path contains a non-canonical segment");
			return false;
		}
		iStart = i + 1u;
	}
	*pPath = Path;
	return true;
}



/* 验证 provider 相对路径；空路径只表示 mount 根。 */
bool __xrtVfsRelativePath(xstrview Path, bool bAllowEmpty)
{
	size_t iStart = 0u;

	if ( ((Path.Data == NULL) && (Path.Size != 0u)) ||
		 (Path.Size > (XRT_VFS_PATH_MAX - 1u)) ||
		 ((Path.Size == 0u) && !bAllowEmpty) ||
		 ((Path.Size != 0u) &&
		  ((Path.Data[0] == '/') || (Path.Data[Path.Size - 1u] == '/'))) ||
		 !xrtUtf8Valid(Path, NULL) ) {
		__xrtVfsError(XERR_ARGUMENT, XVFS_ERROR_PATH,
			"relative-path", "the provider path is not canonical UTF-8");
		return false;
	}
	for ( size_t i = 0u; i <= Path.Size; i++ ) {
		unsigned char iByte = (i == Path.Size) ? '/' :
			(unsigned char)Path.Data[i];

		if ( (i < Path.Size) &&
			 ((iByte == 0u) || (iByte == (unsigned char)'\\')) ) {
			__xrtVfsError(XERR_ARGUMENT, XVFS_ERROR_PATH,
				"relative-path", "the provider path contains a forbidden byte");
			return false;
		}
		if ( iByte != (unsigned char)'/' ) continue;
		if ( (i == iStart) ||
			 ((i - iStart == 1u) && (Path.Data[iStart] == '.')) ||
			 ((i - iStart == 2u) && (Path.Data[iStart] == '.') &&
			  (Path.Data[iStart + 1u] == '.')) ) {
			if ( (Path.Size == 0u) && bAllowEmpty ) return true;
			__xrtVfsError(XERR_ARGUMENT, XVFS_ERROR_PATH,
				"relative-path", "the provider path contains a non-canonical segment");
			return false;
		}
		iStart = i + 1u;
	}
	return true;
}



static void __xrtVfsRestoreError(xerror* pError)
{
	xrtClearError();
	if ( pError != NULL ) __xrtErrorSetOwned(pError);
}

static void __xrtVfsGenerationRef(xrt_vfs_generation* pGeneration)
{
	if ( xrtRefRetain(&pGeneration->RefCount) < 0 ) abort();
}

static void __xrtVfsGenerationRelease(xrt_vfs_generation* pGeneration)
{
	xerror* pError;

	if ( (pGeneration == NULL) ||
		 (xrtRefRelease(&pGeneration->RefCount) != 0) ) return;
	pError = xrtTakeError();
	if ( pGeneration->Provider.ContextRelease != NULL )
		pGeneration->Provider.ContextRelease(pGeneration->Context);
	xrtFree(pGeneration->Prefix);
	xrtFree(pGeneration);
	__xrtVfsRestoreError(pError);
}



static xrt_vfs_snapshot* __xrtVfsSnapshotCreate(size_t iCount)
{
	xrt_vfs_snapshot* pSnapshot;
	size_t iSize;

	if ( iCount > ((SIZE_MAX - sizeof(*pSnapshot)) /
		sizeof(pSnapshot->Items[0])) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iSize = sizeof(*pSnapshot) +
		(iCount * sizeof(pSnapshot->Items[0]));
	pSnapshot = (xrt_vfs_snapshot*)xrtMalloc(iSize);
	if ( pSnapshot == NULL ) return NULL;
	pSnapshot->RefCount = 1;
	pSnapshot->Count = iCount;
	return pSnapshot;
}

static void __xrtVfsSnapshotRef(xrt_vfs_snapshot* pSnapshot)
{
	if ( xrtRefRetain(&pSnapshot->RefCount) < 0 ) abort();
}

static void __xrtVfsSnapshotRelease(xrt_vfs_snapshot* pSnapshot)
{
	if ( (pSnapshot == NULL) ||
		 (xrtRefRelease(&pSnapshot->RefCount) != 0) ) return;
	for ( size_t i = 0u; i < pSnapshot->Count; i++ )
		__xrtVfsGenerationRelease(pSnapshot->Items[i]);
	xrtFree(pSnapshot);
}



static int __xrtVfsGenerationCompare(const void* pLeft, const void* pRight)
{
	const xrt_vfs_generation* pA =
		*(xrt_vfs_generation* const*)pLeft;
	const xrt_vfs_generation* pB =
		*(xrt_vfs_generation* const*)pRight;

	if ( pA->PrefixSize != pB->PrefixSize )
		return (pA->PrefixSize > pB->PrefixSize) ? -1 : 1;
	if ( pA->Priority != pB->Priority )
		return (pA->Priority > pB->Priority) ? -1 : 1;
	if ( pA->MountId == pB->MountId ) return 0;
	return (pA->MountId < pB->MountId) ? -1 : 1;
}



static xrt_vfs_snapshot* __xrtVfsSnapshotEdit(
	const xrt_vfs_snapshot* pOld,
	xrt_vfs_generation* pAdd,
	const xrt_vfs_generation* pRemove)
{
	size_t iCount = pOld->Count + ((pAdd != NULL) ? 1u : 0u) -
		((pRemove != NULL) ? 1u : 0u);
	xrt_vfs_snapshot* pNew = __xrtVfsSnapshotCreate(iCount);
	size_t iOutput = 0u;

	if ( pNew == NULL ) return NULL;
	for ( size_t i = 0u; i < pOld->Count; i++ ) {
		if ( pOld->Items[i] == pRemove ) continue;
		pNew->Items[iOutput] = pOld->Items[i];
		__xrtVfsGenerationRef(pNew->Items[iOutput++]);
	}
	if ( pAdd != NULL ) {
		pNew->Items[iOutput++] = pAdd;
		__xrtVfsGenerationRef(pAdd);
	}
	if ( iOutput != iCount ) abort();
	if ( iCount > 1u ) qsort(pNew->Items, iCount,
		sizeof(pNew->Items[0]), __xrtVfsGenerationCompare);
	return pNew;
}



static xrt_vfs_snapshot* __xrtVfsSnapshotAcquire(xvfs Vfs)
{
	xrt_vfs_snapshot* pSnapshot;

	if ( (Vfs == NULL) || !xrtMutexLock(&Vfs->Lock) ) {
		if ( Vfs == NULL ) __xrtErrorSetInvalidArgument();
		return NULL;
	}
	pSnapshot = Vfs->Snapshot;
	if ( pSnapshot != NULL ) __xrtVfsSnapshotRef(pSnapshot);
	(void)xrtMutexUnlock(&Vfs->Lock);
	if ( pSnapshot == NULL ) __xrtErrorSetInvalidState();
	return pSnapshot;
}



static bool __xrtVfsProviderCopy(
	const xvfsprovider_v1* pInput, xvfsprovider_v1* pOutput)
{
	const size_t iMinimum = offsetof(xvfsprovider_v1, OpenNative);
	size_t iCopy;
	bool bStat;
	bool bDir;
	bool bNative;

	if ( (pInput == NULL) || (pInput->Size < iMinimum) ||
		 (pInput->Version != XRT_VFS_PROVIDER_VERSION) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pOutput, 0, sizeof(*pOutput));
	iCopy = pInput->Size;
	if ( iCopy > sizeof(*pOutput) ) iCopy = sizeof(*pOutput);
	memcpy(pOutput, pInput, iCopy);
	pOutput->Size = (uint32)sizeof(*pOutput);
	if ( ((pOutput->Capabilities & ~XVFS_PROVIDER_CAPABILITIES) != 0u) ||
		 ((pOutput->ContextRetain == NULL) !=
		  (pOutput->ContextRelease == NULL)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	bStat = (pOutput->Capabilities & XVFS_PROVIDER_STAT) != 0u;
	bDir = (pOutput->Capabilities & XVFS_PROVIDER_DIRECTORY) != 0u;
	bNative = (pOutput->Capabilities & XVFS_PROVIDER_NATIVE_OPEN) != 0u;
	if ( bStat != (pOutput->Stat != NULL) ||
		 bDir != (pOutput->DirOpen != NULL) ||
		 bNative != (pOutput->OpenNative != NULL) ||
		 bNative == (pOutput->Open != NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 文件 close 后释放 provider generation；xfile 仅借用此函数地址。 */
static void __xrtVfsFileGenerationRelease(ptr pOwner)
{
	__xrtVfsGenerationRelease((xrt_vfs_generation*)pOwner);
}



static bool __xrtVfsPrefixMatch(const xrt_vfs_generation* pGeneration,
	xstrview Path, xstrview* pRelative)
{
	size_t iPrefix = pGeneration->PrefixSize;
	bool bEqual;

	if ( iPrefix > Path.Size ) return false;
	if ( pGeneration->CaseMode == XVFS_CASE_SENSITIVE ) {
		bEqual = memcmp(pGeneration->Prefix, Path.Data, iPrefix) == 0;
	} else {
		bEqual = xrtStrCaseEqual(
			(xstrview){ pGeneration->Prefix, iPrefix },
			(xstrview){ Path.Data, iPrefix });
	}
	if ( !bEqual || ((iPrefix != 1u) && (Path.Size > iPrefix) &&
		(Path.Data[iPrefix] != '/')) ) return false;
	if ( Path.Size == iPrefix ) {
		pRelative->Data = Path.Data + Path.Size;
		pRelative->Size = 0u;
	} else if ( iPrefix == 1u ) {
		pRelative->Data = Path.Data + 1u;
		pRelative->Size = Path.Size - 1u;
	} else {
		pRelative->Data = Path.Data + iPrefix + 1u;
		pRelative->Size = Path.Size - iPrefix - 1u;
	}
	return true;
}



static void __xrtVfsProviderError(cstr sOperation, cstr sMessage)
{
	if ( xrtGetError() == NULL )
		__xrtVfsError(XERR_PROTOCOL, XVFS_ERROR_PROVIDER,
			sOperation, sMessage);
}



XRT_API xvfs xrtVfsCreate(void)
{
	xvfs Vfs = (xvfs)xrtCalloc(1u, sizeof(*Vfs));

	if ( Vfs == NULL ) return NULL;
	Vfs->RefCount = 1;
	if ( !xrtMutexInit(&Vfs->Lock) ) {
		xrtFree(Vfs);
		return NULL;
	}
	Vfs->Snapshot = __xrtVfsSnapshotCreate(0u);
	if ( Vfs->Snapshot == NULL ) {
		(void)xrtMutexUnit(&Vfs->Lock);
		xrtFree(Vfs);
		return NULL;
	}
	return Vfs;
}

XRT_API void xrtVfsRef(xvfs Vfs)
{
	if ( Vfs == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	if ( xrtRefRetain(&Vfs->RefCount) < 0 ) abort();
}

XRT_API void xrtVfsDestroy(xvfs Vfs)
{
	xrt_vfs_snapshot* pSnapshot;

	if ( Vfs == NULL ) return;
	if ( xrtRefRelease(&Vfs->RefCount) != 0 ) return;
	if ( !xrtMutexLock(&Vfs->Lock) ) abort();
	pSnapshot = Vfs->Snapshot;
	Vfs->Snapshot = NULL;
	(void)xrtMutexUnlock(&Vfs->Lock);
	__xrtVfsSnapshotRelease(pSnapshot);
	if ( !xrtMutexUnit(&Vfs->Lock) ) abort();
	xrtFree(Vfs);
}



XRT_API xvfsmount xrtVfsMount(xvfs Vfs, cstr sVirtualPrefix,
	int32 iPriority, xvfscase CaseMode,
	const xvfsprovider_v1* pProvider, void* pProviderContext,
	uint32 iFlags)
{
	xstrview Prefix;
	xvfsprovider_v1 Provider;
	xrt_vfs_generation* pGeneration = NULL;
	xvfsmount Mount = NULL;
	xrt_vfs_snapshot* pOld = NULL;
	xrt_vfs_snapshot* pNew = NULL;

	if ( (Vfs == NULL) || (iFlags != 0u) ||
		 ((CaseMode != XVFS_CASE_SENSITIVE) &&
		  (CaseMode != XVFS_CASE_ASCII_INSENSITIVE)) ||
		 !__xrtVfsPath(sVirtualPrefix, &Prefix) ||
		 !__xrtVfsProviderCopy(pProvider, &Provider) ) {
		if ( (Vfs == NULL) || (iFlags != 0u) ||
			 ((CaseMode != XVFS_CASE_SENSITIVE) &&
			  (CaseMode != XVFS_CASE_ASCII_INSENSITIVE)) )
			__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pGeneration = (xrt_vfs_generation*)xrtCalloc(1u, sizeof(*pGeneration));
	Mount = (xvfsmount)xrtCalloc(1u, sizeof(*Mount));
	if ( (pGeneration == NULL) || (Mount == NULL) ) goto fail;
	pGeneration->Prefix = xrtStrDupN(Prefix.Data, Prefix.Size);
	if ( pGeneration->Prefix == NULL ) goto fail;
	pGeneration->RefCount = 1;
	pGeneration->Priority = iPriority;
	pGeneration->CaseMode = CaseMode;
	pGeneration->PrefixSize = Prefix.Size;
	pGeneration->Provider = Provider;
	pGeneration->Context = pProviderContext;
	if ( Provider.ContextRetain != NULL ) Provider.ContextRetain(pProviderContext);
	Mount->RefCount = 1;
	Mount->Vfs = Vfs;
	Mount->Generation = pGeneration;
	if ( !xrtMutexLock(&Vfs->Lock) ) goto fail;
	pOld = Vfs->Snapshot;
	if ( (pOld == NULL) || (Vfs->NextMountId == UINT64_MAX) ) {
		(void)xrtMutexUnlock(&Vfs->Lock);
		__xrtVfsError(XERR_RANGE, XVFS_ERROR_LIMIT,
			"mount", "the namespace exhausted mount identifiers");
		goto fail;
	}
	pGeneration->MountId = Vfs->NextMountId + 1u;
	pNew = __xrtVfsSnapshotEdit(pOld, pGeneration, NULL);
	if ( pNew == NULL ) {
		(void)xrtMutexUnlock(&Vfs->Lock);
		goto fail;
	}
	Vfs->NextMountId = pGeneration->MountId;
	Vfs->Snapshot = pNew;
	Mount->Active = true;
	xrtVfsRef(Vfs);
	(void)xrtMutexUnlock(&Vfs->Lock);
	__xrtVfsSnapshotRelease(pOld);
	return Mount;

fail:
	if ( pGeneration != NULL ) {
		if ( pGeneration->RefCount == 0 ) {
			xrtFree(pGeneration->Prefix);
			xrtFree(pGeneration);
		} else {
			__xrtVfsGenerationRelease(pGeneration);
		}
	}
	xrtFree(Mount);
	return NULL;
}

XRT_API void xrtVfsMountRef(xvfsmount Mount)
{
	if ( Mount == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	if ( xrtRefRetain(&Mount->RefCount) < 0 ) abort();
}

XRT_API void xrtVfsMountDestroy(xvfsmount Mount)
{
	if ( (Mount == NULL) || (xrtRefRelease(&Mount->RefCount) != 0) ) return;
	__xrtVfsGenerationRelease(Mount->Generation);
	xrtVfsDestroy(Mount->Vfs);
	xrtFree(Mount);
}

XRT_API bool xrtVfsUnmount(xvfsmount Mount)
{
	xrt_vfs_snapshot* pOld;
	xrt_vfs_snapshot* pNew;
	xvfs Vfs;

	if ( Mount == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Vfs = Mount->Vfs;
	if ( !xrtMutexLock(&Vfs->Lock) ) return false;
	if ( !Mount->Active ) {
		(void)xrtMutexUnlock(&Vfs->Lock);
		return true;
	}
	pOld = Vfs->Snapshot;
	pNew = __xrtVfsSnapshotEdit(pOld, NULL, Mount->Generation);
	if ( pNew == NULL ) {
		(void)xrtMutexUnlock(&Vfs->Lock);
		return false;
	}
	Vfs->Snapshot = pNew;
	Mount->Active = false;
	(void)xrtMutexUnlock(&Vfs->Lock);
	__xrtVfsSnapshotRelease(pOld);
	return true;
}

XRT_API uint64 xrtVfsMountId(xvfsmount Mount)
{
	if ( Mount == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0u;
	}
	return Mount->Generation->MountId;
}



static bool __xrtVfsFileOpsCopy(const xvfsfile_v1* pFile,
	const xfileoptions* pOptions, xvfsfileops_v1* pOps)
{
	const xvfsfileops_v1* pInput = pFile->Ops;
	uint64 iCapabilities;

	if ( (pInput == NULL) || (pFile->State == NULL) ||
		 (pFile->Flags != pOptions->Flags) || (pFile->Reserved != 0u) ||
		 (pInput->Size < sizeof(*pOps)) ||
		 (pInput->Version != XRT_VFS_FILE_OPS_VERSION) ) return false;
	memcpy(pOps, pInput, sizeof(*pOps));
	pOps->Size = (uint32)sizeof(*pOps);
	iCapabilities = pOps->Capabilities;
	if ( ((iCapabilities & ~XVFS_FILE_CAPABILITIES) != 0u) ||
		 (pOps->Close == NULL) ||
		 (((iCapabilities & XVFS_FILE_READ) != 0u) != (pOps->Read != NULL)) ||
		 (((iCapabilities & XVFS_FILE_WRITE) != 0u) != (pOps->Write != NULL)) ||
		 (((iCapabilities & XVFS_FILE_READ_AT) != 0u) != (pOps->ReadAt != NULL)) ||
		 (((iCapabilities & XVFS_FILE_WRITE_AT) != 0u) != (pOps->WriteAt != NULL)) ||
		 (((iCapabilities & XVFS_FILE_SEEK) != 0u) != (pOps->Seek != NULL)) ||
		 (((iCapabilities & XVFS_FILE_STAT) != 0u) != (pOps->Stat != NULL)) ||
		 (((iCapabilities & XVFS_FILE_RESIZE) != 0u) != (pOps->Resize != NULL)) ||
		 (((iCapabilities & XVFS_FILE_FLUSH) != 0u) != (pOps->Flush != NULL)) )
		return false;
	if ( ((pOptions->Flags & XFILE_READ) != 0u) &&
		 ((iCapabilities & (XVFS_FILE_READ | XVFS_FILE_READ_AT)) == 0u) )
		return false;
	if ( ((pOptions->Flags & XFILE_WRITE) != 0u) &&
		 ((iCapabilities & (XVFS_FILE_WRITE | XVFS_FILE_WRITE_AT)) == 0u) )
		return false;
	if ( ((pOptions->Flags & XFILE_APPEND) != 0u) &&
		 (pOps->Write == NULL) && (pOps->Stat == NULL) ) return false;
	return true;
}



static void __xrtVfsProviderFileClose(xvfsfile_v1* pFile)
{
	xerror* pError = xrtTakeError();

	if ( (pFile->Ops != NULL) &&
		 (pFile->Ops->Size >= sizeof(xvfsfileops_v1)) &&
		 (pFile->Ops->Close != NULL) && (pFile->State != NULL) )
		pFile->Ops->Close(pFile->State);
	__xrtVfsRestoreError(pError);
}



static bool __xrtVfsFileCount(bool bResult, size_t iRequest,
	size_t iDone, size_t* pDone, cstr sOperation)
{
	if ( !bResult ) {
		__xrtVfsProviderError(sOperation,
			"the provider file callback failed without an error");
		return false;
	}
	if ( iDone > iRequest ) {
		__xrtVfsError(XERR_PROTOCOL, XVFS_ERROR_PROVIDER,
			sOperation, "the provider reported more bytes than requested");
		return false;
	}
	*pDone = iDone;
	return true;
}



static bool __xrtVfsFileRead(ptr pState, ptr pBuffer,
	size_t iRequest, size_t* pRead)
{
	xrt_vfs_file_state* pFile = (xrt_vfs_file_state*)pState;
	size_t iDone = 0u;
	bool bResult;

	if ( !xrtMutexLock(&pFile->CursorLock) ) return false;
	if ( pFile->Provider.Read != NULL ) {
		bResult = pFile->Provider.Read(pFile->State,
			pBuffer, iRequest, &iDone);
	} else {
		bResult = pFile->Provider.ReadAt(pFile->State,
			pFile->Cursor, pBuffer, iRequest, &iDone);
	}
	if ( bResult && (iDone <= iRequest) &&
		 (pFile->Cursor <= UINT64_MAX - (uint64)iDone) ) {
		pFile->Cursor += (uint64)iDone;
	} else if ( bResult && (iDone <= iRequest) ) {
		bResult = false;
		__xrtErrorSetSizeOverflow();
	}
	(void)xrtMutexUnlock(&pFile->CursorLock);
	return __xrtVfsFileCount(bResult, iRequest, iDone, pRead, "read");
}



static bool __xrtVfsFileWrite(ptr pState, const void* pBuffer,
	size_t iRequest, size_t* pWritten)
{
	xrt_vfs_file_state* pFile = (xrt_vfs_file_state*)pState;
	size_t iDone = 0u;
	bool bResult;

	if ( !xrtMutexLock(&pFile->CursorLock) ) return false;
	if ( pFile->Provider.Write != NULL ) {
		bResult = pFile->Provider.Write(pFile->State,
			pBuffer, iRequest, &iDone);
	} else {
		bResult = pFile->Provider.WriteAt(pFile->State,
			pFile->Cursor, pBuffer, iRequest, &iDone);
	}
	if ( bResult && (iDone <= iRequest) &&
		 (pFile->Cursor <= UINT64_MAX - (uint64)iDone) ) {
		pFile->Cursor += (uint64)iDone;
	} else if ( bResult && (iDone <= iRequest) ) {
		bResult = false;
		__xrtErrorSetSizeOverflow();
	}
	(void)xrtMutexUnlock(&pFile->CursorLock);
	return __xrtVfsFileCount(bResult, iRequest, iDone, pWritten, "write");
}

static bool __xrtVfsFileReadAt(ptr pState, uint64 iOffset,
	ptr pBuffer, size_t iRequest, size_t* pRead)
{
	xrt_vfs_file_state* pFile = (xrt_vfs_file_state*)pState;
	size_t iDone = 0u;
	bool bResult = pFile->Provider.ReadAt(pFile->State,
		iOffset, pBuffer, iRequest, &iDone);

	return __xrtVfsFileCount(bResult, iRequest, iDone, pRead, "read-at");
}

static bool __xrtVfsFileWriteAt(ptr pState, uint64 iOffset,
	const void* pBuffer, size_t iRequest, size_t* pWritten)
{
	xrt_vfs_file_state* pFile = (xrt_vfs_file_state*)pState;
	size_t iDone = 0u;
	bool bResult = pFile->Provider.WriteAt(pFile->State,
		iOffset, pBuffer, iRequest, &iDone);

	return __xrtVfsFileCount(bResult, iRequest, iDone, pWritten, "write-at");
}



static bool __xrtVfsSeekAdd(uint64 iBase, int64 iOffset, uint64* pPosition)
{
	if ( iOffset >= 0 ) {
		uint64 iAdd = (uint64)iOffset;
		if ( iBase > UINT64_MAX - iAdd ) return false;
		*pPosition = iBase + iAdd;
	} else {
		uint64 iSubtract = (uint64)(-(iOffset + 1)) + 1u;
		if ( iBase < iSubtract ) return false;
		*pPosition = iBase - iSubtract;
	}
	return true;
}

static bool __xrtVfsFileSeek(ptr pState, int64 iOffset,
	xseek Origin, uint64* pPosition)
{
	xrt_vfs_file_state* pFile = (xrt_vfs_file_state*)pState;
	uint64 iPosition = 0u;
	bool bResult = false;

	if ( !xrtMutexLock(&pFile->CursorLock) ) return false;
	if ( pFile->Provider.Seek != NULL ) {
		bResult = pFile->Provider.Seek(pFile->State,
			iOffset, Origin, &iPosition);
		if ( !bResult ) __xrtVfsProviderError("seek",
			"the provider seek callback failed without an error");
	} else {
		uint64 iBase;

		if ( Origin == XSEEK_START ) {
			iBase = 0u;
		} else if ( Origin == XSEEK_CURRENT ) {
			iBase = pFile->Cursor;
		} else {
			xfileinfo Info;

			if ( (pFile->Provider.Stat == NULL) ||
				 !pFile->Provider.Stat(pFile->State, &Info) ||
				 ((Info.Available & XFILE_INFO_SIZE) == 0u) ) {
				__xrtVfsProviderError("seek",
					"the provider cannot resolve seek from end");
				goto done;
			}
			iBase = Info.Size;
		}
		bResult = __xrtVfsSeekAdd(iBase, iOffset, &iPosition);
		if ( !bResult ) __xrtVfsError(XERR_RANGE, XVFS_ERROR_OPEN,
			"seek", "the requested provider file position is out of range");
	}
	if ( bResult ) {
		pFile->Cursor = iPosition;
		if ( pPosition != NULL ) *pPosition = iPosition;
	}
done:
	(void)xrtMutexUnlock(&pFile->CursorLock);
	return bResult;
}



static bool __xrtVfsFileStat(ptr pState, xfileinfo* pInfo)
{
	xrt_vfs_file_state* pFile = (xrt_vfs_file_state*)pState;
	xfileinfo Info;

	memset(&Info, 0, sizeof(Info));
	if ( !pFile->Provider.Stat(pFile->State, &Info) ) {
		__xrtVfsProviderError("file-stat",
			"the provider file stat callback failed without an error");
		return false;
	}
	*pInfo = Info;
	return true;
}

static bool __xrtVfsFileResize(ptr pState, uint64 iSize)
{
	xrt_vfs_file_state* pFile = (xrt_vfs_file_state*)pState;
	bool bResult = pFile->Provider.Resize(pFile->State, iSize);

	if ( !bResult ) __xrtVfsProviderError("resize",
		"the provider resize callback failed without an error");
	return bResult;
}

static bool __xrtVfsFileFlush(ptr pState)
{
	xrt_vfs_file_state* pFile = (xrt_vfs_file_state*)pState;
	bool bResult = pFile->Provider.Flush(pFile->State);

	if ( !bResult ) __xrtVfsProviderError("flush",
		"the provider flush callback failed without an error");
	return bResult;
}

static bool __xrtVfsFileClose(ptr pState)
{
	xrt_vfs_file_state* pFile = (xrt_vfs_file_state*)pState;

	pFile->Provider.Close(pFile->State);
	if ( !xrtMutexUnit(&pFile->CursorLock) ) abort();
	__xrtVfsGenerationRelease(pFile->Generation);
	xrtFree(pFile);
	return true;
}



static xfile __xrtVfsFileTake(xrt_vfs_generation* pGeneration,
	xvfsfile_v1* pSource, const xfileoptions* pOptions)
{
	xrt_vfs_file_state* pFile;
	xvfsfileops_v1 Provider;
	uint64 iPublic;
	uint64 iBackend = 0u;

	if ( !__xrtVfsFileOpsCopy(pSource, pOptions, &Provider) ) {
		__xrtVfsError(XERR_PROTOCOL, XVFS_ERROR_PROVIDER,
			"open", "the provider returned an invalid file contract");
		__xrtVfsProviderFileClose(pSource);
		return NULL;
	}
	pFile = (xrt_vfs_file_state*)xrtCalloc(1u, sizeof(*pFile));
	if ( pFile == NULL ) {
		__xrtVfsProviderFileClose(pSource);
		return NULL;
	}
	if ( !xrtMutexInit(&pFile->CursorLock) ) {
		xerror* pError = xrtTakeError();
		xrtFree(pFile);
		__xrtVfsProviderFileClose(pSource);
		__xrtVfsRestoreError(pError);
		return NULL;
	}
	pFile->Provider = Provider;
	pFile->State = pSource->State;
	pFile->Generation = pGeneration;
	__xrtVfsGenerationRef(pGeneration);
	iPublic = Provider.Capabilities;
	if ( (iPublic & (XVFS_FILE_READ | XVFS_FILE_READ_AT)) != 0u )
		iBackend |= XRT_FILE_BACKEND_READ;
	if ( (iPublic & (XVFS_FILE_WRITE | XVFS_FILE_WRITE_AT)) != 0u )
		iBackend |= XRT_FILE_BACKEND_WRITE;
	if ( (iPublic & XVFS_FILE_READ_AT) != 0u )
		iBackend |= XRT_FILE_BACKEND_READ_AT;
	if ( (iPublic & XVFS_FILE_WRITE_AT) != 0u )
		iBackend |= XRT_FILE_BACKEND_WRITE_AT;
	if ( ((iPublic & XVFS_FILE_SEEK) != 0u) ||
		 ((iPublic & (XVFS_FILE_READ_AT | XVFS_FILE_WRITE_AT)) != 0u) )
		iBackend |= XRT_FILE_BACKEND_SEEK;
	if ( (iPublic & XVFS_FILE_STAT) != 0u ) iBackend |= XRT_FILE_BACKEND_STAT;
	if ( (iPublic & XVFS_FILE_RESIZE) != 0u ) iBackend |= XRT_FILE_BACKEND_RESIZE;
	if ( (iPublic & XVFS_FILE_FLUSH) != 0u ) iBackend |= XRT_FILE_BACKEND_FLUSH;
	pFile->Backend.Size = (uint32)sizeof(pFile->Backend);
	pFile->Backend.Version = XRT_FILE_BACKEND_VERSION;
	pFile->Backend.Capabilities = iBackend;
	pFile->Backend.Read = ((iBackend & XRT_FILE_BACKEND_READ) != 0u) ?
		__xrtVfsFileRead : NULL;
	pFile->Backend.Write = ((iBackend & XRT_FILE_BACKEND_WRITE) != 0u) ?
		__xrtVfsFileWrite : NULL;
	pFile->Backend.ReadAt = ((iBackend & XRT_FILE_BACKEND_READ_AT) != 0u) ?
		__xrtVfsFileReadAt : NULL;
	pFile->Backend.WriteAt = ((iBackend & XRT_FILE_BACKEND_WRITE_AT) != 0u) ?
		__xrtVfsFileWriteAt : NULL;
	pFile->Backend.Seek = ((iBackend & XRT_FILE_BACKEND_SEEK) != 0u) ?
		__xrtVfsFileSeek : NULL;
	pFile->Backend.Stat = ((iBackend & XRT_FILE_BACKEND_STAT) != 0u) ?
		__xrtVfsFileStat : NULL;
	pFile->Backend.Resize = ((iBackend & XRT_FILE_BACKEND_RESIZE) != 0u) ?
		__xrtVfsFileResize : NULL;
	pFile->Backend.Flush = ((iBackend & XRT_FILE_BACKEND_FLUSH) != 0u) ?
		__xrtVfsFileFlush : NULL;
	pFile->Backend.Close = __xrtVfsFileClose;
	if ( ((pOptions->Flags & XFILE_APPEND) != 0u) &&
		 (Provider.Write == NULL) ) {
		xfileinfo Info;

		if ( !Provider.Stat(pFile->State, &Info) ||
			 ((Info.Available & XFILE_INFO_SIZE) == 0u) ) {
			__xrtVfsProviderError("open",
				"the provider cannot establish append position");
			(void)__xrtVfsFileClose(pFile);
			return NULL;
		}
		pFile->Cursor = Info.Size;
	}
	return __xrtFileTakeBackend(&pFile->Backend, pFile, pOptions->Flags);
}



XRT_API xfile xrtVfsOpen(xvfs Vfs, cstr sVirtualPath,
	const xfileoptions* pOptions)
{
	xstrview Path;
	xfileoptions Options;
	xrt_vfs_snapshot* pSnapshot;
	xerror* pBefore;

	if ( (Vfs == NULL) || !__xrtVfsPath(sVirtualPath, &Path) ||
		 !__xrtFileOptions(pOptions, &Options) ) {
		if ( Vfs == NULL ) __xrtErrorSetInvalidArgument();
		return NULL;
	}
	pSnapshot = __xrtVfsSnapshotAcquire(Vfs);
	if ( pSnapshot == NULL ) return NULL;
	pBefore = xrtTakeError();
	for ( size_t i = 0u; i < pSnapshot->Count; i++ ) {
		xrt_vfs_generation* pGeneration = pSnapshot->Items[i];
		xstrview Relative;
		xvfslookup Result;

		if ( !__xrtVfsPrefixMatch(pGeneration, Path, &Relative) ) continue;
		if ( (pGeneration->Provider.Capabilities &
			XVFS_PROVIDER_NATIVE_OPEN) != 0u ) {
			xfile Native = NULL;
			intptr_t iNative;

			xrtClearError();
			Result = pGeneration->Provider.OpenNative(
				pGeneration->Context, pGeneration->CaseMode,
				Relative, &Options, &Native);
			if ( Result == XVFS_LOOKUP_MISS ) {
				if ( Native != NULL ) {
					xrtErrorFree(pBefore);
					(void)xrtClose(Native);
					__xrtVfsSnapshotRelease(pSnapshot);
					__xrtVfsError(XERR_PROTOCOL, XVFS_ERROR_PROVIDER,
						"open", "a native provider returned a file on MISS");
					return NULL;
				}
				continue;
			}
			if ( Result == XVFS_LOOKUP_ERROR ) {
				if ( Native != NULL ) {
					(void)xrtClose(Native);
					xrtClearError();
					__xrtVfsError(XERR_PROTOCOL, XVFS_ERROR_PROVIDER,
						"open", "a native provider returned a file on ERROR");
				}
				xrtErrorFree(pBefore);
				__xrtVfsProviderError("open",
					"the native provider returned ERROR without an error object");
				__xrtVfsSnapshotRelease(pSnapshot);
				return NULL;
			}
			if ( Result != XVFS_LOOKUP_OPENED ) {
				if ( Native != NULL ) (void)xrtClose(Native);
				xrtErrorFree(pBefore);
				__xrtVfsSnapshotRelease(pSnapshot);
				__xrtVfsError(XERR_PROTOCOL, XVFS_ERROR_PROVIDER,
					"open", "the native provider returned an unknown lookup result");
				return NULL;
			}
			if ( (Native == NULL) ||
				 (xrtFileFlags(Native) != Options.Flags) ) {
				if ( Native != NULL ) (void)xrtClose(Native);
				xrtErrorFree(pBefore);
				__xrtVfsSnapshotRelease(pSnapshot);
				__xrtVfsError(XERR_PROTOCOL, XVFS_ERROR_PROVIDER,
					"open", "the native provider returned an invalid file contract");
				return NULL;
			}
			xrtClearError();
			iNative = xrtFileNative(Native);
			if ( iNative == (intptr_t)-1 ) {
				(void)xrtClose(Native);
				xrtErrorFree(pBefore);
				__xrtVfsSnapshotRelease(pSnapshot);
				__xrtVfsError(XERR_PROTOCOL, XVFS_ERROR_PROVIDER,
					"open", "the native provider returned a non-native file");
				return NULL;
			}
			(void)iNative;
			__xrtVfsGenerationRef(pGeneration);
			if ( !__xrtFileAttachOwner(Native, pGeneration,
				__xrtVfsFileGenerationRelease) ) {
				__xrtVfsGenerationRelease(pGeneration);
				(void)xrtClose(Native);
				xrtErrorFree(pBefore);
				__xrtVfsSnapshotRelease(pSnapshot);
				__xrtVfsError(XERR_PROTOCOL, XVFS_ERROR_PROVIDER,
					"open", "the native provider returned an already-owned file");
				return NULL;
			}
			__xrtVfsSnapshotRelease(pSnapshot);
			__xrtVfsRestoreError(pBefore);
			return Native;
		}
		{
			xvfsfile_v1 File;

		memset(&File, 0, sizeof(File));
		xrtClearError();
		Result = pGeneration->Provider.Open(pGeneration->Context,
			pGeneration->CaseMode, Relative, &Options, &File);
		if ( Result == XVFS_LOOKUP_MISS ) {
			if ( (File.Ops != NULL) || (File.State != NULL) ||
				 (File.Flags != 0u) || (File.Reserved != 0u) ) {
				xrtErrorFree(pBefore);
				__xrtVfsSnapshotRelease(pSnapshot);
				__xrtVfsError(XERR_PROTOCOL, XVFS_ERROR_PROVIDER,
					"open", "a provider modified file output on MISS");
				return NULL;
			}
			continue;
		}
		if ( Result == XVFS_LOOKUP_ERROR ) {
			xrtErrorFree(pBefore);
			__xrtVfsProviderError("open",
				"the provider returned ERROR without an error object");
			__xrtVfsSnapshotRelease(pSnapshot);
			return NULL;
		}
		if ( Result == XVFS_LOOKUP_OPENED ) {
			xfile Opened;

			if ( (Options.Flags & XFILE_ASYNC) != 0u ) {
				__xrtVfsProviderFileClose(&File);
				xrtErrorFree(pBefore);
				__xrtVfsSnapshotRelease(pSnapshot);
				__xrtVfsError(XERR_UNSUPPORTED, XVFS_ERROR_UNSUPPORTED,
					"open", "callback provider files do not support native async I/O");
				return NULL;
			}
			xrtClearError();
			Opened = __xrtVfsFileTake(pGeneration, &File, &Options);
			if ( Opened == NULL ) {
				xrtErrorFree(pBefore);
				__xrtVfsSnapshotRelease(pSnapshot);
				return NULL;
			}
			__xrtVfsSnapshotRelease(pSnapshot);
			__xrtVfsRestoreError(pBefore);
			return Opened;
		}
		xrtErrorFree(pBefore);
		__xrtVfsSnapshotRelease(pSnapshot);
		__xrtVfsError(XERR_PROTOCOL, XVFS_ERROR_PROVIDER,
			"open", "the provider returned an unknown lookup result");
		return NULL;
		}
	}
	xrtErrorFree(pBefore);
	__xrtVfsSnapshotRelease(pSnapshot);
	__xrtVfsError(XERR_NOT_FOUND, XVFS_ERROR_NOT_FOUND,
		"open", "the virtual path was not found");
	return NULL;
}



XRT_API bool xrtVfsStat(xvfs Vfs, cstr sVirtualPath,
	bool bFollowLink, xfileinfo* pInfo)
{
	xstrview Path;
	xrt_vfs_snapshot* pSnapshot;
	xerror* pBefore;

	if ( (Vfs == NULL) || (pInfo == NULL) ||
		 !__xrtVfsPath(sVirtualPath, &Path) ) {
		if ( (Vfs == NULL) || (pInfo == NULL) )
			__xrtErrorSetInvalidArgument();
		return false;
	}
	pSnapshot = __xrtVfsSnapshotAcquire(Vfs);
	if ( pSnapshot == NULL ) return false;
	pBefore = xrtTakeError();
	for ( size_t i = 0u; i < pSnapshot->Count; i++ ) {
		xrt_vfs_generation* pGeneration = pSnapshot->Items[i];
		xstrview Relative;
		xfileinfo Info;
		xvfslookup Result;

		if ( !__xrtVfsPrefixMatch(pGeneration, Path, &Relative) ||
			 ((pGeneration->Provider.Capabilities & XVFS_PROVIDER_STAT) == 0u) )
			continue;
		memset(&Info, 0, sizeof(Info));
		xrtClearError();
		Result = pGeneration->Provider.Stat(pGeneration->Context,
			pGeneration->CaseMode, Relative, bFollowLink, &Info);
		if ( Result == XVFS_LOOKUP_MISS ) continue;
		if ( Result == XVFS_LOOKUP_OPENED ) {
			*pInfo = Info;
			__xrtVfsSnapshotRelease(pSnapshot);
			__xrtVfsRestoreError(pBefore);
			return true;
		}
		if ( Result == XVFS_LOOKUP_ERROR ) {
			xrtErrorFree(pBefore);
			__xrtVfsProviderError("stat",
				"the provider returned ERROR without an error object");
			__xrtVfsSnapshotRelease(pSnapshot);
			return false;
		}
		xrtErrorFree(pBefore);
		__xrtVfsSnapshotRelease(pSnapshot);
		__xrtVfsError(XERR_PROTOCOL, XVFS_ERROR_PROVIDER,
			"stat", "the provider returned an unknown lookup result");
		return false;
	}
	xrtErrorFree(pBefore);
	__xrtVfsSnapshotRelease(pSnapshot);
	__xrtVfsError(XERR_NOT_FOUND, XVFS_ERROR_NOT_FOUND,
		"stat", "the virtual path was not found");
	return false;
}



static void __xrtVfsFileCleanup(xfile File)
{
	xerror* pError = xrtTakeError();
	(void)xrtClose(File);
	__xrtVfsRestoreError(pError);
}

XRT_API bytes xrtVfsReadAllLimit(xvfs Vfs, cstr sVirtualPath,
	size_t iLimit, size_t* pSize)
{
	xfileoptions Options = { XFILE_READ, 0u, XFILE_SHARE_ALL };
	size_t iUseLimit = (iLimit == SIZE_MAX) ? (SIZE_MAX - 1u) : iLimit;
	size_t iCapacity = (iUseLimit < 4096u) ? iUseLimit : 4096u;
	size_t iSize = 0u;
	bytes pBuffer;
	xfile File;

	File = xrtVfsOpen(Vfs, sVirtualPath, &Options);
	if ( File == NULL ) return NULL;
	pBuffer = (bytes)xrtMalloc(iCapacity + 1u);
	if ( pBuffer == NULL ) {
		__xrtVfsFileCleanup(File);
		return NULL;
	}
	for ( ;; ) {
		size_t iDone = 0u;

		if ( iSize == iCapacity ) {
			unsigned char iProbe;
			if ( !xrtRead(File, &iProbe, 1u, &iDone) ) goto fail;
			if ( iDone == 0u ) break;
			if ( iCapacity == iUseLimit ) {
				__xrtVfsError(XERR_RANGE, XVFS_ERROR_LIMIT,
					"read-all", "the virtual file exceeds the configured limit");
				goto fail;
			}
			{
				size_t iNext = (iCapacity == 0u) ? 1u : iCapacity * 2u;
				bytes pNext;
				if ( (iNext < iCapacity) || (iNext > iUseLimit) ) iNext = iUseLimit;
				pNext = (bytes)xrtRealloc(pBuffer, iNext + 1u);
				if ( pNext == NULL ) goto fail;
				pBuffer = pNext;
				iCapacity = iNext;
			}
			pBuffer[iSize++] = iProbe;
			continue;
		}
		if ( !xrtRead(File, pBuffer + iSize,
			iCapacity - iSize, &iDone) ) goto fail;
		if ( iDone == 0u ) break;
		iSize += iDone;
	}
	if ( !xrtClose(File) ) {
		xrtFree(pBuffer);
		return NULL;
	}
	pBuffer[iSize] = 0u;
	if ( pSize != NULL ) *pSize = iSize;
	return pBuffer;

fail:
	xrtFree(pBuffer);
	__xrtVfsFileCleanup(File);
	return NULL;
}

XRT_API bytes xrtVfsReadAll(xvfs Vfs, cstr sVirtualPath, size_t* pSize)
{
	return xrtVfsReadAllLimit(Vfs, sVirtualPath, SIZE_MAX - 1u, pSize);
}



static uint64 __xrtVfsNameHash(xstrview Name)
{
	uint64 iHash = UINT64_C(1469598103934665603);

	for ( size_t i = 0u; i < Name.Size; i++ ) {
		unsigned char iByte = (unsigned char)Name.Data[i];
		if ( (iByte >= 'A') && (iByte <= 'Z') ) iByte += 'a' - 'A';
		iHash ^= (uint64)iByte;
		iHash *= UINT64_C(1099511628211);
	}
	return iHash;
}

static bool __xrtVfsDirName(xstrview Name)
{
	if ( (Name.Data == NULL) || (Name.Size == 0u) ||
		 (Name.Size > XRT_VFS_DIR_MAX_NAME) ||
		 !xrtUtf8Valid(Name, NULL) ||
		 ((Name.Size == 1u) && (Name.Data[0] == '.')) ||
		 ((Name.Size == 2u) && (Name.Data[0] == '.') &&
		  (Name.Data[1] == '.')) ) return false;
	for ( size_t i = 0u; i < Name.Size; i++ ) {
		if ( (Name.Data[i] == 0) || (Name.Data[i] == '/') ||
			 (Name.Data[i] == '\\') ) return false;
	}
	return true;
}



static void __xrtVfsDirStateFree(xrt_vfs_dir_state* pState)
{
	if ( pState == NULL ) return;
	for ( size_t i = 0u; i < pState->Count; i++ )
		xrtFree(pState->Items[i].Name);
	for ( size_t i = 0u; i < pState->GenerationCount; i++ )
		__xrtVfsGenerationRelease(pState->Generations[i]);
	xrtFree(pState->Generations);
	xrtFree(pState->Buckets);
	xrtFree(pState->Items);
	xrtFree(pState);
}

static bool __xrtVfsDirRehash(xrt_vfs_dir_state* pState, size_t iCount)
{
	size_t* pBuckets;

	if ( (iCount == 0u) || ((iCount & (iCount - 1u)) != 0u) ||
		 (iCount > (SIZE_MAX / sizeof(size_t))) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	pBuckets = (size_t*)xrtMalloc(iCount * sizeof(size_t));
	if ( pBuckets == NULL ) return false;
	for ( size_t i = 0u; i < iCount; i++ )
		pBuckets[i] = XRT_VFS_DIR_BUCKET_NONE;
	for ( size_t i = 0u; i < pState->Count; i++ ) {
		size_t iBucket = (size_t)pState->Items[i].FoldHash & (iCount - 1u);
		pState->Items[i].NextHash = pBuckets[iBucket];
		pBuckets[iBucket] = i;
	}
	xrtFree(pState->Buckets);
	pState->Buckets = pBuckets;
	pState->BucketCount = iCount;
	return true;
}

static bool __xrtVfsDirGenerationAdd(xrt_vfs_dir_state* pState,
	xrt_vfs_generation* pGeneration)
{
	if ( pState->GenerationCount == pState->GenerationCapacity ) {
		size_t iCapacity = (pState->GenerationCapacity == 0u) ?
			4u : pState->GenerationCapacity * 2u;
		xrt_vfs_generation** pItems;

		if ( (iCapacity < pState->GenerationCapacity) ||
			 (iCapacity > SIZE_MAX / sizeof(*pItems)) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		pItems = (xrt_vfs_generation**)xrtRealloc(pState->Generations,
			iCapacity * sizeof(*pItems));
		if ( pItems == NULL ) return false;
		pState->Generations = pItems;
		pState->GenerationCapacity = iCapacity;
	}
	__xrtVfsGenerationRef(pGeneration);
	pState->Generations[pState->GenerationCount++] = pGeneration;
	return true;
}



static bool __xrtVfsDirItemAdd(xrt_vfs_dir_state* pState,
	const xdirentry* pEntry, xvfscase CaseMode)
{
	uint64 iHash;
	size_t iBucket;

	if ( !__xrtVfsDirName(pEntry->Name) ) {
		__xrtVfsError(XERR_PROTOCOL, XVFS_ERROR_PROVIDER,
			"directory", "the provider returned an invalid directory name");
		return false;
	}
	if ( (pState->Count >= XRT_VFS_DIR_MAX_ENTRIES) ||
		 (pEntry->Name.Size > XRT_VFS_DIR_MAX_NAME_BYTES - pState->NameBytes) ) {
		__xrtVfsError(XERR_RANGE, XVFS_ERROR_LIMIT,
			"directory", "the materialized directory exceeds its hard limit");
		return false;
	}
	if ( pState->BucketCount == 0u ) {
		if ( !__xrtVfsDirRehash(pState, 16u) ) return false;
	} else if ( ((pState->Count + 1u) * 4u) >=
		(pState->BucketCount * 3u) ) {
		if ( (pState->BucketCount > (SIZE_MAX / 2u)) ||
			 !__xrtVfsDirRehash(pState, pState->BucketCount * 2u) ) return false;
	}
	iHash = __xrtVfsNameHash(pEntry->Name);
	iBucket = (size_t)iHash & (pState->BucketCount - 1u);
	for ( size_t i = pState->Buckets[iBucket];
		i != XRT_VFS_DIR_BUCKET_NONE; i = pState->Items[i].NextHash ) {
		xrt_vfs_dir_item* pCurrent = &pState->Items[i];

		if ( (pCurrent->FoldHash == iHash) &&
			 xrtStrCaseEqual(pCurrent->Entry.Name, pEntry->Name) &&
			 ((pCurrent->CaseMode == XVFS_CASE_ASCII_INSENSITIVE) ||
			  (CaseMode == XVFS_CASE_ASCII_INSENSITIVE) ||
			  xrtStrEqual(pCurrent->Entry.Name, pEntry->Name)) ) return true;
	}
	if ( pState->Count == pState->Capacity ) {
		size_t iCapacity = (pState->Capacity == 0u) ? 16u :
			pState->Capacity * 2u;
		xrt_vfs_dir_item* pItems;

		if ( (iCapacity < pState->Capacity) ||
			 (iCapacity > SIZE_MAX / sizeof(*pItems)) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		pItems = (xrt_vfs_dir_item*)xrtRealloc(pState->Items,
			iCapacity * sizeof(*pItems));
		if ( pItems == NULL ) return false;
		pState->Items = pItems;
		pState->Capacity = iCapacity;
	}
	{
		xrt_vfs_dir_item* pItem = &pState->Items[pState->Count];

		memset(pItem, 0, sizeof(*pItem));
		pItem->Name = xrtStrDupN(pEntry->Name.Data, pEntry->Name.Size);
		if ( pItem->Name == NULL ) return false;
		pItem->Entry = *pEntry;
		pItem->Entry.Name.Data = pItem->Name;
		pItem->Entry.Name.Size = pEntry->Name.Size;
		pItem->Entry.Flags |= XDIR_ENTRY_UTF8;
		pItem->CaseMode = CaseMode;
		pItem->FoldHash = iHash;
		pItem->NextHash = pState->Buckets[iBucket];
		pState->Buckets[iBucket] = pState->Count;
		pState->NameBytes += pEntry->Name.Size;
		pState->Count++;
	}
	return true;
}

static int __xrtVfsDirItemCompare(const void* pLeft, const void* pRight)
{
	const xrt_vfs_dir_item* pA = (const xrt_vfs_dir_item*)pLeft;
	const xrt_vfs_dir_item* pB = (const xrt_vfs_dir_item*)pRight;
	return xrtStrCompare(pA->Entry.Name, pB->Entry.Name);
}

static xdirnext __xrtVfsDirNext(ptr pState, cstr sPath,
	uint32 iFlags, xdirentry* pEntry)
{
	xrt_vfs_dir_state* pDir = (xrt_vfs_dir_state*)pState;
	(void)sPath;
	(void)iFlags;
	if ( pDir->Position == pDir->Count ) return XDIR_NEXT_END;
	*pEntry = pDir->Items[pDir->Position++].Entry;
	return XDIR_NEXT_ITEM;
}

static bool __xrtVfsDirClose(ptr pState)
{
	__xrtVfsDirStateFree((xrt_vfs_dir_state*)pState);
	return true;
}

static const xrt_dir_backend_ops __xrtVfsDirOps = {
	(uint32)sizeof(xrt_dir_backend_ops),
	XRT_DIR_BACKEND_VERSION,
	true,
	{ 0u, 0u, 0u },
	__xrtVfsDirNext,
	__xrtVfsDirClose
};



static bool __xrtVfsProviderDirOps(const xvfsdir_v1* pDir,
	xvfsdirops_v1* pOps)
{
	if ( (pDir->Ops == NULL) || (pDir->State == NULL) ||
		 (pDir->Ops->Size < sizeof(*pOps)) ||
		 (pDir->Ops->Version != XRT_VFS_DIR_OPS_VERSION) ) return false;
	memcpy(pOps, pDir->Ops, sizeof(*pOps));
	pOps->Size = (uint32)sizeof(*pOps);
	return (pOps->Next != NULL) && (pOps->Close != NULL);
}

static void __xrtVfsProviderDirClose(xvfsdir_v1* pDir)
{
	xerror* pError = xrtTakeError();

	if ( (pDir->Ops != NULL) &&
		 (pDir->Ops->Size >= sizeof(xvfsdirops_v1)) &&
		 (pDir->Ops->Close != NULL) && (pDir->State != NULL) )
		pDir->Ops->Close(pDir->State);
	__xrtVfsRestoreError(pError);
}



XRT_API xdir xrtVfsDirOpen(xvfs Vfs, cstr sVirtualPath,
	uint32 iFlags)
{
	xstrview Path;
	xrt_vfs_snapshot* pSnapshot;
	xrt_vfs_dir_state* pState;
	xerror* pBefore;
	bool bOpened = false;

	if ( (Vfs == NULL) || !__xrtDirFlagsValid(iFlags) ||
		 !__xrtVfsPath(sVirtualPath, &Path) ) {
		if ( Vfs == NULL ) __xrtErrorSetInvalidArgument();
		return NULL;
	}
	pSnapshot = __xrtVfsSnapshotAcquire(Vfs);
	if ( pSnapshot == NULL ) return NULL;
	pState = (xrt_vfs_dir_state*)xrtCalloc(1u, sizeof(*pState));
	if ( pState == NULL ) {
		__xrtVfsSnapshotRelease(pSnapshot);
		return NULL;
	}
	pBefore = xrtTakeError();
	for ( size_t i = 0u; i < pSnapshot->Count; i++ ) {
		xrt_vfs_generation* pGeneration = pSnapshot->Items[i];
		xstrview Relative;
		xvfsdir_v1 Dir;
		xvfsdirops_v1 Ops;
		xvfslookup Result;

		if ( !__xrtVfsPrefixMatch(pGeneration, Path, &Relative) ||
			 ((pGeneration->Provider.Capabilities & XVFS_PROVIDER_DIRECTORY) == 0u) )
			continue;
		memset(&Dir, 0, sizeof(Dir));
		xrtClearError();
		Result = pGeneration->Provider.DirOpen(pGeneration->Context,
			pGeneration->CaseMode, Relative, iFlags, &Dir);
		if ( Result == XVFS_LOOKUP_MISS ) {
			if ( (Dir.Ops != NULL) || (Dir.State != NULL) ) {
				__xrtVfsError(XERR_PROTOCOL, XVFS_ERROR_PROVIDER,
					"directory", "a provider modified directory output on MISS");
				goto fail;
			}
			continue;
		}
		if ( Result == XVFS_LOOKUP_ERROR ) {
			__xrtVfsProviderError("directory",
				"the provider returned ERROR without an error object");
			goto fail;
		}
		if ( (Result != XVFS_LOOKUP_OPENED) ||
			 !__xrtVfsProviderDirOps(&Dir, &Ops) ) {
			__xrtVfsError(XERR_PROTOCOL, XVFS_ERROR_PROVIDER,
				"directory", "the provider returned an invalid directory contract");
			if ( Result == XVFS_LOOKUP_OPENED ) __xrtVfsProviderDirClose(&Dir);
			goto fail;
		}
		bOpened = true;
		if ( !__xrtVfsDirGenerationAdd(pState, pGeneration) ) {
			__xrtVfsProviderDirClose(&Dir);
			goto fail;
		}
		for ( ;; ) {
			xdirentry Entry;
			bool bEnd = false;

			memset(&Entry, 0, sizeof(Entry));
			xrtClearError();
			if ( !Ops.Next(Dir.State, &Entry, &bEnd) ) {
				__xrtVfsProviderError("directory-next",
					"the provider directory callback failed without an error");
				__xrtVfsProviderDirClose(&Dir);
				goto fail;
			}
			if ( bEnd ) break;
			if ( !__xrtVfsDirItemAdd(pState, &Entry,
				pGeneration->CaseMode) ) {
				__xrtVfsProviderDirClose(&Dir);
				goto fail;
			}
		}
		Ops.Close(Dir.State);
	}
	if ( !bOpened ) {
		xrtErrorFree(pBefore);
		__xrtVfsSnapshotRelease(pSnapshot);
		__xrtVfsDirStateFree(pState);
		__xrtVfsError(XERR_NOT_FOUND, XVFS_ERROR_NOT_FOUND,
			"directory", "the virtual directory was not found");
		return NULL;
	}
	xrtFree(pState->Buckets);
	pState->Buckets = NULL;
	pState->BucketCount = 0u;
	if ( pState->Count > 1u ) qsort(pState->Items, pState->Count,
		sizeof(pState->Items[0]), __xrtVfsDirItemCompare);
	__xrtVfsSnapshotRelease(pSnapshot);
	{
		xdir Result = __xrtDirTakeBackend(&__xrtVfsDirOps,
			pState, sVirtualPath, iFlags);

		if ( Result == NULL ) {
			xrtErrorFree(pBefore);
			return NULL;
		}
		__xrtVfsRestoreError(pBefore);
		return Result;
	}

fail:
	xrtErrorFree(pBefore);
	__xrtVfsSnapshotRelease(pSnapshot);
	__xrtVfsDirStateFree(pState);
	return NULL;
}

#endif

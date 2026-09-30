#include "../internal/xrt_vfs.h"
#include "../internal/xrt_file_root.h"

#include <stdlib.h>



#if defined(XRT_FEATURE_VFS_DISK)

typedef struct xrt_vfs_disk_entry {
	str Name;
	size_t NameSize;
	xfileinfo Info;
} xrt_vfs_disk_entry;

typedef struct xrt_vfs_disk_dir {
	xrt_vfs_disk_entry* Items;
	size_t Count;
	size_t Capacity;
	size_t Position;
} xrt_vfs_disk_dir;

struct xvfs_disk_impl {
	volatile int32 RefCount;
	xroot Root;
	uint32 Access;
};



/* 把 root 的不存在错误转换为可回退 MISS。 */
static xvfslookup __xrtVfsDiskFailure(void)
{
	const xerror* pError = xrtGetError();

	if ( (pError != NULL) &&
		 (xrtErrorKind(pError) == XERR_NOT_FOUND) ) {
		xrtClearError();
		return XVFS_LOOKUP_MISS;
	}
	return XVFS_LOOKUP_ERROR;
}



/* 检查一次文件打开所需的冻结权限。 */
static bool __xrtVfsDiskOpenAccess(xvfsdisk Disk,
	const xfileoptions* pOptions)
{
	uint32 iWrite = XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE |
		XFILE_APPEND | XFILE_EXCLUSIVE | XFILE_SYNC;

	if ( ((pOptions->Flags & XFILE_READ) != 0u) &&
		 ((Disk->Access & XVFS_DISK_READ) == 0u) ) {
		__xrtVfsError(XERR_PERMISSION, XVFS_ERROR_OPEN,
			"disk-open", "the disk provider does not grant read access");
		return false;
	}
	if ( ((pOptions->Flags & iWrite) != 0u) &&
		 ((Disk->Access & XVFS_DISK_WRITE) == 0u) ) {
		__xrtVfsError(XERR_PERMISSION, XVFS_ERROR_OPEN,
			"disk-open", "the disk provider does not grant write access");
		return false;
	}
	return true;
}



/* 释放 ASCII-insensitive 解析过程中取得的子根。 */
static bool __xrtVfsDiskCloseChild(xroot* pOwned)
{
	bool bResult;

	if ( *pOwned == NULL ) return true;
	bResult = xrtRootClose(*pOwned);
	*pOwned = NULL;
	return bResult;
}



/* 按 ASCII 折叠逐分量选择唯一的实际磁盘名称。 */
static xvfslookup __xrtVfsDiskResolveFolded(xvfsdisk Disk,
	xstrview RelativePath, bool bAllowMissingFinal, str* pPath)
{
	str sPath;
	size_t iStart = 0u;
	size_t iWrite = 0u;
	xroot Current = Disk->Root;
	xroot Owned = NULL;
	xvfslookup Result = XVFS_LOOKUP_ERROR;

	if ( RelativePath.Size == 0u ) {
		*pPath = xrtStrDup(".");
		return *pPath != NULL ? XVFS_LOOKUP_OPENED : XVFS_LOOKUP_ERROR;
	}
	if ( RelativePath.Size == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return XVFS_LOOKUP_ERROR;
	}
	sPath = (str)xrtMalloc(RelativePath.Size + 1u);
	if ( sPath == NULL ) return XVFS_LOOKUP_ERROR;
	while ( iStart < RelativePath.Size ) {
		size_t iEnd = iStart;
		size_t iOutputStart = iWrite;
		xstrview Requested;
		xdir Dir;
		xdirentry Entry;
		xdirnext Next;
		size_t iMatches = 0u;
		bool bFinal;

		while ( (iEnd < RelativePath.Size) &&
			 (RelativePath.Data[iEnd] != '/') ) iEnd++;
		Requested.Data = RelativePath.Data + iStart;
		Requested.Size = iEnd - iStart;
		bFinal = iEnd == RelativePath.Size;
		Dir = __xrtRootDirOpenPolicy(Current, ".", 0u,
			XROOT_POLICY_CASE_SENSITIVE);
		if ( Dir == NULL ) goto done;
		while ( (Next = xrtDirNext(Dir, &Entry)) == XDIR_NEXT_ITEM ) {
			if ( ((Entry.Flags & XDIR_ENTRY_UTF8) == 0u) ||
				 !xrtStrCaseEqual(Entry.Name, Requested) ) continue;
			iMatches++;
			if ( iMatches == 1u ) {
				memcpy(sPath + iWrite,
					Entry.Name.Data, Entry.Name.Size);
				iWrite += Entry.Name.Size;
			}
		}
		if ( Next == XDIR_NEXT_ERROR ) {
			(void)xrtDirClose(Dir);
			goto done;
		}
		if ( !xrtDirClose(Dir) ) goto done;
		if ( iMatches > 1u ) {
			__xrtVfsError(XERR_EXISTS, XVFS_ERROR_OPEN,
				"disk-resolve", "the disk path is ambiguous under ASCII folding");
			goto done;
		}
		if ( iMatches == 0u ) {
			if ( !bFinal || !bAllowMissingFinal ) {
				Result = XVFS_LOOKUP_MISS;
				goto done;
			}
			memcpy(sPath + iWrite, Requested.Data, Requested.Size);
			iWrite += Requested.Size;
		}
		sPath[iWrite] = '\0';
		if ( !bFinal ) {
			xroot Child = __xrtRootOpenInPolicy(Current,
				sPath + iOutputStart, XROOT_POLICY_CASE_SENSITIVE);

			if ( Child == NULL ) {
				Result = __xrtVfsDiskFailure();
				goto done;
			}
			if ( !__xrtVfsDiskCloseChild(&Owned) ) {
				(void)xrtRootClose(Child);
				goto done;
			}
			Owned = Child;
			Current = Child;
			sPath[iWrite++] = '/';
		}
		iStart = iEnd + 1u;
	}
	sPath[iWrite] = '\0';
	*pPath = sPath;
	sPath = NULL;
	Result = XVFS_LOOKUP_OPENED;

done:
	if ( !__xrtVfsDiskCloseChild(&Owned) ) Result = XVFS_LOOKUP_ERROR;
	xrtFree(sPath);
	return Result;
}



/* 生成根 API 接受的实际相对路径。 */
static xvfslookup __xrtVfsDiskResolve(xvfsdisk Disk,
	xvfscase CaseMode, xstrview RelativePath,
	bool bAllowMissingFinal, str* pPath)
{
	*pPath = NULL;
	if ( CaseMode == XVFS_CASE_ASCII_INSENSITIVE ) {
		return __xrtVfsDiskResolveFolded(Disk,
			RelativePath, bAllowMissingFinal, pPath);
	}
	if ( CaseMode != XVFS_CASE_SENSITIVE ) {
		__xrtErrorSetInvalidArgument();
		return XVFS_LOOKUP_ERROR;
	}
	*pPath = RelativePath.Size == 0u ?
		xrtStrDup(".") : xrtStrDupView(RelativePath);
	return *pPath != NULL ? XVFS_LOOKUP_OPENED : XVFS_LOOKUP_ERROR;
}



/* 解析并严格查询对象；可为末级创建保留不存在结果。 */
static xvfslookup __xrtVfsDiskResolveStat(xvfsdisk Disk,
	xvfscase CaseMode, xstrview RelativePath,
	bool bAllowMissingFinal, str* pPath,
	xfileinfo* pInfo, bool* pExists)
{
	xvfslookup Result = __xrtVfsDiskResolve(Disk,
		CaseMode, RelativePath, bAllowMissingFinal, pPath);

	*pExists = false;
	if ( Result != XVFS_LOOKUP_OPENED ) return Result;
	if ( __xrtRootStatPolicy(Disk->Root, *pPath, true, pInfo,
		XROOT_POLICY_CASE_SENSITIVE) ) {
		*pExists = true;
		return XVFS_LOOKUP_OPENED;
	}
	Result = __xrtVfsDiskFailure();
	if ( (Result == XVFS_LOOKUP_MISS) && bAllowMissingFinal ) {
		return XVFS_LOOKUP_OPENED;
	}
	xrtFree(*pPath);
	*pPath = NULL;
	return Result;
}



/* 返回原生 xfile，使 native handle、map、lock 和 async 能力保持不变。 */
static xvfslookup __xrtVfsDiskOpenNative(void* pContext,
	xvfscase CaseMode, xstrview RelativePath,
	const xfileoptions* pOptions, xfile* pFile)
{
	xvfsdisk Disk = (xvfsdisk)pContext;
	str sPath = NULL;
	xfileinfo Info;
	bool bExists;
	bool bCreate = (pOptions->Flags & XFILE_CREATE) != 0u;
	xvfslookup Result;

	if ( !__xrtVfsDiskOpenAccess(Disk, pOptions) )
		return XVFS_LOOKUP_ERROR;
	Result = __xrtVfsDiskResolveStat(Disk, CaseMode, RelativePath,
		bCreate, &sPath, &Info, &bExists);
	if ( Result != XVFS_LOOKUP_OPENED ) return Result;
	if ( bExists && (Info.Type == XFILE_TYPE_DIRECTORY) ) {
		xrtFree(sPath);
		return XVFS_LOOKUP_MISS;
	}
	if ( bExists && (Info.Type != XFILE_TYPE_FILE) ) {
		xrtFree(sPath);
		__xrtVfsError(XERR_UNSUPPORTED, XVFS_ERROR_OPEN,
			"disk-open", "the disk provider only opens regular files");
		return XVFS_LOOKUP_ERROR;
	}
	*pFile = __xrtRootFileOpenPolicy(Disk->Root, sPath,
		pOptions, XROOT_POLICY_CASE_SENSITIVE | XROOT_POLICY_REGULAR_FILE);
	xrtFree(sPath);
	if ( *pFile == NULL ) return __xrtVfsDiskFailure();
	return XVFS_LOOKUP_OPENED;
}



/* Disk stat 从不跟随链接；发现链接时 root policy 返回权限错误。 */
static xvfslookup __xrtVfsDiskStat(void* pContext,
	xvfscase CaseMode, xstrview RelativePath,
	bool bFollowLink, xfileinfo* pInfo)
{
	xvfsdisk Disk = (xvfsdisk)pContext;
	str sPath = NULL;
	bool bExists;
	xvfslookup Result;
	(void)bFollowLink;

	if ( (Disk->Access & XVFS_DISK_READ) == 0u ) {
		__xrtVfsError(XERR_PERMISSION, XVFS_ERROR_STAT,
			"disk-stat", "the disk provider does not grant read access");
		return XVFS_LOOKUP_ERROR;
	}
	Result = __xrtVfsDiskResolveStat(Disk, CaseMode, RelativePath,
		false, &sPath, pInfo, &bExists);
	xrtFree(sPath);
	return Result;
}



static void __xrtVfsDiskEntriesFree(xrt_vfs_disk_entry* pItems,
	size_t iCount)
{
	if ( pItems == NULL ) return;
	for ( size_t i = 0u; i < iCount; i++ ) xrtFree(pItems[i].Name);
	xrtFree(pItems);
}

static bool __xrtVfsDiskDirReserve(xrt_vfs_disk_dir* pDir)
{
	xrt_vfs_disk_entry* pItems;
	size_t iCapacity;

	if ( pDir->Count < pDir->Capacity ) return true;
	iCapacity = pDir->Capacity == 0u ? 16u : pDir->Capacity * 2u;
	if ( (iCapacity < pDir->Capacity) ||
		 (iCapacity > (SIZE_MAX / sizeof(*pItems))) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	pItems = (xrt_vfs_disk_entry*)xrtRealloc(pDir->Items,
		iCapacity * sizeof(*pItems));
	if ( pItems == NULL ) return false;
	pDir->Items = pItems;
	pDir->Capacity = iCapacity;
	return true;
}

static int __xrtVfsDiskEntryCompare(const void* pLeft, const void* pRight)
{
	const xrt_vfs_disk_entry* pA = (const xrt_vfs_disk_entry*)pLeft;
	const xrt_vfs_disk_entry* pB = (const xrt_vfs_disk_entry*)pRight;
	return xrtStrCompare((xstrview){ pA->Name, pA->NameSize },
		(xstrview){ pB->Name, pB->NameSize });
}

static int __xrtVfsDiskEntryFoldCompare(const void* pLeft,
	const void* pRight)
{
	const xrt_vfs_disk_entry* pA = (const xrt_vfs_disk_entry*)pLeft;
	const xrt_vfs_disk_entry* pB = (const xrt_vfs_disk_entry*)pRight;
	int iResult = xrtStrCaseCompare(
		(xstrview){ pA->Name, pA->NameSize },
		(xstrview){ pB->Name, pB->NameSize });

	return iResult != 0 ? iResult : __xrtVfsDiskEntryCompare(pLeft, pRight);
}



static bool __xrtVfsDiskDirNext(void* pState,
	xdirentry* pEntry, bool* pEnd)
{
	xrt_vfs_disk_dir* pDir = (xrt_vfs_disk_dir*)pState;
	xrt_vfs_disk_entry* pItem;

	if ( pDir->Position == pDir->Count ) {
		*pEnd = true;
		return true;
	}
	pItem = &pDir->Items[pDir->Position++];
	memset(pEntry, 0, sizeof(*pEntry));
	pEntry->Name.Data = pItem->Name;
	pEntry->Name.Size = pItem->NameSize;
	pEntry->Info = pItem->Info;
	pEntry->Flags = XDIR_ENTRY_UTF8;
	*pEnd = false;
	return true;
}

static void __xrtVfsDiskDirClose(void* pState)
{
	xrt_vfs_disk_dir* pDir = (xrt_vfs_disk_dir*)pState;
	__xrtVfsDiskEntriesFree(pDir->Items, pDir->Count);
	xrtFree(pDir);
}

static const xvfsdirops_v1 __xrtVfsDiskDirOps = {
	(uint32)sizeof(xvfsdirops_v1),
	XRT_VFS_DIR_OPS_VERSION,
	__xrtVfsDiskDirNext,
	__xrtVfsDiskDirClose
};



/* 物化物理目录，拒绝链接和不区分大小写时的名称冲突。 */
static xvfslookup __xrtVfsDiskDirOpen(void* pContext,
	xvfscase CaseMode, xstrview RelativePath,
	uint32 iFlags, xvfsdir_v1* pOutput)
{
	xvfsdisk Disk = (xvfsdisk)pContext;
	str sPath = NULL;
	xfileinfo Info;
	bool bExists;
	xvfslookup Result;
	xdir Native = NULL;
	xrt_vfs_disk_dir* pDir = NULL;
	xdirentry Entry;
	xdirnext Next;
	(void)iFlags;

	if ( (Disk->Access & XVFS_DISK_READ) == 0u ) {
		__xrtVfsError(XERR_PERMISSION, XVFS_ERROR_DIRECTORY,
			"disk-directory", "the disk provider does not grant read access");
		return XVFS_LOOKUP_ERROR;
	}
	Result = __xrtVfsDiskResolveStat(Disk, CaseMode, RelativePath,
		false, &sPath, &Info, &bExists);
	if ( Result != XVFS_LOOKUP_OPENED ) return Result;
	if ( Info.Type != XFILE_TYPE_DIRECTORY ) {
		xrtFree(sPath);
		return XVFS_LOOKUP_MISS;
	}
	Native = __xrtRootDirOpenPolicy(Disk->Root, sPath,
		XDIR_STAT, XROOT_POLICY_CASE_SENSITIVE);
	xrtFree(sPath);
	if ( Native == NULL ) return __xrtVfsDiskFailure();
	pDir = (xrt_vfs_disk_dir*)xrtCalloc(1u, sizeof(*pDir));
	if ( pDir == NULL ) goto fail;
	while ( (Next = xrtDirNext(Native, &Entry)) == XDIR_NEXT_ITEM ) {
		xrt_vfs_disk_entry* pItem;

		if ( (Entry.Flags & XDIR_ENTRY_UTF8) == 0u ) continue;
		if ( Entry.Info.Type == XFILE_TYPE_LINK ) {
			__xrtVfsError(XERR_PERMISSION, XVFS_ERROR_DIRECTORY,
				"disk-directory", "the disk provider rejects symbolic links");
			goto fail;
		}
		if ( !__xrtVfsDiskDirReserve(pDir) ) goto fail;
		pItem = &pDir->Items[pDir->Count];
		memset(pItem, 0, sizeof(*pItem));
		pItem->Name = xrtStrDupView(Entry.Name);
		if ( pItem->Name == NULL ) goto fail;
		pItem->NameSize = Entry.Name.Size;
		pItem->Info = Entry.Info;
		pDir->Count++;
	}
	if ( Next == XDIR_NEXT_ERROR ) goto fail;
	if ( !xrtDirClose(Native) ) {
		Native = NULL;
		goto fail;
	}
	Native = NULL;
	if ( pDir->Count > 1u ) {
		qsort(pDir->Items, pDir->Count, sizeof(pDir->Items[0]),
			CaseMode == XVFS_CASE_ASCII_INSENSITIVE ?
			__xrtVfsDiskEntryFoldCompare : __xrtVfsDiskEntryCompare);
	}
	if ( CaseMode == XVFS_CASE_ASCII_INSENSITIVE ) {
		for ( size_t i = 1u; i < pDir->Count; i++ ) {
			if ( xrtStrCaseEqual(
				(xstrview){ pDir->Items[i - 1u].Name,
					pDir->Items[i - 1u].NameSize },
				(xstrview){ pDir->Items[i].Name,
					pDir->Items[i].NameSize }) ) {
				__xrtVfsError(XERR_EXISTS, XVFS_ERROR_DIRECTORY,
					"disk-directory",
					"the physical directory has names that collide under ASCII folding");
				goto fail;
			}
		}
	}
	pOutput->Ops = &__xrtVfsDiskDirOps;
	pOutput->State = pDir;
	return XVFS_LOOKUP_OPENED;

fail:
	if ( Native != NULL ) (void)xrtDirClose(Native);
	if ( pDir != NULL ) __xrtVfsDiskDirClose(pDir);
	return XVFS_LOOKUP_ERROR;
}



XRT_API xvfsdisk xrtVfsDiskCreate(cstr sPhysicalRoot, uint32 iAccess)
{
	xroot Root;
	xvfsdisk Disk;

	if ( (sPhysicalRoot == NULL) || (sPhysicalRoot[0] == '\0') ||
		 (iAccess == 0u) || ((iAccess & ~XVFS_DISK_ACCESS) != 0u) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	Root = xrtRootOpen(sPhysicalRoot);
	if ( Root == NULL ) return NULL;
	Disk = (xvfsdisk)xrtMalloc(sizeof(*Disk));
	if ( Disk == NULL ) {
		xerror* pBefore = xrtTakeError();

		(void)xrtRootClose(Root);
		xrtClearError();
		if ( pBefore != NULL ) __xrtErrorSetOwned(pBefore);
		return NULL;
	}
	Disk->RefCount = 1;
	Disk->Root = Root;
	Disk->Access = iAccess;
	return Disk;
}

XRT_API void xrtVfsDiskRef(xvfsdisk Disk)
{
	if ( Disk == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	if ( xrtRefRetain(&Disk->RefCount) < 0 ) abort();
}

XRT_API void xrtVfsDiskDestroy(xvfsdisk Disk)
{
	xerror* pBefore;

	if ( (Disk == NULL) || (xrtRefRelease(&Disk->RefCount) != 0) ) return;
	pBefore = xrtTakeError();
	(void)xrtRootClose(Disk->Root);
	xrtClearError();
	if ( pBefore != NULL ) __xrtErrorSetOwned(pBefore);
	xrtFree(Disk);
}



static void __xrtVfsDiskContextRetain(void* pContext)
{
	xrtVfsDiskRef((xvfsdisk)pContext);
}

static void __xrtVfsDiskContextRelease(void* pContext)
{
	xrtVfsDiskDestroy((xvfsdisk)pContext);
}

static const xvfsprovider_v1 __xrtVfsDiskProvider = {
	(uint32)sizeof(xvfsprovider_v1),
	XRT_VFS_PROVIDER_VERSION,
	XVFS_PROVIDER_STAT | XVFS_PROVIDER_DIRECTORY |
		XVFS_PROVIDER_NATIVE_OPEN,
	__xrtVfsDiskContextRetain,
	__xrtVfsDiskContextRelease,
	NULL,
	__xrtVfsDiskStat,
	__xrtVfsDiskDirOpen,
	__xrtVfsDiskOpenNative
};

XRT_API xvfsmount xrtVfsDiskMount(xvfs Vfs,
	cstr sVirtualPrefix, int32 iPriority, xvfscase CaseMode,
	xvfsdisk Disk, uint32 iFlags)
{
	if ( (Vfs == NULL) || (Disk == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return xrtVfsMount(Vfs, sVirtualPrefix, iPriority, CaseMode,
		&__xrtVfsDiskProvider, Disk, iFlags);
}

#endif

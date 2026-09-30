#include "../internal/xrt_vfs.h"

#include <stdlib.h>



#if defined(XRT_FEATURE_VFS_MEMORY)

typedef struct xrt_vfs_memory_blob {
	volatile int32 RefCount;
	bytes Data;
	size_t Size;
} xrt_vfs_memory_blob;

typedef struct xrt_vfs_memory_build {
	str Path;
	size_t PathSize;
	xrt_vfs_memory_blob* Blob;
} xrt_vfs_memory_build;

typedef struct xrt_vfs_memory_node {
	str Path;
	size_t PathSize;
	size_t ParentSize;
	size_t NameOffset;
	bool Directory;
	xrt_vfs_memory_blob* Blob;
} xrt_vfs_memory_node;

typedef struct xrt_vfs_memory_file {
	xrt_vfs_memory_blob* Blob;
	uint64 Cursor;
} xrt_vfs_memory_file;

typedef struct xrt_vfs_memory_dir {
	xvfsmemory Memory;
	size_t* Items;
	size_t Count;
	size_t Position;
} xrt_vfs_memory_dir;

struct xvfs_memory_impl {
	volatile int32 RefCount;
	bool Sealed;
	bool FoldCollision;
	xrt_vfs_memory_build* Build;
	size_t BuildCount;
	size_t BuildCapacity;
	xrt_vfs_memory_node* Nodes;
	xrt_vfs_memory_node** Folded;
	size_t NodeCount;
};



static void __xrtVfsMemoryBlobRef(xrt_vfs_memory_blob* pBlob)
{
	if ( xrtRefRetain(&pBlob->RefCount) < 0 ) abort();
}

static void __xrtVfsMemoryBlobRelease(xrt_vfs_memory_blob* pBlob)
{
	if ( (pBlob == NULL) || (xrtRefRelease(&pBlob->RefCount) != 0) ) return;
	xrtFree(pBlob->Data);
	xrtFree(pBlob);
}

static void __xrtVfsMemoryBuildFree(xrt_vfs_memory_build* pBuild,
	size_t iCount)
{
	if ( pBuild == NULL ) return;
	for ( size_t i = 0u; i < iCount; i++ ) {
		xrtFree(pBuild[i].Path);
		__xrtVfsMemoryBlobRelease(pBuild[i].Blob);
	}
	xrtFree(pBuild);
}

static void __xrtVfsMemoryNodesFree(xrt_vfs_memory_node* pNodes,
	size_t iCount)
{
	if ( pNodes == NULL ) return;
	for ( size_t i = 0u; i < iCount; i++ ) {
		xrtFree(pNodes[i].Path);
		__xrtVfsMemoryBlobRelease(pNodes[i].Blob);
	}
	xrtFree(pNodes);
}



XRT_API xvfsmemory xrtVfsMemoryCreate(void)
{
	xvfsmemory Memory = (xvfsmemory)xrtCalloc(1u, sizeof(*Memory));
	if ( Memory != NULL ) Memory->RefCount = 1;
	return Memory;
}

XRT_API void xrtVfsMemoryRef(xvfsmemory Memory)
{
	if ( Memory == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	if ( xrtRefRetain(&Memory->RefCount) < 0 ) abort();
}

XRT_API void xrtVfsMemoryDestroy(xvfsmemory Memory)
{
	if ( (Memory == NULL) || (xrtRefRelease(&Memory->RefCount) != 0) ) return;
	__xrtVfsMemoryBuildFree(Memory->Build, Memory->BuildCount);
	__xrtVfsMemoryNodesFree(Memory->Nodes, Memory->NodeCount);
	xrtFree(Memory->Folded);
	xrtFree(Memory);
}



static bool __xrtVfsMemoryBuildReserve(xvfsmemory Memory)
{
	xrt_vfs_memory_build* pBuild;
	size_t iCapacity;

	if ( Memory->BuildCount < Memory->BuildCapacity ) return true;
	iCapacity = (Memory->BuildCapacity == 0u) ? 8u :
		Memory->BuildCapacity * 2u;
	if ( (iCapacity < Memory->BuildCapacity) ||
		 (iCapacity > (SIZE_MAX / sizeof(*pBuild))) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	pBuild = (xrt_vfs_memory_build*)xrtRealloc(Memory->Build,
		iCapacity * sizeof(*pBuild));
	if ( pBuild == NULL ) return false;
	Memory->Build = pBuild;
	Memory->BuildCapacity = iCapacity;
	return true;
}

static bool __xrtVfsMemoryPut(xvfsmemory Memory,
	cstr sRelativePath, bytes pData, size_t iSize)
{
	xstrview Path;
	str sPath;
	xrt_vfs_memory_blob* pBlob;

	if ( (Memory == NULL) || (sRelativePath == NULL) ||
		 ((pData == NULL) && (iSize != 0u)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( Memory->Sealed ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	Path = xrtStrView(sRelativePath);
	if ( !__xrtVfsRelativePath(Path, false) ) return false;
	for ( size_t i = 0u; i < Memory->BuildCount; i++ ) {
		if ( xrtStrEqual(Path, (xstrview){ Memory->Build[i].Path,
			Memory->Build[i].PathSize }) ) {
			__xrtVfsError(XERR_EXISTS, XVFS_ERROR_CREATE,
				"memory-put", "the memory provider path already exists");
			return false;
		}
	}
	sPath = xrtStrDupView(Path);
	if ( sPath == NULL ) return false;
	pBlob = (xrt_vfs_memory_blob*)xrtMalloc(sizeof(*pBlob));
	if ( pBlob == NULL ) {
		xrtFree(sPath);
		return false;
	}
	if ( !__xrtVfsMemoryBuildReserve(Memory) ) {
		xrtFree(pBlob);
		xrtFree(sPath);
		return false;
	}
	pBlob->RefCount = 1;
	pBlob->Data = pData;
	pBlob->Size = iSize;
	Memory->Build[Memory->BuildCount].Path = sPath;
	Memory->Build[Memory->BuildCount].PathSize = Path.Size;
	Memory->Build[Memory->BuildCount].Blob = pBlob;
	Memory->BuildCount++;
	return true;
}

XRT_API bool xrtVfsMemoryPutOwned(xvfsmemory Memory,
	cstr sRelativePath, bytes pData, size_t iSize)
{
	return __xrtVfsMemoryPut(Memory, sRelativePath, pData, iSize);
}

XRT_API bool xrtVfsMemoryPutCopy(xvfsmemory Memory,
	cstr sRelativePath, const void* pData, size_t iSize)
{
	bytes pCopy = NULL;
	bool bResult;

	if ( (Memory == NULL) || (sRelativePath == NULL) ||
		 ((pData == NULL) && (iSize != 0u)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iSize != 0u ) {
		pCopy = (bytes)xrtMemDup(pData, iSize);
		if ( pCopy == NULL ) return false;
	}
	bResult = __xrtVfsMemoryPut(Memory,
		sRelativePath, pCopy, iSize);
	if ( !bResult ) xrtFree(pCopy);
	return bResult;
}



static bool __xrtVfsMemoryNodeAdd(xrt_vfs_memory_node* pNodes,
	size_t* pCount, xstrview Path, bool bDirectory,
	xrt_vfs_memory_blob* pBlob)
{
	xrt_vfs_memory_node* pNode = &pNodes[*pCount];
	size_t iSlash = SIZE_MAX;

	pNode->Path = xrtStrDupView(Path);
	if ( pNode->Path == NULL ) return false;
	pNode->PathSize = Path.Size;
	for ( size_t i = 0u; i < Path.Size; i++ )
		if ( Path.Data[i] == '/' ) iSlash = i;
	pNode->ParentSize = (Path.Size == 0u) ? SIZE_MAX :
		((iSlash == SIZE_MAX) ? 0u : iSlash);
	pNode->NameOffset = (iSlash == SIZE_MAX) ? 0u : iSlash + 1u;
	pNode->Directory = bDirectory;
	pNode->Blob = pBlob;
	if ( pBlob != NULL ) __xrtVfsMemoryBlobRef(pBlob);
	(*pCount)++;
	return true;
}

static int __xrtVfsMemoryNodeCompare(const void* pLeft, const void* pRight)
{
	const xrt_vfs_memory_node* pA = (const xrt_vfs_memory_node*)pLeft;
	const xrt_vfs_memory_node* pB = (const xrt_vfs_memory_node*)pRight;
	return xrtStrCompare((xstrview){ pA->Path, pA->PathSize },
		(xstrview){ pB->Path, pB->PathSize });
}

static int __xrtVfsMemoryFoldCompare(const void* pLeft, const void* pRight)
{
	const xrt_vfs_memory_node* pA =
		*(xrt_vfs_memory_node* const*)pLeft;
	const xrt_vfs_memory_node* pB =
		*(xrt_vfs_memory_node* const*)pRight;
	int iResult = xrtStrCaseCompare(
		(xstrview){ pA->Path, pA->PathSize },
		(xstrview){ pB->Path, pB->PathSize });
	return (iResult != 0) ? iResult : __xrtVfsMemoryNodeCompare(pA, pB);
}

static bool __xrtVfsMemoryCandidateCount(xvfsmemory Memory,
	size_t* pCount)
{
	size_t iCount = 1u;

	for ( size_t i = 0u; i < Memory->BuildCount; i++ ) {
		if ( iCount == SIZE_MAX ) goto overflow;
		iCount++;
		for ( size_t j = 0u; j < Memory->Build[i].PathSize; j++ ) {
			if ( Memory->Build[i].Path[j] != '/' ) continue;
			if ( iCount == SIZE_MAX ) goto overflow;
			iCount++;
		}
	}
	if ( iCount > (SIZE_MAX / sizeof(xrt_vfs_memory_node)) )
		goto overflow;
	*pCount = iCount;
	return true;

overflow:
	__xrtErrorSetSizeOverflow();
	return false;
}

XRT_API bool xrtVfsMemorySeal(xvfsmemory Memory)
{
	xrt_vfs_memory_node* pNodes = NULL;
	xrt_vfs_memory_node** pFolded = NULL;
	size_t iCandidates;
	size_t iCount = 0u;
	size_t iOutput = 0u;
	bool bFoldCollision = false;

	if ( Memory == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( Memory->Sealed ) return true;
	if ( !__xrtVfsMemoryCandidateCount(Memory, &iCandidates) ) return false;
	pNodes = (xrt_vfs_memory_node*)xrtCalloc(
		iCandidates, sizeof(*pNodes));
	if ( pNodes == NULL ) return false;
	if ( !__xrtVfsMemoryNodeAdd(pNodes, &iCount,
		XRT_STR_LITERAL(""), true, NULL) ) goto fail;
	for ( size_t i = 0u; i < Memory->BuildCount; i++ ) {
		xrt_vfs_memory_build* pBuild = &Memory->Build[i];

		for ( size_t j = 0u; j < pBuild->PathSize; j++ ) {
			if ( pBuild->Path[j] != '/' ) continue;
			if ( !__xrtVfsMemoryNodeAdd(pNodes, &iCount,
				(xstrview){ pBuild->Path, j }, true, NULL) ) goto fail;
		}
		if ( !__xrtVfsMemoryNodeAdd(pNodes, &iCount,
			(xstrview){ pBuild->Path, pBuild->PathSize },
			false, pBuild->Blob) ) goto fail;
	}
	qsort(pNodes, iCount, sizeof(*pNodes), __xrtVfsMemoryNodeCompare);
	for ( size_t i = 0u; i < iCount; i++ ) {
		if ( (iOutput != 0u) &&
			 xrtStrEqual(
				(xstrview){ pNodes[iOutput - 1u].Path,
					pNodes[iOutput - 1u].PathSize },
				(xstrview){ pNodes[i].Path, pNodes[i].PathSize }) ) {
			if ( pNodes[iOutput - 1u].Directory != pNodes[i].Directory ) {
				__xrtVfsError(XERR_EXISTS, XVFS_ERROR_CREATE,
					"memory-seal", "a path is both a file and a directory");
				goto fail;
			}
			xrtFree(pNodes[i].Path);
			pNodes[i].Path = NULL;
			continue;
		}
		if ( iOutput != i ) {
			pNodes[iOutput] = pNodes[i];
			memset(&pNodes[i], 0, sizeof(pNodes[i]));
		}
		iOutput++;
	}
	pFolded = (xrt_vfs_memory_node**)xrtMalloc(
		iOutput * sizeof(*pFolded));
	if ( pFolded == NULL ) goto fail;
	for ( size_t i = 0u; i < iOutput; i++ ) pFolded[i] = &pNodes[i];
	qsort(pFolded, iOutput, sizeof(*pFolded), __xrtVfsMemoryFoldCompare);
	for ( size_t i = 1u; i < iOutput; i++ ) {
		if ( xrtStrCaseEqual(
			(xstrview){ pFolded[i - 1u]->Path,
				pFolded[i - 1u]->PathSize },
			(xstrview){ pFolded[i]->Path, pFolded[i]->PathSize }) ) {
			bFoldCollision = true;
			break;
		}
	}
	__xrtVfsMemoryBuildFree(Memory->Build, Memory->BuildCount);
	Memory->Build = NULL;
	Memory->BuildCount = 0u;
	Memory->BuildCapacity = 0u;
	Memory->Nodes = pNodes;
	Memory->Folded = pFolded;
	Memory->NodeCount = iOutput;
	Memory->FoldCollision = bFoldCollision;
	Memory->Sealed = true;
	return true;

fail:
	xrtFree(pFolded);
	__xrtVfsMemoryNodesFree(pNodes, iCount);
	return false;
}



static xrt_vfs_memory_node* __xrtVfsMemoryFind(
	xvfsmemory Memory, xvfscase CaseMode, xstrview Path)
{
	size_t iLow = 0u;
	size_t iHigh = Memory->NodeCount;

	while ( iLow < iHigh ) {
		size_t iMiddle = iLow + ((iHigh - iLow) / 2u);
		xrt_vfs_memory_node* pNode =
			(CaseMode == XVFS_CASE_SENSITIVE) ?
			&Memory->Nodes[iMiddle] : Memory->Folded[iMiddle];
		int iCompare = (CaseMode == XVFS_CASE_SENSITIVE) ?
			xrtStrCompare((xstrview){ pNode->Path, pNode->PathSize }, Path) :
			xrtStrCaseCompare((xstrview){ pNode->Path, pNode->PathSize }, Path);

		if ( iCompare < 0 ) iLow = iMiddle + 1u;
		else if ( iCompare > 0 ) iHigh = iMiddle;
		else return pNode;
	}
	return NULL;
}

static bool __xrtVfsMemoryReadAt(void* pState, uint64 iOffset,
	void* pBuffer, size_t iRequest, size_t* pRead)
{
	xrt_vfs_memory_file* pFile = (xrt_vfs_memory_file*)pState;
	size_t iStart = (iOffset > (uint64)SIZE_MAX) ? SIZE_MAX :
		(size_t)iOffset;
	size_t iDone = (iStart < pFile->Blob->Size) ?
		pFile->Blob->Size - iStart : 0u;

	if ( iDone > iRequest ) iDone = iRequest;
	if ( iDone != 0u ) memcpy(pBuffer, pFile->Blob->Data + iStart, iDone);
	*pRead = iDone;
	return true;
}

static bool __xrtVfsMemoryRead(void* pState, void* pBuffer,
	size_t iRequest, size_t* pRead)
{
	xrt_vfs_memory_file* pFile = (xrt_vfs_memory_file*)pState;
	bool bResult = __xrtVfsMemoryReadAt(
		pState, pFile->Cursor, pBuffer, iRequest, pRead);
	if ( bResult ) pFile->Cursor += (uint64)*pRead;
	return bResult;
}

static bool __xrtVfsMemorySeekAdd(uint64 iBase,
	int64 iOffset, uint64* pResult)
{
	if ( iOffset >= 0 ) {
		uint64 iAdd = (uint64)iOffset;
		if ( iBase > UINT64_MAX - iAdd ) return false;
		*pResult = iBase + iAdd;
	} else {
		uint64 iSubtract = (uint64)(-(iOffset + 1)) + 1u;
		if ( iBase < iSubtract ) return false;
		*pResult = iBase - iSubtract;
	}
	return true;
}

static bool __xrtVfsMemorySeek(void* pState, int64 iOffset,
	xseek Origin, uint64* pPosition)
{
	xrt_vfs_memory_file* pFile = (xrt_vfs_memory_file*)pState;
	uint64 iBase;
	uint64 iResult;

	if ( Origin == XSEEK_START ) iBase = 0u;
	else if ( Origin == XSEEK_CURRENT ) iBase = pFile->Cursor;
	else if ( Origin == XSEEK_END ) iBase = (uint64)pFile->Blob->Size;
	else {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtVfsMemorySeekAdd(iBase, iOffset, &iResult) ) {
		__xrtVfsError(XERR_RANGE, XVFS_ERROR_OPEN,
			"memory-seek", "the requested position is out of range");
		return false;
	}
	pFile->Cursor = iResult;
	if ( pPosition != NULL ) *pPosition = iResult;
	return true;
}

static bool __xrtVfsMemoryFileStat(void* pState, xfileinfo* pInfo)
{
	xrt_vfs_memory_file* pFile = (xrt_vfs_memory_file*)pState;
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->Type = XFILE_TYPE_FILE;
	pInfo->Available = XFILE_INFO_SIZE;
	pInfo->Size = (uint64)pFile->Blob->Size;
	return true;
}

static void __xrtVfsMemoryFileClose(void* pState)
{
	xrt_vfs_memory_file* pFile = (xrt_vfs_memory_file*)pState;
	__xrtVfsMemoryBlobRelease(pFile->Blob);
	xrtFree(pFile);
}

static const xvfsfileops_v1 __xrtVfsMemoryFileOps = {
	(uint32)sizeof(xvfsfileops_v1),
	XRT_VFS_FILE_OPS_VERSION,
	XVFS_FILE_READ | XVFS_FILE_READ_AT | XVFS_FILE_SEEK | XVFS_FILE_STAT,
	__xrtVfsMemoryRead, NULL,
	__xrtVfsMemoryReadAt, NULL,
	__xrtVfsMemorySeek, __xrtVfsMemoryFileStat,
	NULL, NULL,
	__xrtVfsMemoryFileClose
};



static bool __xrtVfsMemoryReady(xvfsmemory Memory, cstr sOperation)
{
	if ( Memory->Sealed ) return true;
	__xrtVfsError(XERR_STATE, XVFS_ERROR_PROVIDER,
		sOperation, "the memory provider is not sealed");
	return false;
}

static bool __xrtVfsMemoryReadOnly(const xfileoptions* pOptions)
{
	uint32 iForbidden = XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE |
		XFILE_APPEND | XFILE_EXCLUSIVE | XFILE_SYNC;

	if ( (pOptions->Flags & iForbidden) == 0u ) return true;
	__xrtVfsError(XERR_UNSUPPORTED, XVFS_ERROR_UNSUPPORTED,
		"memory-open", "the memory provider is read-only");
	return false;
}

static xvfslookup __xrtVfsMemoryOpen(void* pContext,
	xvfscase CaseMode, xstrview RelativePath,
	const xfileoptions* pOptions, xvfsfile_v1* pFile)
{
	xvfsmemory Memory = (xvfsmemory)pContext;
	xrt_vfs_memory_node* pNode;
	xrt_vfs_memory_file* pState;

	if ( !__xrtVfsMemoryReady(Memory, "memory-open") )
		return XVFS_LOOKUP_ERROR;
	pNode = __xrtVfsMemoryFind(Memory, CaseMode, RelativePath);
	if ( (pNode == NULL) || pNode->Directory ) return XVFS_LOOKUP_MISS;
	if ( !__xrtVfsMemoryReadOnly(pOptions) ) return XVFS_LOOKUP_ERROR;
	pState = (xrt_vfs_memory_file*)xrtCalloc(1u, sizeof(*pState));
	if ( pState == NULL ) return XVFS_LOOKUP_ERROR;
	pState->Blob = pNode->Blob;
	__xrtVfsMemoryBlobRef(pState->Blob);
	pFile->Ops = &__xrtVfsMemoryFileOps;
	pFile->State = pState;
	pFile->Flags = pOptions->Flags;
	return XVFS_LOOKUP_OPENED;
}

static xvfslookup __xrtVfsMemoryStat(void* pContext,
	xvfscase CaseMode, xstrview RelativePath,
	bool bFollowLink, xfileinfo* pInfo)
{
	xvfsmemory Memory = (xvfsmemory)pContext;
	xrt_vfs_memory_node* pNode;
	(void)bFollowLink;

	if ( !__xrtVfsMemoryReady(Memory, "memory-stat") )
		return XVFS_LOOKUP_ERROR;
	pNode = __xrtVfsMemoryFind(Memory, CaseMode, RelativePath);
	if ( pNode == NULL ) return XVFS_LOOKUP_MISS;
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->Type = pNode->Directory ? XFILE_TYPE_DIRECTORY : XFILE_TYPE_FILE;
	if ( !pNode->Directory ) {
		pInfo->Available = XFILE_INFO_SIZE;
		pInfo->Size = (uint64)pNode->Blob->Size;
	}
	return XVFS_LOOKUP_OPENED;
}

static bool __xrtVfsMemoryDirNext(void* pState,
	xdirentry* pEntry, bool* pEnd)
{
	xrt_vfs_memory_dir* pDir = (xrt_vfs_memory_dir*)pState;
	xrt_vfs_memory_node* pNode;

	if ( pDir->Position == pDir->Count ) {
		*pEnd = true;
		return true;
	}
	pNode = &pDir->Memory->Nodes[pDir->Items[pDir->Position++]];
	memset(pEntry, 0, sizeof(*pEntry));
	pEntry->Name.Data = pNode->Path + pNode->NameOffset;
	pEntry->Name.Size = pNode->PathSize - pNode->NameOffset;
	pEntry->Info.Type = pNode->Directory ?
		XFILE_TYPE_DIRECTORY : XFILE_TYPE_FILE;
	if ( !pNode->Directory ) {
		pEntry->Info.Available = XFILE_INFO_SIZE;
		pEntry->Info.Size = (uint64)pNode->Blob->Size;
	}
	pEntry->Flags = XDIR_ENTRY_UTF8;
	return true;
}

static void __xrtVfsMemoryDirClose(void* pState)
{
	xrt_vfs_memory_dir* pDir = (xrt_vfs_memory_dir*)pState;
	xrtVfsMemoryDestroy(pDir->Memory);
	xrtFree(pDir->Items);
	xrtFree(pDir);
}

static const xvfsdirops_v1 __xrtVfsMemoryDirOps = {
	(uint32)sizeof(xvfsdirops_v1),
	XRT_VFS_DIR_OPS_VERSION,
	__xrtVfsMemoryDirNext,
	__xrtVfsMemoryDirClose
};

static bool __xrtVfsMemoryParentEqual(const xrt_vfs_memory_node* pNode,
	xstrview Path, xvfscase CaseMode)
{
	xstrview Parent;

	if ( pNode->ParentSize == SIZE_MAX ) return false;
	Parent.Data = pNode->Path;
	Parent.Size = pNode->ParentSize;
	return (CaseMode == XVFS_CASE_SENSITIVE) ?
		xrtStrEqual(Parent, Path) : xrtStrCaseEqual(Parent, Path);
}

static xvfslookup __xrtVfsMemoryDirOpen(void* pContext,
	xvfscase CaseMode, xstrview RelativePath,
	uint32 iFlags, xvfsdir_v1* pDir)
{
	xvfsmemory Memory = (xvfsmemory)pContext;
	xrt_vfs_memory_node* pNode;
	xrt_vfs_memory_dir* pState;
	size_t iCount = 0u;
	(void)iFlags;

	if ( !__xrtVfsMemoryReady(Memory, "memory-directory") )
		return XVFS_LOOKUP_ERROR;
	pNode = __xrtVfsMemoryFind(Memory, CaseMode, RelativePath);
	if ( (pNode == NULL) || !pNode->Directory ) return XVFS_LOOKUP_MISS;
	for ( size_t i = 0u; i < Memory->NodeCount; i++ )
		if ( __xrtVfsMemoryParentEqual(&Memory->Nodes[i],
			RelativePath, CaseMode) ) iCount++;
	pState = (xrt_vfs_memory_dir*)xrtCalloc(1u, sizeof(*pState));
	if ( pState == NULL ) return XVFS_LOOKUP_ERROR;
	if ( iCount != 0u ) {
		if ( iCount > (SIZE_MAX / sizeof(pState->Items[0])) ) {
			__xrtErrorSetSizeOverflow();
			xrtFree(pState);
			return XVFS_LOOKUP_ERROR;
		}
		pState->Items = (size_t*)xrtMalloc(
			iCount * sizeof(pState->Items[0]));
		if ( pState->Items == NULL ) {
			xrtFree(pState);
			return XVFS_LOOKUP_ERROR;
		}
	}
	for ( size_t i = 0u; i < Memory->NodeCount; i++ ) {
		if ( __xrtVfsMemoryParentEqual(&Memory->Nodes[i],
			RelativePath, CaseMode) )
			pState->Items[pState->Count++] = i;
	}
	pState->Memory = Memory;
	xrtVfsMemoryRef(Memory);
	pDir->Ops = &__xrtVfsMemoryDirOps;
	pDir->State = pState;
	return XVFS_LOOKUP_OPENED;
}

static void __xrtVfsMemoryContextRetain(void* pContext)
{
	xrtVfsMemoryRef((xvfsmemory)pContext);
}

static void __xrtVfsMemoryContextRelease(void* pContext)
{
	xrtVfsMemoryDestroy((xvfsmemory)pContext);
}

static const xvfsprovider_v1 __xrtVfsMemoryProvider = {
	(uint32)sizeof(xvfsprovider_v1),
	XRT_VFS_PROVIDER_VERSION,
	XVFS_PROVIDER_STAT | XVFS_PROVIDER_DIRECTORY,
	__xrtVfsMemoryContextRetain,
	__xrtVfsMemoryContextRelease,
	__xrtVfsMemoryOpen,
	__xrtVfsMemoryStat,
	__xrtVfsMemoryDirOpen,
	NULL
};

XRT_API xvfsmount xrtVfsMemoryMount(xvfs Vfs,
	cstr sVirtualPrefix, int32 iPriority, xvfscase CaseMode,
	xvfsmemory Memory, uint32 iFlags)
{
	if ( (Vfs == NULL) || (Memory == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !Memory->Sealed ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	if ( (CaseMode == XVFS_CASE_ASCII_INSENSITIVE) &&
		 Memory->FoldCollision ) {
		__xrtVfsError(XERR_EXISTS, XVFS_ERROR_MOUNT,
			"memory-mount",
			"the memory provider contains paths that collide under ASCII folding");
		return NULL;
	}
	return xrtVfsMount(Vfs, sVirtualPrefix, iPriority, CaseMode,
		&__xrtVfsMemoryProvider, Memory, iFlags);
}

#endif

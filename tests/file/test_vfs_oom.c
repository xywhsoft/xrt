#ifdef VFS_OOM_SINGLE
	#define XRT_IMPLEMENTATION
	#include "../../single/xrt.h"
#else
	#include <xrt/memory_debug.h>
	#include <xrt/vfs.h>
#endif

#include "../test.h"



typedef struct test_vfs_oom_context {
	uint32 Retains;
	uint32 Releases;
	uint32 FileCloses;
	uint32 DirCloses;
} test_vfs_oom_context;

typedef struct test_vfs_oom_file {
	test_vfs_oom_context* Context;
} test_vfs_oom_file;

typedef struct test_vfs_oom_dir {
	test_vfs_oom_context* Context;
	size_t Position;
} test_vfs_oom_dir;



static bool testVfsOomReadAt(void* pState, uint64 iOffset,
	void* pBuffer, size_t iRequest, size_t* pRead)
{
	static const char sData[] = "payload";
	(void)pState;
	if ( iOffset >= sizeof(sData) - 1u ) {
		*pRead = 0u;
		return true;
	}
	*pRead = sizeof(sData) - 1u - (size_t)iOffset;
	if ( *pRead > iRequest ) *pRead = iRequest;
	memcpy(pBuffer, sData + (size_t)iOffset, *pRead);
	return true;
}

static bool testVfsOomFileStat(void* pState, xfileinfo* pInfo)
{
	(void)pState;
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->Type = XFILE_TYPE_FILE;
	pInfo->Available = XFILE_INFO_SIZE;
	pInfo->Size = 7u;
	return true;
}

static void testVfsOomFileClose(void* pState)
{
	test_vfs_oom_file* pFile = (test_vfs_oom_file*)pState;
	pFile->Context->FileCloses++;
	xrtFree(pFile);
}

static const xvfsfileops_v1 testVfsOomFileOps = {
	(uint32)sizeof(xvfsfileops_v1), XRT_VFS_FILE_OPS_VERSION,
	XVFS_FILE_READ_AT | XVFS_FILE_STAT,
	NULL, NULL, testVfsOomReadAt, NULL, NULL, testVfsOomFileStat,
	NULL, NULL, testVfsOomFileClose
};

static void testVfsOomRetain(void* pContext)
{
	((test_vfs_oom_context*)pContext)->Retains++;
}

static void testVfsOomRelease(void* pContext)
{
	((test_vfs_oom_context*)pContext)->Releases++;
}

static xvfslookup testVfsOomOpen(void* pContext, xvfscase CaseMode,
	xstrview Relative,
	const xfileoptions* pOptions, xvfsfile_v1* pFile)
{
	test_vfs_oom_file* pState;
	(void)CaseMode;
	if ( !xrtStrEqual(Relative, XRT_STR_LITERAL("file")) )
		return XVFS_LOOKUP_MISS;
	pState = (test_vfs_oom_file*)xrtMalloc(sizeof(*pState));
	if ( pState == NULL ) return XVFS_LOOKUP_ERROR;
	pState->Context = (test_vfs_oom_context*)pContext;
	pFile->Ops = &testVfsOomFileOps;
	pFile->State = pState;
	pFile->Flags = pOptions->Flags;
	return XVFS_LOOKUP_OPENED;
}

static bool testVfsOomDirNext(void* pState, xdirentry* pEntry, bool* pEnd)
{
	test_vfs_oom_dir* pDir = (test_vfs_oom_dir*)pState;
	static const cstr arrNames[] = { "alpha", "beta", "gamma" };
	if ( pDir->Position == 3u ) {
		*pEnd = true;
		return true;
	}
	memset(pEntry, 0, sizeof(*pEntry));
	pEntry->Name = xrtStrView(arrNames[pDir->Position++]);
	pEntry->Info.Type = XFILE_TYPE_FILE;
	return true;
}

static void testVfsOomDirClose(void* pState)
{
	test_vfs_oom_dir* pDir = (test_vfs_oom_dir*)pState;
	pDir->Context->DirCloses++;
	xrtFree(pDir);
}

static const xvfsdirops_v1 testVfsOomDirOps = {
	(uint32)sizeof(xvfsdirops_v1), XRT_VFS_DIR_OPS_VERSION,
	testVfsOomDirNext, testVfsOomDirClose
};

static xvfslookup testVfsOomDirOpen(void* pContext, xvfscase CaseMode,
	xstrview Relative,
	uint32 iFlags, xvfsdir_v1* pDir)
{
	test_vfs_oom_dir* pState;
	(void)CaseMode;
	(void)iFlags;
	if ( Relative.Size != 0u ) return XVFS_LOOKUP_MISS;
	pState = (test_vfs_oom_dir*)xrtCalloc(1u, sizeof(*pState));
	if ( pState == NULL ) return XVFS_LOOKUP_ERROR;
	pState->Context = (test_vfs_oom_context*)pContext;
	pDir->Ops = &testVfsOomDirOps;
	pDir->State = pState;
	return XVFS_LOOKUP_OPENED;
}

static const xvfsprovider_v1 testVfsOomProvider = {
	(uint32)sizeof(xvfsprovider_v1), XRT_VFS_PROVIDER_VERSION,
	XVFS_PROVIDER_DIRECTORY,
	testVfsOomRetain, testVfsOomRelease,
	testVfsOomOpen, NULL, testVfsOomDirOpen, NULL
};



static bool testVfsOomScenario(bool bDirectory,
	test_vfs_oom_context* pContext, bool* pHit)
{
	xvfs Vfs = xrtVfsCreate();
	xvfsmount Mount = NULL;
	bool bSuccess = false;

	if ( Vfs == NULL ) goto done;
	Mount = xrtVfsMount(Vfs, "/", 0, XVFS_CASE_SENSITIVE,
		&testVfsOomProvider, pContext, 0u);
	if ( Mount == NULL ) goto done;
	if ( bDirectory ) {
		xdir Dir = xrtVfsDirOpen(Vfs, "/", 0u);
		xdirentry Entry;
		size_t iCount = 0u;
		if ( Dir == NULL ) goto done;
		while ( xrtDirNext(Dir, &Entry) == XDIR_NEXT_ITEM ) iCount++;
		if ( !xrtDirClose(Dir) || (iCount != 3u) ) goto done;
	} else {
		xfile File = xrtVfsOpen(Vfs, "/file", NULL);
		char sData[8] = { 0 };
		size_t iRead = 0u;
		if ( File == NULL ) goto done;
		if ( !xrtRead(File, sData, 7u, &iRead) ||
			 (iRead != 7u) || !xrtClose(File) ) goto done;
	}
	bSuccess = true;

done:
	*pHit = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	if ( Mount != NULL ) {
		(void)xrtVfsUnmount(Mount);
		xrtVfsMountDestroy(Mount);
	}
	if ( Vfs != NULL ) xrtVfsDestroy(Vfs);
	return bSuccess;
}



static void testVfsOomSweep(bool bDirectory)
{
	xmemdebugsnapshot Before;
	xmemdebugsnapshot After;
	bool bCompleted = false;
	size_t iPoints = 0u;

	xrtMemDebugSnapshot(&Before);
	for ( uint64 i = 0u; i < 128u; i++ ) {
		test_vfs_oom_context Context;
		bool bResult;
		bool bHit;

		memset(&Context, 0, sizeof(Context));
		testRequire(xrtMemDebugFailAfter(i), "VFS OOM injection setup failed");
		bResult = testVfsOomScenario(bDirectory, &Context, &bHit);
		if ( bHit ) {
			testRequire(!bResult, "VFS operation ignored an injected OOM");
			iPoints++;
		} else {
			testRequire(bResult, "VFS operation failed after the OOM sweep completed");
			bCompleted = true;
		}
		testRequire(Context.Retains == Context.Releases,
			"VFS OOM unbalanced provider context references");
		testRequire(Context.FileCloses <= 1u,
			"VFS OOM closed one file state more than once");
		xrtClearError();
		xrtMemDebugSnapshot(&After);
		testRequire((After.LiveCount == Before.LiveCount) &&
			(After.LiveBytes == Before.LiveBytes),
			"VFS OOM leaked a logical allocation");
		if ( bCompleted ) break;
	}
	testRequire(bCompleted && (iPoints != 0u),
		"VFS OOM sweep did not cover any allocation point");
}



static void testVfsUnmountOomAtomic(void)
{
	test_vfs_oom_context Context;
	xmemdebugsnapshot Before;
	xmemdebugsnapshot After;
	xvfs Vfs;
	xvfsmount Mount;
	xfile File;
	char sData[8] = { 0 };
	size_t iRead = 0u;

	memset(&Context, 0, sizeof(Context));
	xrtMemDebugSnapshot(&Before);
	Vfs = xrtVfsCreate();
	testRequire(Vfs != NULL, "VFS unmount OOM namespace setup failed");
	Mount = xrtVfsMount(Vfs, "/", 0, XVFS_CASE_SENSITIVE,
		&testVfsOomProvider, &Context, 0u);
	testRequire(Mount != NULL, "VFS unmount OOM mount setup failed");

	testRequire(xrtMemDebugFailAfter(0u),
		"VFS unmount OOM injection setup failed");
	testRequire(!xrtVfsUnmount(Mount),
		"VFS unmount must report snapshot allocation failure");
	testRequire(xrtMemDebugFailTriggered(),
		"VFS unmount did not reach the injected allocation failure");
	xrtMemDebugFailClear();
	xrtClearError();

	File = xrtVfsOpen(Vfs, "/file", NULL);
	testRequire(File != NULL,
		"failed VFS unmount must leave the old snapshot published");
	testRequire(xrtRead(File, sData, 7u, &iRead) &&
		(iRead == 7u) && (memcmp(sData, "payload", 7u) == 0),
		"mount did not remain usable after failed unmount");
	testRequire(xrtClose(File),
		"file close failed after failed VFS unmount");
	testRequire(xrtVfsUnmount(Mount),
		"VFS unmount did not recover after OOM was cleared");
	xrtVfsMountDestroy(Mount);
	xrtVfsDestroy(Vfs);
	testRequire(Context.Retains == Context.Releases,
		"VFS unmount OOM unbalanced provider context references");
	testRequire(Context.FileCloses == 1u,
		"VFS unmount OOM did not close the provider file exactly once");
	xrtClearError();
	xrtMemDebugSnapshot(&After);
	testRequire((After.LiveCount == Before.LiveCount) &&
		(After.LiveBytes == Before.LiveBytes),
		"VFS unmount OOM leaked a logical allocation");
}



int main(void)
{
	testRequire(xrtMemDebugEnable(true), "memory debug enable failed");
	testVfsOomSweep(false);
	testVfsOomSweep(true);
	testVfsUnmountOomAtomic();
	testRequire(xrtMemDebugReset(), "VFS OOM left live allocations");
	return 0;
}

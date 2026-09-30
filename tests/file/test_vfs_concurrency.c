#ifdef VFS_CONCURRENCY_SINGLE
	#define XRT_IMPLEMENTATION
	#include "../../single/xrt.h"
#else
	#include <xrt/atomic.h>
	#include <xrt/memory_debug.h>
	#include <xrt/thread.h>
	#include <xrt/vfs.h>
#endif

#include "../test.h"



#define TEST_VFS_READER_COUNT 4u
#define TEST_VFS_READER_ITERATIONS 6000u
#define TEST_VFS_PUBLISH_ITERATIONS 1000u



typedef struct test_vfs_concurrent_context {
	xvfs Vfs;
	char Value;
	bool Reenter;
	bool ReleaseReenter;
	xatomic32 Retains;
	xatomic32 Releases;
	xatomic32 Opens;
	xatomic32 Closes;
	xatomic32 ReentrantCalls;
	xatomic32 ReentrantReleases;
	xatomic32 Failures;
} test_vfs_concurrent_context;

typedef struct test_vfs_concurrent_file {
	test_vfs_concurrent_context* Context;
} test_vfs_concurrent_file;

typedef struct test_vfs_concurrent_run {
	xvfs Vfs;
	xatomic32 Ready;
	xatomic32 Start;
	xatomic32 Failures;
} test_vfs_concurrent_run;



static void testVfsConcurrentContextInit(
	test_vfs_concurrent_context* pContext, xvfs Vfs, char iValue)
{
	memset(pContext, 0, sizeof(*pContext));
	pContext->Vfs = Vfs;
	pContext->Value = iValue;
	xrtAtomic32Init(&pContext->Retains, 0u);
	xrtAtomic32Init(&pContext->Releases, 0u);
	xrtAtomic32Init(&pContext->Opens, 0u);
	xrtAtomic32Init(&pContext->Closes, 0u);
	xrtAtomic32Init(&pContext->ReentrantCalls, 0u);
	xrtAtomic32Init(&pContext->ReentrantReleases, 0u);
	xrtAtomic32Init(&pContext->Failures, 0u);
}

static bool testVfsConcurrentPath(xstrview Path, cstr sExpected)
{
	return xrtStrEqual(Path, xrtStrView(sExpected));
}

static bool testVfsConcurrentReadAt(void* pState, uint64 iOffset,
	void* pBuffer, size_t iRequest, size_t* pRead)
{
	test_vfs_concurrent_file* pFile =
		(test_vfs_concurrent_file*)pState;

	if ( (iOffset != 0u) || (iRequest == 0u) ) {
		*pRead = 0u;
		return true;
	}
	((char*)pBuffer)[0] = pFile->Context->Value;
	*pRead = 1u;
	return true;
}

static bool testVfsConcurrentFileStat(void* pState, xfileinfo* pInfo)
{
	(void)pState;
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->Type = XFILE_TYPE_FILE;
	pInfo->Available = XFILE_INFO_SIZE;
	pInfo->Size = 1u;
	return true;
}

static void testVfsConcurrentFileClose(void* pState)
{
	test_vfs_concurrent_file* pFile =
		(test_vfs_concurrent_file*)pState;

	(void)xrtAtomic32FetchAdd(&pFile->Context->Closes,
		1u, XMEMORY_RELAXED);
	xrtFree(pFile);
}

static const xvfsfileops_v1 testVfsConcurrentFileOps = {
	(uint32)sizeof(xvfsfileops_v1),
	XRT_VFS_FILE_OPS_VERSION,
	XVFS_FILE_READ_AT | XVFS_FILE_STAT,
	NULL, NULL,
	testVfsConcurrentReadAt, NULL,
	NULL, testVfsConcurrentFileStat,
	NULL, NULL,
	testVfsConcurrentFileClose
};



static void testVfsConcurrentRetain(void* pContext)
{
	test_vfs_concurrent_context* pProvider =
		(test_vfs_concurrent_context*)pContext;

	(void)xrtAtomic32FetchAdd(&pProvider->Retains, 1u, XMEMORY_RELAXED);
}

static const xvfsprovider_v1 testVfsConcurrentProvider;

static void testVfsConcurrentRelease(void* pContext)
{
	test_vfs_concurrent_context* pProvider =
		(test_vfs_concurrent_context*)pContext;

	(void)xrtAtomic32FetchAdd(&pProvider->Releases, 1u, XMEMORY_RELAXED);
	if ( pProvider->ReleaseReenter ) {
		xfileinfo Info;

		(void)xrtAtomic32FetchAdd(&pProvider->ReentrantReleases,
			1u, XMEMORY_RELAXED);
		if ( !xrtVfsStat(pProvider->Vfs,
			"/inner/status", false, &Info) || (Info.Size != 1u) )
			(void)xrtAtomic32FetchAdd(&pProvider->Failures,
				1u, XMEMORY_RELAXED);
	}
}

static xvfslookup testVfsConcurrentOpen(void* pContext,
	xvfscase CaseMode, xstrview RelativePath, const xfileoptions* pOptions,
	xvfsfile_v1* pFile)
{
	test_vfs_concurrent_context* pProvider =
		(test_vfs_concurrent_context*)pContext;
	test_vfs_concurrent_file* pState;
	(void)CaseMode;

	if ( !testVfsConcurrentPath(RelativePath, "file") &&
		 !testVfsConcurrentPath(RelativePath, "probe") )
		return XVFS_LOOKUP_MISS;
	if ( pProvider->Reenter &&
		 testVfsConcurrentPath(RelativePath, "probe") ) {
		xfileinfo Info;
		xfile Nested;
		xvfsmount NestedMount;
		char iValue = 0;
		size_t iRead = 0u;
		bool bNested;

		(void)xrtAtomic32FetchAdd(&pProvider->ReentrantCalls,
			1u, XMEMORY_RELAXED);
		bNested = xrtVfsStat(pProvider->Vfs,
			"/inner/status", false, &Info) && (Info.Size == 1u);
		Nested = xrtVfsOpen(pProvider->Vfs, "/inner/file", NULL);
		bNested = (Nested != NULL) && bNested;
		if ( Nested != NULL ) {
			bNested = xrtRead(Nested, &iValue, 1u, &iRead) &&
				(iRead == 1u) && (iValue == 'I') && bNested;
			bNested = xrtClose(Nested) && bNested;
		}
		NestedMount = xrtVfsMount(pProvider->Vfs, "/callback-temp", 0,
			XVFS_CASE_SENSITIVE, &testVfsConcurrentProvider, pProvider, 0u);
		bNested = (NestedMount != NULL) && bNested;
		if ( NestedMount != NULL ) {
			bNested = xrtVfsUnmount(NestedMount) && bNested;
			xrtVfsMountDestroy(NestedMount);
		}
		if ( !bNested ) {
			(void)xrtAtomic32FetchAdd(&pProvider->Failures,
				1u, XMEMORY_RELAXED);
			return XVFS_LOOKUP_ERROR;
		}
	}
	pState = (test_vfs_concurrent_file*)xrtMalloc(sizeof(*pState));
	if ( pState == NULL ) return XVFS_LOOKUP_ERROR;
	pState->Context = pProvider;
	(void)xrtAtomic32FetchAdd(&pProvider->Opens, 1u, XMEMORY_RELAXED);
	pFile->Ops = &testVfsConcurrentFileOps;
	pFile->State = pState;
	pFile->Flags = pOptions->Flags;
	return XVFS_LOOKUP_OPENED;
}

static xvfslookup testVfsConcurrentStat(void* pContext,
	xvfscase CaseMode, xstrview RelativePath,
	bool bFollowLink, xfileinfo* pInfo)
{
	(void)pContext;
	(void)CaseMode;
	(void)bFollowLink;
	if ( !testVfsConcurrentPath(RelativePath, "file") &&
		 !testVfsConcurrentPath(RelativePath, "status") )
		return XVFS_LOOKUP_MISS;
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->Type = XFILE_TYPE_FILE;
	pInfo->Available = XFILE_INFO_SIZE;
	pInfo->Size = 1u;
	return XVFS_LOOKUP_OPENED;
}

static const xvfsprovider_v1 testVfsConcurrentProvider = {
	(uint32)sizeof(xvfsprovider_v1),
	XRT_VFS_PROVIDER_VERSION,
	XVFS_PROVIDER_STAT,
	testVfsConcurrentRetain,
	testVfsConcurrentRelease,
	testVfsConcurrentOpen,
	testVfsConcurrentStat,
	NULL,
	NULL
};



static void testVfsConcurrentContextBalanced(
	const test_vfs_concurrent_context* pContext, cstr sMessage)
{
	testRequire(xrtAtomic32Load(&pContext->Retains,
		XMEMORY_ACQUIRE) == xrtAtomic32Load(&pContext->Releases,
		XMEMORY_ACQUIRE), sMessage);
	testRequire(xrtAtomic32Load(&pContext->Opens,
		XMEMORY_ACQUIRE) == xrtAtomic32Load(&pContext->Closes,
		XMEMORY_ACQUIRE), "VFS provider file states were not closed exactly once");
}

static void testVfsReentrantCallback(void)
{
	test_vfs_concurrent_context Inner;
	test_vfs_concurrent_context Outer;
	xvfs Vfs = xrtVfsCreate();
	xvfsmount InnerMount;
	xvfsmount OuterMount;
	xfile File;
	char iValue = 0;
	size_t iRead = 0u;

	testRequire(Vfs != NULL, "reentrant VFS namespace creation failed");
	testVfsConcurrentContextInit(&Inner, Vfs, 'I');
	testVfsConcurrentContextInit(&Outer, Vfs, 'R');
	Outer.Reenter = true;
	Outer.ReleaseReenter = true;
	InnerMount = xrtVfsMount(Vfs, "/inner", 0,
		XVFS_CASE_SENSITIVE, &testVfsConcurrentProvider, &Inner, 0u);
	OuterMount = xrtVfsMount(Vfs, "/outer", 0,
		XVFS_CASE_SENSITIVE, &testVfsConcurrentProvider, &Outer, 0u);
	testRequire((InnerMount != NULL) && (OuterMount != NULL),
		"reentrant VFS fixture mount failed");

	File = xrtVfsOpen(Vfs, "/outer/probe", NULL);
	testRequire(File != NULL,
		"provider callback could not reenter the same VFS namespace");
	testRequire(xrtRead(File, &iValue, 1u, &iRead) &&
		(iRead == 1u) && (iValue == 'R'),
		"reentrant provider returned the wrong file");
	testRequire(xrtClose(File), "reentrant provider file close failed");
	testRequire(xrtAtomic32Load(&Outer.ReentrantCalls,
		XMEMORY_ACQUIRE) == 1u,
		"provider callback did not perform its nested VFS lookup");
	testRequire(xrtAtomic32Load(&Outer.Failures,
		XMEMORY_ACQUIRE) == 0u,
		"nested VFS lookup failed inside provider callback");

	testRequire(xrtVfsUnmount(OuterMount) && xrtVfsUnmount(InnerMount),
		"reentrant VFS fixture unmount failed");
	xrtVfsMountDestroy(OuterMount);
	testRequire(xrtAtomic32Load(&Outer.ReentrantReleases,
		XMEMORY_ACQUIRE) >= 2u,
		"provider ContextRelease did not reenter an unlocked VFS registry");
	xrtVfsMountDestroy(InnerMount);
	xrtVfsDestroy(Vfs);
	testVfsConcurrentContextBalanced(&Outer,
		"reentrant outer provider context reference imbalance");
	testVfsConcurrentContextBalanced(&Inner,
		"reentrant inner provider context reference imbalance");
}



static int32 testVfsConcurrentReader(void* pData)
{
	test_vfs_concurrent_run* pRun = (test_vfs_concurrent_run*)pData;

	(void)xrtAtomic32FetchAdd(&pRun->Ready, 1u, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(&pRun->Start, XMEMORY_ACQUIRE) == 0u )
		xrtThreadYield();
	for ( uint32 i = 0u; i < TEST_VFS_READER_ITERATIONS; i++ ) {
		xfile File = xrtVfsOpen(pRun->Vfs, "/file", NULL);
		char iValue = 0;
		size_t iRead = 0u;
		bool bValid;

		if ( File == NULL ) {
			(void)xrtAtomic32FetchAdd(&pRun->Failures,
				1u, XMEMORY_RELAXED);
			xrtClearError();
			continue;
		}
		bValid = xrtRead(File, &iValue, 1u, &iRead) &&
			(iRead == 1u) && ((iValue == 'B') || (iValue == 'O'));
		if ( !xrtClose(File) ) bValid = false;
		if ( !bValid ) {
			(void)xrtAtomic32FetchAdd(&pRun->Failures,
				1u, XMEMORY_RELAXED);
			xrtClearError();
		}
		if ( (i & 63u) == 0u ) xrtThreadYield();
	}
	return 0;
}

static void testVfsConcurrentPublication(void)
{
	test_vfs_concurrent_context Base;
	test_vfs_concurrent_context Overlay;
	test_vfs_concurrent_run Run;
	xmemdebugsnapshot Before;
	xmemdebugsnapshot After;
	xthread* arrReader[TEST_VFS_READER_COUNT];
	xvfs Vfs;
	xvfsmount BaseMount;

	xrtMemDebugSnapshot(&Before);
	Vfs = xrtVfsCreate();
	testRequire(Vfs != NULL, "concurrent VFS namespace creation failed");
	testVfsConcurrentContextInit(&Base, Vfs, 'B');
	testVfsConcurrentContextInit(&Overlay, Vfs, 'O');
	BaseMount = xrtVfsMount(Vfs, "/", 0,
		XVFS_CASE_SENSITIVE, &testVfsConcurrentProvider, &Base, 0u);
	testRequire(BaseMount != NULL, "concurrent VFS base mount failed");
	memset(&Run, 0, sizeof(Run));
	Run.Vfs = Vfs;
	xrtAtomic32Init(&Run.Ready, 0u);
	xrtAtomic32Init(&Run.Start, 0u);
	xrtAtomic32Init(&Run.Failures, 0u);
	for ( size_t i = 0u; i < TEST_VFS_READER_COUNT; i++ ) {
		arrReader[i] = xrtThreadCreate(testVfsConcurrentReader, &Run, 0u);
		testRequire(arrReader[i] != NULL,
			"concurrent VFS reader thread creation failed");
	}
	while ( xrtAtomic32Load(&Run.Ready,
		XMEMORY_ACQUIRE) != TEST_VFS_READER_COUNT ) xrtThreadYield();
	xrtAtomic32Store(&Run.Start, 1u, XMEMORY_RELEASE);

	for ( uint32 i = 0u; i < TEST_VFS_PUBLISH_ITERATIONS; i++ ) {
		xvfsmount Mount = xrtVfsMount(Vfs, "/", 10,
			XVFS_CASE_SENSITIVE, &testVfsConcurrentProvider, &Overlay, 0u);

		testRequire(Mount != NULL,
			"concurrent VFS overlay publication failed");
		xrtThreadYield();
		testRequire(xrtVfsUnmount(Mount),
			"concurrent VFS overlay unmount failed");
		xrtVfsMountDestroy(Mount);
	}
	for ( size_t i = 0u; i < TEST_VFS_READER_COUNT; i++ ) {
		testRequire(xrtThreadWait(arrReader[i]) == XWAIT_OK,
			"concurrent VFS reader wait failed");
		testRequire(xrtThreadExitCode(arrReader[i]) == 0,
			"concurrent VFS reader returned an error");
		xrtThreadDestroy(arrReader[i]);
	}
	testRequire(xrtAtomic32Load(&Run.Failures,
		XMEMORY_ACQUIRE) == 0u,
		"VFS lookup failed during concurrent snapshot publication");
	testRequire(xrtVfsUnmount(BaseMount),
		"concurrent VFS base unmount failed");
	xrtVfsMountDestroy(BaseMount);
	xrtVfsDestroy(Vfs);
	testVfsConcurrentContextBalanced(&Base,
		"concurrent base provider context reference imbalance");
	testVfsConcurrentContextBalanced(&Overlay,
		"concurrent overlay provider context reference imbalance");
	xrtClearError();
	xrtMemDebugSnapshot(&After);
	testRequire((After.LiveCount == Before.LiveCount) &&
		(After.LiveBytes == Before.LiveBytes),
		"concurrent VFS publication leaked a logical allocation");
}



int main(void)
{
	testRequire(xrtMemDebugEnable(true),
		"VFS concurrency memory debug enable failed");
	testVfsReentrantCallback();
	testVfsConcurrentPublication();
	testRequire(xrtMemDebugReset(),
		"VFS concurrency test left live allocations");
	return 0;
}

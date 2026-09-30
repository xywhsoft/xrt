#ifdef VFS_PACK_CONCURRENCY_SINGLE
	#define XRT_IMPLEMENTATION
	#include "../../single/xrt.h"
#else
	#include <xrt/atomic.h>
	#include <xrt/memory_debug.h>
	#include <xrt/thread.h>
	#include <xrt/vfs.h>
#endif

#include "../test.h"
#define TEST_VFS_PACK_FIXTURE_FILE
#include "vfs_pack_fixture.h"



#define TEST_VFS_PACK_THREADS 100u
#define TEST_VFS_PACK_PAYLOAD (1024u * 1024u)

typedef struct test_vfs_pack_run {
	xvfs Vfs;
	const uint8* Expected;
	size_t Size;
	bool ShouldOpen;
	xatomic32 Ready;
	xatomic32 Start;
	xatomic32 Failures;
} test_vfs_pack_run;



static int32 testVfsPackReader(void* pData)
{
	test_vfs_pack_run* pRun = (test_vfs_pack_run*)pData;
	uint8 arrFirst[32];
	uint8 arrLast[32];
	size_t iFirst = 0u;
	size_t iLast = 0u;
	xfile File;
	bool bValid;

	(void)xrtAtomic32FetchAdd(&pRun->Ready, 1u, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(&pRun->Start, XMEMORY_ACQUIRE) == 0u )
		xrtThreadYield();
	File = xrtVfsOpen(pRun->Vfs, "/payload", NULL);
	if ( pRun->ShouldOpen ) {
		bValid = (File != NULL) &&
			xrtReadAt(File, 0u, arrFirst, sizeof(arrFirst), &iFirst) &&
			xrtReadAt(File, pRun->Size - sizeof(arrLast),
				arrLast, sizeof(arrLast), &iLast) &&
			(iFirst == sizeof(arrFirst)) && (iLast == sizeof(arrLast)) &&
			(memcmp(arrFirst, pRun->Expected, sizeof(arrFirst)) == 0) &&
			(memcmp(arrLast, pRun->Expected + pRun->Size - sizeof(arrLast),
				sizeof(arrLast)) == 0);
		if ( File != NULL ) bValid = xrtClose(File) && bValid;
	} else {
		bValid = (File == NULL) && (xrtGetError() != NULL) &&
			(xrtErrorCode(xrtGetError()) == XVFS_ERROR_CHECKSUM);
		if ( File != NULL ) (void)xrtClose(File);
	}
	if ( !bValid )
		(void)xrtAtomic32FetchAdd(&pRun->Failures, 1u, XMEMORY_RELAXED);
	xrtClearError();
	return 0;
}



static void testVfsPackConcurrent(bool bBadChecksum)
{
	bytes pPayload = (bytes)xrtMalloc(TEST_VFS_PACK_PAYLOAD);
	bytes pArchive;
	size_t iArchiveSize;
	str sPath = NULL;
	xfile Source;
	xmemdebugsnapshot Before;
	xmemdebugsnapshot After;
	xvfspack Pack;
	xvfs Vfs;
	xvfsmount Mount;
	test_vfs_pack_run Run;
	xthread* arrThreads[TEST_VFS_PACK_THREADS];
	xvfspackstats Stats;

	testRequire(pPayload != NULL,
		"pack concurrency payload allocation failed");
	for ( size_t i = 0u; i < TEST_VFS_PACK_PAYLOAD; i++ )
		pPayload[i] = (uint8)((i * 131u + 17u) & 0xFFu);
	pArchive = testVfsPackFixtureBuild("payload",
		pPayload, TEST_VFS_PACK_PAYLOAD,
		pPayload, TEST_VFS_PACK_PAYLOAD,
		XVFS_PACK_STORE, bBadChecksum, &iArchiveSize);
	Source = testVfsPackFixtureFile(pArchive, iArchiveSize, &sPath);
	testRequire((Source != NULL) && xrtClose(Source),
		"pack concurrency fixture initialization failed");
	xrtMemDebugSnapshot(&Before);
	Source = xrtFileOpen(sPath, NULL);
	Pack = xrtVfsPackCreate(Source, 0u, iArchiveSize, NULL);
	testRequire(Pack != NULL, "pack concurrency Create failed");
	Vfs = xrtVfsCreate();
	Mount = xrtVfsPackMount(Vfs, "/", 0,
		XVFS_CASE_SENSITIVE, Pack, 0u);
	testRequire((Vfs != NULL) && (Mount != NULL),
		"pack concurrency mount failed");
	memset(&Run, 0, sizeof(Run));
	Run.Vfs = Vfs;
	Run.Expected = pPayload;
	Run.Size = TEST_VFS_PACK_PAYLOAD;
	Run.ShouldOpen = !bBadChecksum;
	xrtAtomic32Init(&Run.Ready, 0u);
	xrtAtomic32Init(&Run.Start, 0u);
	xrtAtomic32Init(&Run.Failures, 0u);
	for ( size_t i = 0u; i < TEST_VFS_PACK_THREADS; i++ ) {
		arrThreads[i] = xrtThreadCreate(testVfsPackReader, &Run, 0u);
		testRequire(arrThreads[i] != NULL,
			"pack concurrency thread creation failed");
	}
	while ( xrtAtomic32Load(&Run.Ready,
		XMEMORY_ACQUIRE) != TEST_VFS_PACK_THREADS ) xrtThreadYield();
	xrtAtomic32Store(&Run.Start, 1u, XMEMORY_RELEASE);
	for ( size_t i = 0u; i < TEST_VFS_PACK_THREADS; i++ ) {
		testRequire(xrtThreadWait(arrThreads[i]) == XWAIT_OK,
			"pack concurrency thread wait failed");
		testRequire(xrtThreadExitCode(arrThreads[i]) == 0,
			"pack concurrency thread returned an error");
		xrtThreadDestroy(arrThreads[i]);
	}
	if ( xrtAtomic32Load(&Run.Failures, XMEMORY_ACQUIRE) != 0u ) {
		fprintf(stderr, "[diagnostic] pack concurrency mode=%s failures=%u\n",
			bBadChecksum ? "checksum" : "success",
			(unsigned)xrtAtomic32Load(&Run.Failures, XMEMORY_ACQUIRE));
	}
	testRequire(xrtAtomic32Load(&Run.Failures, XMEMORY_ACQUIRE) == 0u,
		"pack concurrent first access failed");
	testRequire(xrtVfsPackStats(Pack, &Stats) &&
		(Stats.Loads == 1u) &&
		(Stats.Failures == (bBadChecksum ? 1u : 0u)),
		"pack concurrent access performed more than one load");
	testRequire(xrtVfsUnmount(Mount), "pack concurrency unmount failed");
	xrtVfsMountDestroy(Mount);
	xrtVfsDestroy(Vfs);
	xrtVfsPackDestroy(Pack);
	xrtClearError();
	xrtMemDebugSnapshot(&After);
	testRequire((After.LiveCount == Before.LiveCount) &&
		(After.LiveBytes == Before.LiveBytes),
		"pack concurrency leaked a logical allocation");
	testRequire(xrtFileDelete(sPath),
		"pack concurrency fixture delete failed");
	xrtFree(sPath);
	xrtFree(pArchive);
	xrtFree(pPayload);
}



int main(void)
{
	testRequire(xrtMemDebugEnable(true),
		"pack concurrency memory debug enable failed");
	testVfsPackConcurrent(false);
	testVfsPackConcurrent(true);
	testRequire(xrtMemDebugReset(),
		"pack concurrency tests left live allocations");
	return 0;
}

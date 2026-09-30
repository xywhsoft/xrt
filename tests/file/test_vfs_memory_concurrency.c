#ifdef VFS_MEMORY_CONCURRENCY_SINGLE
	#define XRT_IMPLEMENTATION
	#include "../../single/xrt.h"
#else
	#include <xrt/atomic.h>
	#include <xrt/memory_debug.h>
	#include <xrt/thread.h>
	#include <xrt/vfs.h>
#endif

#include "../test.h"



#define TEST_VFS_MEMORY_READERS 4u
#define TEST_VFS_MEMORY_ITERATIONS 4000u

typedef struct test_vfs_memory_run {
	xvfs Vfs;
	xatomic32 Ready;
	xatomic32 Start;
	xatomic32 Failures;
} test_vfs_memory_run;



static int32 testVfsMemoryReader(void* pData)
{
	test_vfs_memory_run* pRun = (test_vfs_memory_run*)pData;
	static const char sExpected[] = "immutable-memory-payload";

	(void)xrtAtomic32FetchAdd(&pRun->Ready, 1u, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(&pRun->Start, XMEMORY_ACQUIRE) == 0u )
		xrtThreadYield();
	for ( uint32 i = 0u; i < TEST_VFS_MEMORY_ITERATIONS; i++ ) {
		xfile File = xrtVfsOpen(pRun->Vfs, "/data/value.txt", NULL);
		char sData[sizeof(sExpected)] = { 0 };
		size_t iRead = 0u;
		xfileinfo Info;
		bool bValid = File != NULL;

		if ( File != NULL ) {
			bValid = xrtReadAt(File, 0u, sData, sizeof(sExpected) - 1u,
				&iRead) && (iRead == sizeof(sExpected) - 1u) &&
				(memcmp(sData, sExpected, iRead) == 0) &&
				xrtFileStat(File, &Info) &&
				(Info.Size == sizeof(sExpected) - 1u) &&
				xrtClose(File);
		}
		if ( (i & 63u) == 0u ) {
			xdir Dir = xrtVfsDirOpen(pRun->Vfs, "/data", 0u);
			xdirentry Entry;
			size_t iEntries = 0u;

			bValid = (Dir != NULL) && bValid;
			if ( Dir != NULL ) {
				while ( xrtDirNext(Dir, &Entry) == XDIR_NEXT_ITEM ) iEntries++;
				bValid = (iEntries == 2u) && xrtDirClose(Dir) && bValid;
			}
		}
		if ( !bValid ) {
			(void)xrtAtomic32FetchAdd(&pRun->Failures,
				1u, XMEMORY_RELAXED);
			xrtClearError();
		}
		if ( (i & 127u) == 0u ) xrtThreadYield();
	}
	return 0;
}



int main(void)
{
	xmemdebugsnapshot Before;
	xmemdebugsnapshot After;
	test_vfs_memory_run Run;
	xthread* arrReaders[TEST_VFS_MEMORY_READERS];
	xvfsmemory Memory;
	xvfs Vfs;
	xvfsmount Mount;

	testRequire(xrtMemDebugEnable(true), "memory debug enable failed");
	xrtMemDebugSnapshot(&Before);
	Memory = xrtVfsMemoryCreate();
	Vfs = xrtVfsCreate();
	testRequire((Memory != NULL) && (Vfs != NULL) &&
		xrtVfsMemoryPutCopy(Memory, "data/value.txt",
			"immutable-memory-payload", 24u) &&
		xrtVfsMemoryPutCopy(Memory, "data/empty.bin", NULL, 0u) &&
		xrtVfsMemorySeal(Memory),
		"memory concurrency fixture build failed");
	Mount = xrtVfsMemoryMount(Vfs, "/", 0,
		XVFS_CASE_SENSITIVE, Memory, 0u);
	testRequire(Mount != NULL, "memory concurrency mount failed");
	xrtVfsMemoryDestroy(Memory);

	memset(&Run, 0, sizeof(Run));
	Run.Vfs = Vfs;
	xrtAtomic32Init(&Run.Ready, 0u);
	xrtAtomic32Init(&Run.Start, 0u);
	xrtAtomic32Init(&Run.Failures, 0u);
	for ( size_t i = 0u; i < TEST_VFS_MEMORY_READERS; i++ ) {
		arrReaders[i] = xrtThreadCreate(testVfsMemoryReader, &Run, 0u);
		testRequire(arrReaders[i] != NULL,
			"memory concurrency reader creation failed");
	}
	while ( xrtAtomic32Load(&Run.Ready,
		XMEMORY_ACQUIRE) != TEST_VFS_MEMORY_READERS ) xrtThreadYield();
	xrtAtomic32Store(&Run.Start, 1u, XMEMORY_RELEASE);
	for ( size_t i = 0u; i < TEST_VFS_MEMORY_READERS; i++ ) {
		testRequire(xrtThreadWait(arrReaders[i]) == XWAIT_OK,
			"memory concurrency reader wait failed");
		testRequire(xrtThreadExitCode(arrReaders[i]) == 0,
			"memory concurrency reader returned an error");
		xrtThreadDestroy(arrReaders[i]);
	}
	testRequire(xrtAtomic32Load(&Run.Failures,
		XMEMORY_ACQUIRE) == 0u,
		"concurrent memory provider access failed");
	testRequire(xrtVfsUnmount(Mount), "memory concurrency unmount failed");
	xrtVfsMountDestroy(Mount);
	xrtVfsDestroy(Vfs);
	xrtClearError();
	xrtMemDebugSnapshot(&After);
	testRequire((After.LiveCount == Before.LiveCount) &&
		(After.LiveBytes == Before.LiveBytes),
		"memory provider concurrency leaked a logical allocation");
	testRequire(xrtMemDebugReset(),
		"memory concurrency test left live allocations");
	return 0;
}

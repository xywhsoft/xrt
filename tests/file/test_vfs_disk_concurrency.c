#ifdef VFS_DISK_CONCURRENCY_SINGLE
	#define XRT_IMPLEMENTATION
	#include "../../single/xrt.h"
#else
	#include <xrt/atomic.h>
	#include <xrt/memory_debug.h>
	#include <xrt/thread.h>
	#include <xrt/vfs.h>
#endif

#include "../test.h"



#define TEST_VFS_DISK_READERS 4u
#define TEST_VFS_DISK_ITERATIONS 1000u

typedef struct test_vfs_disk_run {
	xvfs Vfs;
	xatomic32 Ready;
	xatomic32 Start;
	xatomic32 Failures;
	xatomic32 OpenFailures;
	xatomic32 IoFailures;
	xatomic32 DirFailures;
} test_vfs_disk_run;



static int32 testVfsDiskReader(void* pData)
{
	test_vfs_disk_run* pRun = (test_vfs_disk_run*)pData;
	static const char sExpected[] = "concurrent-disk-payload";

	(void)xrtAtomic32FetchAdd(&pRun->Ready, 1u, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(&pRun->Start, XMEMORY_ACQUIRE) == 0u )
		xrtThreadYield();
	for ( uint32 i = 0u; i < TEST_VFS_DISK_ITERATIONS; i++ ) {
		xfile File = xrtVfsOpen(pRun->Vfs, "/DATA/VALUE.TXT", NULL);
		char sData[sizeof(sExpected)] = { 0 };
		size_t iRead = 0u;
		xfileinfo Info;
		bool bValid = File != NULL;

		if ( File == NULL ) {
			(void)xrtAtomic32FetchAdd(&pRun->OpenFailures,
				1u, XMEMORY_RELAXED);
		} else {
			bValid = xrtReadAt(File, 0u, sData, sizeof(sExpected) - 1u,
				&iRead) && (iRead == sizeof(sExpected) - 1u) &&
				(memcmp(sData, sExpected, iRead) == 0) &&
				xrtFileStat(File, &Info) &&
				(Info.Size == sizeof(sExpected) - 1u) &&
				xrtClose(File);
			if ( !bValid ) (void)xrtAtomic32FetchAdd(&pRun->IoFailures,
				1u, XMEMORY_RELAXED);
		}
		if ( (i & 63u) == 0u ) {
			xdir Dir = xrtVfsDirOpen(pRun->Vfs, "/data", 0u);
			xdirentry Entry;
			xdirnext Next = XDIR_NEXT_ERROR;
			size_t iEntries = 0u;

			bool bDirValid = Dir != NULL;
			if ( Dir != NULL ) {
				while ( (Next = xrtDirNext(Dir, &Entry)) ==
					XDIR_NEXT_ITEM ) iEntries++;
				bDirValid = (Next == XDIR_NEXT_END) && (iEntries == 1u) &&
					xrtDirClose(Dir);
			}
			if ( !bDirValid ) (void)xrtAtomic32FetchAdd(&pRun->DirFailures,
				1u, XMEMORY_RELAXED);
			bValid = bDirValid && bValid;
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
	char sDirectory[96];
	int iNameSize;
	xroot Parent;
	xroot Root;
	xfileoptions Options;
	xfile Fixture;
	xmemdebugsnapshot Before;
	xmemdebugsnapshot After;
	test_vfs_disk_run Run;
	xthread* arrReaders[TEST_VFS_DISK_READERS];
	xvfsdisk Disk;
	xvfs Vfs;
	xvfsmount Mount;

	testRequire(xrtMemDebugEnable(true),
		"disk concurrency memory debug enable failed");
	iNameSize = snprintf(sDirectory, sizeof(sDirectory),
		".xrt-vfs-disk-threads-%lld", (long long)xrtNow());
	testRequire((iNameSize > 0) && ((size_t)iNameSize < sizeof(sDirectory)),
		"disk concurrency fixture name failed");
	Parent = xrtRootOpen(".");
	testRequire(Parent != NULL, "disk concurrency parent root open failed");
	if ( !xrtRootRemove(Parent, sDirectory) ) xrtClearError();
	testRequire(xrtRootDirCreate(Parent, sDirectory, 0700u),
		"disk concurrency fixture directory create failed");
	Root = xrtRootOpenIn(Parent, sDirectory);
	testRequire((Root != NULL) && xrtRootDirCreate(Root, "Data", 0700u),
		"disk concurrency fixture root create failed");
	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE;
	Fixture = xrtRootFileOpen(Root, "Data/Value.txt", &Options);
	testRequire((Fixture != NULL) &&
		xrtWriteFull(Fixture, "concurrent-disk-payload", 23u, NULL) &&
		xrtClose(Fixture), "disk concurrency fixture write failed");

	xrtMemDebugSnapshot(&Before);
	Disk = xrtVfsDiskCreate(sDirectory, XVFS_DISK_READ);
	Vfs = xrtVfsCreate();
	testRequire((Disk != NULL) && (Vfs != NULL),
		"disk concurrency objects creation failed");
	Mount = xrtVfsDiskMount(Vfs, "/", 0,
		XVFS_CASE_ASCII_INSENSITIVE, Disk, 0u);
	testRequire(Mount != NULL, "disk concurrency mount failed");
	xrtVfsDiskDestroy(Disk);

	memset(&Run, 0, sizeof(Run));
	Run.Vfs = Vfs;
	xrtAtomic32Init(&Run.Ready, 0u);
	xrtAtomic32Init(&Run.Start, 0u);
	xrtAtomic32Init(&Run.Failures, 0u);
	xrtAtomic32Init(&Run.OpenFailures, 0u);
	xrtAtomic32Init(&Run.IoFailures, 0u);
	xrtAtomic32Init(&Run.DirFailures, 0u);
	for ( size_t i = 0u; i < TEST_VFS_DISK_READERS; i++ ) {
		arrReaders[i] = xrtThreadCreate(testVfsDiskReader, &Run, 0u);
		testRequire(arrReaders[i] != NULL,
			"disk concurrency reader creation failed");
	}
	while ( xrtAtomic32Load(&Run.Ready,
		XMEMORY_ACQUIRE) != TEST_VFS_DISK_READERS ) xrtThreadYield();
	xrtAtomic32Store(&Run.Start, 1u, XMEMORY_RELEASE);
	for ( size_t i = 0u; i < TEST_VFS_DISK_READERS; i++ ) {
		testRequire(xrtThreadWait(arrReaders[i]) == XWAIT_OK,
			"disk concurrency reader wait failed");
		testRequire(xrtThreadExitCode(arrReaders[i]) == 0,
			"disk concurrency reader returned an error");
		xrtThreadDestroy(arrReaders[i]);
	}
	if ( xrtAtomic32Load(&Run.Failures, XMEMORY_ACQUIRE) != 0u ) {
		fprintf(stderr, "[diagnostic] disk concurrency failures: "
			"open=%u io=%u dir=%u total=%u\n",
			(unsigned)xrtAtomic32Load(&Run.OpenFailures, XMEMORY_ACQUIRE),
			(unsigned)xrtAtomic32Load(&Run.IoFailures, XMEMORY_ACQUIRE),
			(unsigned)xrtAtomic32Load(&Run.DirFailures, XMEMORY_ACQUIRE),
			(unsigned)xrtAtomic32Load(&Run.Failures, XMEMORY_ACQUIRE));
	}
	testRequire(xrtAtomic32Load(&Run.Failures, XMEMORY_ACQUIRE) == 0u,
		"concurrent disk provider access failed");
	testRequire(xrtVfsUnmount(Mount), "disk concurrency unmount failed");
	xrtVfsMountDestroy(Mount);
	xrtVfsDestroy(Vfs);
	xrtClearError();
	xrtMemDebugSnapshot(&After);
	testRequire((After.LiveCount == Before.LiveCount) &&
		(After.LiveBytes == Before.LiveBytes),
		"disk provider concurrency leaked a logical allocation");
	testRequire(xrtRootRemove(Root, "Data/Value.txt") &&
		xrtRootRemove(Root, "Data") && xrtRootClose(Root) &&
		xrtRootRemove(Parent, sDirectory) && xrtRootClose(Parent),
		"disk concurrency fixture cleanup failed");
	testRequire(xrtMemDebugReset(),
		"disk concurrency test left live allocations");
	return 0;
}

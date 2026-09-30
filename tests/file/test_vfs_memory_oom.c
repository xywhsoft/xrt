#ifdef VFS_MEMORY_OOM_SINGLE
	#define XRT_IMPLEMENTATION
	#include "../../single/xrt.h"
#else
	#include <xrt/memory_debug.h>
	#include <xrt/vfs.h>
#endif

#include "../test.h"



static void testVfsMemoryOomSweep(void)
{
	size_t iPoints = 0u;
	bool bCompleted = false;

	for ( size_t iFail = 0u; iFail < 512u; iFail++ ) {
		xmemdebugsnapshot Before;
		xmemdebugsnapshot After;
		xvfsmemory Memory = NULL;
		xvfs Vfs = NULL;
		xvfsmount Mount = NULL;
		xfile File = NULL;
		xdir Dir = NULL;
		char sData[8] = { 0 };
		size_t iRead = 0u;

		xrtMemDebugSnapshot(&Before);
		testRequire(xrtMemDebugFailAfter(iFail),
			"memory provider OOM injection setup failed");
		Memory = xrtVfsMemoryCreate();
		if ( Memory == NULL ) goto cleanup;
		if ( !xrtVfsMemoryPutCopy(Memory,
			"dir/value.txt", "payload", 7u) ) goto cleanup;
		if ( !xrtVfsMemoryPutCopy(Memory,
			"dir/empty.bin", NULL, 0u) ) goto cleanup;
		if ( !xrtVfsMemorySeal(Memory) ) goto cleanup;
		Vfs = xrtVfsCreate();
		if ( Vfs == NULL ) goto cleanup;
		Mount = xrtVfsMemoryMount(Vfs, "/mem", 0,
			XVFS_CASE_SENSITIVE, Memory, 0u);
		if ( Mount == NULL ) goto cleanup;
		File = xrtVfsOpen(Vfs, "/mem/dir/value.txt", NULL);
		if ( File == NULL ) goto cleanup;
		if ( !xrtRead(File, sData, sizeof(sData), &iRead) ||
			 (iRead != 7u) || (memcmp(sData, "payload", 7u) != 0) )
			goto cleanup;
		Dir = xrtVfsDirOpen(Vfs, "/mem/dir", 0u);
		if ( Dir == NULL ) goto cleanup;
		bCompleted = true;

cleanup:
		if ( xrtMemDebugFailTriggered() ) iPoints++;
		xrtMemDebugFailClear();
		if ( Dir != NULL ) testRequire(xrtDirClose(Dir),
			"memory provider OOM directory cleanup failed");
		if ( File != NULL ) testRequire(xrtClose(File),
			"memory provider OOM file cleanup failed");
		if ( Mount != NULL ) {
			testRequire(xrtVfsUnmount(Mount),
				"memory provider OOM unmount cleanup failed");
			xrtVfsMountDestroy(Mount);
		}
		xrtVfsDestroy(Vfs);
		xrtVfsMemoryDestroy(Memory);
		xrtClearError();
		xrtMemDebugSnapshot(&After);
		testRequire((After.LiveCount == Before.LiveCount) &&
			(After.LiveBytes == Before.LiveBytes),
			"memory provider OOM sweep leaked a logical allocation");
		if ( bCompleted ) break;
	}
	testRequire(bCompleted && (iPoints != 0u),
		"memory provider OOM sweep did not cover allocation failures");
}



static void testVfsMemorySealRetry(void)
{
	xmemdebugsnapshot Before;
	xmemdebugsnapshot After;
	xvfsmemory Memory;
	xvfs Vfs;
	xvfsmount Mount;

	xrtMemDebugSnapshot(&Before);
	Memory = xrtVfsMemoryCreate();
	testRequire((Memory != NULL) &&
		xrtVfsMemoryPutCopy(Memory, "dir/value", "x", 1u),
		"memory seal retry fixture build failed");
	testRequire(xrtMemDebugFailAfter(0u),
		"memory seal retry OOM injection setup failed");
	testRequire(!xrtVfsMemorySeal(Memory) && xrtMemDebugFailTriggered(),
		"memory seal did not report its injected allocation failure");
	xrtMemDebugFailClear();
	xrtClearError();
	testRequire(xrtVfsMemorySeal(Memory),
		"memory seal did not recover after allocation failure");
	Vfs = xrtVfsCreate();
	testRequire(Vfs != NULL, "memory seal retry VFS creation failed");
	Mount = xrtVfsMemoryMount(Vfs, "/", 0,
		XVFS_CASE_SENSITIVE, Memory, 0u);
	testRequire(Mount != NULL,
		"memory seal retry did not publish a mountable index");
	testRequire(xrtVfsUnmount(Mount),
		"memory seal retry unmount failed");
	xrtVfsMountDestroy(Mount);
	xrtVfsDestroy(Vfs);
	xrtVfsMemoryDestroy(Memory);
	xrtClearError();
	xrtMemDebugSnapshot(&After);
	testRequire((After.LiveCount == Before.LiveCount) &&
		(After.LiveBytes == Before.LiveBytes),
		"memory seal retry leaked a logical allocation");
}



static void testVfsMemoryOwnedFailure(void)
{
	xmemdebugsnapshot Before;
	xmemdebugsnapshot After;
	xvfsmemory Memory;
	bytes pOwned;

	xrtMemDebugSnapshot(&Before);
	Memory = xrtVfsMemoryCreate();
	pOwned = (bytes)xrtMemDup("owned", 5u);
	testRequire((Memory != NULL) && (pOwned != NULL),
		"memory owned OOM fixture allocation failed");
	testRequire(xrtMemDebugFailAfter(0u),
		"memory owned OOM injection setup failed");
	testRequire(!xrtVfsMemoryPutOwned(Memory,
		"value", pOwned, 5u) && xrtMemDebugFailTriggered(),
		"failed owned put did not preserve caller ownership");
	xrtMemDebugFailClear();
	xrtClearError();
	xrtFree(pOwned);
	xrtVfsMemoryDestroy(Memory);
	xrtMemDebugSnapshot(&After);
	testRequire((After.LiveCount == Before.LiveCount) &&
		(After.LiveBytes == Before.LiveBytes),
		"failed owned put consumed or leaked the caller buffer");
}



int main(void)
{
	testRequire(xrtMemDebugEnable(true), "memory debug enable failed");
	testVfsMemoryOomSweep();
	testVfsMemorySealRetry();
	testVfsMemoryOwnedFailure();
	testRequire(xrtMemDebugReset(),
		"memory provider OOM tests left live allocations");
	return 0;
}

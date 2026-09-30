#ifdef VFS_DISK_OOM_SINGLE
	#define XRT_IMPLEMENTATION
	#include "../../single/xrt.h"
#else
	#include <xrt/memory_debug.h>
	#include <xrt/vfs.h>
#endif

#include "../test.h"



/* 穷举 context、路径解析、原生打开和目录物化的分配失败。 */
int main(void)
{
	char sDirectory[96];
	xroot Parent;
	xroot Root;
	xfileoptions Options;
	xfile Fixture;
	bool bCompleted = false;
	size_t iPoints = 0u;
	int iNameSize;

	testRequire(xrtMemDebugEnable(true), "disk provider memory debug enable failed");
	iNameSize = snprintf(sDirectory, sizeof(sDirectory),
		".xrt-vfs-disk-oom-%lld", (long long)xrtNow());
	testRequire((iNameSize > 0) && ((size_t)iNameSize < sizeof(sDirectory)),
		"disk provider OOM fixture name failed");
	Parent = xrtRootOpen(".");
	testRequire(Parent != NULL, "disk provider OOM parent open failed");
	if ( !xrtRootRemove(Parent, sDirectory) ) xrtClearError();
	testRequire(xrtRootDirCreate(Parent, sDirectory, 0700u),
		"disk provider OOM fixture directory create failed");
	Root = xrtRootOpenIn(Parent, sDirectory);
	testRequire(Root != NULL, "disk provider OOM fixture root open failed");
	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE;
	Fixture = xrtRootFileOpen(Root, "entry.txt", &Options);
	testRequire((Fixture != NULL) &&
		xrtWriteFull(Fixture, "payload", 7u, NULL) && xrtClose(Fixture),
		"disk provider OOM fixture file initialization failed");

	for ( size_t iFail = 0u; iFail < 512u; iFail++ ) {
		xmemdebugsnapshot Before;
		xmemdebugsnapshot After;
		xvfsdisk Disk = NULL;
		xvfs Vfs = NULL;
		xvfsmount Mount = NULL;
		xfile File = NULL;
		xdir Dir = NULL;
		char sData[8] = { 0 };
		size_t iRead = 0u;

		xrtMemDebugSnapshot(&Before);
		testRequire(xrtMemDebugFailAfter(iFail),
			"disk provider OOM injection setup failed");
		Disk = xrtVfsDiskCreate(sDirectory, XVFS_DISK_READ);
		if ( Disk == NULL ) goto cleanup;
		Vfs = xrtVfsCreate();
		if ( Vfs == NULL ) goto cleanup;
		Mount = xrtVfsDiskMount(Vfs, "/", 0,
			XVFS_CASE_ASCII_INSENSITIVE, Disk, 0u);
		if ( Mount == NULL ) goto cleanup;
		File = xrtVfsOpen(Vfs, "/ENTRY.TXT", NULL);
		if ( File == NULL ) goto cleanup;
		if ( !xrtRead(File, sData, sizeof(sData), &iRead) ||
			 (iRead != 7u) || (memcmp(sData, "payload", 7u) != 0) )
			goto cleanup;
		Dir = xrtVfsDirOpen(Vfs, "/", 0u);
		if ( Dir == NULL ) goto cleanup;
		bCompleted = true;

cleanup:
		if ( xrtMemDebugFailTriggered() ) iPoints++;
		xrtMemDebugFailClear();
		if ( Dir != NULL ) testRequire(xrtDirClose(Dir),
			"disk provider OOM directory cleanup failed");
		if ( File != NULL ) testRequire(xrtClose(File),
			"disk provider OOM file cleanup failed");
		if ( Mount != NULL ) {
			testRequire(xrtVfsUnmount(Mount),
				"disk provider OOM unmount cleanup failed");
			xrtVfsMountDestroy(Mount);
		}
		xrtVfsDestroy(Vfs);
		xrtVfsDiskDestroy(Disk);
		xrtClearError();
		xrtMemDebugSnapshot(&After);
		testRequire((After.LiveCount == Before.LiveCount) &&
			(After.LiveBytes == Before.LiveBytes),
			"disk provider OOM sweep leaked a logical allocation");
		if ( bCompleted ) break;
	}
	testRequire(bCompleted && (iPoints != 0u),
		"disk provider OOM sweep covered no allocation points");
	testRequire(xrtRootRemove(Root, "entry.txt") && xrtRootClose(Root) &&
		xrtRootRemove(Parent, sDirectory) && xrtRootClose(Parent),
		"disk provider OOM fixture cleanup failed");
	testRequire(xrtMemDebugReset(),
		"disk provider OOM tests left live allocations");
	return 0;
}

#ifdef VFS_PACK_OOM_SINGLE
	#define XRT_IMPLEMENTATION
	#include "../../single/xrt.h"
#else
	#include <xrt/memory_debug.h>
	#include <xrt/vfs.h>
#endif

#include "../test.h"
#define TEST_VFS_PACK_FIXTURE_FILE
#include "vfs_pack_fixture.h"



int main(void)
{
	static const uint8 Original[] = "compressed hello";
	static const uint8 Stored[] = {
		0x5D, 0x00, 0x00, 0x01, 0x00,
		0x00, 0x31, 0x9B, 0xC9, 0xF3, 0xF6, 0xBC, 0x8E,
		0xC5, 0xCE, 0x1D, 0x57, 0xBA, 0x49, 0xEF, 0xDA,
		0x98, 0x6D, 0x35, 0xCB, 0xFF, 0xFF, 0xDD, 0x9D, 0x00, 0x00
	};
	bytes pArchive;
	size_t iArchiveSize;
	str sPath = NULL;
	xfile Fixture;
	bool bCompleted = false;
	size_t iPoints = 0u;

	testRequire(xrtMemDebugEnable(true),
		"pack provider memory debug enable failed");
	pArchive = testVfsPackFixtureBuild("dir/value.txt",
		Stored, sizeof(Stored), Original, sizeof(Original) - 1u,
		XVFS_PACK_LZMA1, false, &iArchiveSize);
	Fixture = testVfsPackFixtureFile(pArchive, iArchiveSize, &sPath);
	testRequire((Fixture != NULL) && xrtClose(Fixture),
		"pack OOM fixture initialization failed");

	for ( size_t iFail = 0u; iFail < 1024u; iFail++ ) {
		xmemdebugsnapshot Before;
		xmemdebugsnapshot After;
		xfile Source = NULL;
		xvfspack Pack = NULL;
		xvfs Vfs = NULL;
		xvfsmount Mount = NULL;
		xfile File = NULL;
		xdir Dir = NULL;
		char arrData[sizeof(Original)] = { 0 };
		size_t iRead = 0u;

		xrtMemDebugSnapshot(&Before);
		testRequire(xrtMemDebugFailAfter(iFail),
			"pack provider OOM injection setup failed");
		Source = xrtFileOpen(sPath, NULL);
		if ( Source == NULL ) goto cleanup;
		Pack = xrtVfsPackCreate(Source, 0u, iArchiveSize, NULL);
		if ( Pack == NULL ) goto cleanup;
		Source = NULL;
		Vfs = xrtVfsCreate();
		if ( Vfs == NULL ) goto cleanup;
		Mount = xrtVfsPackMount(Vfs, "/pack", 0,
			XVFS_CASE_SENSITIVE, Pack, 0u);
		if ( Mount == NULL ) goto cleanup;
		File = xrtVfsOpen(Vfs, "/pack/dir/value.txt", NULL);
		if ( File == NULL ) goto cleanup;
		if ( !xrtRead(File, arrData, sizeof(arrData), &iRead) ||
			(iRead != sizeof(Original) - 1u) ||
			(memcmp(arrData, Original, iRead) != 0) ) goto cleanup;
		Dir = xrtVfsDirOpen(Vfs, "/pack/dir", 0u);
		if ( Dir == NULL ) goto cleanup;
		bCompleted = true;

cleanup:
		if ( xrtMemDebugFailTriggered() ) iPoints++;
		xrtMemDebugFailClear();
		if ( Dir != NULL ) testRequire(xrtDirClose(Dir),
			"pack OOM directory cleanup failed");
		if ( File != NULL ) testRequire(xrtClose(File),
			"pack OOM file cleanup failed");
		if ( Mount != NULL ) {
			testRequire(xrtVfsUnmount(Mount),
				"pack OOM unmount cleanup failed");
			xrtVfsMountDestroy(Mount);
		}
		xrtVfsDestroy(Vfs);
		xrtVfsPackDestroy(Pack);
		if ( Source != NULL ) testRequire(xrtClose(Source),
			"failed pack Create did not leave a closable source");
		xrtClearError();
		xrtMemDebugSnapshot(&After);
		testRequire((After.LiveCount == Before.LiveCount) &&
			(After.LiveBytes == Before.LiveBytes),
			"pack provider OOM sweep leaked a logical allocation");
		if ( bCompleted ) break;
	}
	testRequire(bCompleted && (iPoints != 0u),
		"pack provider OOM sweep covered no allocation points");
	testRequire(xrtFileDelete(sPath), "pack OOM fixture delete failed");
	xrtFree(sPath);
	xrtFree(pArchive);
	testRequire(xrtMemDebugReset(),
		"pack provider OOM tests left live allocations");
	return 0;
}

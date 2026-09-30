#ifdef FILE_ROOT_DIR_OOM_SINGLE
	#define XRT_IMPLEMENTATION
	#include "../../single/xrt.h"
#else
	#include <xrt/memory_debug.h>
	#include <xrt/file.h>
#endif

#include "../test.h"



/* 执行一次完整枚举，并在任何失败后关闭已经取得的迭代器。 */
static bool testRootDirOomScenario(xroot Root, bool* pHit)
{
	xdir Dir = xrtRootDirOpen(Root, ".", XDIR_STAT);
	xdirentry Entry;
	xdirnext Next;
	bool bFound = false;
	bool bResult = false;

	if ( Dir == NULL ) goto done;
	while ( (Next = xrtDirNext(Dir, &Entry)) == XDIR_NEXT_ITEM ) {
		if ( xrtStrEqual(Entry.Name, xrtStrView("entry.txt")) &&
			 (Entry.Info.Type == XFILE_TYPE_FILE) ) bFound = true;
	}
	bResult = (Next == XDIR_NEXT_END) && bFound;
	if ( !xrtDirClose(Dir) ) bResult = false;

done:
	*pHit = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	return bResult;
}



/* 穷举 root iterator 的路径、状态、平台缓冲和 xdir 包装分配点。 */
int main(void)
{
	char sDirectory[96];
	xfileoptions Options;
	xmemdebugsnapshot Before;
	xmemdebugsnapshot After;
	xroot Parent;
	xroot Root;
	xfile File;
	bool bCompleted = false;
	size_t iPoints = 0u;
	int iNameSize;

	iNameSize = snprintf(sDirectory, sizeof(sDirectory),
		".xrt-root-dir-oom-%lld", (long long)xrtNow());
	testRequire((iNameSize > 0) && ((size_t)iNameSize < sizeof(sDirectory)),
		"root directory OOM fixture name failed");
	Parent = xrtRootOpen(".");
	testRequire(Parent != NULL, "root directory OOM parent open failed");
	if ( !xrtRootRemove(Parent, sDirectory) ) xrtClearError();
	testRequire(xrtRootDirCreate(Parent, sDirectory, 0700u),
		"root directory OOM fixture create failed");
	Root = xrtRootOpenIn(Parent, sDirectory);
	testRequire(Root != NULL, "root directory OOM root open failed");
	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE;
	File = xrtRootFileOpen(Root, "entry.txt", &Options);
	testRequire(File != NULL, "root directory OOM file open failed");
	testRequire(xrtWriteFull(File, "entry", 5u, NULL),
		"root directory OOM file write failed");
	testRequire(xrtClose(File), "root directory OOM file close failed");
	xrtClearError();
	xrtMemDebugSnapshot(&Before);
	for ( uint64 i = 0u; i < 128u; i++ ) {
		bool bHit;
		bool bResult;

		testRequire(xrtMemDebugFailAfter(i),
			"root directory OOM injection setup failed");
		bResult = testRootDirOomScenario(Root, &bHit);
		if ( bHit ) {
			testRequire(!bResult,
				"root directory iterator ignored an injected OOM");
			iPoints++;
		} else {
			testRequire(bResult,
				"root directory iterator failed after OOM sweep completion");
			bCompleted = true;
		}
		xrtClearError();
		xrtMemDebugSnapshot(&After);
		testRequire((After.LiveCount == Before.LiveCount) &&
			(After.LiveBytes == Before.LiveBytes),
			"root directory iterator leaked after injected OOM");
		if ( bCompleted ) break;
	}
	testRequire(bCompleted && (iPoints != 0u),
		"root directory OOM sweep covered no allocation points");
	testRequire(xrtRootRemove(Root, "entry.txt") &&
		xrtRootClose(Root) && xrtRootRemove(Parent, sDirectory) &&
		xrtRootClose(Parent),
		"root directory OOM fixture cleanup failed");
	return 0;
}

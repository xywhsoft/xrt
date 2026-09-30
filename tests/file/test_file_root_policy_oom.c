#ifdef FILE_ROOT_POLICY_OOM_SINGLE
	#define XRT_IMPLEMENTATION
	#include "../../single/xrt.h"
#else
	#include <xrt/memory_debug.h>
	#include <xrt/file.h>
	#include "../../src/internal/xrt_file_root.h"
#endif

#include "../test.h"



/* 执行一次精确名称打开，并在成功时关闭取得的文件。 */
static bool testRootPolicyOomScenario(xroot Root, bool* pHit)
{
	xfile File = __xrtRootFileOpenPolicy(Root, "entry.txt", NULL,
		XROOT_POLICY_CASE_SENSITIVE);
	bool bResult = false;

	if ( File != NULL ) bResult = xrtClose(File);
	*pHit = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	return bResult;
}



/* 穷举精确名称枚举、迭代器和路径解析中的分配失败。 */
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
		".xrt-root-policy-oom-%lld", (long long)xrtNow());
	testRequire((iNameSize > 0) && ((size_t)iNameSize < sizeof(sDirectory)),
		"root policy OOM fixture name failed");
	Parent = xrtRootOpen(".");
	testRequire(Parent != NULL, "root policy OOM parent open failed");
	if ( !xrtRootRemove(Parent, sDirectory) ) xrtClearError();
	testRequire(xrtRootDirCreate(Parent, sDirectory, 0700u),
		"root policy OOM fixture create failed");
	Root = xrtRootOpenIn(Parent, sDirectory);
	testRequire(Root != NULL, "root policy OOM root open failed");
	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE;
	File = xrtRootFileOpen(Root, "entry.txt", &Options);
	testRequire(File != NULL, "root policy OOM file open failed");
	testRequire(xrtWriteFull(File, "entry", 5u, NULL) && xrtClose(File),
		"root policy OOM file initialization failed");
	xrtClearError();
	xrtMemDebugSnapshot(&Before);
	for ( uint64 i = 0u; i < 128u; i++ ) {
		bool bHit;
		bool bResult;

		testRequire(xrtMemDebugFailAfter(i),
			"root policy OOM injection setup failed");
		bResult = testRootPolicyOomScenario(Root, &bHit);
		if ( bHit ) {
			testRequire(!bResult,
				"root exact policy ignored an injected OOM");
			iPoints++;
		} else {
			testRequire(bResult,
				"root exact policy failed after OOM sweep completion");
			bCompleted = true;
		}
		xrtClearError();
		xrtMemDebugSnapshot(&After);
		testRequire((After.LiveCount == Before.LiveCount) &&
			(After.LiveBytes == Before.LiveBytes),
			"root exact policy leaked after injected OOM");
		if ( bCompleted ) break;
	}
	testRequire(bCompleted && (iPoints != 0u),
		"root policy OOM sweep covered no allocation points");
	testRequire(xrtRootRemove(Root, "entry.txt") &&
		xrtRootClose(Root) && xrtRootRemove(Parent, sDirectory) &&
		xrtRootClose(Parent),
		"root policy OOM fixture cleanup failed");
	return 0;
}

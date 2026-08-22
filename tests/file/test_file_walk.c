#if !defined(_WIN32) && !defined(_WIN64) && !defined(_POSIX_C_SOURCE)
	#define _POSIX_C_SOURCE 200809L
#endif

#include "../test.h"

#if !defined(_WIN32) && !defined(_WIN64)
	#include <unistd.h>
#endif



/* 遍历测试回调上下文。 */
typedef struct test_walk_context {
	ptr Expected;
	uint64 Enter;
	uint64 Leave;
	uint64 Item;
	uint64 BadParam;
	uint64 BadPath;
	uint64 Cycle;
	uint64 Error;
	uint64 BadError;
	cstr SkipName;
	cstr ErrorPath;
	xwalkerroraction ErrorAction;
	bool StopAtItem;
	bool InvalidAtItem;
	bool FailAtItem;
	bool ReplaceError;
} test_walk_context;



/* 在系统临时目录构造遍历测试路径。 */
static str testWalkPath(cstr sName)
{
	str sDirectory = xrtPathTemp();
	str sPath;

	testRequire(sDirectory != NULL, "temporary directory query failed");
	sPath = xrtPathJoin(sDirectory, sName);
	xrtFree(sDirectory);
	testRequire(sPath != NULL, "walk test path allocation failed");
	return sPath;
}



/* 使用基础文件 API 写入遍历夹具。 */
static void testWalkWrite(cstr sPath, cstr sText)
{
	xfile File = xrtOpen(sPath,
		XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE);

	testRequire(File != NULL, "walk fixture file open failed");
	testRequire(xrtWriteFull(File, sText, strlen(sText), NULL),
		"walk fixture file write failed");
	testRequire(xrtClose(File), "walk fixture file close failed");
}



/* 记录事件并按上下文请求跳过、停止或失败。 */
static xwalkcontrol testWalkProc(const xwalkentry* pEntry, ptr pUserData)
{
	test_walk_context* pContext = (test_walk_context*)pUserData;
	xpathparts Parts;

	if ( pUserData != pContext->Expected ) {
		pContext->BadParam++;
	}
	if ( (pEntry->Path == NULL) || (pEntry->Path[0] == '\0') ||
		 (pEntry->Parent.Data == NULL) || (pEntry->Name.Data == NULL) ||
		 !xrtPathParse(xrtStrView(pEntry->Path), XPATH_NATIVE, &Parts) ||
		 !xrtStrEqual(pEntry->Parent, Parts.Parent) ||
		 !xrtStrEqual(pEntry->Name, Parts.Name) ) {
		pContext->BadPath++;
	}
	if ( (pEntry->Event == XWALK_ENTER) &&
		 ((pEntry->Flags & XWALK_ENTRY_CYCLE) != 0u) ) {
		pContext->Cycle++;
	}
	if ( pEntry->Event == XWALK_ENTER ) {
		pContext->Enter++;
		if ( (pContext->SkipName != NULL) &&
			xrtStrEqual(pEntry->Name, xrtStrView(pContext->SkipName)) ) {
			return XWALK_SKIP;
		}
		return XWALK_CONTINUE;
	}
	if ( pEntry->Event == XWALK_LEAVE ) {
		pContext->Leave++;
		return XWALK_CONTINUE;
	}
	pContext->Item++;
	if ( pContext->InvalidAtItem ) {
		return XWALK_SKIP;
	}
	if ( pContext->FailAtItem ) {
		xerror* pError = xrtErrorCreate(XERR_IO,
			"test.walk", 77, "callback failure");

		xrtSetError(pError);
		xrtErrorFree(pError);
		return XWALK_ERROR;
	}
	return pContext->StopAtItem ? XWALK_STOP : XWALK_CONTINUE;
}



/* 记录系统遍历错误并按上下文选择恢复动作。 */
static xwalkerroraction testWalkError(cstr sPath,
	const xerror* pError, ptr pUserData)
{
	test_walk_context* pContext = (test_walk_context*)pUserData;

	pContext->Error++;
	if ( (pUserData != pContext->Expected) || (pError == NULL) ||
		 (sPath == NULL) || ((pContext->ErrorPath != NULL) &&
		 (strcmp(sPath, pContext->ErrorPath) != 0)) ) {
		pContext->BadError++;
	}
	if ( pContext->ReplaceError ) {
		xerror* pReplacement = xrtErrorCreate(XERR_IO,
			"test.walk.error", 88, "replacement walk error");

		xrtSetError(pReplacement);
		xrtErrorFree(pReplacement);
	}
	return pContext->ErrorAction;
}



/* 完整深度优先遍历必须配对目录边界并得到精确统计。 */
static void testWalkAll(cstr sRoot)
{
	test_walk_context Context;
	xwalkstats Stats;

	memset(&Context, 0, sizeof(Context));
	Context.Expected = &Context;
	testRequire(xrtFileWalk(sRoot, NULL, testWalkProc, &Context, &Stats),
		"full file walk failed");
	testRequire((Context.Enter == 3u) && (Context.Leave == 3u) &&
		(Context.Item == 3u) && (Context.BadParam == 0u) &&
		(Context.BadPath == 0u), "file walk event contract mismatch");
	testRequire((Stats.Items == 6u) && (Stats.Directories == 3u) &&
		(Stats.Files == 3u) && (Stats.Links == 0u) &&
		(Stats.Others == 0u) && (Stats.Bytes == 15u) && !Stats.Stopped,
		"file walk statistics are incorrect");
}



/* 跳过目录仍必须产生一对进入和离开事件。 */
static void testWalkSkip(cstr sRoot)
{
	test_walk_context Context;
	xwalkstats Stats;

	memset(&Context, 0, sizeof(Context));
	Context.Expected = &Context;
	Context.SkipName = "sub";
	testRequire(xrtFileWalk(sRoot, NULL, testWalkProc, &Context, &Stats),
		"skipping file walk failed");
	testRequire((Context.Enter == 2u) && (Context.Leave == 2u) &&
		(Context.Item == 1u), "skipped directory boundary events are incorrect");
	testRequire((Stats.Items == 3u) && (Stats.Directories == 2u) &&
		(Stats.Files == 1u) && (Stats.Bytes == 4u),
		"skipping file walk statistics are incorrect");
}



/* 最大深度为零时只访问根目录但仍配对根事件。 */
static void testWalkDepth(cstr sRoot)
{
	xwalkoptions Options;
	test_walk_context Context;
	xwalkstats Stats;

	xrtWalkOptionsInit(&Options);
	Options.MaxDepth = 0u;
	memset(&Context, 0, sizeof(Context));
	Context.Expected = &Context;
	testRequire(xrtFileWalk(sRoot, &Options,
		testWalkProc, &Context, &Stats), "depth-limited walk failed");
	testRequire((Context.Enter == 1u) && (Context.Leave == 1u) &&
		(Context.Item == 0u) && (Stats.Items == 1u) &&
		(Stats.Directories == 1u), "depth zero walk crossed the root");
}



/* 成功停止必须关闭未完成迭代器并保留调用前错误。 */
static void testWalkStop(cstr sRoot)
{
	test_walk_context Context;
	xwalkstats Stats;
	xerror* pOld = xrtErrorCreate(XERR_VALUE, "test.old", 11, "old error");

	testRequire(pOld != NULL, "old error allocation failed");
	xrtSetError(pOld);
	xrtErrorFree(pOld);
	memset(&Context, 0, sizeof(Context));
	Context.Expected = &Context;
	Context.StopAtItem = true;
	testRequire(xrtFileWalk(sRoot, NULL, testWalkProc, &Context, &Stats),
		"successful walk stop failed");
	testRequire(Stats.Stopped && (Context.Item == 1u),
		"walk stop did not stop at the first item");
	testRequire((xrtGetError() != NULL) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "test.old") == 0),
		"successful walk stop replaced the previous error");
	xrtClearError();
}



/* 回调非法控制和显式错误必须成为本次遍历失败。 */
static void testWalkCallbackErrors(cstr sRoot)
{
	test_walk_context Context;

	memset(&Context, 0, sizeof(Context));
	Context.Expected = &Context;
	Context.InvalidAtItem = true;
	testRequire(!xrtFileWalk(sRoot, NULL, testWalkProc, &Context, NULL),
		"walk accepted skip control on a file item");
	testRequire((xrtGetError() != NULL) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.walk") == 0) &&
		(xrtErrorCode(xrtGetError()) == XWALK_ERROR_CALLBACK),
		"invalid callback control reported the wrong error");
	xrtClearError();

	memset(&Context, 0, sizeof(Context));
	Context.Expected = &Context;
	Context.FailAtItem = true;
	testRequire(!xrtFileWalk(sRoot, NULL, testWalkProc, &Context, NULL),
		"walk ignored callback failure");
	testRequire((xrtGetError() != NULL) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "test.walk") == 0) &&
		(xrtErrorCode(xrtGetError()) == 77),
		"walk lost the callback-provided error");
	xrtClearError();
}



/* 普通文件根必须产生单个 ITEM，失败则不能修改统计输出。 */
static void testWalkRootAndFailure(cstr sFile)
{
	xwalkoptions Options;
	test_walk_context Context;
	xwalkstats Stats;
	xwalkstats Saved;
	str sMissing = testWalkPath("xrt-file-walk-missing");
	xerror* pOld;

	testRequire(xrtFileWalk(sFile, NULL, NULL, NULL, &Stats),
		"single-file walk failed");
	testRequire((Stats.Items == 1u) && (Stats.Files == 1u) &&
		(Stats.Directories == 0u) && (Stats.Bytes == 4u),
		"single-file walk statistics are incorrect");
	memset(&Stats, 0xA5, sizeof(Stats));
	Saved = Stats;
	testRequire(!xrtFileWalk(sMissing, NULL, NULL, NULL, &Stats),
		"missing walk root unexpectedly succeeded");
	testRequire(memcmp(&Stats, &Saved, sizeof(Stats)) == 0,
		"failed file walk modified statistics output");
	xrtClearError();

	xrtWalkOptionsInit(&Options);
	Options.OnError = testWalkError;
	memset(&Context, 0, sizeof(Context));
	Context.Expected = &Context;
	Context.ErrorPath = sMissing;
	Context.ErrorAction = XWALK_ERROR_SKIP;
	pOld = xrtErrorCreate(XERR_VALUE, "test.old", 12, "old walk error");
	testRequire(pOld != NULL, "walk old error allocation failed");
	xrtSetError(pOld);
	xrtErrorFree(pOld);
	testRequire(xrtFileWalk(sMissing, &Options,
		NULL, &Context, &Stats), "walk error skip failed");
	testRequire((Context.Error == 1u) && (Context.BadError == 0u) &&
		(Stats.Items == 0u) && !Stats.Stopped,
		"walk error skip produced the wrong result");
	testRequire((xrtGetError() != NULL) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "test.old") == 0),
		"successful error recovery lost the previous error");
	xrtClearError();

	memset(&Context, 0, sizeof(Context));
	Context.Expected = &Context;
	Context.ErrorPath = sMissing;
	Context.ErrorAction = XWALK_ERROR_STOP;
	testRequire(xrtFileWalk(sMissing, &Options,
		NULL, &Context, &Stats) && Stats.Stopped,
		"walk error stop did not stop successfully");

	memset(&Context, 0, sizeof(Context));
	Context.Expected = &Context;
	Context.ErrorPath = sMissing;
	Context.ErrorAction = XWALK_ERROR_ABORT;
	Context.ReplaceError = true;
	testRequire(!xrtFileWalk(sMissing, &Options,
		NULL, &Context, NULL), "walk error abort unexpectedly succeeded");
	testRequire((xrtGetError() != NULL) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "test.walk.error") == 0) &&
		(xrtErrorCode(xrtGetError()) == 88),
		"walk error callback replacement was lost");
	xrtClearError();

	memset(&Context, 0, sizeof(Context));
	Context.Expected = &Context;
	Context.ErrorPath = sMissing;
	Context.ErrorAction = (xwalkerroraction)99;
	testRequire(!xrtFileWalk(sMissing, &Options,
		NULL, &Context, NULL), "walk accepted an invalid error action");
	testRequire((xrtGetError() != NULL) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.walk") == 0) &&
		(xrtErrorCode(xrtGetError()) == XWALK_ERROR_CALLBACK),
		"invalid walk error action reported the wrong error");
	xrtClearError();
	xrtFree(sMissing);
}



#if !defined(_WIN32) && !defined(_WIN64)

/* 跟随链接必须标出祖先环，并允许错误回调跳过损坏链接。 */
static void testWalkPosixLinks(cstr sRoot, cstr sDeep)
{
	str sCycle = xrtPathJoin(sDeep, "cycle");
	str sBroken = xrtPathJoin(sRoot, "broken");
	xwalkoptions Options;
	test_walk_context Context;
	xwalkstats Stats;

	testRequire((sCycle != NULL) && (sBroken != NULL),
		"walk link fixture path build failed");
	testRequire((symlink("../..", sCycle) == 0) &&
		(symlink("missing-target", sBroken) == 0),
		"walk link fixture create failed");

	xrtWalkOptionsInit(&Options);
	Options.Flags = XWALK_FOLLOW_LINKS;
	testRequire(!xrtFileWalk(sRoot, &Options,
		NULL, NULL, NULL), "follow walk ignored a broken link");
	xrtClearError();

	memset(&Context, 0, sizeof(Context));
	Context.Expected = &Context;
	Context.ErrorPath = sBroken;
	Context.ErrorAction = XWALK_ERROR_SKIP;
	Options.OnError = testWalkError;
	testRequire(xrtFileWalk(sRoot, &Options,
		testWalkProc, &Context, &Stats), "recovering link walk failed");
	if ( (Context.Enter != 4u) || (Context.Leave != 4u) ||
		 (Context.Item != 3u) || (Context.Cycle != 1u) ||
		 (Context.Error != 1u) || (Context.BadError != 0u) ) {
		fprintf(stderr,
			"[walk-links] enter=%llu leave=%llu item=%llu cycle=%llu "
			"error=%llu bad-error=%llu\n",
			(unsigned long long)Context.Enter,
			(unsigned long long)Context.Leave,
			(unsigned long long)Context.Item,
			(unsigned long long)Context.Cycle,
			(unsigned long long)Context.Error,
			(unsigned long long)Context.BadError);
	}
	testRequire((Context.Enter == 4u) && (Context.Leave == 4u) &&
		(Context.Item == 3u) && (Context.Cycle == 1u) &&
		(Context.Error == 1u) && (Context.BadError == 0u),
		"recovering link walk emitted the wrong events");
	testRequire((Stats.Items == 7u) && (Stats.Directories == 4u) &&
		(Stats.Files == 3u) && (Stats.Links == 1u) &&
		(Stats.Bytes == 15u), "recovering link walk statistics are wrong");

	xrtWalkOptionsInit(&Options);
	memset(&Context, 0, sizeof(Context));
	Context.Expected = &Context;
	testRequire(xrtFileWalk(sRoot, &Options,
		testWalkProc, &Context, &Stats), "no-follow link walk failed");
	testRequire((Context.Enter == 3u) && (Context.Leave == 3u) &&
		(Context.Item == 5u) && (Stats.Items == 8u) &&
		(Stats.Links == 2u), "no-follow link walk lost physical links");
	testRequire(xrtFileDelete(sBroken) && xrtFileDelete(sCycle),
		"walk link fixture cleanup failed");
	xrtFree(sBroken);
	xrtFree(sCycle);
}

#endif



/* 文件遍历层回归入口。 */
int main(void)
{
	str sRoot = testWalkPath("xrt-file-walk");
	str sSub = xrtPathJoin(sRoot, "sub");
	str sDeep = xrtPathJoin(sSub, "deep");
	str sRootFile = xrtPathJoin(sRoot, "root.txt");
	str sChildFile = xrtPathJoin(sSub, "child.txt");
	str sDeepFile = xrtPathJoin(sDeep, "deep.txt");
	#if !defined(_WIN32) && !defined(_WIN64)
		str sBroken = xrtPathJoin(sRoot, "broken");
		str sCycle = xrtPathJoin(sDeep, "cycle");
	#endif

	testRequire((sSub != NULL) && (sDeep != NULL) && (sRootFile != NULL) &&
		(sChildFile != NULL) && (sDeepFile != NULL),
		"walk fixture path build failed");
	#if !defined(_WIN32) && !defined(_WIN64)
		testRequire((sBroken != NULL) && (sCycle != NULL),
			"walk link cleanup path build failed");
		(void)xrtFileDelete(sBroken);
		(void)xrtFileDelete(sCycle);
	#endif
	(void)xrtFileDelete(sDeepFile);
	(void)xrtFileDelete(sChildFile);
	(void)xrtFileDelete(sRootFile);
	(void)xrtDirRemove(sDeep);
	(void)xrtDirRemove(sSub);
	(void)xrtDirRemove(sRoot);
	xrtClearError();
	testRequire(xrtDirCreateAll(sDeep), "walk fixture directory create failed");
	testWalkWrite(sRootFile, "root");
	testWalkWrite(sChildFile, "child");
	testWalkWrite(sDeepFile, "nested");
	testWalkAll(sRoot);
	testWalkSkip(sRoot);
	testWalkDepth(sRoot);
	testWalkStop(sRoot);
	testWalkCallbackErrors(sRoot);
	testWalkRootAndFailure(sRootFile);
	#if !defined(_WIN32) && !defined(_WIN64)
		testWalkPosixLinks(sRoot, sDeep);
	#endif
	testRequire(xrtFileDelete(sDeepFile) && xrtFileDelete(sChildFile) &&
		xrtFileDelete(sRootFile) && xrtDirRemove(sDeep) &&
		xrtDirRemove(sSub) && xrtDirRemove(sRoot),
		"walk fixture cleanup failed");
	xrtFree(sDeepFile);
	xrtFree(sChildFile);
	xrtFree(sRootFile);
	xrtFree(sDeep);
	xrtFree(sSub);
	xrtFree(sRoot);
	#if !defined(_WIN32) && !defined(_WIN64)
		xrtFree(sCycle);
		xrtFree(sBroken);
	#endif
	return 0;
}

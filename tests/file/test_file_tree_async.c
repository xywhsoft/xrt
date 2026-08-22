#include "../test.h"



/* 在系统临时目录下构造当前测试独占使用的路径。 */
static str testTreeAsyncPath(cstr sName)
{
	str sDirectory = xrtPathTemp();
	str sPath;

	testRequire(sDirectory != NULL, "temporary directory query failed");
	sPath = xrtPathJoin(sDirectory, sName);
	xrtFree(sDirectory);
	testRequire(sPath != NULL, "async tree path allocation failed");
	return sPath;
}



/* 等待目录树 Future 成功并返回借用值。 */
static ptr testTreeAsyncValue(xfuture* pFuture, cstr sMessage)
{
	testRequire(pFuture != NULL, sMessage);
	testRequire(
		xrtFutureWaitFor(
			pFuture,
			UINT64_C(3000000)
		) == XWAIT_OK,
		sMessage
	);
	testRequire(
		xrtFutureState(pFuture) == XFUTURE_RESOLVED,
		sMessage
	);
	return xrtFutureValue(pFuture);
}



/* 构造包含目录和两个普通文件的小型目录树。 */
static void testTreeAsyncFixture(cstr sRoot)
{
	str sSub = xrtPathJoin(sRoot, "sub");
	str sFirst = xrtPathJoin(sRoot, "first.txt");
	str sSecond = xrtPathJoin(sSub, "second.txt");

	testRequire(
		(sSub != NULL) && (sFirst != NULL) && (sSecond != NULL),
		"async tree fixture path creation failed"
	);
	testRequire(xrtDirCreateAll(sSub), "async tree fixture directory create failed");
	testRequire(
		xrtFileWriteAll(sFirst, XRT_BYTES_LITERAL("abc")) &&
		xrtFileWriteAll(sSecond, XRT_BYTES_LITERAL("de")),
		"async tree fixture file write failed"
	);
	xrtFree(sSecond);
	xrtFree(sFirst);
	xrtFree(sSub);
}



/* 目录树异步层覆盖复制、统计、大小、清理、移动和递归删除。 */
int main(void)
{
	xtaskpoolconfig Config = { 2, 16, 0 };
	xtaskpool* pPool = xrtTaskPoolCreate(&Config);
	str sSource = testTreeAsyncPath("xrt-tree-async-source");
	str sTarget = testTreeAsyncPath("xrt-tree-async-target");
	str sMoved = testTreeAsyncPath("xrt-tree-async-moved");
	xfuture* pFuture;
	xwalkstats* pStats;
	xfilesize* pSize;
	const xerror* pError;

	testRequire(pPool != NULL, "async tree pool create failed");
	(void)xrtDirRemoveAll(sSource);
	(void)xrtDirRemoveAll(sTarget);
	(void)xrtDirRemoveAll(sMoved);
	xrtClearError();
	testTreeAsyncFixture(sSource);

	pFuture = xrtFileTreeCopyAsync(pPool, sSource, sTarget, NULL);
	pStats = (xwalkstats*)testTreeAsyncValue(pFuture, "async tree copy failed");
	testRequire(
		(pStats != NULL) &&
		(pStats->Items == 4) &&
		(pStats->Directories == 2) &&
		(pStats->Files == 2) &&
		(pStats->Bytes == 5),
		"async tree copy statistics mismatch"
	);
	xrtFutureDestroy(pFuture);

	pFuture = xrtDirStatsAsync(pPool, sTarget, true);
	pStats = (xwalkstats*)testTreeAsyncValue(pFuture, "async tree stats failed");
	testRequire(
		(pStats != NULL) && (pStats->Items == 4) && (pStats->Bytes == 5),
		"async tree stats mismatch"
	);
	xrtFutureDestroy(pFuture);
	pFuture = xrtDirSizeAsync(pPool, sTarget, true);
	pSize = (xfilesize*)testTreeAsyncValue(pFuture, "async tree size failed");
	testRequire(
		(pSize != NULL) && (pSize->Size == 5),
		"async tree size mismatch"
	);
	xrtFutureDestroy(pFuture);

	pFuture = xrtDirCleanAsync(pPool, sTarget);
	pStats = (xwalkstats*)testTreeAsyncValue(pFuture, "async tree clean failed");
	testRequire(
		(pStats != NULL) && xrtDirExists(sTarget),
		"async tree clean removed its root"
	);
	xrtFutureDestroy(pFuture);
	pFuture = xrtDirEnsureEmptyAsync(pPool, sTarget);
	testRequire(
		testTreeAsyncValue(pFuture, "async ensure-empty failed") == NULL,
		"async ensure-empty returned an unexpected value"
	);
	xrtFutureDestroy(pFuture);
	testRequire(xrtDirRemove(sTarget), "async empty target cleanup failed");

	pFuture = xrtDirMoveAsync(pPool, sSource, sMoved, false);
	(void)testTreeAsyncValue(pFuture, "async tree move failed");
	xrtFutureDestroy(pFuture);
	testRequire(
		!xrtPathExists(sSource) && xrtDirExists(sMoved),
		"async tree move path state mismatch"
	);
	pFuture = xrtDirRemoveAllAsync(pPool, sMoved);
	pStats = (xwalkstats*)testTreeAsyncValue(pFuture, "async tree remove failed");
	testRequire(
		(pStats != NULL) && !xrtPathExists(sMoved),
		"async tree remove left its root behind"
	);
	xrtFutureDestroy(pFuture);
	pFuture = xrtDirStatsAsync(pPool, sMoved, true);
	testRequire(pFuture != NULL, "missing async tree stats submit failed");
	testRequire(
		(xrtFutureWaitFor(pFuture, UINT64_C(3000000)) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_FAILED),
		"missing async tree stats did not fail"
	);
	pError = xrtFutureError(pFuture);
	testRequire(
		(pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.file.async") == 0) &&
		(xrtErrorCode(pError) == XFILE_ASYNC_ERROR_TREE) &&
		(xrtErrorCause(pError) != NULL) &&
		(xrtErrorKind(xrtErrorCause(pError)) == XERR_NOT_FOUND),
		"async tree error chain mismatch"
	);
	xrtFutureDestroy(pFuture);

	testRequire(
		xrtDirCopyAsync(NULL, sSource, sTarget, false) == NULL,
		"null async tree pool was accepted"
	);
	testRequire(
		xrtFileTreeRemoveAsync(pPool, NULL, false) == NULL,
		"null async tree path was accepted"
	);
	xrtClearError();
	testRequire(xrtTaskPoolDestroy(pPool), "async tree pool destroy failed");
	xrtFree(sMoved);
	xrtFree(sTarget);
	xrtFree(sSource);
	printf("[PASS] async file tree\n");
	return 0;
}

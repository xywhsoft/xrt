#include "../test.h"



/* 在系统临时目录下构造当前测试独占使用的路径。 */
static str testDirAsyncPath(cstr sName)
{
	str sDirectory = xrtPathTemp();
	str sPath;

	testRequire(sDirectory != NULL, "temporary directory query failed");
	sPath = xrtPathJoin(sDirectory, sName);
	xrtFree(sDirectory);
	testRequire(sPath != NULL, "async directory path allocation failed");
	return sPath;
}



/* 等待目录 Future 成功并返回借用值。 */
static ptr testDirAsyncValue(xfuture* pFuture, cstr sMessage)
{
	testRequire(pFuture != NULL, sMessage);
	testRequire(
		xrtFutureWaitFor(
			pFuture,
			UINT64_C(2000000)
		) == XWAIT_OK,
		sMessage
	);
	testRequire(
		xrtFutureState(pFuture) == XFUTURE_RESOLVED,
		sMessage
	);
	return xrtFutureValue(pFuture);
}



/* 基础目录异步层覆盖创建、递归创建、查询和空目录删除。 */
int main(void)
{
	xtaskpoolconfig Config = { 2, 16, 0 };
	xtaskpool* pPool = xrtTaskPoolCreate(&Config);
	str sRoot = testDirAsyncPath("xrt-dir-async");
	str sNested = xrtPathJoin(sRoot, "one/two");
	str sParent = xrtPathJoin(sRoot, "one");
	str sMode = xrtPathJoin(sRoot, "mode");
	xfuture* pFuture;
	xdirquery* pQuery;
	const xerror* pError;

	testRequire(
		(pPool != NULL) && (sNested != NULL) &&
		(sParent != NULL) && (sMode != NULL),
		"async directory fixture creation failed"
	);
	(void)xrtDirRemove(sMode);
	(void)xrtDirRemove(sNested);
	(void)xrtDirRemove(sRoot);
	xrtClearError();

	pFuture = xrtDirCreateAllAsync(pPool, sNested);
	testRequire(
		testDirAsyncValue(pFuture, "async recursive directory create failed") == NULL,
		"async directory create returned an unexpected value"
	);
	xrtFutureDestroy(pFuture);
	testRequire(
		xrtDirExists(sRoot) && xrtDirExists(sNested),
		"async recursive directory create omitted a component"
	);

	pFuture = xrtDirEmptyAsync(pPool, sNested);
	pQuery = (xdirquery*)testDirAsyncValue(
		pFuture,
		"async directory empty query failed"
	);
	testRequire(
		(pQuery != NULL) && pQuery->Empty,
		"new async directory is not empty"
	);
	xrtFutureDestroy(pFuture);

	pFuture = xrtDirCreateModeAsync(pPool, sMode, 0750u);
	(void)testDirAsyncValue(pFuture, "async mode directory create failed");
	xrtFutureDestroy(pFuture);
	pFuture = xrtDirRemoveAsync(pPool, sMode);
	(void)testDirAsyncValue(pFuture, "async mode directory remove failed");
	xrtFutureDestroy(pFuture);

	pFuture = xrtDirRemoveAsync(pPool, sNested);
	(void)testDirAsyncValue(pFuture, "async nested directory remove failed");
	xrtFutureDestroy(pFuture);
	testRequire(xrtDirRemove(sParent), "async directory parent cleanup failed");
	testRequire(
		xrtDirRemoveAsync(NULL, sRoot) == NULL,
		"null async directory pool was accepted"
	);
	testRequire(
		xrtDirCreateAllModeAsync(pPool, NULL, 0755u) == NULL,
		"null async directory path was accepted"
	);
	xrtClearError();
	testRequire(xrtDirRemove(sRoot), "async directory root cleanup failed");
	pFuture = xrtDirEmptyAsync(pPool, sRoot);
	testRequire(pFuture != NULL, "missing async directory query submit failed");
	testRequire(
		(xrtFutureWaitFor(pFuture, UINT64_C(2000000)) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_FAILED),
		"missing async directory query did not fail"
	);
	pError = xrtFutureError(pFuture);
	testRequire(
		(pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.file.async") == 0) &&
		(xrtErrorCode(pError) == XFILE_ASYNC_ERROR_QUERY) &&
		(xrtErrorCause(pError) != NULL) &&
		(xrtErrorKind(xrtErrorCause(pError)) == XERR_NOT_FOUND),
		"async directory error chain mismatch"
	);
	xrtFutureDestroy(pFuture);
	testRequire(xrtTaskPoolDestroy(pPool), "async directory pool destroy failed");
	xrtFree(sMode);
	xrtFree(sParent);
	xrtFree(sNested);
	xrtFree(sRoot);
	printf("[PASS] async directory\n");
	return 0;
}

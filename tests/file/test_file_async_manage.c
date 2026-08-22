#include "../test.h"



/* 在系统临时目录下构造当前测试独占使用的路径。 */
static str testAsyncManagePath(cstr sName)
{
	str sDirectory = xrtPathTemp();
	str sPath;

	testRequire(sDirectory != NULL, "temporary directory query failed");
	sPath = xrtPathJoin(sDirectory, sName);
	xrtFree(sDirectory);
	testRequire(sPath != NULL, "async file management path allocation failed");
	return sPath;
}



/* 等待一个无结果值的文件管理 Future 成功。 */
static void testAsyncManageWait(xfuture* pFuture, cstr sMessage)
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
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED) &&
		(xrtFutureValue(pFuture) == NULL),
		sMessage
	);
	xrtFutureDestroy(pFuture);
}



/* 文件复制、替换、移动和删除必须保持同步底座的完整语义。 */
static void testAsyncManageOperations(
	xtaskpool* pPool,
	str sSource,
	str sTarget,
	str sMoved
)
{
	static const unsigned char arrData[] = { 1, 0, 2, 3 };
	xfuture* pFuture;
	bytes pData;
	size_t iSize;

	(void)xrtFileDelete(sSource);
	(void)xrtFileDelete(sTarget);
	(void)xrtFileDelete(sMoved);
	xrtClearError();
	testRequire(
		xrtFileWriteAll(
			sSource,
			(xbytesview) { arrData, sizeof(arrData) }
		),
		"async management fixture write failed"
	);
	pFuture = xrtFileCopyAsync(
		pPool,
		sSource,
		sTarget,
		false
	);
	testAsyncManageWait(pFuture, "async file copy failed");
	pData = xrtFileReadAll(sTarget, &iSize);
	testRequire(
		(pData != NULL) &&
		(iSize == sizeof(arrData)) &&
		(memcmp(pData, arrData, sizeof(arrData)) == 0),
		"async file copy data mismatch"
	);
	xrtFree(pData);

	pFuture = xrtFileMoveAsync(
		pPool,
		sTarget,
		sMoved,
		false
	);
	testAsyncManageWait(pFuture, "async file move failed");
	testRequire(
		!xrtPathExists(sTarget) && xrtFileExists(sMoved),
		"async file move path state mismatch"
	);

	pFuture = xrtFileDeleteAsync(pPool, sMoved);
	testAsyncManageWait(pFuture, "async file delete failed");
	testRequire(
		!xrtPathExists(sMoved),
		"async file delete left the target behind"
	);
}



/* 异步失败必须保留操作码和底层文件错误原因。 */
static void testAsyncManageErrors(
	xtaskpool* pPool,
	cstr sSource,
	cstr sTarget
)
{
	xfuture* pFuture = xrtFileCopyAsync(
		pPool,
		sSource,
		sTarget,
		false
	);
	const xerror* pError;
	const xerror* pCause;

	testRequire(pFuture != NULL, "failing async file copy submission failed");
	testRequire(
		xrtFutureWaitFor(
			pFuture,
			UINT64_C(2000000)
		) == XWAIT_OK,
		"failing async file copy wait failed"
	);
	testRequire(
		xrtFutureState(pFuture) == XFUTURE_FAILED,
		"missing async file copy did not reject"
	);
	pError = xrtFutureError(pFuture);
	pCause = xrtErrorCause(pError);
	testRequire(
		(pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.file.async") == 0) &&
		(xrtErrorCode(pError) == XFILE_ASYNC_ERROR_COPY) &&
		(pCause != NULL) &&
		(xrtErrorKind(pCause) == XERR_NOT_FOUND),
		"async file management error chain mismatch"
	);
	xrtFutureDestroy(pFuture);

	testRequire(
		xrtFileDeleteAsync(NULL, sSource) == NULL,
		"null async file management pool was accepted"
	);
	testRequire(
		xrtFileMoveAsync(pPool, sSource, NULL, false) == NULL,
		"null async file move target was accepted"
	);
	xrtClearError();
}



/* 文件管理异步 Helper 回归入口。 */
int main(void)
{
	xtaskpoolconfig Config = { 2, 16, 0 };
	xtaskpool* pPool = xrtTaskPoolCreate(&Config);
	str sSource = testAsyncManagePath("xrt-file-async-manage-source.tmp");
	str sTarget = testAsyncManagePath("xrt-file-async-manage-target.tmp");
	str sMoved = testAsyncManagePath("xrt-file-async-manage-moved.tmp");

	testRequire(pPool != NULL, "async file management pool create failed");
	testAsyncManageOperations(pPool, sSource, sTarget, sMoved);
	testRequire(
		xrtFileDelete(sSource),
		"async file management source cleanup failed"
	);
	testAsyncManageErrors(pPool, sSource, sTarget);
	testRequire(
		xrtTaskPoolDestroy(pPool),
		"async file management pool destroy failed"
	);
	(void)xrtFileDelete(sTarget);
	(void)xrtFileDelete(sMoved);
	xrtFree(sSource);
	xrtFree(sTarget);
	xrtFree(sMoved);
	printf("[PASS] async file management\n");
	return 0;
}

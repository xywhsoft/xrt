#include "../test.h"



/* 在系统临时目录下构造当前测试独占使用的路径。 */
static str testAsyncWholePath(cstr sName)
{
	str sDirectory = xrtPathTemp();
	str sPath;

	testRequire(sDirectory != NULL, "temporary directory query failed");
	sPath = xrtPathJoin(sDirectory, sName);
	xrtFree(sDirectory);
	testRequire(sPath != NULL, "async whole-file path allocation failed");
	return sPath;
}



/* 等待 Future 成功，并返回由 Future 拥有的借用值。 */
static ptr testAsyncWholeValue(xfuture* pFuture, cstr sMessage)
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



/* 整文件异步操作必须复制输入，并保持二进制数据与结果所有权。 */
static void testAsyncWholeOperations(
	xtaskpool* pPool,
	cstr sPath
)
{
	unsigned char arrSource[] = { 1, 0, 2, 3 };
	static const unsigned char arrTail[] = { 0, 4 };
	static const unsigned char arrAtomic[] = { 9, 8, 0, 7 };
	xfuture* pFuture;
	xfilechange* pChange;
	xfiledata* pData;

	(void)xrtFileDelete(sPath);
	xrtClearError();
	pFuture = xrtFileWriteAllAsync(
		pPool,
		sPath,
		(xbytesview) { arrSource, sizeof(arrSource) }
	);
	memset(arrSource, 0xFF, sizeof(arrSource));
	pChange = (xfilechange*)testAsyncWholeValue(
		pFuture,
		"async whole-file write failed"
	);
	testRequire(
		(pChange != NULL) &&
		(pChange->Offset == 0) &&
		(pChange->Size == 4),
		"async whole-file write result mismatch"
	);
	xrtFutureDestroy(pFuture);

	pFuture = xrtFileAppendAsync(
		pPool,
		sPath,
		(xbytesview) { arrTail, sizeof(arrTail) }
	);
	pChange = (xfilechange*)testAsyncWholeValue(
		pFuture,
		"async whole-file append failed"
	);
	testRequire(
		(pChange != NULL) &&
		(pChange->Size == sizeof(arrTail)),
		"async whole-file append result mismatch"
	);
	xrtFutureDestroy(pFuture);

	pFuture = xrtFileReadAllAsync(pPool, sPath);
	pData = (xfiledata*)testAsyncWholeValue(
		pFuture,
		"async whole-file read failed"
	);
	testRequire(
		(pData != NULL) &&
		(pData->Size == 6) &&
		pData->End &&
		(memcmp(pData->Data, "\1\0\2\3\0\4", 6) == 0) &&
		(pData->Data[pData->Size] == 0),
		"async whole-file data mismatch"
	);
	xrtFutureDestroy(pFuture);

	pFuture = xrtFileWriteAtomicAsync(
		pPool,
		sPath,
		(xbytesview) { arrAtomic, sizeof(arrAtomic) }
	);
	pChange = (xfilechange*)testAsyncWholeValue(
		pFuture,
		"async atomic file write failed"
	);
	testRequire(
		(pChange != NULL) &&
		(pChange->Size == sizeof(arrAtomic)),
		"async atomic file write result mismatch"
	);
	xrtFutureDestroy(pFuture);

	pFuture = xrtFileReadAllLimitAsync(
		pPool,
		sPath,
		sizeof(arrAtomic)
	);
	pData = (xfiledata*)testAsyncWholeValue(
		pFuture,
		"limited async whole-file read failed"
	);
	testRequire(
		(pData != NULL) &&
		(pData->Size == sizeof(arrAtomic)) &&
		(memcmp(pData->Data, arrAtomic, sizeof(arrAtomic)) == 0),
		"limited async whole-file data mismatch"
	);
	xrtFutureDestroy(pFuture);
}



/* 读取上限失败必须由 Future 保留稳定异步错误与同步文件错误原因。 */
static void testAsyncWholeErrors(
	xtaskpool* pPool,
	cstr sPath
)
{
	xfuture* pFuture;
	const xerror* pError;
	const xerror* pCause;

	pFuture = xrtFileReadAllLimitAsync(pPool, sPath, 1);
	testRequire(pFuture != NULL, "limited async read submission failed");
	testRequire(
		xrtFutureWaitFor(
			pFuture,
			UINT64_C(2000000)
		) == XWAIT_OK,
		"limited async read wait failed"
	);
	testRequire(
		xrtFutureState(pFuture) == XFUTURE_FAILED,
		"oversized async read did not reject"
	);
	pError = xrtFutureError(pFuture);
	pCause = xrtErrorCause(pError);
	testRequire(
		(pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.file.async") == 0) &&
		(xrtErrorCode(pError) == XFILE_ASYNC_ERROR_READ) &&
		(pCause != NULL) &&
		(xrtErrorKind(pCause) == XERR_RANGE),
		"async whole-file error chain mismatch"
	);
	xrtFutureDestroy(pFuture);

	testRequire(
		xrtFileReadAllAsync(NULL, sPath) == NULL,
		"null async whole-file pool was accepted"
	);
	testRequire(
		xrtFileWriteAllAsync(
			pPool,
			sPath,
			(xbytesview) { NULL, 1 }
		) == NULL,
		"invalid async whole-file data was accepted"
	);
	xrtClearError();
}



/* 整文件异步 Helper 回归入口。 */
int main(void)
{
	xtaskpoolconfig Config = { 2, 16, 0 };
	xtaskpool* pPool = xrtTaskPoolCreate(&Config);
	str sPath = testAsyncWholePath("xrt-file-async-whole.tmp");

	testRequire(pPool != NULL, "async whole-file pool create failed");
	testAsyncWholeOperations(pPool, sPath);
	testAsyncWholeErrors(pPool, sPath);
	testRequire(
		xrtTaskPoolDestroy(pPool),
		"async whole-file pool destroy failed"
	);
	testRequire(
		xrtFileDelete(sPath),
		"async whole-file cleanup failed"
	);
	xrtFree(sPath);
	printf("[PASS] async whole file\n");
	return 0;
}

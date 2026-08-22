#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立接通异步文件、任务池、Future 和关闭生命周期。 */
int main(void)
{
	xtaskpoolconfig Config = { 1, 8, 0 };
	xfileoptions Options;
	xtaskpool* pPool = NULL;
	xasyncfile* pFile = NULL;
	xfuture* pWrite = NULL;
	xfuture* pRead = NULL;
	xfuture* pClose = NULL;
	xfiledata* pData;
	str sDirectory = NULL;
	str sPath = NULL;
	int iResult = 1;

	sDirectory = xrtPathTemp();
	if ( sDirectory == NULL ) {
		goto Exit;
	}
	sPath = xrtPathJoin(
		sDirectory,
		"xrt-single-file-async.tmp"
	);
	xrtFree(sDirectory);
	sDirectory = NULL;
	if ( sPath == NULL ) {
		goto Exit;
	}
	(void)xrtFileDelete(sPath);
	xrtClearError();
	pPool = xrtTaskPoolCreate(&Config);
	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_READ | XFILE_WRITE |
		XFILE_CREATE | XFILE_TRUNCATE;
	pFile = xrtAsyncFileOpen(pPool, sPath, &Options);
	if ( pFile == NULL ) {
		goto Exit;
	}
	pWrite = xrtAsyncFileWriteAt(
		pFile,
		0,
		XRT_BYTES_LITERAL("abc")
	);
	if ( (pWrite == NULL) ||
		(xrtFutureWaitFor(
			pWrite,
			UINT64_C(2000000)
		) != XWAIT_OK) ||
		(xrtFutureState(pWrite) != XFUTURE_RESOLVED) ) {
		goto Exit;
	}
	pRead = xrtAsyncFileReadAt(pFile, 0, 4);
	if ( (pRead == NULL) ||
		(xrtFutureWaitFor(
			pRead,
			UINT64_C(2000000)
		) != XWAIT_OK) ||
		(xrtFutureState(pRead) != XFUTURE_RESOLVED) ) {
		goto Exit;
	}
	pData = (xfiledata*)xrtFutureValue(pRead);
	if ( (pData == NULL) ||
		(pData->Size != 3) ||
		!pData->End ||
		(memcmp(pData->Data, "abc", 3) != 0) ) {
		goto Exit;
	}
	pClose = xrtAsyncFileClose(pFile);
	pFile = NULL;
	if ( (pClose == NULL) ||
		(xrtFutureWaitFor(
			pClose,
			UINT64_C(2000000)
		) != XWAIT_OK) ||
		(xrtFutureState(pClose) != XFUTURE_RESOLVED) ) {
		goto Exit;
	}
	iResult = 0;

Exit:
	if ( pFile != NULL ) {
		xfuture* pPending = xrtAsyncFileClose(pFile);

		if ( pPending != NULL ) {
			(void)xrtFutureWaitFor(
				pPending,
				UINT64_C(2000000)
			);
			xrtFutureDestroy(pPending);
		}
	}
	xrtFutureDestroy(pWrite);
	xrtFutureDestroy(pRead);
	xrtFutureDestroy(pClose);
	if ( pPool != NULL ) {
		(void)xrtTaskPoolDestroy(pPool);
	}
	if ( sPath != NULL ) {
		(void)xrtFileDelete(sPath);
	}
	xrtFree(sPath);
	xrtFree(sDirectory);
	return iResult;
}

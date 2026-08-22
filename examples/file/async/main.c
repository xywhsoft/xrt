#include <stdio.h>
#include <string.h>
#include <xrt.h>



/* 等待 Future 成功并返回借用值。 */
static ptr waitValue(xfuture* pFuture)
{
	if ( (pFuture == NULL) ||
		(xrtFutureWait(pFuture) != XWAIT_OK) ||
		(xrtFutureState(pFuture) != XFUTURE_RESOLVED) ) {
		return NULL;
	}
	return xrtFutureValue(pFuture);
}



/* 使用有界任务池完成异步绝对偏移读写。 */
int main(void)
{
	xtaskpoolconfig Config = { 2, 32, 0 };
	xfileoptions Options;
	xtaskpool* pPool = xrtTaskPoolCreate(&Config);
	xasyncfile* pFile = NULL;
	xfuture* pWrite = NULL;
	xfuture* pRead = NULL;
	xfuture* pClose = NULL;
	xfiledata* pData;
	int iResult = 1;

	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_READ | XFILE_WRITE |
		XFILE_CREATE | XFILE_TRUNCATE;
	if ( pPool == NULL ) {
		goto Exit;
	}
	pFile = xrtAsyncFileOpen(
		pPool,
		"xrt-async-example.txt",
		&Options
	);
	if ( pFile == NULL ) {
		goto Exit;
	}
	pWrite = xrtAsyncFileWriteAt(
		pFile,
		0,
		XRT_BYTES_LITERAL("hello async file")
	);
	if ( waitValue(pWrite) == NULL ) {
		goto Exit;
	}
	pRead = xrtAsyncFileReadAt(pFile, 0, 64);
	pData = (xfiledata*)waitValue(pRead);
	if ( pData == NULL ) {
		goto Exit;
	}
	printf(
		"%.*s\n",
		(int)pData->Size,
		(const char*)pData->Data
	);
	pClose = xrtAsyncFileClose(pFile);
	pFile = NULL;
	if ( (pClose == NULL) ||
		(xrtFutureWait(pClose) != XWAIT_OK) ||
		(xrtFutureState(pClose) != XFUTURE_RESOLVED) ) {
		goto Exit;
	}
	iResult = 0;

Exit:
	if ( pFile != NULL ) {
		xfuture* pPending = xrtAsyncFileClose(pFile);

		if ( pPending != NULL ) {
			(void)xrtFutureWait(pPending);
			xrtFutureDestroy(pPending);
		}
	}
	xrtFutureDestroy(pWrite);
	xrtFutureDestroy(pRead);
	xrtFutureDestroy(pClose);
	if ( pPool != NULL ) {
		(void)xrtTaskPoolDestroy(pPool);
	}
	return iResult;
}

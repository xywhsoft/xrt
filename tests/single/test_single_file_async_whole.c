#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立接通整文件异步读写与结果所有权。 */
int main(void)
{
	xtaskpoolconfig Config = { 1, 4, 0 };
	xtaskpool* pPool = xrtTaskPoolCreate(&Config);
	xfuture* pWrite = NULL;
	xfuture* pRead = NULL;
	xfiledata* pData;
	int iResult = 1;
	static const char sPath[] = "xrt-single-file-async-whole.tmp";

	if ( pPool == NULL ) {
		goto Exit;
	}
	pWrite = xrtFileWriteAtomicAsync(
		pPool,
		sPath,
		XRT_BYTES_LITERAL("async")
	);
	if ( (pWrite == NULL) ||
		(xrtFutureWait(pWrite) != XWAIT_OK) ||
		(xrtFutureState(pWrite) != XFUTURE_RESOLVED) ) {
		goto Exit;
	}
	pRead = xrtFileReadAllAsync(pPool, sPath);
	if ( (pRead == NULL) ||
		(xrtFutureWait(pRead) != XWAIT_OK) ||
		(xrtFutureState(pRead) != XFUTURE_RESOLVED) ) {
		goto Exit;
	}
	pData = (xfiledata*)xrtFutureValue(pRead);
	if ( (pData == NULL) ||
		(pData->Size != 5) ||
		(memcmp(pData->Data, "async", 5) != 0) ) {
		goto Exit;
	}
	iResult = 0;

Exit:
	xrtFutureDestroy(pWrite);
	xrtFutureDestroy(pRead);
	if ( pPool != NULL ) {
		(void)xrtTaskPoolDestroy(pPool);
	}
	(void)xrtFileDelete(sPath);
	return iResult;
}

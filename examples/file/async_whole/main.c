#include <stdio.h>
#include <xrt.h>



/* 使用显式任务池完成原子写入和整文件读取。 */
int main(void)
{
	xtaskpoolconfig Config = { 2, 16, 0 };
	xtaskpool* pPool = xrtTaskPoolCreate(&Config);
	xfuture* pWrite = NULL;
	xfuture* pRead = NULL;
	xfiledata* pData;
	int iResult = 1;
	static const char sPath[] = "xrt-async-whole-example.txt";

	if ( pPool == NULL ) {
		goto Exit;
	}
	pWrite = xrtFileWriteAtomicAsync(
		pPool,
		sPath,
		XRT_BYTES_LITERAL("hello async whole file")
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
	if ( pData == NULL ) {
		goto Exit;
	}
	printf("%.*s\n", (int)pData->Size, (const char*)pData->Data);
	iResult = 0;

Exit:
	xrtFutureDestroy(pWrite);
	xrtFutureDestroy(pRead);
	if ( pPool != NULL ) {
		(void)xrtTaskPoolDestroy(pPool);
	}
	return iResult;
}

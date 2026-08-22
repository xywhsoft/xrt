#include <xrt.h>



/* 使用文件管理异步 Helper 复制并删除文件。 */
int main(void)
{
	xtaskpoolconfig Config = { 1, 8, 0 };
	xtaskpool* pPool = xrtTaskPoolCreate(&Config);
	xfuture* pCopy = NULL;
	xfuture* pDelete = NULL;
	int iResult = 1;
	static const char sSource[] = "xrt-async-manage-source.txt";
	static const char sTarget[] = "xrt-async-manage-target.txt";

	if ( (pPool == NULL) ||
		!xrtFileWriteAll(sSource, XRT_BYTES_LITERAL("copy me")) ) {
		goto Exit;
	}
	pCopy = xrtFileCopyAsync(pPool, sSource, sTarget, true);
	if ( (pCopy == NULL) ||
		(xrtFutureWait(pCopy) != XWAIT_OK) ||
		(xrtFutureState(pCopy) != XFUTURE_RESOLVED) ) {
		goto Exit;
	}
	pDelete = xrtFileDeleteAsync(pPool, sTarget);
	if ( (pDelete == NULL) ||
		(xrtFutureWait(pDelete) != XWAIT_OK) ||
		(xrtFutureState(pDelete) != XFUTURE_RESOLVED) ) {
		goto Exit;
	}
	iResult = 0;

Exit:
	xrtFutureDestroy(pCopy);
	xrtFutureDestroy(pDelete);
	if ( pPool != NULL ) {
		(void)xrtTaskPoolDestroy(pPool);
	}
	(void)xrtFileDelete(sSource);
	(void)xrtFileDelete(sTarget);
	return iResult;
}

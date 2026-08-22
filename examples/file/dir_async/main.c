#include <xrt.h>



/* 使用显式任务池创建并删除目录。 */
int main(void)
{
	xtaskpoolconfig Config = { 1, 8, 0 };
	xtaskpool* pPool = xrtTaskPoolCreate(&Config);
	xfuture* pCreate = NULL;
	xfuture* pRemove = NULL;
	int iResult = 1;
	static const char sPath[] = "xrt-dir-async-example";

	if ( pPool == NULL ) {
		goto Exit;
	}
	(void)xrtDirRemove(sPath);
	xrtClearError();
	pCreate = xrtDirCreateAsync(pPool, sPath);
	if ( (pCreate == NULL) ||
		(xrtFutureWait(pCreate) != XWAIT_OK) ||
		(xrtFutureState(pCreate) != XFUTURE_RESOLVED) ) {
		goto Exit;
	}
	pRemove = xrtDirRemoveAsync(pPool, sPath);
	if ( (pRemove == NULL) ||
		(xrtFutureWait(pRemove) != XWAIT_OK) ||
		(xrtFutureState(pRemove) != XFUTURE_RESOLVED) ) {
		goto Exit;
	}
	iResult = 0;

Exit:
	xrtFutureDestroy(pCreate);
	xrtFutureDestroy(pRemove);
	if ( pPool != NULL ) {
		(void)xrtTaskPoolDestroy(pPool);
	}
	(void)xrtDirRemove(sPath);
	return iResult;
}

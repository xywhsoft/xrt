#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立接通基础目录异步操作。 */
int main(void)
{
	xtaskpoolconfig Config = { 1, 4, 0 };
	xtaskpool* pPool = xrtTaskPoolCreate(&Config);
	xfuture* pCreate = NULL;
	xfuture* pRemove = NULL;
	int iResult = 1;
	static const char sPath[] = "xrt-single-dir-async";

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

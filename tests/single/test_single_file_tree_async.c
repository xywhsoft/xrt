#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立接通目录树异步复制和删除。 */
int main(void)
{
	xtaskpoolconfig Config = { 1, 4, 0 };
	xtaskpool* pPool = xrtTaskPoolCreate(&Config);
	xfuture* pCopy = NULL;
	xfuture* pRemove = NULL;
	int iResult = 1;
	static const char sSource[] = "xrt-single-tree-async-source";
	static const char sTarget[] = "xrt-single-tree-async-target";

	if ( pPool == NULL ) {
		goto Exit;
	}
	(void)xrtDirRemoveAll(sSource);
	(void)xrtDirRemoveAll(sTarget);
	xrtClearError();
	if ( !xrtDirCreate(sSource) ) {
		goto Exit;
	}
	pCopy = xrtDirCopyAsync(pPool, sSource, sTarget, false);
	if ( (pCopy == NULL) ||
		(xrtFutureWait(pCopy) != XWAIT_OK) ||
		(xrtFutureState(pCopy) != XFUTURE_RESOLVED) ) {
		goto Exit;
	}
	pRemove = xrtDirRemoveAllAsync(pPool, sTarget);
	if ( (pRemove == NULL) ||
		(xrtFutureWait(pRemove) != XWAIT_OK) ||
		(xrtFutureState(pRemove) != XFUTURE_RESOLVED) ) {
		goto Exit;
	}
	iResult = 0;

Exit:
	xrtFutureDestroy(pCopy);
	xrtFutureDestroy(pRemove);
	if ( pPool != NULL ) {
		(void)xrtTaskPoolDestroy(pPool);
	}
	(void)xrtDirRemoveAll(sSource);
	(void)xrtDirRemoveAll(sTarget);
	return iResult;
}

#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立接通文件复制、移动和删除异步操作。 */
int main(void)
{
	xtaskpoolconfig Config = { 1, 4, 0 };
	xtaskpool* pPool = xrtTaskPoolCreate(&Config);
	xfuture* pCopy = NULL;
	xfuture* pMove = NULL;
	xfuture* pDelete = NULL;
	int iResult = 1;
	static const char sSource[] = "xrt-single-file-async-source.tmp";
	static const char sTarget[] = "xrt-single-file-async-target.tmp";
	static const char sMoved[] = "xrt-single-file-async-moved.tmp";

	if ( pPool == NULL ) {
		goto Exit;
	}
	(void)xrtFileDelete(sSource);
	(void)xrtFileDelete(sTarget);
	(void)xrtFileDelete(sMoved);
	xrtClearError();
	if ( !xrtFileWriteAll(sSource, XRT_BYTES_LITERAL("data")) ) {
		goto Exit;
	}
	pCopy = xrtFileCopyAsync(pPool, sSource, sTarget, false);
	if ( (pCopy == NULL) ||
		(xrtFutureWait(pCopy) != XWAIT_OK) ||
		(xrtFutureState(pCopy) != XFUTURE_RESOLVED) ) {
		goto Exit;
	}
	pMove = xrtFileMoveAsync(pPool, sTarget, sMoved, false);
	if ( (pMove == NULL) ||
		(xrtFutureWait(pMove) != XWAIT_OK) ||
		(xrtFutureState(pMove) != XFUTURE_RESOLVED) ) {
		goto Exit;
	}
	pDelete = xrtFileDeleteAsync(pPool, sMoved);
	if ( (pDelete == NULL) ||
		(xrtFutureWait(pDelete) != XWAIT_OK) ||
		(xrtFutureState(pDelete) != XFUTURE_RESOLVED) ) {
		goto Exit;
	}
	iResult = 0;

Exit:
	xrtFutureDestroy(pCopy);
	xrtFutureDestroy(pMove);
	xrtFutureDestroy(pDelete);
	if ( pPool != NULL ) {
		(void)xrtTaskPoolDestroy(pPool);
	}
	(void)xrtFileDelete(sSource);
	(void)xrtFileDelete(sTarget);
	(void)xrtFileDelete(sMoved);
	return iResult;
}

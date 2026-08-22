#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件包含可独立裁剪的 Future 组合器。 */
int main(void)
{
	xfuture* pSource;
	xfuture* pAllFuture;
	xfuture* arrFuture[1];
	xpromise* pPromise = xrtPromiseCreate(&pSource, NULL);
	const xfutureall* pAll;
	int iResult = 1;

	if ( pPromise == NULL ) {
		return 1;
	}
	arrFuture[0] = pSource;
	pAllFuture = xrtFutureAll(arrFuture, 1);
	if ( (pAllFuture != NULL) && xrtPromiseResolve(pPromise, NULL) &&
		(xrtFutureWait(pAllFuture) == XWAIT_OK) ) {
		pAll = (const xfutureall*)xrtFutureValue(pAllFuture);
		if ( (pAll != NULL) && (pAll->Count == 1) &&
			(pAll->Futures[0] == pSource) ) {
			iResult = 0;
		}
	}
	xrtFutureDestroy(pAllFuture);
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pSource);
	return iResult;
}

#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件协程过程等待已经完成的 Future。 */
static ptr testSingleFutureAwait(ptr pData)
{
	return xrtFutureAwait((xfuture*)pData) == XWAIT_OK ? pData : NULL;
}



/* 验证单头文件接通 Future 与协程调度器的可裁剪桥。 */
int main(void)
{
	xfuture* pFuture;
	xpromise* pPromise;
	xcosched* pSched;
	xcoro* pCo;
	int iResult = 1;

	pPromise = xrtPromiseCreate(&pFuture, NULL);
	pSched = xrtCoSchedCreate();
	if ( (pPromise != NULL) && (pSched != NULL) &&
		xrtPromiseResolve(pPromise, pFuture) ) {
		pCo = xrtCoSpawn(pSched, testSingleFutureAwait, pFuture, NULL);
		if ( (pCo != NULL) && xrtCoSchedRun(pSched) &&
			(xrtCoResult(pCo) == pFuture) ) {
			iResult = 0;
		}
		xrtCoDestroy(pCo);
	}
	xrtCoSchedDestroy(pSched);
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);
	return iResult;
}

#include <stdio.h>
#include <xrt.h>



/* 协程等待示例在同一调度器中消费一个已完成 Future。 */
static ptr awaitFuture(ptr pData)
{
	xfuture* pFuture = (xfuture*)pData;

	if ( xrtFutureAwait(pFuture) != XWAIT_OK ) {
		return NULL;
	}
	return xrtFutureValue(pFuture);
}



/* 演示 Future 与协程调度器之间不阻塞原生线程的等待桥。 */
int main(void)
{
	xfuture* pFuture;
	xpromise* pPromise;
	xcosched* pSched;
	xcoro* pCo;
	int iValue = 64;

	pPromise = xrtPromiseCreate(&pFuture, NULL);
	pSched = xrtCoSchedCreate();
	if ( (pPromise == NULL) || (pSched == NULL) ||
		 !xrtPromiseResolve(pPromise, &iValue) ) {
		return 1;
	}
	pCo = xrtCoSpawn(pSched, awaitFuture, pFuture, NULL);
	if ( (pCo == NULL) || !xrtCoSchedRun(pSched) ) {
		return 2;
	}
	printf("await value: %d\n", *(int*)xrtCoResult(pCo));
	xrtCoDestroy(pCo);
	xrtCoSchedDestroy(pSched);
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);
	return 0;
}

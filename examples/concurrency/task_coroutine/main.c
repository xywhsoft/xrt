#include <stdio.h>

#include <xrt.h>



/* 协程任务可以直接使用调度睡眠，而不会阻塞所属原生线程。 */
static xtaskoutcome delayedValue(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	if ( xrtCoSleep(10000) == XWAIT_CANCELLED || xrtCancelRequested(pCancel) ) {
		return XTASK_CANCELLED;
	}
	pResult->Value = pData;
	return XTASK_SUCCESS;
}



/* 提交协程任务，运行调度器并读取统一 Future 结果。 */
int main(void)
{
	xcosched* pSched = xrtCoSchedCreate();
	int iValue = 27;
	xfuture* pFuture;
	int iResult = 1;

	if ( pSched == NULL ) {
		return 1;
	}
	pFuture = xrtTaskCo(pSched, delayedValue, &iValue, NULL, 0);
	if (
		(pFuture != NULL) && xrtCoSchedRun(pSched) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED)
	) {
		printf("value = %d\n", *(int*)xrtFutureValue(pFuture));
		iResult = 0;
	}
	xrtFutureDestroy(pFuture);
	(void)xrtCoSchedDestroy(pSched);
	(void)xrtCoThreadDetach();
	return iResult;
}

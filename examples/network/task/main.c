#include <stdio.h>
#include <xrt.h>



/* 在 Engine Worker 上生成一个轻量结果。 */
static xtaskoutcome buildValue(
	xnetworker* pWorker,
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	(void)pCancel;
	printf("worker=%u\n", xrtNetWorkerIndex(pWorker));
	pResult->Value = pData;
	return XTASK_SUCCESS;
}



/* 演示立即网络任务与统一 Future 等待。 */
int main(void)
{
	xnetengine* pEngine = xrtNetEngineCreate(NULL);
	xfuture* pFuture;
	int iValue = 42;

	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		return 1;
	}
	pFuture = xrtTaskNet(
		pEngine,
		0,
		buildValue,
		&iValue,
		NULL
	);
	if ( (pFuture == NULL) ||
		(xrtFutureWaitFor(pFuture, 3000000u) != XWAIT_OK) ||
		(xrtFutureState(pFuture) != XFUTURE_RESOLVED) ) {
		xrtFutureDestroy(pFuture);
		(void)xrtNetEngineDestroy(pEngine);
		return 1;
	}
	printf("value=%d\n", *(int*)xrtFutureValue(pFuture));
	xrtFutureDestroy(pFuture);
	return xrtNetEngineDestroy(pEngine) ? 0 : 1;
}

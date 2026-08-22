#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头网络任务返回调用方提供的借用值。 */
static xtaskoutcome testSingleTaskNet(
	xnetworker* pWorker,
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	(void)pCancel;
	if ( pWorker == NULL ) {
		return XTASK_FAILED;
	}
	pResult->Value = pData;
	return XTASK_SUCCESS;
}



/* 验证单头文件任务核心与网络 Engine 的可裁剪桥。 */
int main(void)
{
	xnetengine* pEngine = xrtNetEngineCreate(NULL);
	xfuture* pFuture = NULL;
	int iValue = 29;
	int iResult = 1;

	if ( (pEngine != NULL) && xrtNetEngineStart(pEngine) ) {
		pFuture = xrtTaskNet(
			pEngine,
			0,
			testSingleTaskNet,
			&iValue,
			NULL
		);
	}
	if ( (pFuture != NULL) &&
		(xrtFutureWaitFor(pFuture, 3000000u) == XWAIT_OK) &&
		(xrtFutureValue(pFuture) == &iValue) ) {
		iResult = 0;
	}
	xrtFutureDestroy(pFuture);
	(void)xrtNetEngineDestroy(pEngine);
	return iResult;
}

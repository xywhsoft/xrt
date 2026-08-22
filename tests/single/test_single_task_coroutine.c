#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件协程任务返回调用方提供的借用值。 */
static xtaskoutcome testSingleTaskCo(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	(void)pCancel;
	pResult->Value = pData;
	return XTASK_SUCCESS;
}



/* 验证单头文件任务核心与协程调度器的生命周期桥。 */
int main(void)
{
	xcosched* pSched = xrtCoSchedCreate();
	xfuture* pFuture;
	int iValue = 19;
	int iResult = 1;

	pFuture = pSched != NULL ?
		xrtTaskCo(pSched, testSingleTaskCo, &iValue, NULL, 0) : NULL;
	if (
		(pFuture != NULL) && xrtCoSchedRun(pSched) &&
		(xrtFutureValue(pFuture) == &iValue)
	) {
		iResult = 0;
	}
	xrtFutureDestroy(pFuture);
	(void)xrtCoSchedDestroy(pSched);
	(void)xrtCoThreadDetach();
	return iResult;
}

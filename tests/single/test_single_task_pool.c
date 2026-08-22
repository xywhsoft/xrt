#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件任务返回调用方提供的借用值。 */
static xtaskoutcome testSingleTask(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	(void)pCancel;
	pResult->Value = pData;
	return XTASK_SUCCESS;
}



/* 验证单头文件接通任务、Future 和有界线程池。 */
int main(void)
{
	xtaskpoolconfig tConfig = { 1, 4, 0 };
	xtaskpool* pPool;
	xfuture* pFuture;
	int iValue = 73;
	int iResult = 1;

	pPool = xrtTaskPoolCreate(&tConfig);
	pFuture = pPool != NULL ?
		xrtTaskSubmitFor(
			pPool,
			testSingleTask,
			&iValue,
			NULL,
			UINT64_C(2000000)
		) : NULL;
	if (
		(pFuture != NULL) &&
		(xrtFutureWaitFor(pFuture, UINT64_C(2000000)) == XWAIT_OK) &&
		(xrtFutureValue(pFuture) == &iValue) &&
		xrtTaskPoolDestroy(pPool)
	) {
		iResult = 0;
		pPool = NULL;
	}
	xrtFutureDestroy(pFuture);
	(void)xrtTaskPoolDestroy(pPool);
	return iResult;
}

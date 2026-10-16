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
			INT64_C(2000)
		) : NULL;
	if (
		(pFuture != NULL) &&
		(xrtFutureWaitFor(pFuture, INT64_C(2000)) == XWAIT_OK) &&
		(xrtFutureValue(pFuture) == &iValue) &&
		xrtTaskPoolClose(pPool) && xrtTaskPoolWaitFor(pPool, 2000) == XWAIT_OK
	) {
		const xrtownershipquiescencev1* pPolicy = xrtTaskPoolOwnershipQuiescenceV1Get();
		xrtownershipref View = xrtTaskPoolOwnership(pPool);
		xrtownershipscope Freeze = {0}; size_t iCount = 0;
		if (!pPolicy || pPolicy->size != sizeof(*pPolicy) || pPolicy->Ops != View.Ops) return 2;
		double iDeadline = xrtTimer() + 3;
		while (!xrtOwnershipFreezeTryBegin(&Freeze)) {
			if (xrtTimer() >= iDeadline) return 2;
			xrtThreadYield();
		}
		bool bReady = !pPolicy->Busy(View.Data) && View.Ops->Count(View.Data, &iCount);
		if (!xrtOwnershipScopeEnd(&Freeze) || !bReady) return 3;
		if (xrtTaskPoolDestroy(pPool)) {
			iResult = 0;
			pPool = NULL;
		}
	}
	xrtFutureDestroy(pFuture);
	(void)xrtTaskPoolDestroy(pPool);
	return iResult;
}

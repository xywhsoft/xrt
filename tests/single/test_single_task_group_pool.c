#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件分组任务返回调用方提供的借用值。 */
static xtaskoutcome testSingleTaskGroupPoolRun(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	(void)pCancel;
	pResult->Value = pData;
	return XTASK_SUCCESS;
}



/* 验证组合裁剪模块在单头文件中接通任务组和任务池。 */
int main(void)
{
	xtaskpoolconfig tConfig = { 1, 2, 0 };
	xtaskpool* pPool = xrtTaskPoolCreate(&tConfig);
	xtaskgroup* pGroup = xrtTaskGroupCreate(NULL);
	xfuture* pFuture;
	int iValue = 81;
	int iResult = 1;

	pFuture = (pPool != NULL) && (pGroup != NULL) ?
		xrtTaskGroupSubmitUntilCancel(
			pGroup,
			pPool,
			testSingleTaskGroupPoolRun,
			&iValue,
			NULL,
			xrtDeadlineAfter(UINT64_C(2000000)),
			NULL
		) : NULL;
	if (
		(pFuture != NULL) &&
		(xrtTaskGroupWait(pGroup) == XWAIT_OK) &&
		(xrtFutureValue(pFuture) == &iValue) &&
		xrtTaskPoolDestroy(pPool)
	) {
		iResult = 0;
		pPool = NULL;
	}
	xrtFutureDestroy(pFuture);
	xrtTaskGroupDestroy(pGroup);
	(void)xrtTaskPoolDestroy(pPool);
	return iResult;
}

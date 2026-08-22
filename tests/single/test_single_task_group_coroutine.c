#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件分组协程任务返回借用值。 */
static xtaskoutcome testSingleTaskGroupCo(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	(void)pCancel;
	pResult->Value = pData;
	return XTASK_SUCCESS;
}



/* 验证组合裁剪模块接通任务组、任务核心和协程调度器。 */
int main(void)
{
	xcosched* pSched = xrtCoSchedCreate();
	xtaskgroup* pGroup = xrtTaskGroupCreate(NULL);
	xfuture* pFuture;
	int iValue = 92;
	int iResult = 1;

	pFuture = (pSched != NULL) && (pGroup != NULL) ?
		xrtTaskGroupCo(
			pGroup,
			pSched,
			testSingleTaskGroupCo,
			&iValue,
			NULL,
			0
		) : NULL;
	if (
		(pFuture != NULL) && xrtCoSchedRun(pSched) &&
		(xrtTaskGroupWait(pGroup) == XWAIT_OK) &&
		(xrtFutureValue(pFuture) == &iValue)
	) {
		iResult = 0;
	}
	xrtFutureDestroy(pFuture);
	xrtTaskGroupDestroy(pGroup);
	(void)xrtCoSchedDestroy(pSched);
	(void)xrtCoThreadDetach();
	return iResult;
}

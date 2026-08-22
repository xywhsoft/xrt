#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头分组网络任务返回调用方提供的借用值。 */
static xtaskoutcome testSingleTaskGroupNet(
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



/* 验证单头网络任务可以原子纳入结构化任务组。 */
int main(void)
{
	xnetengine* pEngine = xrtNetEngineCreate(NULL);
	xtaskgroup* pGroup = xrtTaskGroupCreate(NULL);
	xfuture* pFuture = NULL;
	int iValue = 67;
	int iResult = 1;

	if ( (pEngine != NULL) && (pGroup != NULL) &&
		xrtNetEngineStart(pEngine) ) {
		pFuture = xrtTaskGroupNetAfter(
			pGroup,
			pEngine,
			0,
			testSingleTaskGroupNet,
			&iValue,
			NULL,
			1000u
		);
	}
	if ( (pFuture != NULL) &&
		(xrtTaskGroupWaitFor(pGroup, 3000000u) == XWAIT_OK) &&
		(xrtFutureValue(pFuture) == &iValue) ) {
		iResult = 0;
	}
	xrtFutureDestroy(pFuture);
	xrtTaskGroupDestroy(pGroup);
	(void)xrtNetEngineDestroy(pEngine);
	return iResult;
}

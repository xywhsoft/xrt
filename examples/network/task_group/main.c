#include <stdio.h>
#include <xrt.h>



/* 在亲和 Worker 上返回一个借用整数。 */
static xtaskoutcome buildGroupValue(
	xnetworker* pWorker,
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	(void)pWorker;
	(void)pCancel;
	pResult->Value = pData;
	return XTASK_SUCCESS;
}



/* 演示立即和延迟网络任务共享一个结构化作用域。 */
int main(void)
{
	xnetengine* pEngine = xrtNetEngineCreate(NULL);
	xtaskgroup* pGroup = xrtTaskGroupCreate(NULL);
	xfuture* pFirst = NULL;
	xfuture* pSecond = NULL;
	int iFirst = 7;
	int iSecond = 11;
	int iResult = 1;

	if ( (pEngine != NULL) && (pGroup != NULL) &&
		xrtNetEngineStart(pEngine) ) {
		pFirst = xrtTaskGroupNet(
			pGroup,
			pEngine,
			0,
			buildGroupValue,
			&iFirst,
			NULL
		);
		pSecond = xrtTaskGroupNetAfter(
			pGroup,
			pEngine,
			0,
			buildGroupValue,
			&iSecond,
			NULL,
			1000u
		);
	}
	if ( (pFirst != NULL) && (pSecond != NULL) &&
		(xrtTaskGroupWaitFor(pGroup, 3000000u) == XWAIT_OK) ) {
		printf(
			"values=%d,%d\n",
			*(int*)xrtFutureValue(pFirst),
			*(int*)xrtFutureValue(pSecond)
		);
		iResult = 0;
	}
	xrtFutureDestroy(pFirst);
	xrtFutureDestroy(pSecond);
	xrtTaskGroupDestroy(pGroup);
	if ( !xrtNetEngineDestroy(pEngine) ) {
		iResult = 1;
	}
	return iResult;
}

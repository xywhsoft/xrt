#include <stdio.h>
#include <xrt.h>



/* 协程任务在不阻塞原生线程的情况下等待后返回借用值。 */
static xtaskoutcome groupedCoroutine(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	if ( (xrtCoSleep(1000) == XWAIT_CANCELLED) ||
		xrtCancelRequested(pCancel) ) {
		return XTASK_CANCELLED;
	}
	pResult->Value = pData;
	return XTASK_SUCCESS;
}



/* 把协程任务原子纳入组，再统一运行和等待。 */
int main(void)
{
	xcosched* pSched = xrtCoSchedCreate();
	xtaskgroup* pGroup = xrtTaskGroupCreate(NULL);
	xfuture* pFuture = NULL;
	int iValue = 47;
	int iResult = 1;

	if ( (pSched == NULL) || (pGroup == NULL) ) {
		goto cleanup;
	}
	pFuture = xrtTaskGroupCo(
		pGroup,
		pSched,
		groupedCoroutine,
		&iValue,
		NULL,
		0
	);
	if (
		(pFuture != NULL) && xrtCoSchedRun(pSched) &&
		(xrtTaskGroupWait(pGroup) == XWAIT_OK)
	) {
		printf("value = %d\n", *(int*)xrtFutureValue(pFuture));
		iResult = 0;
	}

cleanup:
	xrtFutureDestroy(pFuture);
	xrtTaskGroupDestroy(pGroup);
	(void)xrtCoSchedDestroy(pSched);
	(void)xrtCoThreadDetach();
	return iResult;
}

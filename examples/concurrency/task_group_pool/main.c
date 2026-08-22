#include <stdio.h>
#include <xrt.h>



/* 线程池任务返回输入整数的借用地址。 */
static xtaskoutcome groupedWork(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	if ( xrtCancelRequested(pCancel) ) {
		return XTASK_CANCELLED;
	}
	pResult->Value = pData;
	return XTASK_SUCCESS;
}



/* 用一个结构化任务组原子提交并等待多个有界线程池任务。 */
int main(void)
{
	xtaskpoolconfig tConfig = { 2, 16, 0 };
	xtaskpool* pPool = xrtTaskPoolCreate(&tConfig);
	xtaskgroup* pGroup = xrtTaskGroupCreate(NULL);
	xfuture* arrFuture[3] = { NULL, NULL, NULL };
	int arrValue[3] = { 11, 22, 33 };
	int iResult = 1;

	if ( (pPool == NULL) || (pGroup == NULL) ) {
		goto cleanup;
	}
	for ( size_t i = 0; i < 3; i++ ) {
		arrFuture[i] = xrtTaskGroupSubmitFor(
			pGroup,
			pPool,
			groupedWork,
			&arrValue[i],
			NULL,
			UINT64_C(2000000)
		);
		if ( arrFuture[i] == NULL ) {
			(void)xrtTaskGroupCancel(pGroup);
			goto cleanup;
		}
	}
	if ( xrtTaskGroupWait(pGroup) != XWAIT_OK ) {
		goto cleanup;
	}
	for ( size_t i = 0; i < 3; i++ ) {
		printf("value[%u] = %d\n", (unsigned)i,
			*(int*)xrtFutureValue(arrFuture[i]));
	}
	iResult = 0;

cleanup:
	for ( size_t i = 0; i < 3; i++ ) {
		xrtFutureDestroy(arrFuture[i]);
	}
	xrtTaskGroupDestroy(pGroup);
	(void)xrtTaskPoolDestroy(pPool);
	return iResult;
}

#include <stdio.h>

#include <xrt.h>



/* 保存一个报告分片的输入和输出。 */
typedef struct reportpart {
	int32 Input;
	int32 Output;
} reportpart;



/* 保存协程编排过程需要的执行器、分片和最终结果。 */
typedef struct reportcontext {
	xtaskpool* Pool;
	reportpart Parts[3];
	int32 Total;
	bool TimedOut;
	bool Succeeded;
} reportcontext;



/* 在线程池中计算一个分片，并把借用结果交给 Future。 */
static xtaskoutcome reportPartRun(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	reportpart* pPart = (reportpart*)pData;

	if ( xrtCancelRequested(pCancel) ) {
		return XTASK_CANCELLED;
	}
	pPart->Output = pPart->Input * pPart->Input;
	pResult->Value = pPart;
	return XTASK_SUCCESS;
}



/* 用协程非阻塞等待整个任务作用域，并汇总各 Future 的结果。 */
static ptr reportRun(ptr pData)
{
	reportcontext* pContext = (reportcontext*)pData;
	xtaskgroup* pGroup = NULL;
	xfuture* pDone = NULL;
	xfuture* arrFuture[3] = { NULL, NULL, NULL };
	xwaitresult eWait = XWAIT_ERROR;
	int32 iTotal = 0;

	pGroup = xrtTaskGroupCreate(NULL);
	if ( pGroup == NULL ) {
		goto cleanup;
	}
	pDone = xrtTaskGroupFuture(pGroup);
	if ( pDone == NULL ) {
		goto cleanup;
	}

	/* 组合入口保证任务不会在登记到作用域前提前逃逸。 */
	for ( size_t i = 0; i < 3; i++ ) {
		arrFuture[i] = xrtTaskGroupSubmit(
			pGroup,
			pContext->Pool,
			reportPartRun,
			&pContext->Parts[i],
			NULL
		);
		if ( arrFuture[i] == NULL ) {
			(void)xrtTaskGroupCancel(pGroup);
			(void)xrtFutureAwait(pDone);
			goto cleanup;
		}
	}

	/* Close 只停止新增；超时路径再显式请求取消并等待真实终态。 */
	(void)xrtTaskGroupClose(pGroup);
	eWait = xrtFutureAwaitFor(pDone, UINT64_C(2000000));
	if ( eWait == XWAIT_TIMEOUT ) {
		pContext->TimedOut = true;
		(void)xrtTaskGroupCancel(pGroup);
		(void)xrtFutureAwait(pDone);
		goto cleanup;
	}
	if ( eWait != XWAIT_OK ) {
		(void)xrtTaskGroupCancel(pGroup);
		(void)xrtFutureAwait(pDone);
		goto cleanup;
	}

	/* Done Future 只表示作用域排空，逐项结果仍从各自 Future 读取。 */
	for ( size_t i = 0; i < 3; i++ ) {
		if ( xrtFutureState(arrFuture[i]) != XFUTURE_RESOLVED ) {
			goto cleanup;
		}
		iTotal += ((reportpart*)xrtFutureValue(arrFuture[i]))->Output;
	}
	pContext->Total = iTotal;
	pContext->Succeeded = true;

cleanup:
	for ( size_t i = 0; i < 3; i++ ) {
		xrtFutureDestroy(arrFuture[i]);
	}
	xrtFutureDestroy(pDone);
	xrtTaskGroupDestroy(pGroup);
	return pContext->Succeeded ? pContext : NULL;
}



/* 创建有界线程池和协程调度器，运行一次结构化批量报告。 */
int main(void)
{
	xtaskpoolconfig tPoolConfig = { 2, 8, 0 };
	reportcontext tContext = { 0 };
	xtaskpool* pPool = NULL;
	xcosched* pSched = NULL;
	xcoro* pReport = NULL;
	int iResult = 1;

	pPool = xrtTaskPoolCreate(&tPoolConfig);
	pSched = xrtCoSchedCreate();
	if ( (pPool == NULL) || (pSched == NULL) ) {
		goto cleanup;
	}
	tContext.Pool = pPool;
	tContext.Parts[0].Input = 10;
	tContext.Parts[1].Input = 20;
	tContext.Parts[2].Input = 30;

	pReport = xrtCoSpawn(pSched, reportRun, &tContext, NULL);
	if ( (pReport == NULL) || !xrtCoSchedRun(pSched) ||
		(xrtCoResult(pReport) != &tContext) || !tContext.Succeeded ) {
		goto cleanup;
	}
	printf("report total = %d\n", (int)tContext.Total);
	iResult = (tContext.Total == 1400) ? 0 : 1;

cleanup:
	xrtCoDestroy(pReport);
	(void)xrtCoSchedDestroy(pSched);
	(void)xrtCoThreadDetach();
	(void)xrtTaskPoolDestroy(pPool);
	return iResult;
}

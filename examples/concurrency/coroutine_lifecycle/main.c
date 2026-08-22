#include <stdio.h>

#include <xrt.h>



typedef struct examplecontext {
	xcoro* Worker;
	xcocleanup ManualCleanup;
	xwaitresult JoinResult;
	xcoroterm Term;
	int ManualCleaned;
	int DeferredCleaned;
	int CleanedAtFinalize;
} examplecontext;



/* 记录清理过程恰好执行一次。 */
static void exampleCleanup(ptr pData)
{
	int* pCount = (int*)pData;

	(*pCount)++;
}



/* 注册两种清理节点，然后挂起等待协作取消。 */
static ptr exampleWorker(ptr pData)
{
	examplecontext* pContext = (examplecontext*)pData;
	xcocleanup* pDeferred;
	xwaitresult Result;

	pDeferred = xrtCoDefer(exampleCleanup, &pContext->DeferredCleaned);
	if ( pDeferred == NULL ) {
		return NULL;
	}
	if ( !xrtCoCleanupPush(
		&pContext->ManualCleanup,
		exampleCleanup,
		&pContext->ManualCleaned
	) ) {
		(void)xrtCoCleanupPop(pDeferred, true);
		return NULL;
	}
	if ( !xrtCoCleanupPop(&pContext->ManualCleanup, true) ) {
		return NULL;
	}

	Result = xrtCoPark();
	if ( Result == XWAIT_CANCELLED ) {
		(void)xrtCoConfirmCancel();
	}
	return pContext;
}



/* 在工作协程已经挂起后请求取消。 */
static ptr exampleCancel(ptr pData)
{
	examplecontext* pContext = (examplecontext*)pData;

	return xrtCoCancel(pContext->Worker) ? pContext : NULL;
}



/* 在同一调度器中等待工作协程发布终态。 */
static ptr exampleJoin(ptr pData)
{
	examplecontext* pContext = (examplecontext*)pData;

	pContext->JoinResult = xrtCoJoin(pContext->Worker);
	return pContext;
}



/* 在全部清理完成后记录不可变的终态快照。 */
static void exampleFinalize(
	xcoroterm Term,
	ptr pResult,
	const xerror* pError,
	ptr pData
)
{
	examplecontext* pContext = (examplecontext*)pData;

	(void)pResult;
	(void)pError;
	pContext->Term = Term;
	pContext->CleanedAtFinalize =
		pContext->ManualCleaned + pContext->DeferredCleaned;
}



/* 展示取消、Join、清理栈和终态回调之间的完整生命周期。 */
int main(void)
{
	examplecontext Context = { 0 };
	xcoroargs Args = { 0 };
	xcosched* pSched = xrtCoSchedCreate();
	bool bOkay = false;

	if ( pSched == NULL ) {
		return 1;
	}
	Args.Finalize = exampleFinalize;
	Args.FinalizeData = &Context;
	Context.Worker = xrtCoSpawn(
		pSched,
		exampleWorker,
		&Context,
		&Args
	);
	if ( (Context.Worker != NULL) &&
		xrtCoGo(pSched, exampleCancel, &Context, NULL) &&
		xrtCoGo(pSched, exampleJoin, &Context, NULL) ) {
		bOkay = xrtCoSchedRun(pSched) &&
			(Context.JoinResult == XWAIT_OK) &&
			(Context.Term == XCORO_TERM_CANCELLED) &&
			(Context.ManualCleaned == 1) &&
			(Context.DeferredCleaned == 1) &&
			(Context.CleanedAtFinalize == 2);
	} else {
		(void)xrtCoSchedClose(pSched);
		(void)xrtCoSchedRun(pSched);
	}

	printf(
		"join=%d term=%d cleanup=%d\n",
		(int)Context.JoinResult,
		(int)Context.Term,
		Context.CleanedAtFinalize
	);
	if ( Context.Worker != NULL ) {
		(void)xrtCoDestroy(Context.Worker);
	}
	(void)xrtCoSchedDestroy(pSched);
	(void)xrtCoThreadDetach();
	return bOkay ? 0 : 1;
}

#include "../test.h"



/* 基础任务上下文记录过程命中和任务数据析构次数。 */
typedef struct testtaskcontext {
	int Value;
	int Hits;
	int Destroyed;
	int OwnedDestroyed;
	bool ErrorIsolated;
	bool DestroyRejected;
	xtaskpool* Pool;
} testtaskcontext;



/* 终态屏障上下文让数据析构停在可观察窗口。 */
typedef struct testtaskbarrier {
	xatomic32 Destroying;
	xatomic32 Release;
} testtaskbarrier;



/* 受理后的任务数据析构只记录次数，测试数据本身位于栈上。 */
static void testTaskDataDestroy(ptr pValue, ptr pData)
{
	testtaskcontext* pContext = (testtaskcontext*)pData;

	(void)pValue;
	pContext->Destroyed++;
}



/* 拒绝路径若错误取得数据所有权，就会增加独立计数。 */
static void testTaskRejectedDestroy(ptr pValue, ptr pData)
{
	int* pDestroyed = (int*)pData;

	(void)pValue;
	(*pDestroyed)++;
}



/* Future 最后释放时销毁 owned 成功值。 */
static void testTaskOwnedDestroy(ptr pValue, ptr pData)
{
	testtaskcontext* pContext = (testtaskcontext*)pData;

	(void)pValue;
	pContext->OwnedDestroyed++;
}



/* 返回一个借用成功值。 */
static xtaskoutcome testTaskSuccess(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtaskcontext* pContext = (testtaskcontext*)pData;

	(void)pCancel;
	pContext->Hits++;
	pResult->Value = &pContext->Value;
	return XTASK_SUCCESS;
}



/* 返回一个由 Future 接管的成功值。 */
static xtaskoutcome testTaskOwned(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtaskcontext* pContext = (testtaskcontext*)pData;

	(void)pCancel;
	pContext->Hits++;
	pResult->Value = &pContext->Value;
	pResult->Destroy = testTaskOwnedDestroy;
	pResult->DestroyData = pContext;
	return XTASK_SUCCESS;
}



/* 设置结构化错误并显式返回失败。 */
static xtaskoutcome testTaskFailed(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtaskcontext* pContext = (testtaskcontext*)pData;
	xerror* pError;

	(void)pCancel;
	(void)pResult;
	pContext->Hits++;
	pError = xrtErrorCreate(XERR_PROTOCOL, "test.task", 17, "task failed");
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return XTASK_FAILED;
}



/* 不设置错误的失败必须由运行库补成 INTERNAL。 */
static xtaskoutcome testTaskFailedWithoutError(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtaskcontext* pContext = (testtaskcontext*)pData;

	(void)pCancel;
	(void)pResult;
	pContext->Hits++;
	return XTASK_FAILED;
}



/* 下一任务必须看不到前一任务留下的错误和临时内存状态。 */
static xtaskoutcome testTaskIsolation(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtaskcontext* pContext = (testtaskcontext*)pData;
	xtempinfo tInfo;

	(void)pCancel;
	pContext->Hits++;
	memset(&tInfo, 0, sizeof(tInfo));
	xrtTempGet(xrtTempCurrent(), &tInfo);
	pContext->ErrorIsolated = (xrtGetError() == NULL) &&
		(tInfo.CurrentBytes == 0) && (xrtTemp(32) != NULL);
	pResult->Value = &pContext->Value;
	return XTASK_SUCCESS;
}



/* 工作线程尝试销毁自身所属任务池必须失败但不能破坏池。 */
static xtaskoutcome testTaskDestroyFromWorker(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtaskcontext* pContext = (testtaskcontext*)pData;

	(void)pCancel;
	(void)pResult;
	pContext->DestroyRejected = !xrtTaskPoolDestroy(pContext->Pool) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE);
	return XTASK_SUCCESS;
}



/* 不应运行的预取消任务。 */
static xtaskoutcome testTaskMustNotRun(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtaskcontext* pContext = (testtaskcontext*)pData;

	(void)pCancel;
	(void)pResult;
	pContext->Hits++;
	return XTASK_SUCCESS;
}



/* 屏障任务本身立即成功，终态必须等待数据析构结束。 */
static xtaskoutcome testTaskBarrierRun(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	(void)pCancel;
	(void)pData;
	(void)pResult;
	return XTASK_SUCCESS;
}



/* 阻塞数据析构，验证 Future 不会提前发布。 */
static void testTaskBarrierDestroy(ptr pValue, ptr pData)
{
	testtaskbarrier* pBarrier = (testtaskbarrier*)pValue;

	(void)pData;
	xrtAtomic32Store(&pBarrier->Destroying, 1, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(&pBarrier->Release, XMEMORY_ACQUIRE) == 0 ) {
		xrtThreadYield();
	}
}



/* 等待任务数据析构进入测试屏障。 */
static void testTaskBarrierWait(testtaskbarrier* pBarrier)
{
	xdeadline iDeadline = xrtDeadlineAfter(3000000u);

	while ( xrtAtomic32Load(
		&pBarrier->Destroying,
		XMEMORY_ACQUIRE
	) == 0 ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"task data destroy barrier did not start");
		xrtThreadYield();
	}
}



/* 验证任务结果、错误、所有权、上下文隔离和任务池生命周期。 */
int main(void)
{
	xtaskpoolconfig tConfig = { 1, 16, 0 };
	xtaskpoolstats tStats;
	xtaskargs tArgs;
	testtaskcontext tContext;
	testtaskcontext tPreCancelled;
	testtaskbarrier tBarrier;
	xtaskpool* pPool;
	xcancel* pParent;
	xfuture* arrFuture[6];
	xfuture* pWorkerFuture;
	xfuture* pCancelledFuture;
	int iRejectedDestroyed = 0;

	memset(&tContext, 0, sizeof(tContext));
	memset(&tPreCancelled, 0, sizeof(tPreCancelled));
	memset(&tBarrier, 0, sizeof(tBarrier));
	memset(&tArgs, 0, sizeof(tArgs));
	memset(&tStats, 0, sizeof(tStats));
	tContext.Value = 42;
	pPool = xrtTaskPoolCreate(&tConfig);
	testRequire(pPool != NULL, "task pool create failed");
	tContext.Pool = pPool;
	tArgs.Destroy = testTaskDataDestroy;
	tArgs.DestroyData = &tContext;

	arrFuture[0] = xrtTaskSubmit(pPool, testTaskSuccess, &tContext, &tArgs);
	arrFuture[1] = xrtTaskSubmit(pPool, testTaskOwned, &tContext, &tArgs);
	arrFuture[2] = xrtTaskSubmit(pPool, testTaskFailed, &tContext, &tArgs);
	arrFuture[3] = xrtTaskSubmit(pPool, testTaskFailedWithoutError, &tContext, &tArgs);
	arrFuture[4] = xrtTaskSubmit(pPool, testTaskIsolation, &tContext, &tArgs);
	arrFuture[5] = xrtTaskSubmit(pPool, testTaskSuccess, &tContext, &tArgs);
	for ( size_t i = 0; i < 6; i++ ) {
		testRequire(arrFuture[i] != NULL, "task submit failed");
	}
	testRequire(xrtTaskPoolWaitFor(pPool, 0) == XWAIT_ERROR,
		"open task pool wait succeeded");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE,
		"open task pool wait error mismatch");
	testRequire(xrtTaskPoolClose(pPool), "task pool close failed");
	testRequire(xrtTaskPoolWaitFor(pPool, UINT64_C(2000000)) == XWAIT_OK,
		"task pool drain failed");

	testRequire(xrtFutureValue(arrFuture[0]) == &tContext.Value,
		"borrowed task value mismatch");
	testRequire(xrtFutureValue(arrFuture[1]) == &tContext.Value,
		"owned task value mismatch");
	testRequire(xrtFutureState(arrFuture[2]) == XFUTURE_FAILED,
		"structured failure state mismatch");
	testRequire(xrtErrorKind(xrtFutureError(arrFuture[2])) == XERR_PROTOCOL,
		"structured failure error mismatch");
	testRequire(xrtErrorKind(xrtFutureError(arrFuture[3])) == XERR_INTERNAL,
		"missing task error was not mapped to INTERNAL");
	testRequire(tContext.ErrorIsolated, "task execution context leaked between jobs");
	testRequire(tContext.Destroyed == 6, "accepted task data destructor count mismatch");
	testRequire(tContext.OwnedDestroyed == 0, "owned result destroyed too early");

	testRequire(xrtTaskPoolGet(pPool, &tStats), "task pool stats failed");
	testRequire(
		(tStats.Submitted == 6) && (tStats.Completed == 6) &&
		(tStats.Succeeded == 4) && (tStats.Failed == 2) &&
		(tStats.Cancelled == 0) && (tStats.Queued == 0) &&
		(tStats.Running == 0) && tStats.Closed,
		"task pool stats mismatch"
	);
	tArgs.Destroy = testTaskRejectedDestroy;
	tArgs.DestroyData = &iRejectedDestroyed;
	testRequire(xrtTaskSubmit(pPool, testTaskSuccess, &tContext, &tArgs) == NULL,
		"closed task pool accepted work");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_CLOSED,
		"closed task pool error mismatch");
	testRequire(iRejectedDestroyed == 0, "rejected task data ownership was consumed");
	for ( size_t i = 0; i < 6; i++ ) {
		xrtFutureDestroy(arrFuture[i]);
	}
	testRequire(tContext.OwnedDestroyed == 1, "owned result destructor count mismatch");
	testRequire(xrtTaskPoolDestroy(pPool), "closed task pool destroy failed");

	/* 单独验证工作线程销毁保护。 */
	pPool = xrtTaskPoolCreate(&tConfig);
	testRequire(pPool != NULL, "worker destroy task pool create failed");
	tContext.Pool = pPool;
	pWorkerFuture = xrtTaskSubmit(pPool, testTaskDestroyFromWorker, &tContext, NULL);
	testRequire(pWorkerFuture != NULL, "worker destroy task submit failed");
	testRequire(xrtFutureWaitFor(pWorkerFuture, UINT64_C(2000000)) == XWAIT_OK,
		"worker destroy task wait failed");
	testRequire(tContext.DestroyRejected, "worker destroyed its own task pool");
	xrtFutureDestroy(pWorkerFuture);
	testRequire(xrtTaskPoolDestroy(pPool), "worker protected pool destroy failed");

	/* Future 终态必须晚于任务数据析构，不得暴露仍在使用的上下文。 */
	pPool = xrtTaskPoolCreate(&tConfig);
	testRequire(pPool != NULL, "task barrier pool create failed");
	memset(&tArgs, 0, sizeof(tArgs));
	tArgs.Destroy = testTaskBarrierDestroy;
	pWorkerFuture = xrtTaskSubmit(
		pPool,
		testTaskBarrierRun,
		&tBarrier,
		&tArgs
	);
	testRequire(pWorkerFuture != NULL, "task barrier submit failed");
	testTaskBarrierWait(&tBarrier);
	testRequire(xrtFutureState(pWorkerFuture) == XFUTURE_PENDING,
		"task Future published before data destruction completed");
	xrtAtomic32Store(&tBarrier.Release, 1, XMEMORY_RELEASE);
	testRequire(xrtFutureWaitFor(pWorkerFuture, 3000000u) == XWAIT_OK,
		"task barrier Future did not complete");
	testRequire(xrtFutureState(pWorkerFuture) == XFUTURE_RESOLVED,
		"task barrier Future did not resolve");
	xrtFutureDestroy(pWorkerFuture);
	testRequire(xrtTaskPoolDestroy(pPool), "task barrier pool destroy failed");

	/* 已取消父上下文产生立即取消 Future，并仍接管受理后的任务数据。 */
	pPool = xrtTaskPoolCreate(&tConfig);
	pParent = xrtCancelCreate();
	testRequire((pPool != NULL) && (pParent != NULL), "pre-cancel setup failed");
	testRequire(xrtCancelRequest(pParent), "parent cancel request failed");
	tArgs.Cancel = pParent;
	tArgs.Destroy = testTaskDataDestroy;
	tArgs.DestroyData = &tPreCancelled;
	pCancelledFuture = xrtTaskSubmit(
		pPool,
		testTaskMustNotRun,
		&tPreCancelled,
		&tArgs
	);
	testRequire(pCancelledFuture != NULL, "pre-cancel task submit failed");
	testRequire(xrtFutureState(pCancelledFuture) == XFUTURE_CANCELLED,
		"pre-cancel task state mismatch");
	testRequire((tPreCancelled.Hits == 0) && (tPreCancelled.Destroyed == 1),
		"pre-cancel task execution or ownership mismatch");
	xrtFutureDestroy(pCancelledFuture);
	xrtCancelDestroy(pParent);
	testRequire(xrtTaskPoolDestroy(pPool), "pre-cancel task pool destroy failed");

	printf("[PASS] task pool\n");
	return 0;
}

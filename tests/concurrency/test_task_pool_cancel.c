#include "../test.h"



/* 队列边界测试上下文由 mutex 保护开始标记和析构计数。 */
typedef struct testtaskcancel {
	xmutex Lock;
	xcond Ready;
	bool Started;
	bool Released;
	int Hits;
	int Destroyed;
} testtaskcancel;



/* 记录受理任务的数据析构。 */
static void testTaskCancelDestroy(ptr pValue, ptr pData)
{
	testtaskcancel* pContext = (testtaskcancel*)pData;

	(void)pValue;
	(void)xrtMutexLock(&pContext->Lock);
	pContext->Destroyed++;
	(void)xrtMutexUnlock(&pContext->Lock);
}



/* 首个任务保持运行，直到任务池发出协作取消。 */
static xtaskoutcome testTaskCancelRunning(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtaskcancel* pContext = (testtaskcancel*)pData;

	(void)pResult;
	(void)xrtMutexLock(&pContext->Lock);
	pContext->Hits++;
	pContext->Started = true;
	(void)xrtCondBroadcast(&pContext->Ready);
	(void)xrtMutexUnlock(&pContext->Lock);
	for ( ;; ) {
		bool bReleased;

		(void)xrtMutexLock(&pContext->Lock);
		bReleased = pContext->Released;
		(void)xrtMutexUnlock(&pContext->Lock);
		if ( bReleased ) {
			return XTASK_SUCCESS;
		}
		if ( xrtCancelRequested(pCancel) ) {
			return XTASK_CANCELLED;
		}
		xrtThreadYield();
	}
}



/* 排队任务如果被执行就会增加命中计数。 */
static xtaskoutcome testTaskCancelQueued(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtaskcancel* pContext = (testtaskcancel*)pData;

	(void)pCancel;
	(void)pResult;
	(void)xrtMutexLock(&pContext->Lock);
	pContext->Hits++;
	(void)xrtMutexUnlock(&pContext->Lock);
	return XTASK_SUCCESS;
}



/* 等待首个任务已经占用唯一工作线程。 */
static bool testTaskWaitStarted(testtaskcancel* pContext)
{
	xdeadline iDeadline = xrtDeadlineAfter(UINT64_C(2000000));
	bool bStarted;

	(void)xrtMutexLock(&pContext->Lock);
	while ( !pContext->Started && !xrtDeadlineExpired(iDeadline) ) {
		(void)xrtCondWaitUntil(&pContext->Ready, &pContext->Lock, iDeadline);
	}
	bStarted = pContext->Started;
	(void)xrtMutexUnlock(&pContext->Lock);
	return bStarted;
}



/* 验证硬队列上限、调用方所有权、排队取消和运行取消。 */
int main(void)
{
	xtaskpoolconfig tConfig = { 1, 1, 0 };
	xtaskpoolstats tStats;
	xtaskargs tArgs;
	testtaskcancel tContext;
	xtaskpool* pPool;
	xfuture* pRunning;
	xfuture* pQueued;
	xfuture* pRejected;
	xfuture* pReplacement;

	memset(&tContext, 0, sizeof(tContext));
	memset(&tStats, 0, sizeof(tStats));
	memset(&tArgs, 0, sizeof(tArgs));
	testRequire(xrtMutexInit(&tContext.Lock), "task cancel lock init failed");
	testRequire(xrtCondInit(&tContext.Ready), "task cancel cond init failed");
	tArgs.Destroy = testTaskCancelDestroy;
	tArgs.DestroyData = &tContext;
	pPool = xrtTaskPoolCreate(&tConfig);
	testRequire(pPool != NULL, "bounded task pool create failed");
	pRunning = xrtTaskSubmit(pPool, testTaskCancelRunning, &tContext, &tArgs);
	testRequire(pRunning != NULL, "running task submit failed");
	testRequire(testTaskWaitStarted(&tContext), "running task did not start");
	pQueued = xrtTaskSubmit(pPool, testTaskCancelQueued, &tContext, &tArgs);
	testRequire(pQueued != NULL, "queued task submit failed");
	pRejected = xrtTaskSubmit(pPool, testTaskCancelQueued, &tContext, &tArgs);
	testRequire(pRejected == NULL, "hard queue limit accepted excess task");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_AGAIN,
		"hard queue limit error mismatch");
	testRequire(tContext.Destroyed == 0, "rejected task data ownership was consumed");

	testRequire(xrtTaskPoolCancel(pPool), "task pool cancel failed");
	testRequire(xrtTaskPoolWaitFor(pPool, UINT64_C(2000000)) == XWAIT_OK,
		"cancelled task pool did not drain");
	testRequire(xrtFutureState(pRunning) == XFUTURE_CANCELLED,
		"running task cancel state mismatch");
	testRequire(xrtFutureState(pQueued) == XFUTURE_CANCELLED,
		"queued task cancel state mismatch");
	testRequire((tContext.Hits == 1) && (tContext.Destroyed == 2),
		"cancelled task execution or ownership count mismatch");
	testRequire(xrtTaskPoolGet(pPool, &tStats), "cancelled task stats failed");
	testRequire(
		(tStats.Submitted == 2) && (tStats.Completed == 2) &&
		(tStats.Cancelled == 2) && (tStats.Rejected == 1) &&
		(tStats.Queued == 0) && (tStats.Running == 0) &&
		tStats.Closed && tStats.Cancelling,
		"cancelled task pool stats mismatch"
	);
	xrtFutureDestroy(pQueued);
	xrtFutureDestroy(pRunning);
	testRequire(xrtTaskPoolDestroy(pPool), "cancelled task pool destroy failed");

	/* 队列满的慢路径应回收单独取消的作业，而不是长期占住槽位。 */
	(void)xrtMutexLock(&tContext.Lock);
	tContext.Started = false;
	tContext.Released = false;
	tContext.Hits = 0;
	tContext.Destroyed = 0;
	(void)xrtMutexUnlock(&tContext.Lock);
	pPool = xrtTaskPoolCreate(&tConfig);
	testRequire(pPool != NULL, "cancel reclaim task pool create failed");
	pRunning = xrtTaskSubmit(pPool, testTaskCancelRunning, &tContext, &tArgs);
	testRequire((pRunning != NULL) && testTaskWaitStarted(&tContext),
		"cancel reclaim running task failed");
	pQueued = xrtTaskSubmit(pPool, testTaskCancelQueued, &tContext, &tArgs);
	testRequire(pQueued != NULL, "cancel reclaim queued task failed");
	testRequire(xrtFutureCancel(pQueued), "queued future cancel request failed");
	pReplacement = xrtTaskSubmit(pPool, testTaskCancelQueued, &tContext, &tArgs);
	testRequire(pReplacement != NULL, "cancelled queue slot was not reclaimed");
	testRequire(xrtFutureState(pQueued) == XFUTURE_CANCELLED,
		"reclaimed queued future was not completed as cancelled");
	(void)xrtMutexLock(&tContext.Lock);
	tContext.Released = true;
	(void)xrtMutexUnlock(&tContext.Lock);
	testRequire(xrtTaskPoolClose(pPool), "cancel reclaim task pool close failed");
	testRequire(xrtTaskPoolWaitFor(pPool, UINT64_C(2000000)) == XWAIT_OK,
		"cancel reclaim task pool did not drain");
	testRequire(
		(xrtFutureState(pRunning) == XFUTURE_RESOLVED) &&
		(xrtFutureState(pReplacement) == XFUTURE_RESOLVED) &&
		(tContext.Hits == 2) && (tContext.Destroyed == 3),
		"cancel reclaim execution or ownership mismatch"
	);
	xrtFutureDestroy(pReplacement);
	xrtFutureDestroy(pQueued);
	xrtFutureDestroy(pRunning);
	testRequire(xrtTaskPoolDestroy(pPool), "cancel reclaim task pool destroy failed");
	testRequire(xrtCondUnit(&tContext.Ready), "task cancel cond unit failed");
	testRequire(xrtMutexUnit(&tContext.Lock), "task cancel lock unit failed");

	printf("[PASS] task pool cancel\n");
	return 0;
}

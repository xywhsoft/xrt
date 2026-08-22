#include "../test.h"
#include "../test_thread.h"



#define TEST_SIGNAL_RUNNING UINT32_C(1)
#define TEST_SIGNAL_ENTERED UINT32_C(2)
#define TEST_SIGNAL_DONE UINT32_C(4)
#define TEST_SIGNAL_SELF_DONE UINT32_C(8)



typedef enum testsubmitmode {
	TEST_SUBMIT_WAIT = 0,
	TEST_SUBMIT_CANCEL,
	TEST_SUBMIT_TASK_CANCEL,
	TEST_DRAIN_CANCEL
} testsubmitmode;



/* 容量等待测试用同一把锁协调任务、提交线程和排空线程。 */
typedef struct testtaskwait {
	xtaskpool* Pool;
	xcancel* Cancel;
	xmutex Lock;
	xcond Changed;
	uint32 Signals;
	bool Release;
	bool Proceed;
	bool WorkerRejected;
	testsubmitmode Mode;
	xfuture* Submitted;
	xwaitresult WaitResult;
	xerrkind Error;
	int Hits;
	int Destroyed;
} testtaskwait;



/* 初始化单个测试场景的同步状态。 */
static void testTaskWaitInit(testtaskwait* pContext)
{
	memset(pContext, 0, sizeof(testtaskwait));
	pContext->WaitResult = XWAIT_ERROR;
	testRequire(xrtMutexInit(&pContext->Lock), "task wait lock init failed");
	testRequire(xrtCondInit(&pContext->Changed), "task wait cond init failed");
}



/* 释放测试场景持有的同步原语。 */
static void testTaskWaitUnit(testtaskwait* pContext)
{
	testRequire(xrtCondUnit(&pContext->Changed), "task wait cond unit failed");
	testRequire(xrtMutexUnit(&pContext->Lock), "task wait lock unit failed");
}



/* 发布一个状态位并唤醒等待测试线程。 */
static void testTaskWaitSignal(testtaskwait* pContext, uint32 iSignal)
{
	(void)xrtMutexLock(&pContext->Lock);
	pContext->Signals |= iSignal;
	(void)xrtCondBroadcast(&pContext->Changed);
	(void)xrtMutexUnlock(&pContext->Lock);
}



/* 在截止时间内等待指定状态位出现。 */
static bool testTaskWaitForSignal(
	testtaskwait* pContext,
	uint32 iSignal,
	uint64 iTimeout
)
{
	xdeadline iDeadline = xrtDeadlineAfter(iTimeout);
	bool bReady;

	(void)xrtMutexLock(&pContext->Lock);
	while (
		((pContext->Signals & iSignal) == 0) &&
		!xrtDeadlineExpired(iDeadline)
	) {
		(void)xrtCondWaitUntil(&pContext->Changed, &pContext->Lock, iDeadline);
	}
	bReady = (pContext->Signals & iSignal) != 0;
	(void)xrtMutexUnlock(&pContext->Lock);
	return bReady;
}



/* 确认提交线程已经进入调用，但在短窗口内尚未返回。 */
static bool testTaskWaitBlocked(testtaskwait* pContext)
{
	if ( !testTaskWaitForSignal(
		pContext,
		TEST_SIGNAL_ENTERED,
		UINT64_C(2000000)
	) ) {
		return false;
	}
	return !testTaskWaitForSignal(
		pContext,
		TEST_SIGNAL_DONE,
		UINT64_C(50000)
	);
}



/* 释放当前占用唯一工作线程的任务。 */
static void testTaskWaitRelease(testtaskwait* pContext)
{
	(void)xrtMutexLock(&pContext->Lock);
	pContext->Release = true;
	(void)xrtCondBroadcast(&pContext->Changed);
	(void)xrtMutexUnlock(&pContext->Lock);
}



/* 记录受理任务的数据所有权已经由任务池回收。 */
static void testTaskWaitDestroy(ptr pValue, ptr pData)
{
	testtaskwait* pContext = (testtaskwait*)pData;

	(void)pValue;
	(void)xrtMutexLock(&pContext->Lock);
	pContext->Destroyed++;
	(void)xrtMutexUnlock(&pContext->Lock);
}



/* 首个任务占住唯一工作线程，直到测试明确释放。 */
static xtaskoutcome testTaskWaitRunning(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtaskwait* pContext = (testtaskwait*)pData;

	(void)pCancel;
	(void)pResult;
	(void)xrtMutexLock(&pContext->Lock);
	pContext->Hits++;
	pContext->Signals |= TEST_SIGNAL_RUNNING;
	(void)xrtCondBroadcast(&pContext->Changed);
	while ( !pContext->Release ) {
		(void)xrtCondWait(&pContext->Changed, &pContext->Lock);
	}
	(void)xrtMutexUnlock(&pContext->Lock);
	return XTASK_SUCCESS;
}



/* 普通排队任务只记录执行次数。 */
static xtaskoutcome testTaskWaitQueued(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtaskwait* pContext = (testtaskwait*)pData;

	(void)pCancel;
	(void)pResult;
	(void)xrtMutexLock(&pContext->Lock);
	pContext->Hits++;
	(void)xrtMutexUnlock(&pContext->Lock);
	return XTASK_SUCCESS;
}



/* 外部线程等待容量或等待任务池排空，并保存自己的线程局部错误。 */
static int testTaskWaitThread(ptr pData)
{
	testtaskwait* pContext = (testtaskwait*)pData;
	xtaskargs tArgs;
	xfuture* pFuture = NULL;
	xwaitresult Result = XWAIT_ERROR;
	xerrkind Error = XERR_NONE;

	memset(&tArgs, 0, sizeof(tArgs));
	tArgs.Destroy = testTaskWaitDestroy;
	tArgs.DestroyData = pContext;
	testTaskWaitSignal(pContext, TEST_SIGNAL_ENTERED);
	if ( pContext->Mode == TEST_SUBMIT_WAIT ) {
		pFuture = xrtTaskSubmitWait(
			pContext->Pool,
			testTaskWaitQueued,
			pContext,
			&tArgs
		);
	} else if ( pContext->Mode == TEST_SUBMIT_CANCEL ) {
		pFuture = xrtTaskSubmitUntilCancel(
			pContext->Pool,
			testTaskWaitQueued,
			pContext,
			&tArgs,
			XRT_DEADLINE_NEVER,
			pContext->Cancel
		);
	} else if ( pContext->Mode == TEST_SUBMIT_TASK_CANCEL ) {
		tArgs.Cancel = pContext->Cancel;
		pFuture = xrtTaskSubmitWait(
			pContext->Pool,
			testTaskWaitQueued,
			pContext,
			&tArgs
		);
	} else {
		Result = xrtTaskPoolWaitUntilCancel(
			pContext->Pool,
			XRT_DEADLINE_NEVER,
			pContext->Cancel
		);
	}
	if ( (pContext->Mode != TEST_DRAIN_CANCEL) && (pFuture == NULL) ) {
		Error = xrtErrorKind(xrtGetError());
	}

	(void)xrtMutexLock(&pContext->Lock);
	pContext->Submitted = pFuture;
	pContext->WaitResult = Result;
	pContext->Error = Error;
	pContext->Signals |= TEST_SIGNAL_DONE;
	(void)xrtCondBroadcast(&pContext->Changed);
	(void)xrtMutexUnlock(&pContext->Lock);
	return 0;
}



/* 创建一工作线程、一队列槽位且已经完全占满的任务池。 */
static void testTaskWaitFill(
	testtaskwait* pContext,
	xfuture** ppRunning,
	xfuture** ppQueued
)
{
	xtaskpoolconfig tConfig = { 1, 1, 0 };

	pContext->Pool = xrtTaskPoolCreate(&tConfig);
	testRequire(pContext->Pool != NULL, "task wait pool create failed");
	*ppRunning = xrtTaskSubmit(
		pContext->Pool,
		testTaskWaitRunning,
		pContext,
		NULL
	);
	testRequire(*ppRunning != NULL, "task wait running submit failed");
	testRequire(testTaskWaitForSignal(
		pContext,
		TEST_SIGNAL_RUNNING,
		UINT64_C(2000000)
	), "task wait running task did not start");
	*ppQueued = xrtTaskSubmit(
		pContext->Pool,
		testTaskWaitQueued,
		pContext,
		NULL
	);
	testRequire(*ppQueued != NULL, "task wait queued submit failed");
}



/* 释放任务、排空任务池并回收场景资源。 */
static void testTaskWaitCleanup(
	testtaskwait* pContext,
	xfuture* pRunning,
	xfuture* pQueued
)
{
	testTaskWaitRelease(pContext);
	(void)xrtTaskPoolClose(pContext->Pool);
	testRequire(
		xrtTaskPoolWaitFor(pContext->Pool, UINT64_C(2000000)) == XWAIT_OK,
		"task wait pool did not drain"
	);
	xrtFutureDestroy(pContext->Submitted);
	xrtFutureDestroy(pQueued);
	xrtFutureDestroy(pRunning);
	testRequire(xrtTaskPoolDestroy(pContext->Pool), "task wait pool destroy failed");
	xrtCancelDestroy(pContext->Cancel);
	testTaskWaitUnit(pContext);
}



/* 验证释放槽位会接纳等待提交，并转移任务数据所有权。 */
static void testTaskWaitSuccess(void)
{
	testtaskwait tContext;
	testthread tThread;
	xtaskpoolstats tStats;
	xfuture* pRunning;
	xfuture* pQueued;

	testTaskWaitInit(&tContext);
	memset(&tThread, 0, sizeof(tThread));
	memset(&tStats, 0, sizeof(tStats));
	testTaskWaitFill(&tContext, &pRunning, &pQueued);
	tContext.Mode = TEST_SUBMIT_WAIT;
	tThread.Proc = testTaskWaitThread;
	tThread.Data = &tContext;
	testThreadsStart(&tThread, 1);
	testRequire(testTaskWaitBlocked(&tContext), "capacity submit did not block");
	testTaskWaitRelease(&tContext);
	testRequire(testTaskWaitForSignal(
		&tContext,
		TEST_SIGNAL_DONE,
		UINT64_C(2000000)
	), "capacity submit was not woken");
	testThreadsJoin(&tThread, 1);
	testRequire(tThread.Result == 0, "capacity submit thread failed");
	testRequire(tContext.Submitted != NULL, "capacity submit was rejected");
	testRequire(xrtTaskPoolGet(tContext.Pool, &tStats), "capacity stats failed");
	testRequire(tStats.QueueLimit == 1, "capacity queue limit stats mismatch");
	testTaskWaitCleanup(&tContext, pRunning, pQueued);
	testRequire(tContext.Destroyed == 1, "accepted capacity task ownership mismatch");
}



/* 验证相对超时不会接纳任务或取得调用方数据所有权。 */
static void testTaskWaitTimeout(void)
{
	testtaskwait tContext;
	xtaskargs tArgs;
	xfuture* pRunning;
	xfuture* pQueued;

	testTaskWaitInit(&tContext);
	memset(&tArgs, 0, sizeof(tArgs));
	testTaskWaitFill(&tContext, &pRunning, &pQueued);
	tArgs.Destroy = testTaskWaitDestroy;
	tArgs.DestroyData = &tContext;
	testRequire(xrtTaskSubmitFor(
		tContext.Pool,
		testTaskWaitQueued,
		&tContext,
		&tArgs,
		UINT64_C(50000)
	) == NULL, "capacity timeout accepted task");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_TIMEOUT,
		"capacity timeout error mismatch");
	testRequire(tContext.Destroyed == 0, "capacity timeout consumed task data");
	testTaskWaitCleanup(&tContext, pRunning, pQueued);
}



/* 验证调用方取消只中止容量等待，不会消费尚未受理的数据。 */
static void testTaskWaitCancel(void)
{
	testtaskwait tContext;
	testthread tThread;
	xfuture* pRunning;
	xfuture* pQueued;

	testTaskWaitInit(&tContext);
	memset(&tThread, 0, sizeof(tThread));
	testTaskWaitFill(&tContext, &pRunning, &pQueued);
	tContext.Mode = TEST_SUBMIT_CANCEL;
	tContext.Cancel = xrtCancelCreate();
	testRequire(tContext.Cancel != NULL, "capacity cancel create failed");
	tThread.Proc = testTaskWaitThread;
	tThread.Data = &tContext;
	testThreadsStart(&tThread, 1);
	testRequire(testTaskWaitBlocked(&tContext), "capacity cancel submit did not block");
	testRequire(xrtCancelRequest(tContext.Cancel), "capacity cancel request failed");
	testRequire(testTaskWaitForSignal(
		&tContext,
		TEST_SIGNAL_DONE,
		UINT64_C(2000000)
	), "capacity cancel did not wake submitter");
	testThreadsJoin(&tThread, 1);
	testRequire((tContext.Submitted == NULL) && (tContext.Error == XERR_CANCELLED),
		"capacity cancel result mismatch");
	testRequire(tContext.Destroyed == 0, "capacity cancel consumed task data");
	testTaskWaitCleanup(&tContext, pRunning, pQueued);
}



/* 验证任务父取消会唤醒提交，并返回已经受理的取消 Future。 */
static void testTaskWaitTaskCancel(void)
{
	testtaskwait tContext;
	testthread tThread;
	xfuture* pRunning;
	xfuture* pQueued;

	testTaskWaitInit(&tContext);
	memset(&tThread, 0, sizeof(tThread));
	testTaskWaitFill(&tContext, &pRunning, &pQueued);
	tContext.Mode = TEST_SUBMIT_TASK_CANCEL;
	tContext.Cancel = xrtCancelCreate();
	testRequire(tContext.Cancel != NULL, "task parent cancel create failed");
	tThread.Proc = testTaskWaitThread;
	tThread.Data = &tContext;
	testThreadsStart(&tThread, 1);
	testRequire(testTaskWaitBlocked(&tContext), "task parent cancel submit did not block");
	testRequire(xrtCancelRequest(tContext.Cancel), "task parent cancel request failed");
	testRequire(testTaskWaitForSignal(
		&tContext,
		TEST_SIGNAL_DONE,
		UINT64_C(2000000)
	), "task parent cancel did not wake submitter");
	testThreadsJoin(&tThread, 1);
	testRequire(
		(tContext.Submitted != NULL) &&
		(xrtFutureState(tContext.Submitted) == XFUTURE_CANCELLED),
		"task parent cancel result mismatch"
	);
	testRequire(tContext.Destroyed == 1,
		"accepted parent-cancelled task ownership mismatch");
	testTaskWaitCleanup(&tContext, pRunning, pQueued);
}



/* 验证关闭任务池会立即唤醒全部容量等待者。 */
static void testTaskWaitClose(void)
{
	testtaskwait tContext;
	testthread tThread;
	xfuture* pRunning;
	xfuture* pQueued;

	testTaskWaitInit(&tContext);
	memset(&tThread, 0, sizeof(tThread));
	testTaskWaitFill(&tContext, &pRunning, &pQueued);
	tContext.Mode = TEST_SUBMIT_WAIT;
	tThread.Proc = testTaskWaitThread;
	tThread.Data = &tContext;
	testThreadsStart(&tThread, 1);
	testRequire(testTaskWaitBlocked(&tContext), "close submit did not block");
	testRequire(xrtTaskPoolClose(tContext.Pool), "capacity close failed");
	testRequire(testTaskWaitForSignal(
		&tContext,
		TEST_SIGNAL_DONE,
		UINT64_C(2000000)
	), "close did not wake capacity submitter");
	testThreadsJoin(&tThread, 1);
	testRequire((tContext.Submitted == NULL) && (tContext.Error == XERR_CLOSED),
		"capacity close result mismatch");
	testRequire(tContext.Destroyed == 0, "capacity close consumed task data");
	testTaskWaitCleanup(&tContext, pRunning, pQueued);
}



/* 验证排空等待可由独立令牌中止，池内任务状态保持不变。 */
static void testTaskDrainCancel(void)
{
	testtaskwait tContext;
	testthread tThread;
	xtaskpoolconfig tConfig = { 1, 1, 0 };
	xfuture* pRunning;

	testTaskWaitInit(&tContext);
	memset(&tThread, 0, sizeof(tThread));
	tContext.Pool = xrtTaskPoolCreate(&tConfig);
	tContext.Cancel = xrtCancelCreate();
	testRequire((tContext.Pool != NULL) && (tContext.Cancel != NULL),
		"drain cancel setup failed");
	pRunning = xrtTaskSubmit(
		tContext.Pool,
		testTaskWaitRunning,
		&tContext,
		NULL
	);
	testRequire((pRunning != NULL) && testTaskWaitForSignal(
		&tContext,
		TEST_SIGNAL_RUNNING,
		UINT64_C(2000000)
	), "drain cancel running task failed");
	testRequire(xrtTaskPoolClose(tContext.Pool), "drain cancel close failed");
	tContext.Mode = TEST_DRAIN_CANCEL;
	tThread.Proc = testTaskWaitThread;
	tThread.Data = &tContext;
	testThreadsStart(&tThread, 1);
	testRequire(testTaskWaitBlocked(&tContext), "drain cancel wait did not block");
	testRequire(xrtCancelRequest(tContext.Cancel), "drain cancel request failed");
	testRequire(testTaskWaitForSignal(
		&tContext,
		TEST_SIGNAL_DONE,
		UINT64_C(2000000)
	), "drain cancel did not wake waiter");
	testThreadsJoin(&tThread, 1);
	testRequire(tContext.WaitResult == XWAIT_CANCELLED,
		"drain cancel result mismatch");
	testTaskWaitCleanup(&tContext, pRunning, NULL);
}



/* 工作线程只在所属池队列已满时尝试阻塞提交。 */
static xtaskoutcome testTaskWaitFromWorker(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtaskwait* pContext = (testtaskwait*)pData;
	xtaskargs tArgs;
	xfuture* pFuture;
	bool bRejected;

	(void)pCancel;
	(void)pResult;
	memset(&tArgs, 0, sizeof(tArgs));
	tArgs.Destroy = testTaskWaitDestroy;
	tArgs.DestroyData = pContext;
	(void)xrtMutexLock(&pContext->Lock);
	pContext->Signals |= TEST_SIGNAL_RUNNING;
	(void)xrtCondBroadcast(&pContext->Changed);
	while ( !pContext->Proceed ) {
		(void)xrtCondWait(&pContext->Changed, &pContext->Lock);
	}
	(void)xrtMutexUnlock(&pContext->Lock);

	pFuture = xrtTaskSubmitWait(
		pContext->Pool,
		testTaskWaitQueued,
		pContext,
		&tArgs
	);
	bRejected = (pFuture == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE);
	(void)xrtMutexLock(&pContext->Lock);
	pContext->WorkerRejected = bRejected;
	pContext->Signals |= TEST_SIGNAL_SELF_DONE;
	(void)xrtCondBroadcast(&pContext->Changed);
	(void)xrtMutexUnlock(&pContext->Lock);
	xrtFutureDestroy(pFuture);
	return XTASK_SUCCESS;
}



/* 验证工作线程不能因等待所属池的队列槽位而自锁。 */
static void testTaskWaitWorkerGuard(void)
{
	testtaskwait tContext;
	xtaskpoolconfig tConfig = { 1, 1, 0 };
	xfuture* pWorker;
	xfuture* pQueued;

	testTaskWaitInit(&tContext);
	tContext.Pool = xrtTaskPoolCreate(&tConfig);
	testRequire(tContext.Pool != NULL, "worker guard pool create failed");
	pWorker = xrtTaskSubmit(
		tContext.Pool,
		testTaskWaitFromWorker,
		&tContext,
		NULL
	);
	testRequire((pWorker != NULL) && testTaskWaitForSignal(
		&tContext,
		TEST_SIGNAL_RUNNING,
		UINT64_C(2000000)
	), "worker guard task did not start");
	pQueued = xrtTaskSubmit(
		tContext.Pool,
		testTaskWaitQueued,
		&tContext,
		NULL
	);
	testRequire(pQueued != NULL, "worker guard queue fill failed");
	(void)xrtMutexLock(&tContext.Lock);
	tContext.Proceed = true;
	(void)xrtCondBroadcast(&tContext.Changed);
	(void)xrtMutexUnlock(&tContext.Lock);
	testRequire(testTaskWaitForSignal(
		&tContext,
		TEST_SIGNAL_SELF_DONE,
		UINT64_C(2000000)
	), "worker guard submit did not return");
	testRequire(tContext.WorkerRejected, "worker guard accepted blocking submit");
	testRequire(tContext.Destroyed == 0, "worker guard consumed rejected data");
	testTaskWaitCleanup(&tContext, pWorker, pQueued);
}



/* 验证任务池容量背压和排空等待的完整截止时间、取消与关闭契约。 */
int main(void)
{
	testTaskWaitSuccess();
	testTaskWaitTimeout();
	testTaskWaitCancel();
	testTaskWaitTaskCancel();
	testTaskWaitClose();
	testTaskDrainCancel();
	testTaskWaitWorkerGuard();

	printf("[PASS] task pool wait\n");
	return 0;
}

#include "../test.h"
#include "../test_thread.h"



#define TEST_GROUP_POOL_ENTERED UINT32_C(1)
#define TEST_GROUP_POOL_DONE UINT32_C(2)



/* 满队列场景协调运行任务、提交线程和任务数据所有权。 */
typedef struct testtaskgrouppool {
	xtaskpool* Pool;
	xtaskgroup* Group;
	xcancel* CallerCancel;
	xmutex Lock;
	xcond Changed;
	uint32 Signals;
	bool Started;
	bool Release;
	bool UseCallerCancel;
	xfuture* Submitted;
	xerrkind Error;
	int Hits;
	int Destroyed;
} testtaskgrouppool;



/* 初始化任务组与任务池组合测试的同步状态。 */
static void testTaskGroupPoolInit(testtaskgrouppool* pContext)
{
	memset(pContext, 0, sizeof(testtaskgrouppool));
	testRequire(xrtMutexInit(&pContext->Lock),
		"task group pool lock init failed");
	testRequire(xrtCondInit(&pContext->Changed),
		"task group pool cond init failed");
}



/* 释放组合测试同步状态。 */
static void testTaskGroupPoolUnit(testtaskgrouppool* pContext)
{
	testRequire(xrtCondUnit(&pContext->Changed),
		"task group pool cond unit failed");
	testRequire(xrtMutexUnit(&pContext->Lock),
		"task group pool lock unit failed");
}



/* 记录只有受理任务才能触发的数据析构。 */
static void testTaskGroupPoolDestroy(ptr pValue, ptr pData)
{
	testtaskgrouppool* pContext = (testtaskgrouppool*)pData;

	(void)pValue;
	(void)xrtMutexLock(&pContext->Lock);
	pContext->Destroyed++;
	(void)xrtMutexUnlock(&pContext->Lock);
}



/* 首个直接任务占住唯一工作线程。 */
static xtaskoutcome testTaskGroupPoolBlock(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtaskgrouppool* pContext = (testtaskgrouppool*)pData;

	(void)pCancel;
	(void)pResult;
	(void)xrtMutexLock(&pContext->Lock);
	pContext->Hits++;
	pContext->Started = true;
	(void)xrtCondBroadcast(&pContext->Changed);
	while ( !pContext->Release ) {
		(void)xrtCondWait(&pContext->Changed, &pContext->Lock);
	}
	(void)xrtMutexUnlock(&pContext->Lock);
	return XTASK_SUCCESS;
}



/* 普通任务只记录执行次数。 */
static xtaskoutcome testTaskGroupPoolRun(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtaskgrouppool* pContext = (testtaskgrouppool*)pData;

	(void)pCancel;
	(void)pResult;
	(void)xrtMutexLock(&pContext->Lock);
	pContext->Hits++;
	(void)xrtMutexUnlock(&pContext->Lock);
	return XTASK_SUCCESS;
}



/* 等待布尔状态或信号位到达。 */
static bool testTaskGroupPoolWaitState(
	testtaskgrouppool* pContext,
	uint32 iSignal,
	bool bStarted,
	uint64 iTimeout
)
{
	xdeadline iDeadline = xrtDeadlineAfter(iTimeout);
	bool bReady;

	(void)xrtMutexLock(&pContext->Lock);
	for ( ;; ) {
		bReady = bStarted ? pContext->Started :
			((pContext->Signals & iSignal) != 0);
		if ( bReady || xrtDeadlineExpired(iDeadline) ) {
			break;
		}
		(void)xrtCondWaitUntil(&pContext->Changed, &pContext->Lock, iDeadline);
	}
	(void)xrtMutexUnlock(&pContext->Lock);
	return bReady;
}



/* 清理上一轮提交线程结果并重置信号。 */
static void testTaskGroupPoolResetSubmit(testtaskgrouppool* pContext)
{
	(void)xrtMutexLock(&pContext->Lock);
	pContext->Signals = 0;
	pContext->Submitted = NULL;
	pContext->Error = XERR_NONE;
	pContext->UseCallerCancel = false;
	(void)xrtMutexUnlock(&pContext->Lock);
}



/* 外部提交线程验证组取消与调用方取消都能中止容量等待。 */
static int testTaskGroupPoolSubmitThread(ptr pData)
{
	testtaskgrouppool* pContext = (testtaskgrouppool*)pData;
	xtaskargs tArgs;
	xfuture* pFuture;
	xerrkind Error = XERR_NONE;

	memset(&tArgs, 0, sizeof(tArgs));
	tArgs.Destroy = testTaskGroupPoolDestroy;
	tArgs.DestroyData = pContext;
	(void)xrtMutexLock(&pContext->Lock);
	pContext->Signals |= TEST_GROUP_POOL_ENTERED;
	(void)xrtCondBroadcast(&pContext->Changed);
	(void)xrtMutexUnlock(&pContext->Lock);
	if ( pContext->UseCallerCancel ) {
		pFuture = xrtTaskGroupSubmitUntilCancel(
			pContext->Group,
			pContext->Pool,
			testTaskGroupPoolRun,
			pContext,
			&tArgs,
			XRT_DEADLINE_NEVER,
			pContext->CallerCancel
		);
	} else {
		pFuture = xrtTaskGroupSubmitWait(
			pContext->Group,
			pContext->Pool,
			testTaskGroupPoolRun,
			pContext,
			&tArgs
		);
	}
	if ( pFuture == NULL ) {
		Error = xrtErrorKind(xrtGetError());
	}
	(void)xrtMutexLock(&pContext->Lock);
	pContext->Submitted = pFuture;
	pContext->Error = Error;
	pContext->Signals |= TEST_GROUP_POOL_DONE;
	(void)xrtCondBroadcast(&pContext->Changed);
	(void)xrtMutexUnlock(&pContext->Lock);
	return 0;
}



/* 启动提交线程并确认它已经在满队列上阻塞。 */
static void testTaskGroupPoolStartBlocked(
	testtaskgrouppool* pContext,
	testthread* pThread
)
{
	memset(pThread, 0, sizeof(testthread));
	pThread->Proc = testTaskGroupPoolSubmitThread;
	pThread->Data = pContext;
	testThreadsStart(pThread, 1);
	testRequire(testTaskGroupPoolWaitState(
		pContext,
		TEST_GROUP_POOL_ENTERED,
		false,
		UINT64_C(2000000)
	), "task group pool submit thread did not enter");
	testRequire(!testTaskGroupPoolWaitState(
		pContext,
		TEST_GROUP_POOL_DONE,
		false,
		UINT64_C(50000)
	), "task group pool capacity wait returned early");
}



/* 验证正常组提交会被任务组完整统计和等待。 */
static void testTaskGroupPoolBasic(void)
{
	testtaskgrouppool tContext;
	xtaskpoolconfig tPoolConfig = { 2, 4, 0 };
	xtaskgroupstats tStats;
	xfuture* pFirst;
	xfuture* pSecond;

	testTaskGroupPoolInit(&tContext);
	memset(&tStats, 0, sizeof(tStats));
	tContext.Pool = xrtTaskPoolCreate(&tPoolConfig);
	tContext.Group = xrtTaskGroupCreate(NULL);
	testRequire((tContext.Pool != NULL) && (tContext.Group != NULL),
		"task group pool basic setup failed");
	pFirst = xrtTaskGroupSubmitFor(
		tContext.Group,
		tContext.Pool,
		testTaskGroupPoolRun,
		&tContext,
		NULL,
		UINT64_C(2000000)
	);
	pSecond = xrtTaskGroupSubmitUntil(
		tContext.Group,
		tContext.Pool,
		testTaskGroupPoolRun,
		&tContext,
		NULL,
		xrtDeadlineAfter(UINT64_C(2000000))
	);
	testRequire((pFirst != NULL) && (pSecond != NULL),
		"task group pool basic submit failed");
	testRequire(xrtTaskGroupWait(tContext.Group) == XWAIT_OK,
		"task group pool basic wait failed");
	testRequire(xrtTaskGroupGet(tContext.Group, &tStats) &&
		(tStats.Added == 2) && (tStats.Completed == 2) &&
		(tStats.Succeeded == 2) && (tContext.Hits == 2),
		"task group pool basic stats mismatch");
	xrtFutureDestroy(pSecond);
	xrtFutureDestroy(pFirst);
	xrtTaskGroupDestroy(tContext.Group);
	testRequire(xrtTaskPoolDestroy(tContext.Pool),
		"task group pool basic destroy failed");
	testTaskGroupPoolUnit(&tContext);
}



/* 验证满队列上的拒绝、超时和两类取消都完整回滚组预留。 */
static void testTaskGroupPoolBackpressure(void)
{
	testtaskgrouppool tContext;
	testthread tThread;
	xtaskpoolconfig tPoolConfig = { 1, 1, 0 };
	xtaskargs tArgs;
	xtaskgroupstats tStats;
	xfuture* pRunning;
	xfuture* pQueued;

	testTaskGroupPoolInit(&tContext);
	memset(&tArgs, 0, sizeof(tArgs));
	memset(&tStats, 0, sizeof(tStats));
	tArgs.Destroy = testTaskGroupPoolDestroy;
	tArgs.DestroyData = &tContext;
	tContext.Pool = xrtTaskPoolCreate(&tPoolConfig);
	tContext.Group = xrtTaskGroupCreate(NULL);
	testRequire((tContext.Pool != NULL) && (tContext.Group != NULL),
		"task group pool backpressure setup failed");
	pRunning = xrtTaskSubmit(
		tContext.Pool,
		testTaskGroupPoolBlock,
		&tContext,
		NULL
	);
	testRequire((pRunning != NULL) && testTaskGroupPoolWaitState(
		&tContext,
		0,
		true,
		UINT64_C(2000000)
	), "task group pool blocking task failed");
	pQueued = xrtTaskSubmit(
		tContext.Pool,
		testTaskGroupPoolRun,
		&tContext,
		NULL
	);
	testRequire(pQueued != NULL, "task group pool queue fill failed");

	testRequire(xrtTaskGroupSubmit(
		tContext.Group,
		tContext.Pool,
		testTaskGroupPoolRun,
		&tContext,
		&tArgs
	) == NULL, "group immediate submit exceeded pool limit");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_AGAIN,
		"group immediate submit error mismatch");
	testRequire(xrtTaskGroupSubmitFor(
		tContext.Group,
		tContext.Pool,
		testTaskGroupPoolRun,
		&tContext,
		&tArgs,
		UINT64_C(50000)
	) == NULL, "group capacity timeout accepted task");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_TIMEOUT,
		"group capacity timeout error mismatch");

	tContext.CallerCancel = xrtCancelCreate();
	tContext.UseCallerCancel = true;
	testRequire(tContext.CallerCancel != NULL,
		"group caller cancel create failed");
	testTaskGroupPoolStartBlocked(&tContext, &tThread);
	testRequire(xrtCancelRequest(tContext.CallerCancel),
		"group caller cancel request failed");
	testRequire(testTaskGroupPoolWaitState(
		&tContext,
		TEST_GROUP_POOL_DONE,
		false,
		UINT64_C(2000000)
	), "group caller cancel did not wake submitter");
	testThreadsJoin(&tThread, 1);
	testRequire((tContext.Submitted == NULL) &&
		(tContext.Error == XERR_CANCELLED),
		"group caller cancel result mismatch");
	xrtCancelDestroy(tContext.CallerCancel);
	tContext.CallerCancel = NULL;

	testTaskGroupPoolResetSubmit(&tContext);
	testTaskGroupPoolStartBlocked(&tContext, &tThread);
	testRequire(xrtTaskGroupCancel(tContext.Group),
		"group cancellation failed");
	testRequire(testTaskGroupPoolWaitState(
		&tContext,
		TEST_GROUP_POOL_DONE,
		false,
		UINT64_C(2000000)
	), "group cancellation did not wake capacity wait");
	testThreadsJoin(&tThread, 1);
	testRequire((tContext.Submitted == NULL) &&
		(tContext.Error == XERR_CANCELLED),
		"group cancellation result mismatch");
	testRequire(xrtTaskGroupWait(tContext.Group) == XWAIT_OK,
		"cancelled group retained capacity reservation");
	testRequire(xrtTaskGroupGet(tContext.Group, &tStats) &&
		(tStats.Active == 0) && (tStats.Added == 0) &&
		(tStats.Rejected == 4), "group rollback stats mismatch");
	testRequire(tContext.Destroyed == 0,
		"rejected grouped task data ownership was consumed");

	(void)xrtMutexLock(&tContext.Lock);
	tContext.Release = true;
	(void)xrtCondBroadcast(&tContext.Changed);
	(void)xrtMutexUnlock(&tContext.Lock);
	testRequire(xrtTaskPoolClose(tContext.Pool),
		"task group pool close failed");
	testRequire(xrtTaskPoolWaitFor(
		tContext.Pool,
		UINT64_C(2000000)
	) == XWAIT_OK, "task group pool drain failed");
	xrtFutureDestroy(pQueued);
	xrtFutureDestroy(pRunning);
	xrtTaskGroupDestroy(tContext.Group);
	testRequire(xrtTaskPoolDestroy(tContext.Pool),
		"task group pool destroy failed");
	testTaskGroupPoolUnit(&tContext);
}



/* 覆盖任务组与有界线程池之间的原子提交和组合背压契约。 */
int main(void)
{
	testTaskGroupPoolBasic();
	testTaskGroupPoolBackpressure();
	printf("[PASS] task group pool\n");
	return 0;
}

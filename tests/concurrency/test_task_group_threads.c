#include "../test.h"
#include "../test_thread.h"
#include "../test_thread_barrier.h"



/* 压力工作槽覆盖源完成、失败、接纳、关闭和取消五种动作。 */
typedef enum testtaskgroupaction {
	TEST_TASK_GROUP_RESOLVE = 1,
	TEST_TASK_GROUP_REJECT = 2,
	TEST_TASK_GROUP_ADD = 3,
	TEST_TASK_GROUP_CLOSE = 4,
	TEST_TASK_GROUP_CANCEL = 5
} testtaskgroupaction;



/* 每个工作槽在共享屏障放行后执行一个独立动作。 */
typedef struct testtaskgroupworker {
	testthreadbarrier* Barrier;
	testtaskgroupaction Action;
	xtaskgroup* Group;
	xfuture* Future;
	xpromise* Promise;
	const xerror* Error;
	bool Accepted;
} testtaskgroupworker;



/* 生命周期竞争槽让 Destroy 与源终态从同一屏障起跑。 */
typedef struct testtaskgrouplifetime {
	testthreadbarrier* Barrier;
	xtaskgroup* Group;
	xpromise* Promise;
	bool Destroy;
} testtaskgrouplifetime;



/* 启动窗口让关闭或取消发生在组槽位预留后、Future 提交前。 */
typedef struct testtaskgroupstartwindow {
	xtaskgroup* Group;
	xmutex Lock;
	xcond Changed;
	bool Entered;
	bool Release;
	xpromise* Promise;
	xfuture* Future;
	xfuture* Returned;
} testtaskgroupstartwindow;



/* 同步起跑后执行指定任务组竞争动作。 */
static int testTaskGroupWorker(ptr pData)
{
	testtaskgroupworker* pWorker = (testtaskgroupworker*)pData;

	if ( !testThreadBarrierWait(pWorker->Barrier) ) {
		return 1;
	}
	if ( pWorker->Action == TEST_TASK_GROUP_RESOLVE ) {
		return xrtPromiseResolve(pWorker->Promise, NULL) ? 0 : 2;
	}
	if ( pWorker->Action == TEST_TASK_GROUP_REJECT ) {
		return xrtPromiseReject(pWorker->Promise, pWorker->Error) ? 0 : 3;
	}
	if ( pWorker->Action == TEST_TASK_GROUP_ADD ) {
		pWorker->Accepted = xrtTaskGroupAdd(pWorker->Group, pWorker->Future);
		return 0;
	}
	if ( pWorker->Action == TEST_TASK_GROUP_CLOSE ) {
		(void)xrtTaskGroupClose(pWorker->Group);
		return 0;
	}
	(void)xrtTaskGroupCancel(pWorker->Group);
	return 0;
}



/* 执行一次任务组销毁或源完成动作。 */
static int testTaskGroupLifetimeWorker(ptr pData)
{
	testtaskgrouplifetime* pWorker =
		(testtaskgrouplifetime*)pData;

	if ( !testThreadBarrierWait(pWorker->Barrier) ) {
		return 1;
	}
	if ( pWorker->Destroy ) {
		xrtTaskGroupDestroy(pWorker->Group);
		return 0;
	}
	return xrtPromiseResolve(pWorker->Promise, NULL) ? 0 : 2;
}



/* 创建 Future 后阻塞启动器，使主线程能够精确命中预留窗口。 */
static xfuture* testTaskGroupStartWindowProc(ptr pData)
{
	testtaskgroupstartwindow* pWindow =
		(testtaskgroupstartwindow*)pData;

	pWindow->Promise = xrtPromiseCreate(&pWindow->Future, NULL);
	if ( pWindow->Promise == NULL ) {
		return NULL;
	}
	(void)xrtMutexLock(&pWindow->Lock);
	pWindow->Entered = true;
	(void)xrtCondBroadcast(&pWindow->Changed);
	while ( !pWindow->Release ) {
		(void)xrtCondWait(&pWindow->Changed, &pWindow->Lock);
	}
	(void)xrtMutexUnlock(&pWindow->Lock);
	return pWindow->Future;
}



/* 在独立线程中执行预留式 Future 启动。 */
static int testTaskGroupStartWindowThread(ptr pData)
{
	testtaskgroupstartwindow* pWindow =
		(testtaskgroupstartwindow*)pData;

	pWindow->Returned = xrtTaskGroupStart(
		pWindow->Group,
		testTaskGroupStartWindowProc,
		pWindow
	);
	return pWindow->Returned != NULL ? 0 : 1;
}



/* 等待启动器已经创建 Future 并进入预留窗口。 */
static bool testTaskGroupStartWindowWait(
	testtaskgroupstartwindow* pWindow
)
{
	xdeadline iDeadline = xrtDeadlineAfter(UINT64_C(2000000));
	bool bEntered;

	(void)xrtMutexLock(&pWindow->Lock);
	while ( !pWindow->Entered && !xrtDeadlineExpired(iDeadline) ) {
		(void)xrtCondWaitUntil(&pWindow->Changed, &pWindow->Lock, iDeadline);
	}
	bEntered = pWindow->Entered;
	(void)xrtMutexUnlock(&pWindow->Lock);
	return bEntered;
}



/* 放行启动器提交已经创建的 Future。 */
static void testTaskGroupStartWindowRelease(
	testtaskgroupstartwindow* pWindow
)
{
	(void)xrtMutexLock(&pWindow->Lock);
	pWindow->Release = true;
	(void)xrtCondBroadcast(&pWindow->Changed);
	(void)xrtMutexUnlock(&pWindow->Lock);
}



/* 验证 Close 和 Cancel 都覆盖尚未提交 Future 的预留窗口。 */
static void testTaskGroupStartWindow(bool bCancel)
{
	testtaskgroupstartwindow tWindow;
	testthread tThread;
	xtaskgroupstats tStats;
	xfuture* pDone;
	xcancel* pCancel;

	memset(&tWindow, 0, sizeof(tWindow));
	memset(&tThread, 0, sizeof(tThread));
	memset(&tStats, 0, sizeof(tStats));
	testRequire(xrtMutexInit(&tWindow.Lock),
		"task group start window lock init failed");
	testRequire(xrtCondInit(&tWindow.Changed),
		"task group start window cond init failed");
	tWindow.Group = xrtTaskGroupCreate(NULL);
	testRequire(tWindow.Group != NULL,
		"task group start window create failed");
	pDone = xrtTaskGroupFuture(tWindow.Group);
	testRequire(pDone != NULL, "task group start window Done failed");
	tThread.Proc = testTaskGroupStartWindowThread;
	tThread.Data = &tWindow;
	testThreadsStart(&tThread, 1);
	testRequire(testTaskGroupStartWindowWait(&tWindow),
		"task group start window did not open");
	testRequire(xrtTaskGroupGet(tWindow.Group, &tStats) &&
		(tStats.Active == 1) && (tStats.Added == 0),
		"task group reservation was not visible as active");
	if ( bCancel ) {
		testRequire(xrtTaskGroupCancel(tWindow.Group),
			"task group start window cancel failed");
	} else {
		testRequire(xrtTaskGroupClose(tWindow.Group),
			"task group start window close failed");
	}
	testRequire(xrtFutureState(pDone) == XFUTURE_PENDING,
		"task group completed while Future start was reserved");
	pCancel = xrtFutureCancelToken(tWindow.Future);
	testRequire(pCancel != NULL, "started Future cancel token failed");
	if ( !bCancel ) {
		testRequire(!xrtCancelRequested(pCancel),
			"Close cancelled a reserved Future");
	}

	testTaskGroupStartWindowRelease(&tWindow);
	testThreadsJoin(&tThread, 1);
	testRequire((tThread.Result == 0) &&
		(tWindow.Returned == tWindow.Future),
		"reserved Future commit failed");
	if ( bCancel ) {
		testRequire(xrtCancelRequested(pCancel),
			"Cancel was not replayed after Future commit");
		testRequire(xrtPromiseCancel(tWindow.Promise),
			"cancelled reserved Future completion failed");
	} else {
		testRequire(!xrtCancelRequested(pCancel),
			"Close cancelled committed Future");
		testRequire(xrtPromiseResolve(tWindow.Promise, NULL),
			"closed reserved Future completion failed");
	}
	testRequire(xrtFutureWaitFor(pDone, UINT64_C(2000000)) == XWAIT_OK,
		"task group start window did not complete");
	testRequire(xrtTaskGroupGet(tWindow.Group, &tStats) &&
		(tStats.Active == 0) && (tStats.Added == 1) &&
		(tStats.Completed == 1), "start window stats mismatch");

	xrtCancelDestroy(pCancel);
	xrtPromiseDestroy(tWindow.Promise);
	xrtFutureDestroy(tWindow.Future);
	xrtFutureDestroy(pDone);
	xrtTaskGroupDestroy(tWindow.Group);
	testRequire(xrtCondUnit(&tWindow.Changed),
		"task group start window cond unit failed");
	testRequire(xrtMutexUnit(&tWindow.Lock),
		"task group start window lock unit failed");
}



/* 竞争失败、成功和显式取消，验证每个源只统计一次。 */
static void testTaskGroupCompletionStress(void)
{
	for ( size_t iRound = 0; iRound < 50; iRound++ ) {
		testthreadbarrier tBarrier;
		testtaskgroupworker arrWorker[5];
		testthread arrThread[5];
		xtaskgroupconfig tConfig = {
			NULL,
			0,
			XRT_TASK_GROUP_CANCEL_ON_FAILED
		};
		xtaskgroup* pGroup = xrtTaskGroupCreate(&tConfig);
		xfuture* arrFuture[4];
		xpromise* arrPromise[4];
		xerror* pError = xrtErrorCreate(
			XERR_PROTOCOL,
			"test.task.group.stress",
			37,
			"concurrent failure"
		);
		xtaskgroupstats tStats;

		testRequire((pGroup != NULL) && (pError != NULL),
			"task group completion stress setup failed");
		for ( size_t i = 0; i < 4; i++ ) {
			arrPromise[i] = xrtPromiseCreate(&arrFuture[i], NULL);
			testRequire((arrPromise[i] != NULL) &&
				xrtTaskGroupAdd(pGroup, arrFuture[i]),
				"task group completion stress add failed");
		}
		testThreadBarrierInit(&tBarrier, 5);
		memset(arrWorker, 0, sizeof(arrWorker));
		memset(arrThread, 0, sizeof(arrThread));
		for ( size_t i = 0; i < 4; i++ ) {
			arrWorker[i].Barrier = &tBarrier;
			arrWorker[i].Action = i == 0 ?
				TEST_TASK_GROUP_REJECT : TEST_TASK_GROUP_RESOLVE;
			arrWorker[i].Promise = arrPromise[i];
			arrWorker[i].Error = pError;
			arrThread[i].Proc = testTaskGroupWorker;
			arrThread[i].Data = &arrWorker[i];
		}
		arrWorker[4].Barrier = &tBarrier;
		arrWorker[4].Action = TEST_TASK_GROUP_CANCEL;
		arrWorker[4].Group = pGroup;
		arrThread[4].Proc = testTaskGroupWorker;
		arrThread[4].Data = &arrWorker[4];
		testThreadsStart(arrThread, 5);
		testThreadBarrierOpen(&tBarrier);
		testThreadsJoin(arrThread, 5);
		for ( size_t i = 0; i < 5; i++ ) {
			testRequire(arrThread[i].Result == 0,
				"task group completion stress worker failed");
		}
		testRequire(xrtTaskGroupWaitFor(
			pGroup,
			UINT64_C(2000000)
		) == XWAIT_OK, "task group completion stress wait failed");
		testRequire(xrtTaskGroupGet(pGroup, &tStats) &&
			(tStats.Added == 4) && (tStats.Completed == 4) &&
			(tStats.Failed == 1) && (tStats.Succeeded == 3) &&
			(tStats.Active == 0) && tStats.Cancelling,
			"task group completion stress stats corrupted");

		testThreadBarrierUnit(&tBarrier);
		xrtErrorFree(pError);
		for ( size_t i = 0; i < 4; i++ ) {
			xrtPromiseDestroy(arrPromise[i]);
			xrtFutureDestroy(arrFuture[i]);
		}
		xrtTaskGroupDestroy(pGroup);
	}
}



/* 竞争接纳和关闭，验证每个尝试只落入 Added 或 Rejected 一侧。 */
static void testTaskGroupCloseStress(void)
{
	for ( size_t iRound = 0; iRound < 50; iRound++ ) {
		testthreadbarrier tBarrier;
		testtaskgroupworker arrWorker[5];
		testthread arrThread[5];
		xtaskgroup* pGroup = xrtTaskGroupCreate(NULL);
		xfuture* arrFuture[4];
		xpromise* arrPromise[4];
		xtaskgroupstats tStats;
		size_t iAccepted = 0;

		testRequire(pGroup != NULL, "task group close stress create failed");
		testThreadBarrierInit(&tBarrier, 5);
		memset(arrWorker, 0, sizeof(arrWorker));
		memset(arrThread, 0, sizeof(arrThread));
		for ( size_t i = 0; i < 4; i++ ) {
			arrPromise[i] = xrtPromiseCreate(&arrFuture[i], NULL);
			testRequire(arrPromise[i] != NULL,
				"task group close stress source create failed");
			arrWorker[i].Barrier = &tBarrier;
			arrWorker[i].Action = TEST_TASK_GROUP_ADD;
			arrWorker[i].Group = pGroup;
			arrWorker[i].Future = arrFuture[i];
			arrThread[i].Proc = testTaskGroupWorker;
			arrThread[i].Data = &arrWorker[i];
		}
		arrWorker[4].Barrier = &tBarrier;
		arrWorker[4].Action = TEST_TASK_GROUP_CLOSE;
		arrWorker[4].Group = pGroup;
		arrThread[4].Proc = testTaskGroupWorker;
		arrThread[4].Data = &arrWorker[4];
		testThreadsStart(arrThread, 5);
		testThreadBarrierOpen(&tBarrier);
		testThreadsJoin(arrThread, 5);
		for ( size_t i = 0; i < 4; i++ ) {
			iAccepted += arrWorker[i].Accepted ? 1 : 0;
			testRequire(xrtPromiseResolve(arrPromise[i], NULL),
				"task group close stress source resolve failed");
		}
		testRequire(xrtTaskGroupWaitFor(
			pGroup,
			UINT64_C(2000000)
		) == XWAIT_OK, "task group close stress wait failed");
		testRequire(xrtTaskGroupGet(pGroup, &tStats) &&
			(tStats.Added == iAccepted) &&
			(tStats.Completed == iAccepted) &&
			((tStats.Added + tStats.Rejected) == 4),
			"task group close stress accounting mismatch");

		testThreadBarrierUnit(&tBarrier);
		for ( size_t i = 0; i < 4; i++ ) {
			xrtPromiseDestroy(arrPromise[i]);
			xrtFutureDestroy(arrFuture[i]);
		}
		xrtTaskGroupDestroy(pGroup);
	}
}



/* 竞争提前 Destroy 与源完成，验证 Done Future 独立保活公共状态。 */
static void testTaskGroupDestroyStress(void)
{
	for ( size_t iRound = 0; iRound < 100; iRound++ ) {
		testthreadbarrier tBarrier;
		testtaskgrouplifetime arrWorker[2];
		testthread arrThread[2];
		xtaskgroup* pGroup = xrtTaskGroupCreate(NULL);
		xfuture* pSource;
		xfuture* pDone;
		xpromise* pPromise = xrtPromiseCreate(&pSource, NULL);

		testRequire((pGroup != NULL) && (pPromise != NULL),
			"task group destroy stress setup failed");
		testRequire(xrtTaskGroupAdd(pGroup, pSource),
			"task group destroy stress add failed");
		pDone = xrtTaskGroupFuture(pGroup);
		testRequire(pDone != NULL,
			"task group destroy stress Done future failed");
		testThreadBarrierInit(&tBarrier, 2);
		memset(arrWorker, 0, sizeof(arrWorker));
		memset(arrThread, 0, sizeof(arrThread));
		arrWorker[0].Barrier = &tBarrier;
		arrWorker[0].Group = pGroup;
		arrWorker[0].Destroy = true;
		arrWorker[1].Barrier = &tBarrier;
		arrWorker[1].Promise = pPromise;
		for ( size_t i = 0; i < 2; i++ ) {
			arrThread[i].Proc = testTaskGroupLifetimeWorker;
			arrThread[i].Data = &arrWorker[i];
		}
		testThreadsStart(arrThread, 2);
		testThreadBarrierOpen(&tBarrier);
		testThreadsJoin(arrThread, 2);
		testRequire((arrThread[0].Result == 0) &&
			(arrThread[1].Result == 0),
			"task group destroy stress worker failed");
		testRequire(xrtFutureWaitFor(
			pDone,
			UINT64_C(2000000)
		) == XWAIT_OK, "task group destroy stress Done wait failed");

		testThreadBarrierUnit(&tBarrier);
		xrtFutureDestroy(pDone);
		xrtPromiseDestroy(pPromise);
		xrtFutureDestroy(pSource);
	}
}



/* 覆盖任务组完成、取消、接纳和关闭之间的高频竞争。 */
int main(void)
{
	testTaskGroupStartWindow(false);
	testTaskGroupStartWindow(true);
	testTaskGroupCompletionStress();
	testTaskGroupCloseStress();
	testTaskGroupDestroyStress();
	printf("[PASS] task group threads\n");
	return 0;
}

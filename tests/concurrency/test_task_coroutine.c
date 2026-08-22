#include "../test.h"
#include "../test_thread.h"



/* 协程任务上下文记录过程、取消等待和数据析构。 */
typedef struct testtaskco {
	int Value;
	int Hits;
	int Destroyed;
	xwaitresult Wait;
	uint64 RunThreadId;
} testtaskco;



/* 受理后的协程任务数据恰好析构一次。 */
static void testTaskCoDestroy(ptr pValue, ptr pData)
{
	testtaskco* pContext = (testtaskco*)pData;

	(void)pValue;
	pContext->Destroyed++;
}



/* 正常协程任务经过调度睡眠后返回借用值。 */
static xtaskoutcome testTaskCoSuccess(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtaskco* pContext = (testtaskco*)pData;

	(void)pCancel;
	pContext->Hits++;
	pContext->RunThreadId = xrtThreadCurrentId();
	pContext->Wait = xrtCoSleep(1000);
	pResult->Value = &pContext->Value;
	return pContext->Wait == XWAIT_OK ? XTASK_SUCCESS : XTASK_FAILED;
}



/* 任务过程可处理取消并用显式结果决定最终 Future 仍然成功。 */
static xtaskoutcome testTaskCoHandleCancel(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtaskco* pContext = (testtaskco*)pData;

	(void)pCancel;
	pContext->Hits++;
	pContext->Wait = xrtCoPark();
	pResult->Value = &pContext->Value;
	return XTASK_SUCCESS;
}



/* 预取消协程不能进入任务用户过程。 */
static xtaskoutcome testTaskCoMustNotRun(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testtaskco* pContext = (testtaskco*)pData;

	(void)pCancel;
	(void)pResult;
	pContext->Hits++;
	return XTASK_SUCCESS;
}



/* 外部线程提交状态把调度器、任务参数和返回 Future 传回所属线程。 */
typedef struct testtaskcopost {
	xcosched* Sched;
	testtaskco* Context;
	xtaskargs Args;
	xfuture* Future;
} testtaskcopost;



/* 外部线程只执行线程安全提交，不参与协程调度。 */
static int testTaskCoSubmitThread(ptr pData)
{
	testtaskcopost* pPost = (testtaskcopost*)pData;

	pPost->Future = xrtTaskCo(
		pPost->Sched,
		testTaskCoSuccess,
		pPost->Context,
		&pPost->Args,
		0
	);
	return pPost->Future != NULL ? 0 : 1;
}



/* 验证跨线程提交、关闭竞态和拒绝时的数据所有权。 */
static void testTaskCoPost(void)
{
	testtaskco tCross;
	testtaskco tClosing;
	testtaskco tRejected;
	testtaskcopost tPost;
	testthread tThread;
	xtaskargs tArgs;
	xcosched* pSched;
	xfuture* pFuture;
	uint64 iOwnerThreadId = xrtThreadCurrentId();

	memset(&tCross, 0, sizeof(tCross));
	memset(&tClosing, 0, sizeof(tClosing));
	memset(&tRejected, 0, sizeof(tRejected));
	memset(&tPost, 0, sizeof(tPost));
	memset(&tArgs, 0, sizeof(tArgs));
	tCross.Value = 201;
	tClosing.Value = 202;
	tArgs.Destroy = testTaskCoDestroy;

	/* 外部线程只负责受理，任务过程仍在调度器所属线程执行。 */
	pSched = xrtCoSchedCreate();
	testRequire(pSched != NULL, "cross-thread task scheduler create failed");
	tPost.Sched = pSched;
	tPost.Context = &tCross;
	tPost.Args = tArgs;
	tPost.Args.DestroyData = &tCross;
	tThread.Proc = testTaskCoSubmitThread;
	tThread.Data = &tPost;
	testThreadsStart(&tThread, 1);
	testThreadsJoin(&tThread, 1);
	testRequire(tPost.Future != NULL, "cross-thread coroutine task submit failed");
	testRequire(xrtCoSchedRun(pSched), "cross-thread coroutine task run failed");
	testRequire((xrtFutureState(tPost.Future) == XFUTURE_RESOLVED) &&
		(tCross.Hits == 1) && (tCross.Destroyed == 1) &&
		(tCross.RunThreadId == iOwnerThreadId),
		"cross-thread coroutine task execution mismatch");
	xrtFutureDestroy(tPost.Future);
	testRequire(xrtCoSchedDestroy(pSched),
		"cross-thread task scheduler destroy failed");

	/* 已受理任务若在创建协程前遇到关闭，以 CLOSED 错误完成其 Future。 */
	pSched = xrtCoSchedCreate();
	testRequire(pSched != NULL, "closing task scheduler create failed");
	memset(&tPost, 0, sizeof(tPost));
	tPost.Sched = pSched;
	tPost.Context = &tClosing;
	tPost.Args = tArgs;
	tPost.Args.DestroyData = &tClosing;
	tThread.Proc = testTaskCoSubmitThread;
	tThread.Data = &tPost;
	testThreadsStart(&tThread, 1);
	testThreadsJoin(&tThread, 1);
	pFuture = tPost.Future;
	testRequire(pFuture != NULL, "pre-close coroutine task submit failed");
	testRequire(xrtCoSchedClose(pSched), "closing task scheduler close failed");
	testRequire(xrtCoSchedRun(pSched), "closing task scheduler drain failed");
	testRequire((xrtFutureState(pFuture) == XFUTURE_FAILED) &&
		(xrtErrorKind(xrtFutureError(pFuture)) == XERR_CLOSED) &&
		(tClosing.Hits == 0) && (tClosing.Destroyed == 1),
		"pre-close coroutine task terminal mismatch");
	xrtFutureDestroy(pFuture);

	/* 关闭后提交直接拒绝，任务数据仍由调用方持有。 */
	tArgs.DestroyData = &tRejected;
	xrtClearError();
	testRequire(xrtTaskCo(pSched, testTaskCoSuccess, &tRejected, &tArgs, 0) == NULL,
		"closed scheduler accepted coroutine task");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_CLOSED) &&
		(tRejected.Destroyed == 0), "closed task submit ownership mismatch");
	testRequire(xrtCoSchedDestroy(pSched), "closing task scheduler destroy failed");
}



/* 验证协程任务的正常、预取消、运行取消和拒绝所有权。 */
int main(void)
{
	testtaskco tSuccess;
	testtaskco tPreCancelled;
	testtaskco tHandled;
	testtaskco tRejected;
	xtaskargs tArgs;
	xcosched* pSched;
	xfuture* pFuture;

	memset(&tSuccess, 0, sizeof(tSuccess));
	memset(&tPreCancelled, 0, sizeof(tPreCancelled));
	memset(&tHandled, 0, sizeof(tHandled));
	memset(&tRejected, 0, sizeof(tRejected));
	memset(&tArgs, 0, sizeof(tArgs));
	tSuccess.Value = 101;
	tPreCancelled.Value = 102;
	tHandled.Value = 103;
	tArgs.Destroy = testTaskCoDestroy;

	/* 正常任务由分离协程执行，Future 在调度器销毁后仍可独立读取。 */
	pSched = xrtCoSchedCreate();
	testRequire(pSched != NULL, "task coroutine scheduler create failed");
	tArgs.DestroyData = &tSuccess;
	pFuture = xrtTaskCo(pSched, testTaskCoSuccess, &tSuccess, &tArgs, 0);
	testRequire(pFuture != NULL, "task coroutine submit failed");
	testRequire(xrtCoSchedRun(pSched), "task coroutine scheduler run failed");
	testRequire(
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED) &&
		(xrtFutureValue(pFuture) == &tSuccess.Value) &&
		(tSuccess.Hits == 1) && (tSuccess.Destroyed == 1),
		"task coroutine success mismatch"
	);
	testRequire(xrtCoSchedDestroy(pSched), "task coroutine scheduler destroy failed");
	xrtFutureDestroy(pFuture);

	/* Future 在首次调度前取消时，终结回调仍完成 Promise 和数据析构。 */
	pSched = xrtCoSchedCreate();
	testRequire(pSched != NULL, "pre-cancel task coroutine scheduler create failed");
	tArgs.DestroyData = &tPreCancelled;
	pFuture = xrtTaskCo(
		pSched,
		testTaskCoMustNotRun,
		&tPreCancelled,
		&tArgs,
		0
	);
	testRequire(pFuture != NULL, "pre-cancel task coroutine submit failed");
	testRequire(xrtFutureCancel(pFuture), "pre-cancel task future request failed");
	testRequire(xrtCoSchedRun(pSched), "pre-cancel task scheduler run failed");
	testRequire(
		(xrtFutureState(pFuture) == XFUTURE_CANCELLED) &&
		(tPreCancelled.Hits == 0) && (tPreCancelled.Destroyed == 1),
		"pre-cancel task coroutine lifecycle mismatch"
	);
	xrtFutureDestroy(pFuture);
	testRequire(xrtCoSchedDestroy(pSched), "pre-cancel task scheduler destroy failed");

	/* 运行中的取消先唤醒协程，任务显式 SUCCESS 仍是最终契约。 */
	pSched = xrtCoSchedCreate();
	testRequire(pSched != NULL, "handled cancel scheduler create failed");
	tArgs.DestroyData = &tHandled;
	pFuture = xrtTaskCo(pSched, testTaskCoHandleCancel, &tHandled, &tArgs, 0);
	testRequire(pFuture != NULL, "handled cancel task submit failed");
	testRequire(xrtCoSchedStep(pSched) == XWAIT_OK, "handled cancel task did not park");
	testRequire(xrtFutureState(pFuture) == XFUTURE_PENDING,
		"parked task future completed early");
	testRequire(xrtFutureCancel(pFuture), "parked task cancel request failed");
	testRequire(xrtCoSchedRun(pSched), "handled cancel scheduler run failed");
	testRequire(
		(tHandled.Wait == XWAIT_CANCELLED) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED) &&
		(xrtFutureValue(pFuture) == &tHandled.Value) &&
		(tHandled.Destroyed == 1),
		"handled cancellation did not preserve explicit task outcome"
	);
	xrtFutureDestroy(pFuture);
	testRequire(xrtCoSchedDestroy(pSched), "handled cancel scheduler destroy failed");

	/* 没有指定或当前调度器时，提交失败且数据所有权留给调用方。 */
	tArgs.DestroyData = &tRejected;
	testRequire(xrtTaskCo(NULL, testTaskCoSuccess, &tRejected, &tArgs, 0) == NULL,
		"task coroutine without scheduler succeeded");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE,
		"task coroutine missing scheduler error mismatch");
	testRequire(tRejected.Destroyed == 0,
		"rejected task coroutine consumed caller data");
	testTaskCoPost();
	testRequire(xrtCoThreadDetach(), "task coroutine runtime detach failed");

	printf("[PASS] task coroutine\n");
	return 0;
}

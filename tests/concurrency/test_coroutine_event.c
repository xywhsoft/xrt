#include "../test.h"
#include "../test_thread.h"



/* 单个事件等待用例记录进入、终态和自动复位 FIFO 次序。 */
typedef struct testcoeventwait {
	xcoevent* Event;
	int Index;
	int* Order;
	int* Completed;
	volatile int Entered;
	uint64 Timeout;
	xwaitresult Result;
} testcoeventwait;



/* 按配置永久等待或有限等待，并在成功后记录恢复次序。 */
static ptr testCoEventWaitProc(ptr pData)
{
	testcoeventwait* pWait = (testcoeventwait*)pData;

	pWait->Entered = 1;
	pWait->Result = pWait->Timeout == UINT64_MAX ?
		xrtCoEventAwait(pWait->Event) :
		xrtCoEventAwaitFor(pWait->Event, pWait->Timeout);
	if ( pWait->Result == XWAIT_OK ) {
		pWait->Order[*pWait->Completed] = pWait->Index;
		(*pWait->Completed)++;
	}
	return pWait;
}



/* 快速路径用例连续检查自动复位和手动复位语义。 */
typedef struct testcoeventfast {
	xcoevent* Auto;
	xcoevent* Manual;
	xwaitresult AutoFirst;
	xwaitresult AutoSecond;
	xwaitresult AutoSet;
	xwaitresult ManualFirst;
	xwaitresult ManualSecond;
	xwaitresult ManualReset;
} testcoeventfast;



/* 在当前协程中消费或保留不同复位模式的信号。 */
static ptr testCoEventFastProc(ptr pData)
{
	testcoeventfast* pFast = (testcoeventfast*)pData;

	pFast->AutoFirst = xrtCoEventTryAwait(pFast->Auto);
	pFast->AutoSecond = xrtCoEventTryAwait(pFast->Auto);
	testRequire(xrtCoEventSet(pFast->Auto), "auto event set in coroutine failed");
	pFast->AutoSet = xrtCoEventTryAwait(pFast->Auto);
	pFast->ManualFirst = xrtCoEventTryAwait(pFast->Manual);
	pFast->ManualSecond = xrtCoEventTryAwait(pFast->Manual);
	testRequire(xrtCoEventReset(pFast->Manual), "manual event reset failed");
	pFast->ManualReset = xrtCoEventTryAwait(pFast->Manual);
	return pFast;
}



/* 外部线程只执行线程安全的事件置位。 */
typedef struct testcoeventset {
	xcoevent* Event;
	bool Result;
} testcoeventset;



/* 跨线程置位事件。 */
static int testCoEventSetThread(ptr pData)
{
	testcoeventset* pSet = (testcoeventset*)pData;

	pSet->Result = xrtCoEventSet(pSet->Event);
	return pSet->Result ? 0 : 1;
}



/* 终结器限制用例验证 Await 不会消费已经存在的信号。 */
typedef struct testcoeventfinal {
	xcoevent* Event;
	xwaitresult Result;
	xerrkind Error;
} testcoeventfinal;



/* 空过程立即进入终结器。 */
static ptr testCoEventFinalRun(ptr pData)
{
	return pData;
}



/* 终结器中的 Await 必须在检查信号前被拒绝。 */
static void testCoEventFinalProc(
	xcoroterm Term,
	ptr pResult,
	const xerror* pError,
	ptr pData
)
{
	testcoeventfinal* pFinal = (testcoeventfinal*)pData;

	(void)Term;
	(void)pResult;
	(void)pError;
	xrtClearError();
	pFinal->Result = xrtCoEventTryAwait(pFinal->Event);
	pFinal->Error = xrtErrorKind(xrtGetError());
	xrtClearError();
}



/* 启动一组永久等待者并保证它们已经挂入事件队列。 */
static void testCoEventStartWaiters(
	xcosched* pSched,
	xcoevent* pEvent,
	testcoeventwait* pWaits,
	xcoro** pCoroutines,
	size_t iCount,
	int* pOrder,
	int* pCompleted
)
{
	for ( size_t i = 0; i < iCount; i++ ) {
		memset(&pWaits[i], 0, sizeof(testcoeventwait));
		pWaits[i].Event = pEvent;
		pWaits[i].Index = (int)i + 1;
		pWaits[i].Order = pOrder;
		pWaits[i].Completed = pCompleted;
		pWaits[i].Timeout = UINT64_MAX;
		pWaits[i].Result = XWAIT_ERROR;
		pCoroutines[i] = xrtCoSpawn(
			pSched,
			testCoEventWaitProc,
			&pWaits[i],
			NULL
		);
		testRequire(pCoroutines[i] != NULL, "event waiter spawn failed");
		testRequire(
			xrtCoSchedStep(pSched) == XWAIT_OK,
			"event waiter entry step failed"
		);
		testRequire(
			pWaits[i].Entered && (xrtCoState(pCoroutines[i]) == XCORO_SUSPENDED),
			"event waiter did not suspend"
		);
	}
}



/* 销毁一组已经完成的保留句柄协程。 */
static void testCoEventDestroyWaiters(
	xcoro** pCoroutines,
	size_t iCount
)
{
	for ( size_t i = 0; i < iCount; i++ ) {
		testRequire(
			xrtCoDestroy(pCoroutines[i]),
			"event waiter coroutine destroy failed"
		);
	}
}



/* 验证信号快速路径和基础对象生命周期。 */
static void testCoroutineEventBasic(void)
{
	testcoeventfast tFast;
	xcoevent tAuto;
	xcoevent tManual;
	xcoevent tInvalid;
	xcosched* pSched;
	xcoro* pCo;

	memset(&tFast, 0, sizeof(tFast));
	memset(&tInvalid, 0, sizeof(tInvalid));
	testRequire(xrtCoEventInit(&tAuto, false, true), "auto event init failed");
	testRequire(xrtCoEventInit(&tManual, true, true), "manual event init failed");
	pSched = xrtCoSchedCreate();
	testRequire(pSched != NULL, "event basic scheduler create failed");
	tFast.Auto = &tAuto;
	tFast.Manual = &tManual;
	pCo = xrtCoSpawn(pSched, testCoEventFastProc, &tFast, NULL);
	testRequire(pCo != NULL, "event fast coroutine spawn failed");
	testRequire(xrtCoSchedRun(pSched), "event fast scheduler run failed");
	testRequire(
		(tFast.AutoFirst == XWAIT_OK) &&
		(tFast.AutoSecond == XWAIT_TIMEOUT) &&
		(tFast.AutoSet == XWAIT_OK),
		"auto reset event fast-path mismatch"
	);
	testRequire(
		(tFast.ManualFirst == XWAIT_OK) &&
		(tFast.ManualSecond == XWAIT_OK) &&
		(tFast.ManualReset == XWAIT_TIMEOUT),
		"manual reset event fast-path mismatch"
	);
	testRequire(xrtCoDestroy(pCo), "event fast coroutine destroy failed");
	testRequire(xrtCoSchedDestroy(pSched), "event basic scheduler destroy failed");
	testRequire(xrtCoEventUnit(&tAuto), "auto event unit failed");
	testRequire(xrtCoEventUnit(&tManual), "manual event unit failed");

	xrtClearError();
	testRequire(!xrtCoEventUnit(&tInvalid), "uninitialized event unit succeeded");
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"uninitialized event error mismatch"
	);
	testRequire(xrtCoEventDestroy(NULL), "null event destroy failed");
	xrtClearError();
}



/* 验证手动复位广播、跨线程置位和活动等待销毁保护。 */
static void testCoroutineEventManual(void)
{
	testcoeventwait tWaits[3];
	testcoeventset tSet;
	testthread tThread;
	xcoevent tEvent;
	xcosched* pSched;
	xcoro* pCoroutines[3];
	int arrOrder[3] = { 0 };
	int iCompleted = 0;

	memset(&tSet, 0, sizeof(tSet));
	memset(&tThread, 0, sizeof(tThread));
	testRequire(xrtCoEventInit(&tEvent, true, false), "manual event init failed");
	pSched = xrtCoSchedCreate();
	testRequire(pSched != NULL, "manual event scheduler create failed");
	testCoEventStartWaiters(
		pSched,
		&tEvent,
		tWaits,
		pCoroutines,
		3,
		arrOrder,
		&iCompleted
	);
	xrtClearError();
	testRequire(!xrtCoEventUnit(&tEvent), "event with linked waiters was released");
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"linked waiter event unit error mismatch"
	);

	tSet.Event = &tEvent;
	tThread.Proc = testCoEventSetThread;
	tThread.Data = &tSet;
	testThreadsStart(&tThread, 1);
	testThreadsJoin(&tThread, 1);
	testRequire(tSet.Result, "cross-thread manual event set failed");
	xrtClearError();
	testRequire(
		!xrtCoEventUnit(&tEvent),
		"event with signaled but unreturned waiters was released"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"signaled waiter event unit error mismatch"
	);
	testRequire(xrtCoSchedRun(pSched), "manual event scheduler run failed");
	testRequire(iCompleted == 3, "manual event did not wake all waiters");
	for ( size_t i = 0; i < 3u; i++ ) {
		testRequire(tWaits[i].Result == XWAIT_OK, "manual event waiter failed");
	}
	testCoEventDestroyWaiters(pCoroutines, 3);
	testRequire(xrtCoSchedDestroy(pSched), "manual event scheduler destroy failed");
	testRequire(xrtCoEventUnit(&tEvent), "manual event final unit failed");
}



/* 验证自动复位严格单唤醒和 FIFO 顺序。 */
static void testCoroutineEventAuto(void)
{
	testcoeventwait tWaits[3];
	xcoevent tEvent;
	xcosched* pSched;
	xcoro* pCoroutines[3];
	int arrOrder[3] = { 0 };
	int iCompleted = 0;

	testRequire(xrtCoEventInit(&tEvent, false, false), "auto event init failed");
	pSched = xrtCoSchedCreate();
	testRequire(pSched != NULL, "auto event scheduler create failed");
	testCoEventStartWaiters(
		pSched,
		&tEvent,
		tWaits,
		pCoroutines,
		3,
		arrOrder,
		&iCompleted
	);
	testRequire(xrtCoEventSet(&tEvent), "first auto event set failed");
	testRequire(xrtCoSchedStep(pSched) == XWAIT_OK, "first auto waiter step failed");
	testRequire(
		(iCompleted == 1) && (arrOrder[0] == 1),
		"auto event did not wake exactly the first waiter"
	);
	testRequire(xrtCoEventSet(&tEvent), "second auto event set failed");
	testRequire(xrtCoEventSet(&tEvent), "third auto event set failed");
	testRequire(xrtCoSchedRun(pSched), "auto event scheduler run failed");
	testRequire(
		(iCompleted == 3) &&
		(arrOrder[1] == 2) &&
		(arrOrder[2] == 3),
		"auto event FIFO order mismatch"
	);
	testCoEventDestroyWaiters(pCoroutines, 3);
	testRequire(xrtCoSchedDestroy(pSched), "auto event scheduler destroy failed");
	testRequire(xrtCoEventUnit(&tEvent), "auto event unit failed");
}



/* 验证通用 Wake 不会伪造事件信号，并覆盖超时和取消。 */
static void testCoroutineEventOutcomes(void)
{
	testcoeventwait tWait;
	xcoevent tEvent;
	xcosched* pSched;
	xcoro* pCo;
	int arrOrder[1] = { 0 };
	int iCompleted = 0;

	testRequire(xrtCoEventInit(&tEvent, false, false), "outcome event init failed");
	pSched = xrtCoSchedCreate();
	testRequire(pSched != NULL, "outcome scheduler create failed");
	testCoEventStartWaiters(
		pSched,
		&tEvent,
		&tWait,
		&pCo,
		1,
		arrOrder,
		&iCompleted
	);
	testRequire(xrtCoWake(pCo), "event generic wake failed");
	testRequire(xrtCoSchedStep(pSched) == XWAIT_OK, "event generic wake step failed");
	testRequire(
		(iCompleted == 0) && (xrtCoState(pCo) == XCORO_SUSPENDED),
		"generic wake was mistaken for an event signal"
	);
	testRequire(xrtCoEventSet(&tEvent), "event set after generic wake failed");
	testRequire(xrtCoSchedRun(pSched), "event wake recovery run failed");
	testRequire(tWait.Result == XWAIT_OK, "event wake recovery result mismatch");
	testRequire(xrtCoDestroy(pCo), "event wake recovery destroy failed");

	memset(&tWait, 0, sizeof(tWait));
	tWait.Event = &tEvent;
	tWait.Order = arrOrder;
	tWait.Completed = &iCompleted;
	tWait.Timeout = 1000;
	tWait.Result = XWAIT_ERROR;
	pCo = xrtCoSpawn(pSched, testCoEventWaitProc, &tWait, NULL);
	testRequire(pCo != NULL, "timeout event waiter spawn failed");
	testRequire(xrtCoSchedRun(pSched), "timeout event scheduler run failed");
	testRequire(tWait.Result == XWAIT_TIMEOUT, "event timeout result mismatch");
	testRequire(xrtCoDestroy(pCo), "timeout event waiter destroy failed");

	memset(&tWait, 0, sizeof(tWait));
	tWait.Event = &tEvent;
	tWait.Order = arrOrder;
	tWait.Completed = &iCompleted;
	tWait.Timeout = UINT64_MAX;
	tWait.Result = XWAIT_ERROR;
	pCo = xrtCoSpawn(pSched, testCoEventWaitProc, &tWait, NULL);
	testRequire(pCo != NULL, "cancel event waiter spawn failed");
	testRequire(xrtCoSchedStep(pSched) == XWAIT_OK, "cancel event waiter entry failed");
	testRequire(xrtCoCancel(pCo), "cancel event waiter request failed");
	testRequire(xrtCoSchedRun(pSched), "cancel event scheduler run failed");
	testRequire(tWait.Result == XWAIT_CANCELLED, "event cancellation result mismatch");
	testRequire(xrtCoDestroy(pCo), "cancel event waiter destroy failed");

	testRequire(xrtCoSchedDestroy(pSched), "outcome scheduler destroy failed");
	testRequire(xrtCoEventUnit(&tEvent), "outcome event unit failed");
}



/* 验证终结器不能通过事件快速路径绕过不可挂起契约。 */
static void testCoroutineEventFinalizer(void)
{
	testcoeventfinal tFinal;
	testcoeventwait tWait;
	xcoroargs tArgs;
	xcoevent tEvent;
	xcosched* pSched;
	xcoro* pFinalCo;
	xcoro* pWaitCo;
	int arrOrder[1] = { 0 };
	int iCompleted = 0;

	memset(&tFinal, 0, sizeof(tFinal));
	memset(&tWait, 0, sizeof(tWait));
	memset(&tArgs, 0, sizeof(tArgs));
	testRequire(xrtCoEventInit(&tEvent, false, true), "finalizer event init failed");
	pSched = xrtCoSchedCreate();
	testRequire(pSched != NULL, "finalizer event scheduler create failed");
	tFinal.Event = &tEvent;
	tArgs.Finalize = testCoEventFinalProc;
	tArgs.FinalizeData = &tFinal;
	pFinalCo = xrtCoSpawn(
		pSched,
		testCoEventFinalRun,
		&tFinal,
		&tArgs
	);
	testRequire(pFinalCo != NULL, "event finalizer coroutine spawn failed");
	testRequire(xrtCoSchedRun(pSched), "event finalizer scheduler run failed");
	testRequire(
		(tFinal.Result == XWAIT_ERROR) &&
		(tFinal.Error == XERR_STATE),
		"event Await succeeded in coroutine finalizer"
	);

	tWait.Event = &tEvent;
	tWait.Order = arrOrder;
	tWait.Completed = &iCompleted;
	tWait.Timeout = 0;
	tWait.Result = XWAIT_ERROR;
	pWaitCo = xrtCoSpawn(pSched, testCoEventWaitProc, &tWait, NULL);
	testRequire(pWaitCo != NULL, "post-finalizer event waiter spawn failed");
	testRequire(xrtCoSchedRun(pSched), "post-finalizer event scheduler run failed");
	testRequire(
		(tWait.Result == XWAIT_OK) && (iCompleted == 1),
		"event finalizer consumed the pending signal"
	);
	testRequire(xrtCoDestroy(pWaitCo), "post-finalizer waiter destroy failed");
	testRequire(xrtCoDestroy(pFinalCo), "event finalizer coroutine destroy failed");
	testRequire(xrtCoSchedDestroy(pSched), "finalizer event scheduler destroy failed");
	testRequire(xrtCoEventUnit(&tEvent), "finalizer event unit failed");
}



/* 运行协程事件的复位、并发、超时、取消和生命周期测试。 */
int main(void)
{
	testCoroutineEventBasic();
	testCoroutineEventManual();
	testCoroutineEventAuto();
	testCoroutineEventOutcomes();
	testCoroutineEventFinalizer();
	testRequire(xrtCoThreadDetach(), "coroutine event runtime detach failed");

	printf("[PASS] coroutine_event\n");
	return 0;
}

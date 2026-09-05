#include "../test.h"
#include "../test_thread.h"



/* 基础调度状态覆盖让出、睡眠、结果和分离协程。 */
typedef struct testcoschedbasic {
	int Step;
	int DetachedRuns;
	xwaitresult SleepResult;
} testcoschedbasic;



/* 保留句柄协程先公平让出，再执行短时异步睡眠。 */
static ptr testCoSchedBasicProc(ptr pData)
{
	testcoschedbasic* pState = (testcoschedbasic*)pData;

	pState->Step = 1;
	testRequire(xrtCoSleep(0) == XWAIT_OK, "scheduler fair yield failed");
	pState->Step = 2;
	pState->SleepResult = xrtCoSleep(1000);
	pState->Step = 3;
	return pState;
}



/* 分离协程只记录实际执行次数。 */
static ptr testCoSchedDetachedProc(ptr pData)
{
	int* pRuns = (int*)pData;

	(*pRuns)++;
	return NULL;
}



/* park 用例记录进入状态和最终等待结果。 */
typedef struct testcoschedpark {
	volatile int Entered;
	xwaitresult Result;
} testcoschedpark;



/* 无限 park 等待提前唤醒、跨线程唤醒或取消。 */
static ptr testCoSchedParkProc(ptr pData)
{
	testcoschedpark* pState = (testcoschedpark*)pData;

	pState->Entered = 1;
	pState->Result = xrtCoPark();
	return pState;
}



/* 从外部线程唤醒调度协程。 */
typedef struct testcoschedwake {
	xcoro* Co;
	bool Result;
} testcoschedwake;



/* 跨线程唤醒过程只使用线程安全的 wake 入口。 */
static int testCoSchedWakeThread(ptr pData)
{
	testcoschedwake* pWake = (testcoschedwake*)pData;

	pWake->Result = xrtCoWake(pWake->Co);
	return 0;
}



/* 高频唤醒状态用条件变量只协调测试进度，不参与调度器实现。 */
typedef struct testcoschedstress {
	xmutex Lock;
	xcond Changed;
	xcoro* Co;
	int Requested;
	int Completed;
	int Iterations;
	bool WakeOkay;
} testcoschedstress;



/* 每轮发布序号后 park，覆盖 wake 与摘链交错。 */
static ptr testCoSchedStressProc(ptr pData)
{
	testcoschedstress* pState = (testcoschedstress*)pData;

	for ( int i = 1; i <= pState->Iterations; i++ ) {
		(void)xrtMutexLock(&pState->Lock);
		pState->Requested = i;
		(void)xrtCondBroadcast(&pState->Changed);
		(void)xrtMutexUnlock(&pState->Lock);
		testRequire(xrtCoPark() == XWAIT_OK, "stress park failed");
		(void)xrtMutexLock(&pState->Lock);
		pState->Completed = i;
		(void)xrtCondBroadcast(&pState->Changed);
		(void)xrtMutexUnlock(&pState->Lock);
	}
	return pState;
}



/* 外部线程逐轮唤醒，既覆盖提前唤醒也覆盖已挂起唤醒。 */
static int testCoSchedStressWake(ptr pData)
{
	testcoschedstress* pState = (testcoschedstress*)pData;

	pState->WakeOkay = true;
	for ( int i = 1; i <= pState->Iterations; i++ ) {
		(void)xrtMutexLock(&pState->Lock);
		while ( pState->Requested < i ) {
			(void)xrtCondWait(&pState->Changed, &pState->Lock);
		}
		(void)xrtMutexUnlock(&pState->Lock);
		if ( !xrtCoWake(pState->Co) ) {
			pState->WakeOkay = false;
			return 1;
		}
		(void)xrtMutexLock(&pState->Lock);
		while ( pState->Completed < i ) {
			(void)xrtCondWait(&pState->Changed, &pState->Lock);
		}
		(void)xrtMutexUnlock(&pState->Lock);
	}
	return 0;
}



/* join 用例保存目标和等待结果。 */
typedef struct testcoschedjoin {
	xcoro* Target;
	xwaitresult Result;
	uint64 Timeout;
} testcoschedjoin;



/* 目标协程在一次睡眠后返回固定输入。 */
static ptr testCoSchedTargetProc(ptr pData)
{
	testRequire(xrtCoSleep(2000) == XWAIT_OK, "join target sleep failed");
	return pData;
}



/* 等待目标完成，可选择有限期限。 */
static ptr testCoSchedJoinProc(ptr pData)
{
	testcoschedjoin* pJoin = (testcoschedjoin*)pData;

	pJoin->Result = pJoin->Timeout == UINT64_MAX ?
		xrtCoJoin(pJoin->Target) :
		xrtCoJoinFor(pJoin->Target, pJoin->Timeout);
	return pJoin;
}



/* join 环检测用例保存对端和检测结果。 */
typedef struct testcoschedcycle {
	xcoro* Other;
	xwaitresult Result;
} testcoschedcycle;



/* 尝试等待对端，并清除预期的环错误。 */
static ptr testCoSchedCycleProc(ptr pData)
{
	testcoschedcycle* pCycle = (testcoschedcycle*)pData;

	pCycle->Result = xrtCoJoin(pCycle->Other);
	if ( pCycle->Result == XWAIT_ERROR ) {
		xrtClearError();
	}
	return pCycle;
}



/* 投递测试记录 FIFO 次序、执行线程和 Owned 数据析构次数。 */
typedef struct testcoschedpoststate {
	int Values[8];
	int Count;
	int Destroyed;
	uint64 OwnerThreadId;
	bool WrongScheduler;
	bool WrongThread;
	bool EnteredCoroutine;
} testcoschedpoststate;



/* 每个投递项保存期望序号和跨线程受理结果。 */
typedef struct testcoschedpostitem {
	xcosched* Sched;
	testcoschedpoststate* State;
	int Value;
	bool Posted;
} testcoschedpostitem;



/* 投递过程验证所属线程和普通调用栈，并记录严格 FIFO 次序。 */
static void testCoSchedPostProc(xcosched* pSched, ptr pData)
{
	testcoschedpostitem* pItem = (testcoschedpostitem*)pData;
	testcoschedpoststate* pState = pItem->State;

	if ( pSched != pItem->Sched ) {
		pState->WrongScheduler = true;
	}
	if ( xrtThreadCurrentId() != pState->OwnerThreadId ) {
		pState->WrongThread = true;
	}
	if ( xrtCoCurrent() != NULL ) {
		pState->EnteredCoroutine = true;
	}
	pState->Values[pState->Count++] = pItem->Value;
}



/* Owned 投递析构在对应过程返回后记录一次。 */
static void testCoSchedPostDestroy(ptr pData)
{
	testcoschedpostitem* pItem = (testcoschedpostitem*)pData;

	pItem->State->Destroyed++;
}



/* 外部线程只负责向调度器投递一个 Owned 过程。 */
static int testCoSchedPostThread(ptr pData)
{
	testcoschedpostitem* pItem = (testcoschedpostitem*)pData;

	pItem->Posted = xrtCoSchedPostOwned(
		pItem->Sched,
		testCoSchedPostProc,
		pItem,
		testCoSchedPostDestroy
	);
	return pItem->Posted ? 0 : 1;
}



/* 多生产者投递状态验证每个生产者自身的 FIFO 顺序。 */
typedef struct testcoschedpoststress {
	int Last[4];
	int Runs;
	int Destroyed;
	bool OrderOkay;
} testcoschedpoststress;



/* 压力投递项记录生产者和该生产者内的递增序号。 */
typedef struct testcoschedpoststressitem {
	testcoschedpoststress* State;
	int Producer;
	int Sequence;
} testcoschedpoststressitem;



/* 每个外部线程连续提交一段独立的投递项。 */
typedef struct testcoschedpostproducer {
	xcosched* Sched;
	testcoschedpoststressitem* Items;
	int Count;
	bool Posted;
} testcoschedpostproducer;



/* 所属线程执行投递时检查同一生产者的严格递增次序。 */
static void testCoSchedPostStressProc(xcosched* pSched, ptr pData)
{
	testcoschedpoststressitem* pItem =
		(testcoschedpoststressitem*)pData;
	testcoschedpoststress* pState = pItem->State;

	(void)pSched;
	if ( pItem->Sequence != (pState->Last[pItem->Producer] + 1) ) {
		pState->OrderOkay = false;
	}
	pState->Last[pItem->Producer] = pItem->Sequence;
	pState->Runs++;
}



/* 压力投递析构与过程在同一所属线程中串行计数。 */
static void testCoSchedPostStressDestroy(ptr pData)
{
	testcoschedpoststressitem* pItem =
		(testcoschedpoststressitem*)pData;

	pItem->State->Destroyed++;
}



/* 单个生产者从外部线程提交全部 Owned 项。 */
static int testCoSchedPostProducer(ptr pData)
{
	testcoschedpostproducer* pProducer =
		(testcoschedpostproducer*)pData;

	pProducer->Posted = true;
	for ( int i = 0; i < pProducer->Count; i++ ) {
		if ( !xrtCoSchedPostOwned(
			pProducer->Sched,
			testCoSchedPostStressProc,
			&pProducer->Items[i],
			testCoSchedPostStressDestroy
		) ) {
			pProducer->Posted = false;
			return 1;
		}
	}
	return 0;
}



/* 验证多生产者并发投递不会丢失、重复或破坏生产者内 FIFO。 */
static void testCoroutineSchedulerPostStress(void)
{
	enum { PRODUCERS = 4, ITEMS_PER_PRODUCER = 1000 };
	testcoschedpoststress tState;
	testcoschedpostproducer tProducers[PRODUCERS];
	testthread tThreads[PRODUCERS];
	testcoschedpoststressitem* pItems;
	xcosched* pSched;

	memset(&tState, 0, sizeof(tState));
	memset(tProducers, 0, sizeof(tProducers));
	memset(tThreads, 0, sizeof(tThreads));
	tState.OrderOkay = true;
	pItems = (testcoschedpoststressitem*)malloc(
		PRODUCERS * ITEMS_PER_PRODUCER * sizeof(testcoschedpoststressitem)
	);
	testRequire(pItems != NULL, "scheduler post stress allocation failed");
	pSched = xrtCoSchedCreateLimit(PRODUCERS * ITEMS_PER_PRODUCER);
	testRequire(pSched != NULL, "scheduler post stress create failed");
	for ( int i = 0; i < PRODUCERS; i++ ) {
		tProducers[i].Sched = pSched;
		tProducers[i].Items = &pItems[i * ITEMS_PER_PRODUCER];
		tProducers[i].Count = ITEMS_PER_PRODUCER;
		tThreads[i].Proc = testCoSchedPostProducer;
		tThreads[i].Data = &tProducers[i];
		for ( int j = 0; j < ITEMS_PER_PRODUCER; j++ ) {
			tProducers[i].Items[j].State = &tState;
			tProducers[i].Items[j].Producer = i;
			tProducers[i].Items[j].Sequence = j + 1;
		}
	}
	testThreadsStart(tThreads, PRODUCERS);
	testThreadsJoin(tThreads, PRODUCERS);
	for ( int i = 0; i < PRODUCERS; i++ ) {
		testRequire(tProducers[i].Posted,
			"scheduler post stress producer failed");
	}
	testRequire(xrtCoSchedRun(pSched), "scheduler post stress drain failed");
	testRequire(tState.OrderOkay &&
		(tState.Runs == (PRODUCERS * ITEMS_PER_PRODUCER)) &&
		(tState.Destroyed == (PRODUCERS * ITEMS_PER_PRODUCER)),
		"scheduler post stress result mismatch");
	testRequire(xrtCoSchedDestroy(pSched),
		"scheduler post stress destroy failed");
	free(pItems);
}



/* 有界队列生产者各自记录受理和析构，避免测试计数自身的数据竞争。 */
typedef struct testcoschedbounded {
	xcosched* Sched;
	size_t Accepted;
	size_t Runs;
	size_t Destroyed;
} testcoschedbounded;

static void testCoSchedBoundedProc(xcosched* pSched, ptr pData)
{
	testcoschedbounded* pState = (testcoschedbounded*)pData;

	(void)pSched;
	pState->Runs++;
}

static void testCoSchedBoundedDestroy(ptr pData)
{
	((testcoschedbounded*)pData)->Destroyed++;
}

static int testCoSchedBoundedProducer(ptr pData)
{
	testcoschedbounded* pState = (testcoschedbounded*)pData;

	for ( size_t i = 0; i < 64u; i++ ) {
		if ( xrtCoSchedPostOwned(pState->Sched, testCoSchedBoundedProc,
			pState, testCoSchedBoundedDestroy) ) {
			pState->Accepted++;
		} else {
			testRequire(xrtErrorKind(xrtGetError()) == XERR_AGAIN,
				"full scheduler post did not return AGAIN");
			xrtClearError();
		}
	}
	return 0;
}

/* 多生产者严格遵守预算；队满不接管数据，也不能阻塞内部唤醒。 */
static void testCoroutineSchedulerBounded(void)
{
	testcoschedbounded States[4] = { 0 };
	testthread Threads[4] = { 0 };
	testcoschedpark Park = { 0 };
	testcoschedwake Wake = { 0 };
	testthread WakeThread = { 0 };
	xcosched* pSched = xrtCoSchedCreateLimit(7u);
	size_t iAccepted = 0;

	testRequire(pSched != NULL, "bounded scheduler create failed");
	Wake.Co = xrtCoSpawn(pSched, testCoSchedParkProc, &Park, NULL);
	testRequire((Wake.Co != NULL) &&
		(xrtCoSchedStep(pSched) == XWAIT_OK) && Park.Entered,
		"bounded scheduler park failed");
	for ( size_t i = 0; i < 4u; i++ ) {
		States[i].Sched = pSched;
		Threads[i].Proc = testCoSchedBoundedProducer;
		Threads[i].Data = &States[i];
	}
	testThreadsStart(Threads, 4);
	testThreadsJoin(Threads, 4);
	for ( size_t i = 0; i < 4u; i++ ) {
		iAccepted += States[i].Accepted;
		testRequire((States[i].Runs == 0) && (States[i].Destroyed == 0),
			"rejected posts transferred ownership");
	}
	testRequire(iAccepted == 7u, "concurrent posts exceeded scheduler limit");
	WakeThread.Proc = testCoSchedWakeThread;
	WakeThread.Data = &Wake;
	testThreadsStart(&WakeThread, 1);
	testThreadsJoin(&WakeThread, 1);
	testRequire(Wake.Result && xrtCoSchedRun(pSched) &&
		(Park.Result == XWAIT_OK), "full queue blocked coroutine wake");
	for ( size_t i = 0; i < 4u; i++ ) {
		testRequire((States[i].Runs == States[i].Accepted) &&
			(States[i].Destroyed == States[i].Accepted),
			"bounded scheduler lost accepted ownership");
	}
	testRequire(xrtCoSchedPostOwned(pSched, testCoSchedBoundedProc,
		&States[0], testCoSchedBoundedDestroy) && xrtCoSchedClose(pSched),
		"drained queue did not release capacity");
	testRequire(!xrtCoSchedPost(pSched, testCoSchedBoundedProc, &States[0]) &&
		(xrtErrorKind(xrtGetError()) == XERR_CLOSED),
		"closed scheduler accepted post");
	xrtClearError();
	testRequire(xrtCoSchedRun(pSched) &&
		(States[0].Destroyed == States[0].Accepted + 1u) &&
		xrtCoDestroy(Wake.Co) && xrtCoSchedDestroy(pSched),
		"bounded close did not drain accepted post");

	/* 0 与原创建入口使用相同默认值；不让新默认悄悄变回无限制。 */
	for ( size_t i = 0; i < 2u; i++ ) {
		pSched = i == 0 ? xrtCoSchedCreate() : xrtCoSchedCreateLimit(0);
		testRequire(pSched != NULL, "default bounded scheduler create failed");
		for ( size_t j = 0; j < XRT_CO_SCHED_POST_LIMIT_DEFAULT; j++ ) {
			testRequire(xrtCoSchedPost(pSched, testCoSchedBoundedProc, &States[0]),
				"default queue filled too early");
		}
		testRequire(!xrtCoSchedPost(pSched, testCoSchedBoundedProc, &States[0]) &&
			(xrtErrorKind(xrtGetError()) == XERR_AGAIN),
			"default queue is unbounded");
		xrtClearError();
		testRequire(xrtCoSchedRun(pSched) && xrtCoSchedDestroy(pSched),
			"default queue cleanup failed");
	}
}



/* 验证跨线程 FIFO 投递、数据所有权、关闭排空和销毁边界。 */
static void testCoroutineSchedulerPost(void)
{
	testcoschedpoststate tState;
	testcoschedpostitem tItems[5];
	testthread tThread;
	xcosched* pSched;

	memset(&tState, 0, sizeof(tState));
	memset(tItems, 0, sizeof(tItems));
	pSched = xrtCoSchedCreate();
	testRequire(pSched != NULL, "post scheduler create failed");
	tState.OwnerThreadId = xrtThreadCurrentId();
	for ( int i = 0; i < 5; i++ ) {
		tItems[i].Sched = pSched;
		tItems[i].State = &tState;
		tItems[i].Value = i + 1;
	}
	testRequire(xrtCoSchedPost(pSched, testCoSchedPostProc, &tItems[0]),
		"first borrowed scheduler post failed");
	testRequire(xrtCoSchedPost(pSched, testCoSchedPostProc, &tItems[1]),
		"second borrowed scheduler post failed");
	tThread.Proc = testCoSchedPostThread;
	tThread.Data = &tItems[2];
	testThreadsStart(&tThread, 1);
	testThreadsJoin(&tThread, 1);
	testRequire(tItems[2].Posted, "cross-thread scheduler post failed");

	/* 未执行投递属于活跃资源，销毁必须明确拒绝而不能静默丢弃。 */
	xrtClearError();
	testRequire(!xrtCoSchedDestroy(pSched), "scheduler destroyed pending posts");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE,
		"pending post destroy error mismatch");
	testRequire(xrtCoSchedRun(pSched), "scheduler post drain failed");
	testRequire((tState.Count == 3) &&
		(tState.Values[0] == 1) &&
		(tState.Values[1] == 2) &&
		(tState.Values[2] == 3), "scheduler post FIFO mismatch");
	testRequire(!tState.WrongScheduler && !tState.WrongThread &&
		!tState.EnteredCoroutine, "scheduler post execution context mismatch");
	testRequire(tState.Destroyed == 1, "scheduler post owned destroy mismatch");

	/* 关闭前已经受理的过程继续排空，关闭后的投递不接管数据。 */
	testRequire(xrtCoSchedPostOwned(
		pSched,
		testCoSchedPostProc,
		&tItems[3],
		testCoSchedPostDestroy
	), "pre-close scheduler post failed");
	testRequire(xrtCoSchedClose(pSched), "post scheduler close failed");
	xrtClearError();
	testRequire(!xrtCoSchedPostOwned(
		pSched,
		testCoSchedPostProc,
		&tItems[4],
		testCoSchedPostDestroy
	), "scheduler accepted post after close");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_CLOSED) &&
		(tState.Destroyed == 1), "closed scheduler post ownership mismatch");
	testRequire(xrtCoSchedRun(pSched), "closed scheduler post drain failed");
	testRequire((tState.Count == 4) && (tState.Values[3] == 4) &&
		(tState.Destroyed == 2), "pre-close scheduler post result mismatch");
	testRequire(xrtCoSchedDestroy(pSched), "post scheduler destroy failed");
}



/* 验证基础调度、公平让出、sleep 和分离回收。 */
static void testCoroutineSchedulerBasic(void)
{
	testcoschedbasic tState;
	xcosched* pSched;
	xcoro* pCo;

	memset(&tState, 0, sizeof(tState));
	pSched = xrtCoSchedCreate();
	testRequire(pSched != NULL, "scheduler create failed");
	pCo = xrtCoSpawn(pSched, testCoSchedBasicProc, &tState, NULL);
	testRequire(pCo != NULL, "scheduler spawn failed");
	testRequire(xrtCoGo(pSched, testCoSchedDetachedProc, &tState.DetachedRuns, NULL), "scheduler go failed");
	testRequire(xrtCoSchedAlive(pSched) == 2, "scheduler initial alive mismatch");
	testRequire(xrtCoSchedStep(pSched) == XWAIT_OK, "scheduler first step failed");
	testRequire(tState.Step == 1, "scheduler first step state mismatch");
	testRequire(xrtCoSchedRun(pSched), "scheduler run failed");
	testRequire(tState.Step == 3, "scheduler completion state mismatch");
	testRequire(tState.SleepResult == XWAIT_OK, "scheduler sleep result mismatch");
	testRequire(tState.DetachedRuns == 1, "detached coroutine run count mismatch");
	testRequire(xrtCoSchedAlive(pSched) == 0, "scheduler final alive mismatch");
	testRequire(xrtCoResult(pCo) == &tState, "scheduler retained result mismatch");
	testRequire(xrtCoDestroy(pCo), "scheduler retained coroutine destroy failed");
	testRequire(xrtCoSchedStep(pSched) == XWAIT_CLOSED, "empty scheduler step mismatch");
	testRequire(xrtCoSchedDestroy(pSched), "scheduler destroy failed");
}



/* 验证提前唤醒、跨线程唤醒和取消 park。 */
static void testCoroutineSchedulerWake(void)
{
	testcoschedpark tEarly;
	testcoschedpark tCross;
	testcoschedpark tCancel;
	testcoschedwake tWake;
	testthread tThread;
	xcosched* pSched;
	xcoro* pEarly;
	xcoro* pCross;
	xcoro* pCancel;

	memset(&tEarly, 0, sizeof(tEarly));
	memset(&tCross, 0, sizeof(tCross));
	memset(&tCancel, 0, sizeof(tCancel));
	memset(&tWake, 0, sizeof(tWake));
	pSched = xrtCoSchedCreate();
	testRequire(pSched != NULL, "wake scheduler create failed");

	pEarly = xrtCoSpawn(pSched, testCoSchedParkProc, &tEarly, NULL);
	testRequire(pEarly != NULL, "early wake spawn failed");
	testRequire(xrtCoWake(pEarly), "early wake post failed");
	testRequire(xrtCoSchedRun(pSched), "early wake run failed");
	testRequire(tEarly.Result == XWAIT_OK, "early wake was lost");
	testRequire(xrtCoDestroy(pEarly), "early wake coroutine destroy failed");

	pCross = xrtCoSpawn(pSched, testCoSchedParkProc, &tCross, NULL);
	testRequire(pCross != NULL, "cross wake spawn failed");
	testRequire(xrtCoSchedStep(pSched) == XWAIT_OK, "cross wake entry step failed");
	testRequire(tCross.Entered == 1, "cross wake coroutine did not enter");
	tWake.Co = pCross;
	tThread.Proc = testCoSchedWakeThread;
	tThread.Data = &tWake;
	testThreadsStart(&tThread, 1);
	testThreadsJoin(&tThread, 1);
	testRequire(tWake.Result, "cross-thread wake failed");
	testRequire(xrtCoSchedRun(pSched), "cross wake run failed");
	testRequire(tCross.Result == XWAIT_OK, "cross wake result mismatch");
	testRequire(xrtCoDestroy(pCross), "cross wake coroutine destroy failed");

	pCancel = xrtCoSpawn(pSched, testCoSchedParkProc, &tCancel, NULL);
	testRequire(pCancel != NULL, "cancel park spawn failed");
	testRequire(xrtCoSchedStep(pSched) == XWAIT_OK, "cancel park entry failed");
	testRequire(xrtCoCancel(pCancel), "cancel park request failed");
	testRequire(xrtCoSchedRun(pSched), "cancel park run failed");
	testRequire(tCancel.Result == XWAIT_CANCELLED, "cancel park result mismatch");
	testRequire(xrtCoTerm(pCancel) == XCORO_TERM_RETURNED,
		"handled park cancellation did not return normally");
	testRequire(xrtCoDestroy(pCancel), "cancel park coroutine destroy failed");
	testRequire(xrtCoSchedDestroy(pSched), "wake scheduler destroy failed");
}



/* 高频交错验证投递节点不会丢失、重复或截断。 */
static void testCoroutineSchedulerStress(void)
{
	testcoschedstress tState;
	testthread tThread;
	xcosched* pSched;

	memset(&tState, 0, sizeof(tState));
	tState.Iterations = 5000;
	testRequire(xrtMutexInit(&tState.Lock), "stress mutex init failed");
	testRequire(xrtCondInit(&tState.Changed), "stress condition init failed");
	pSched = xrtCoSchedCreate();
	testRequire(pSched != NULL, "stress scheduler create failed");
	tState.Co = xrtCoSpawn(pSched, testCoSchedStressProc, &tState, NULL);
	testRequire(tState.Co != NULL, "stress coroutine spawn failed");
	tThread.Proc = testCoSchedStressWake;
	tThread.Data = &tState;
	testThreadsStart(&tThread, 1);
	testRequire(xrtCoSchedRun(pSched), "stress scheduler run failed");
	testThreadsJoin(&tThread, 1);
	testRequire(tState.WakeOkay, "stress wake thread failed");
	testRequire(tState.Completed == tState.Iterations, "stress iteration count mismatch");
	testRequire(xrtCoDestroy(tState.Co), "stress coroutine destroy failed");
	testRequire(xrtCoSchedDestroy(pSched), "stress scheduler destroy failed");
	testRequire(xrtCondUnit(&tState.Changed), "stress condition unit failed");
	testRequire(xrtMutexUnit(&tState.Lock), "stress mutex unit failed");
}



/* 验证多等待者、join 超时和依赖环检测。 */
static void testCoroutineSchedulerJoin(void)
{
	testcoschedjoin tJoinA;
	testcoschedjoin tJoinB;
	testcoschedjoin tTimeout;
	testcoschedcycle tCycleA;
	testcoschedcycle tCycleB;
	xcosched* pSched;
	xcoro* pTarget;
	xcoro* pJoinA;
	xcoro* pJoinB;
	xcoro* pTimeout;
	xcoro* pCycleA;
	xcoro* pCycleB;

	memset(&tJoinA, 0, sizeof(tJoinA));
	memset(&tJoinB, 0, sizeof(tJoinB));
	memset(&tTimeout, 0, sizeof(tTimeout));
	memset(&tCycleA, 0, sizeof(tCycleA));
	memset(&tCycleB, 0, sizeof(tCycleB));
	pSched = xrtCoSchedCreate();
	testRequire(pSched != NULL, "join scheduler create failed");
	pTarget = xrtCoSpawn(pSched, testCoSchedTargetProc, &tJoinA, NULL);
	testRequire(pTarget != NULL, "join target spawn failed");
	tJoinA.Target = pTarget;
	tJoinA.Timeout = UINT64_MAX;
	tJoinB.Target = pTarget;
	tJoinB.Timeout = UINT64_MAX;
	tTimeout.Target = pTarget;
	tTimeout.Timeout = 1;
	pJoinA = xrtCoSpawn(pSched, testCoSchedJoinProc, &tJoinA, NULL);
	pJoinB = xrtCoSpawn(pSched, testCoSchedJoinProc, &tJoinB, NULL);
	pTimeout = xrtCoSpawn(pSched, testCoSchedJoinProc, &tTimeout, NULL);
	testRequire((pJoinA != NULL) && (pJoinB != NULL) && (pTimeout != NULL), "join waiter spawn failed");
	testRequire(xrtCoSchedRun(pSched), "join scheduler run failed");
	testRequire(tJoinA.Result == XWAIT_OK, "first join result mismatch");
	testRequire(tJoinB.Result == XWAIT_OK, "second join result mismatch");
	testRequire(tTimeout.Result == XWAIT_TIMEOUT, "join timeout result mismatch");
	testRequire(xrtCoDestroy(pTarget), "join target destroy failed");
	testRequire(xrtCoDestroy(pJoinA), "first join waiter destroy failed");
	testRequire(xrtCoDestroy(pJoinB), "second join waiter destroy failed");
	testRequire(xrtCoDestroy(pTimeout), "timeout waiter destroy failed");

	pCycleA = xrtCoSpawn(pSched, testCoSchedCycleProc, &tCycleA, NULL);
	pCycleB = xrtCoSpawn(pSched, testCoSchedCycleProc, &tCycleB, NULL);
	testRequire((pCycleA != NULL) && (pCycleB != NULL), "cycle coroutine spawn failed");
	tCycleA.Other = pCycleB;
	tCycleB.Other = pCycleA;
	testRequire(xrtCoSchedRun(pSched), "cycle scheduler run failed");
	testRequire(
		(tCycleA.Result == XWAIT_OK) && (tCycleB.Result == XWAIT_ERROR),
		"join cycle detection mismatch"
	);
	testRequire(xrtCoDestroy(pCycleA), "cycle A destroy failed");
	testRequire(xrtCoDestroy(pCycleB), "cycle B destroy failed");
	testRequire(xrtCoSchedDestroy(pSched), "join scheduler destroy failed");
}



/* 记录调度器受理后、首次运行前被放弃的协程终结状态。 */
typedef struct testcoschedabandon {
	int Started;
	int Finalized;
	xcoroterm Term;
} testcoschedabandon;



/* READY 放弃用例不应进入用户过程。 */
static ptr testCoSchedAbandonProc(ptr pData)
{
	testcoschedabandon* pState = (testcoschedabandon*)pData;

	pState->Started++;
	return pState;
}



/* 调度器受理的 READY 协程必须以取消终态完成一次终结过程。 */
static void testCoSchedAbandonFinal(
	xcoroterm Term,
	ptr pResult,
	const xerror* pError,
	ptr pData
)
{
	testcoschedabandon* pState = (testcoschedabandon*)pData;

	(void)pResult;
	(void)pError;
	pState->Finalized++;
	pState->Term = Term;
}



/* 验证关闭、活跃销毁拒绝和 READY 目标销毁唤醒 join。 */
static void testCoroutineSchedulerLifecycle(void)
{
	testcoschedabandon tAbandon;
	testcoschedpark tPark;
	testcoschedjoin tJoin;
	xcoroargs tArgs;
	xcosched* pSched;
	xcoro* pPark;
	xcoro* pWaiter;
	xcoro* pTarget;

	memset(&tPark, 0, sizeof(tPark));
	memset(&tJoin, 0, sizeof(tJoin));
	memset(&tAbandon, 0, sizeof(tAbandon));
	memset(&tArgs, 0, sizeof(tArgs));
	pSched = xrtCoSchedCreate();
	testRequire(pSched != NULL, "lifecycle scheduler create failed");
	pPark = xrtCoSpawn(pSched, testCoSchedParkProc, &tPark, NULL);
	testRequire(pPark != NULL, "lifecycle park spawn failed");
	xrtClearError();
	testRequire(!xrtCoSchedDestroy(pSched), "active scheduler destroy succeeded");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "active scheduler destroy error mismatch");
	xrtClearError();
	testRequire(xrtCoSchedStep(pSched) == XWAIT_OK, "lifecycle park entry failed");
	testRequire(tPark.Entered == 1, "lifecycle park did not enter");
	testRequire(xrtCoSchedClose(pSched), "scheduler close failed");
	testRequire(xrtCoSchedRun(pSched), "closed scheduler drain failed");
	testRequire(tPark.Result == XWAIT_CANCELLED, "close cancellation result mismatch");
	testRequire(xrtCoDestroy(pPark), "closed coroutine destroy failed");
	testRequire(xrtCoSpawn(pSched, testCoSchedParkProc, &tPark, NULL) == NULL, "spawn after close succeeded");
	testRequire(xrtCoSchedDestroy(pSched), "closed scheduler destroy failed");
	xrtClearError();

	pSched = xrtCoSchedCreate();
	testRequire(pSched != NULL, "join close scheduler create failed");
	tJoin.Timeout = UINT64_MAX;
	pWaiter = xrtCoSpawn(pSched, testCoSchedJoinProc, &tJoin, NULL);
	tArgs.Finalize = testCoSchedAbandonFinal;
	tArgs.FinalizeData = &tAbandon;
	pTarget = xrtCoSpawn(pSched, testCoSchedAbandonProc, &tAbandon, &tArgs);
	testRequire((pWaiter != NULL) && (pTarget != NULL), "join close spawn failed");
	tJoin.Target = pTarget;
	testRequire(xrtCoSchedStep(pSched) == XWAIT_OK, "join close waiter entry failed");
	testRequire(xrtCoDestroy(pTarget), "READY join target destroy failed");
	testRequire(
		(tAbandon.Started == 0) &&
		(tAbandon.Finalized == 1) &&
		(tAbandon.Term == XCORO_TERM_CANCELLED),
		"READY scheduler coroutine finalizer mismatch"
	);
	testRequire(xrtCoSchedRun(pSched), "join close scheduler run failed");
	testRequire(tJoin.Result == XWAIT_CLOSED, "destroyed join target result mismatch");
	testRequire(xrtCoDestroy(pWaiter), "join close waiter destroy failed");
	testRequire(xrtCoSchedDestroy(pSched), "join close scheduler destroy failed");
}



/* 运行调度器执行、公平性、等待、取消和生命周期边界测试。 */
int main(void)
{
	testCoroutineSchedulerPost();
	testCoroutineSchedulerPostStress();
	testCoroutineSchedulerBounded();
	testCoroutineSchedulerBasic();
	testCoroutineSchedulerWake();
	testCoroutineSchedulerStress();
	testCoroutineSchedulerJoin();
	testCoroutineSchedulerLifecycle();
	testRequire(xrtCoThreadDetach(), "scheduler coroutine runtime detach failed");

	printf("[PASS] coroutine_scheduler\n");
	return 0;
}

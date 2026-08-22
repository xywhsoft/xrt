#include "../test.h"
#include "../test_thread.h"



/* 并发 Future 用例共享一个消费端和预期值。 */
typedef struct testfuturethread {
	xfuture* Future;
	xpromise* Promise;
	ptr Value;
	xwaitresult Result;
} testfuturethread;



/* 多生产者竞争共享同一开始屏障，但各自保留独立 Promise 引用和值。 */
typedef struct testfutureproducerbarrier {
	xmutex Lock;
	xcond Ready;
	size_t Waiting;
	bool Start;
} testfutureproducerbarrier;



typedef struct testfutureproducer {
	testfutureproducerbarrier* Barrier;
	xpromise* Promise;
	int Value;
	int Destroyed;
	bool Won;
} testfutureproducer;



/* 阻塞取消监听，用于检查终态发布与令牌通知之间没有可见裂缝。 */
typedef struct testfuturecancelbarrier {
	xmutex Lock;
	xcond Ready;
	xpromise* Promise;
	bool Entered;
	bool Release;
} testfuturecancelbarrier;



/* 外部取消等待用显式同步点确认等待线程已经开始进入公开 API。 */
typedef struct testfuturewaitcancel {
	xmutex Lock;
	xcond Ready;
	xfuture* Future;
	xcancel* Cancel;
	xwaitresult WaitResult;
	bool Entered;
} testfuturewaitcancel;



/* 在允许生产线程继续前保持取消请求派发过程。 */
static void testFutureCancelBarrierProc(ptr pData)
{
	testfuturecancelbarrier* pBarrier =
		(testfuturecancelbarrier*)pData;

	(void)xrtMutexLock(&pBarrier->Lock);
	pBarrier->Entered = true;
	(void)xrtCondBroadcast(&pBarrier->Ready);
	while ( !pBarrier->Release ) {
		(void)xrtCondWait(&pBarrier->Ready, &pBarrier->Lock);
	}
	(void)xrtMutexUnlock(&pBarrier->Lock);
}



/* 在独立生产线程确认取消终态。 */
static int testFutureCancelProducer(ptr pData)
{
	testfuturecancelbarrier* pBarrier =
		(testfuturecancelbarrier*)pData;

	return xrtPromiseCancel(pBarrier->Promise) ? 0 : 1;
}



/* 验证取消通知完成前 Future 仍保持 Pending，且其他生产者不能抢占预留终态。 */
static void testFutureCancelPublication(void)
{
	testfuturecancelbarrier tBarrier = { 0 };
	testthread tProducer = { 0 };
	xcancelwatch* pWatch;
	xcancel* pCancel;
	xfuture* pFuture;
	xpromise* pPromise;

	testRequire(xrtMutexInit(&tBarrier.Lock),
		"future cancel publication lock init failed");
	testRequire(xrtCondInit(&tBarrier.Ready),
		"future cancel publication cond init failed");
	pPromise = xrtPromiseCreate(&pFuture, NULL);
	testRequire(pPromise != NULL,
		"future cancel publication pair create failed");
	pCancel = xrtFutureCancelToken(pFuture);
	testRequire(pCancel != NULL,
		"future cancel publication token failed");
	pWatch = xrtCancelWatch(
		pCancel,
		testFutureCancelBarrierProc,
		&tBarrier
	);
	testRequire(pWatch != NULL,
		"future cancel publication watch failed");
	tBarrier.Promise = pPromise;
	tProducer.Proc = testFutureCancelProducer;
	tProducer.Data = &tBarrier;
	testThreadsStart(&tProducer, 1);

	(void)xrtMutexLock(&tBarrier.Lock);
	while ( !tBarrier.Entered ) {
		(void)xrtCondWait(&tBarrier.Ready, &tBarrier.Lock);
	}
	(void)xrtMutexUnlock(&tBarrier.Lock);
	testRequire(xrtCancelRequested(pCancel),
		"future cancel request was not visible during notification");
	testRequire(xrtFutureState(pFuture) == XFUTURE_PENDING,
		"future cancel terminal was published before cancellation notification");
	xrtClearError();
	testRequire(!xrtPromiseResolve(pPromise, NULL),
		"future cancel reservation allowed another terminal writer");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE,
		"future cancel reservation error mismatch");

	(void)xrtMutexLock(&tBarrier.Lock);
	tBarrier.Release = true;
	(void)xrtCondBroadcast(&tBarrier.Ready);
	(void)xrtMutexUnlock(&tBarrier.Lock);
	testThreadsJoin(&tProducer, 1);
	testRequire((tProducer.Result == 0) &&
		(xrtFutureState(pFuture) == XFUTURE_CANCELLED),
		"future cancel publication terminal mismatch");
	xrtCancelUnwatch(pWatch);
	xrtCancelDestroy(pCancel);
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);
	testRequire(xrtCondUnit(&tBarrier.Ready),
		"future cancel publication cond unit failed");
	testRequire(xrtMutexUnit(&tBarrier.Lock),
		"future cancel publication lock unit failed");
}



/* 记录进入点后等待 Future、截止时间与外部取消中的首个事件。 */
static int testFutureWaitCancelThread(ptr pData)
{
	testfuturewaitcancel* pContext =
		(testfuturewaitcancel*)pData;

	(void)xrtMutexLock(&pContext->Lock);
	pContext->Entered = true;
	(void)xrtCondBroadcast(&pContext->Ready);
	(void)xrtMutexUnlock(&pContext->Lock);
	pContext->WaitResult = xrtFutureWaitUntilCancel(
		pContext->Future,
		XRT_DEADLINE_NEVER,
		pContext->Cancel
	);
	return 0;
}



/*
	验证外部取消与 Future 终态按同一把 Future 锁线性化。
	取消先发生时，随后发布的关闭终态不能改写当前等待者的结果。
*/
static void testFutureWaitCancelFirstEvent(void)
{
	enum { TEST_ROUNDS = 64 };

	for ( size_t i = 0; i < TEST_ROUNDS; i++ ) {
		testfuturewaitcancel Context = { 0 };
		testthread Thread = { 0 };
		xpromise* pPromise;

		testRequire(xrtMutexInit(&Context.Lock),
			"future external cancel lock init failed");
		testRequire(xrtCondInit(&Context.Ready),
			"future external cancel cond init failed");
		pPromise = xrtPromiseCreate(&Context.Future, NULL);
		Context.Cancel = xrtCancelCreate();
		testRequire((pPromise != NULL) && (Context.Cancel != NULL),
			"future external cancel setup failed");
		Thread.Proc = testFutureWaitCancelThread;
		Thread.Data = &Context;
		testThreadsStart(&Thread, 1);
		(void)xrtMutexLock(&Context.Lock);
		while ( !Context.Entered ) {
			(void)xrtCondWait(&Context.Ready, &Context.Lock);
		}
		(void)xrtMutexUnlock(&Context.Lock);
		xrtSleep(1);
		testRequire(xrtCancelRequest(Context.Cancel),
			"future external cancellation request failed");
		testRequire(xrtPromiseClose(pPromise),
			"future close after external cancellation failed");
		testThreadsJoin(&Thread, 1);
		testRequire((Thread.Result == 0) &&
			(Context.WaitResult == XWAIT_CANCELLED),
			"future external cancellation lost to a later terminal");
		testRequire(xrtFutureState(Context.Future) == XFUTURE_CLOSED,
			"future terminal state changed by an external wait cancellation");
		xrtCancelDestroy(Context.Cancel);
		xrtPromiseDestroy(pPromise);
		xrtFutureDestroy(Context.Future);
		testRequire(xrtCondUnit(&Context.Ready),
			"future external cancel cond unit failed");
		testRequire(xrtMutexUnit(&Context.Lock),
			"future external cancel lock unit failed");
	}
	{
		xfuture* pFuture;
		xpromise* pPromise = xrtPromiseCreate(&pFuture, NULL);
		xcancel* pCancel = xrtCancelCreate();

		testRequire((pPromise != NULL) && (pCancel != NULL),
			"future completion-first setup failed");
		testRequire(xrtPromiseResolve(pPromise, NULL),
			"future completion-first resolve failed");
		testRequire(xrtCancelRequest(pCancel),
			"future completion-first cancel request failed");
		testRequire(xrtFutureWaitUntilCancel(
			pFuture,
			XRT_DEADLINE_NEVER,
			pCancel
		) == XWAIT_OK,
			"completed Future was rewritten by a later external cancellation");
		xrtCancelDestroy(pCancel);
		xrtPromiseDestroy(pPromise);
		xrtFutureDestroy(pFuture);
	}
}



/* 多个宿主线程同时等待同一 Future。 */
static int testFutureWaiter(ptr pData)
{
	testfuturethread* pContext = (testfuturethread*)pData;

	pContext->Result = xrtFutureWaitFor(pContext->Future, UINT64_C(2000000));
	if ( pContext->Result != XWAIT_OK ) {
		return 1;
	}
	return xrtFutureValue(pContext->Future) == pContext->Value ? 0 : 2;
}



/* 记录每个 owned 值最终恰好由胜出 Future 或失败生产者释放一次。 */
static void testFutureProducerDestroy(ptr pValue, ptr pData)
{
	testfutureproducer* pContext = (testfutureproducer*)pData;

	testRequire(pValue == &pContext->Value,
		"future producer owned value mismatch");
	pContext->Destroyed++;
}



/* 多个生产者同时竞争唯一终态，失败方仍持有自己的值。 */
static int testFutureRaceProducer(ptr pData)
{
	testfutureproducer* pContext = (testfutureproducer*)pData;

	(void)xrtMutexLock(&pContext->Barrier->Lock);
	pContext->Barrier->Waiting++;
	(void)xrtCondBroadcast(&pContext->Barrier->Ready);
	while ( !pContext->Barrier->Start ) {
		(void)xrtCondWait(
			&pContext->Barrier->Ready,
			&pContext->Barrier->Lock
		);
	}
	(void)xrtMutexUnlock(&pContext->Barrier->Lock);

	pContext->Won = xrtPromiseResolveOwned(
		pContext->Promise,
		&pContext->Value,
		testFutureProducerDestroy,
		pContext
	);
	if ( !pContext->Won ) {
		testFutureProducerDestroy(&pContext->Value, pContext);
	}
	xrtPromiseDestroy(pContext->Promise);
	return 0;
}



/* 验证多 Promise 引用并发完成时只有一个终态和值所有权胜出。 */
static void testFutureProducerRace(void)
{
	enum { TEST_PRODUCERS = 8 };
	testfutureproducerbarrier tBarrier;
	testfutureproducer arrProducer[TEST_PRODUCERS];
	testthread arrThread[TEST_PRODUCERS];
	xfuture* pFuture;
	xpromise* pPromise;
	ptr pValue;
	size_t iWinners = 0;

	memset(&tBarrier, 0, sizeof(tBarrier));
	memset(arrProducer, 0, sizeof(arrProducer));
	memset(arrThread, 0, sizeof(arrThread));
	testRequire(xrtMutexInit(&tBarrier.Lock),
		"future producer barrier lock init failed");
	testRequire(xrtCondInit(&tBarrier.Ready),
		"future producer barrier cond init failed");
	pPromise = xrtPromiseCreate(&pFuture, NULL);
	testRequire(pPromise != NULL, "future producer race pair create failed");
	for ( size_t i = 0; i < TEST_PRODUCERS; i++ ) {
		arrProducer[i].Barrier = &tBarrier;
		arrProducer[i].Promise = xrtPromiseRef(pPromise);
		arrProducer[i].Value = (int)i + 1;
		testRequire(arrProducer[i].Promise != NULL,
			"future producer Promise ref failed");
		arrThread[i].Proc = testFutureRaceProducer;
		arrThread[i].Data = &arrProducer[i];
	}
	xrtPromiseDestroy(pPromise);
	testThreadsStart(arrThread, TEST_PRODUCERS);

	(void)xrtMutexLock(&tBarrier.Lock);
	while ( tBarrier.Waiting != TEST_PRODUCERS ) {
		(void)xrtCondWait(&tBarrier.Ready, &tBarrier.Lock);
	}
	tBarrier.Start = true;
	(void)xrtCondBroadcast(&tBarrier.Ready);
	(void)xrtMutexUnlock(&tBarrier.Lock);
	testThreadsJoin(arrThread, TEST_PRODUCERS);

	testRequire(xrtFutureState(pFuture) == XFUTURE_RESOLVED,
		"future producer race did not resolve");
	pValue = xrtFutureValue(pFuture);
	for ( size_t i = 0; i < TEST_PRODUCERS; i++ ) {
		testRequire(arrThread[i].Result == 0,
			"future producer thread failed");
		if ( arrProducer[i].Won ) {
			iWinners++;
			testRequire(pValue == &arrProducer[i].Value,
				"future producer winner value mismatch");
			testRequire(arrProducer[i].Destroyed == 0,
				"future winner value was released early");
		} else {
			testRequire(arrProducer[i].Destroyed == 1,
				"future losing producer ownership mismatch");
		}
	}
	testRequire(iWinners == 1,
		"future producer race selected multiple terminal writers");
	xrtFutureDestroy(pFuture);
	for ( size_t i = 0; i < TEST_PRODUCERS; i++ ) {
		testRequire(arrProducer[i].Destroyed == 1,
			"future producer value destructor count mismatch");
	}
	testRequire(xrtCondUnit(&tBarrier.Ready),
		"future producer barrier cond unit failed");
	testRequire(xrtMutexUnit(&tBarrier.Lock),
		"future producer barrier lock unit failed");
}



/* 生产线程短暂延迟后完成 Promise。 */
static int testFutureProducer(ptr pData)
{
	testfuturethread* pContext = (testfuturethread*)pData;

	#if defined(_WIN32) || defined(_WIN64)
		Sleep(10);
	#else
		struct timespec tTime = { 0, 10000000 };

		(void)nanosleep(&tTime, NULL);
	#endif
	return xrtPromiseResolve(pContext->Promise, pContext->Value) ? 0 : 1;
}



/* 验证多等待者、完成广播和生产消费引用的并发边界。 */
int main(void)
{
	testfuturethread arrContext[8];
	testthread arrWaiter[8];
	testthread tProducer;
	xfuture* pFuture;
	xpromise* pPromise;
	int iValue = 91;

	pPromise = xrtPromiseCreate(&pFuture, NULL);
	testRequire(pPromise != NULL, "threaded future pair create failed");
	memset(arrContext, 0, sizeof(arrContext));
	memset(arrWaiter, 0, sizeof(arrWaiter));
	for ( size_t i = 0; i < 8; i++ ) {
		arrContext[i].Future = pFuture;
		arrContext[i].Value = &iValue;
		arrWaiter[i].Proc = testFutureWaiter;
		arrWaiter[i].Data = &arrContext[i];
	}
	tProducer.Proc = testFutureProducer;
	tProducer.Data = &arrContext[0];
	arrContext[0].Promise = pPromise;
	testThreadsStart(arrWaiter, 8);
	testThreadsStart(&tProducer, 1);
	testThreadsJoin(&tProducer, 1);
	testThreadsJoin(arrWaiter, 8);
	testRequire(tProducer.Result == 0, "future producer thread failed");
	for ( size_t i = 0; i < 8; i++ ) {
		testRequire(arrWaiter[i].Result == 0, "future waiter thread failed");
		testRequire(arrContext[i].Result == XWAIT_OK, "future waiter result mismatch");
	}
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);

	testFutureProducerRace();
	testFutureCancelPublication();
	testFutureWaitCancelFirstEvent();

	printf("[PASS] future threads\n");
	return 0;
}

#include "../test.h"



#if !defined(TEST_TCP_ACCEPT_THREADS_BACKEND)
	#define TEST_TCP_ACCEPT_THREADS_BACKEND XNET_PORT_SELECT
	#define TEST_TCP_ACCEPT_THREADS_BACKEND_NAME "select"
#endif



#define TEST_TCP_ACCEPT_THREAD_COUNT 4u
#define TEST_TCP_ACCEPT_WAITER_COUNT 256u



typedef struct testtcpacceptthreads {
	xatomic32 Blocked;
	xatomic32 Release;
	xatomic32 Go;
	xatomic32 CancelStarted;
} testtcpacceptthreads;



typedef struct testtcpacceptcancel {
	testtcpacceptthreads* Context;
	xfuture** Futures;
	size_t Begin;
	size_t End;
} testtcpacceptcancel;



/* 在截止时间前等待原子状态到达目标。 */
static void testTcpAcceptThreadsWait(
	xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(10000000u);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(iDeadline), sMessage);
		xrtThreadYield();
	}
}



/* 阻塞唯一 Worker，形成 Listener Close 与取消的确定竞争窗口。 */
static void testTcpAcceptThreadsBlock(xnetworker* pWorker, ptr pData)
{
	testtcpacceptthreads* pContext = (testtcpacceptthreads*)pData;

	(void)pWorker;
	xrtAtomic32Store(&pContext->Blocked, 1, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(&pContext->Release, XMEMORY_ACQUIRE) == 0 ) {
		xrtThreadYield();
	}
}



/* 每个线程竞争取消自己负责的一段 Accept Future。 */
static int32 testTcpAcceptThreadsCancel(ptr pData)
{
	testtcpacceptcancel* pCancel = (testtcpacceptcancel*)pData;
	testtcpacceptthreads* pContext = pCancel->Context;

	(void)xrtAtomic32FetchAdd(
		&pContext->CancelStarted,
		1,
		XMEMORY_RELEASE
	);
	while ( xrtAtomic32Load(&pContext->Go, XMEMORY_ACQUIRE) == 0 ) {
		xrtThreadYield();
	}
	for ( size_t i = pCancel->Begin; i < pCancel->End; i++ ) {
		(void)xrtFutureCancel(pCancel->Futures[i]);
		if ( (i & 7u) == 0 ) {
			xrtThreadYield();
		}
	}
	return 0;
}



/* 验证并发取消与 Listener 关闭之间只有 CANCELLED 或 CLOSED。 */
int main(void)
{
	testtcpacceptthreads Context;
	testtcpacceptcancel Cancel[TEST_TCP_ACCEPT_THREAD_COUNT];
	xthread* Threads[TEST_TCP_ACCEPT_THREAD_COUNT];
	xfuture* Futures[TEST_TCP_ACCEPT_WAITER_COUNT];
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerstats Stats;
	xnetengine* pEngine;
	xnetlistener* pListener;
	size_t iCancelled = 0;
	size_t iClosed = 0;
	xdeadline iDeadline;

	memset(&Context, 0, sizeof(Context));
	memset(Threads, 0, sizeof(Threads));
	memset(Futures, 0, sizeof(Futures));
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TCP_ACCEPT_THREADS_BACKEND;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"threaded TCP accept engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "threaded TCP accept listener address failed");
	ListenConfig.AcceptConcurrency = 1;
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		NULL,
		NULL,
		NULL
	);
	testRequire(pListener != NULL,
		"threaded TCP accept listener start failed");
	testRequire(xrtNetEnginePost(
		pEngine,
		0,
		testTcpAcceptThreadsBlock,
		&Context
	), "threaded TCP accept blocker post failed");
	testTcpAcceptThreadsWait(&Context.Blocked, 1,
		"threaded TCP accept blocker did not start");

	for ( size_t i = 0; i < TEST_TCP_ACCEPT_WAITER_COUNT; i++ ) {
		Futures[i] = xrtNetListenerAcceptAsync(pListener);
		testRequire(Futures[i] != NULL,
			"threaded TCP accept Future creation failed");
	}
	testRequire(xrtNetListenerStats(pListener, &Stats) &&
		 (Stats.AcceptWaiters == TEST_TCP_ACCEPT_WAITER_COUNT),
		"threaded TCP accept waiter count mismatch");
	for ( size_t i = 0; i < TEST_TCP_ACCEPT_THREAD_COUNT; i++ ) {
		Cancel[i].Context = &Context;
		Cancel[i].Futures = Futures;
		Cancel[i].Begin =
			(i * TEST_TCP_ACCEPT_WAITER_COUNT) /
			TEST_TCP_ACCEPT_THREAD_COUNT;
		Cancel[i].End =
			((i + 1u) * TEST_TCP_ACCEPT_WAITER_COUNT) /
			TEST_TCP_ACCEPT_THREAD_COUNT;
		Threads[i] = xrtThreadCreate(
			testTcpAcceptThreadsCancel,
			&Cancel[i],
			0
		);
		testRequire(Threads[i] != NULL,
			"threaded TCP accept cancel thread create failed");
	}
	testTcpAcceptThreadsWait(
		&Context.CancelStarted,
		TEST_TCP_ACCEPT_THREAD_COUNT,
		"threaded TCP accept cancel threads did not start"
	);
	testRequire(xrtNetListenerClose(pListener),
		"threaded TCP accept listener close failed");
	xrtAtomic32Store(&Context.Go, 1, XMEMORY_RELEASE);
	xrtAtomic32Store(&Context.Release, 1, XMEMORY_RELEASE);

	for ( size_t i = 0; i < TEST_TCP_ACCEPT_THREAD_COUNT; i++ ) {
		testRequire(xrtThreadWait(Threads[i]) == XWAIT_OK,
			"threaded TCP accept cancel thread wait failed");
		testRequire(xrtThreadExitCode(Threads[i]) == 0,
			"threaded TCP accept cancel thread failed");
		xrtThreadDestroy(Threads[i]);
	}
	for ( size_t i = 0; i < TEST_TCP_ACCEPT_WAITER_COUNT; i++ ) {
		testRequire(xrtFutureWaitFor(Futures[i], 10000000u) == XWAIT_OK,
			"threaded TCP accept Future did not finish");
		if ( xrtFutureState(Futures[i]) == XFUTURE_CANCELLED ) {
			iCancelled++;
		} else if ( xrtFutureState(Futures[i]) == XFUTURE_CLOSED ) {
			iClosed++;
		} else {
			testRequire(false,
				"threaded TCP accept reached an invalid terminal state");
		}
		xrtFutureDestroy(Futures[i]);
	}
	testRequire((iCancelled + iClosed) == TEST_TCP_ACCEPT_WAITER_COUNT,
		"threaded TCP accept lost a terminal result");
	iDeadline = xrtDeadlineAfter(10000000u);
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"threaded TCP accept listener close timed out");
		xrtThreadYield();
	}
	testRequire(xrtNetListenerStats(pListener, &Stats) &&
		 (Stats.AcceptWaiters == 0) && (Stats.QueuedAccepts == 0),
		"threaded TCP accept final statistics mismatch");
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(pEngine),
		"threaded TCP accept engine destroy failed");
	printf(
		"[PASS] network TCP accept Future %s cancel-close threads (%zu/%zu)\n",
		TEST_TCP_ACCEPT_THREADS_BACKEND_NAME,
		iCancelled,
		iClosed
	);
	return 0;
}

#include "../test.h"



#if !defined(TEST_UDP_FUTURE_THREADS_BACKEND)
	#define TEST_UDP_FUTURE_THREADS_BACKEND XNET_PORT_SELECT
	#define TEST_UDP_FUTURE_THREADS_BACKEND_NAME "select"
#endif



#define TEST_UDP_FUTURE_THREAD_COUNT 4u
#define TEST_UDP_FUTURE_WAITER_COUNT 256u
#define TEST_UDP_FUTURE_ROUND_COUNT 8u



typedef struct testudpfuturethreads {
	xatomic32 Blocked;
	xatomic32 Release;
	xatomic32 Go;
	xatomic32 CancelStarted;
} testudpfuturethreads;



typedef struct testudpfuturecancel {
	testudpfuturethreads* Context;
	xfuture** Futures;
	size_t Begin;
	size_t End;
} testudpfuturecancel;



/* 在截止时间前等待原子计数到达目标。 */
static void testUdpFutureThreadsWait(
	xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(UINT64_C(10000000));

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(iDeadline), sMessage);
		xrtThreadYield();
	}
}



/* 阻塞唯一 Worker，稳定制造等待注册、关闭命令和取消线程的竞争窗口。 */
static void testUdpFutureThreadsBlock(xnetworker* pWorker, ptr pData)
{
	testudpfuturethreads* pContext = (testudpfuturethreads*)pData;

	(void)pWorker;
	xrtAtomic32Store(&pContext->Blocked, 1, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(&pContext->Release, XMEMORY_ACQUIRE) == 0 ) {
		xrtThreadYield();
	}
}



/* 每个取消线程竞争结束自己负责的一段接收等待。 */
static int32 testUdpFutureThreadsCancel(ptr pData)
{
	testudpfuturecancel* pCancel = (testudpfuturecancel*)pData;
	testudpfuturethreads* pContext = pCancel->Context;

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



/* 运行一次取消与正常关闭竞态，并核对每个等待的唯一终态与内部计数。 */
static void testUdpFutureThreadsRound(
	xnetengine* pEngine,
	size_t* pCancelled,
	size_t* pClosed
)
{
	testudpfuturethreads Context;
	testudpfuturecancel Cancel[TEST_UDP_FUTURE_THREAD_COUNT];
	xthread* Threads[TEST_UDP_FUTURE_THREAD_COUNT];
	xfuture* Futures[TEST_UDP_FUTURE_WAITER_COUNT];
	xnetudpconfig UdpConfig;
	xnetudpstats Stats;
	xnetudp* pUdp;
	xnetaddr Address;
	xfuture* pOpen;
	xfuture* pClose;

	memset(&Context, 0, sizeof(Context));
	memset(Threads, 0, sizeof(Threads));
	memset(Futures, 0, sizeof(Futures));
	xrtNetUdpConfigInit(&UdpConfig);
	testRequire(xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0),
		"threaded UDP Future loopback address failed");
	pUdp = xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&UdpConfig,
		NULL,
		NULL
	);
	testRequire(pUdp != NULL,
		"threaded UDP Future bind failed");
	pOpen = xrtNetUdpWaitAsync(pUdp, XNET_UDP_WAIT_OPEN);
	testRequire((pOpen != NULL) &&
		 (xrtFutureWaitFor(pOpen, UINT64_C(10000000)) == XWAIT_OK) &&
		 (xrtFutureState(pOpen) == XFUTURE_RESOLVED),
		"threaded UDP Future open wait failed");
	xrtFutureDestroy(pOpen);

	testRequire(xrtNetEnginePost(
		pEngine,
		0,
		testUdpFutureThreadsBlock,
		&Context
	), "threaded UDP Future blocker post failed");
	testUdpFutureThreadsWait(&Context.Blocked, 1,
		"threaded UDP Future blocker did not start");
	for ( size_t i = 0; i < TEST_UDP_FUTURE_WAITER_COUNT; i++ ) {
		Futures[i] = xrtNetUdpReceiveAsync(pUdp);
		testRequire(Futures[i] != NULL,
			"threaded UDP Future receive creation failed");
	}
	pClose = xrtNetUdpWaitAsync(pUdp, XNET_UDP_WAIT_CLOSE);
	testRequire((pClose != NULL) && xrtNetUdpStats(pUdp, &Stats) &&
		 (Stats.ReceiveWaiters == TEST_UDP_FUTURE_WAITER_COUNT),
		"threaded UDP Future waiter registration mismatch");

	for ( size_t i = 0; i < TEST_UDP_FUTURE_THREAD_COUNT; i++ ) {
		Cancel[i].Context = &Context;
		Cancel[i].Futures = Futures;
		Cancel[i].Begin =
			(i * TEST_UDP_FUTURE_WAITER_COUNT) /
			TEST_UDP_FUTURE_THREAD_COUNT;
		Cancel[i].End =
			((i + 1u) * TEST_UDP_FUTURE_WAITER_COUNT) /
			TEST_UDP_FUTURE_THREAD_COUNT;
		Threads[i] = xrtThreadCreate(
			testUdpFutureThreadsCancel,
			&Cancel[i],
			0
		);
		testRequire(Threads[i] != NULL,
			"threaded UDP Future cancel thread create failed");
	}
	testUdpFutureThreadsWait(
		&Context.CancelStarted,
		TEST_UDP_FUTURE_THREAD_COUNT,
		"threaded UDP Future cancel threads did not start"
	);
	testRequire(xrtNetUdpClose(pUdp),
		"threaded UDP Future close request failed");
	xrtAtomic32Store(&Context.Go, 1, XMEMORY_RELEASE);
	xrtAtomic32Store(&Context.Release, 1, XMEMORY_RELEASE);

	for ( size_t i = 0; i < TEST_UDP_FUTURE_THREAD_COUNT; i++ ) {
		testRequire(xrtThreadWait(Threads[i]) == XWAIT_OK,
			"threaded UDP Future cancel thread wait failed");
		testRequire(xrtThreadExitCode(Threads[i]) == 0,
			"threaded UDP Future cancel thread failed");
		xrtThreadDestroy(Threads[i]);
	}
	testRequire((xrtFutureWaitFor(
		pClose,
		UINT64_C(10000000)
	) == XWAIT_OK) && (xrtFutureState(pClose) == XFUTURE_RESOLVED),
		"threaded UDP Future close wait failed");
	for ( size_t i = 0; i < TEST_UDP_FUTURE_WAITER_COUNT; i++ ) {
		testRequire(xrtFutureWaitFor(
			Futures[i],
			UINT64_C(10000000)
		) == XWAIT_OK, "threaded UDP Future receive did not finish");
		if ( xrtFutureState(Futures[i]) == XFUTURE_CANCELLED ) {
			(*pCancelled)++;
		} else if ( xrtFutureState(Futures[i]) == XFUTURE_CLOSED ) {
			(*pClosed)++;
		} else {
			testRequire(false,
				"threaded UDP Future reached an invalid terminal state");
		}
		xrtFutureDestroy(Futures[i]);
	}
	testRequire(xrtNetUdpStats(pUdp, &Stats) &&
		 (Stats.State == XNET_UDP_CLOSED) &&
		 (Stats.ReceiveWaiters == 0) &&
		 (Stats.ReceiveQueued == 0),
		"threaded UDP Future close retained waiters or packets");
	xrtFutureDestroy(pClose);
	xrtNetUdpDestroy(pUdp);
}



/* 反复验证并发取消与正常关闭之间只有 CANCELLED 或 CLOSED 唯一终态。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetengine* pEngine;
	size_t iCancelled = 0;
	size_t iClosed = 0;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_UDP_FUTURE_THREADS_BACKEND;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"threaded UDP Future engine start failed");
	for ( size_t i = 0; i < TEST_UDP_FUTURE_ROUND_COUNT; i++ ) {
		testUdpFutureThreadsRound(pEngine, &iCancelled, &iClosed);
	}
	testRequire((iCancelled + iClosed) ==
		 (TEST_UDP_FUTURE_WAITER_COUNT * TEST_UDP_FUTURE_ROUND_COUNT),
		"threaded UDP Future lost a terminal result");
	testRequire(xrtNetEngineDestroy(pEngine),
		"threaded UDP Future engine destroy failed");
	printf(
		"[PASS] network UDP Future %s cancel-close threads (%zu/%zu)\n",
		TEST_UDP_FUTURE_THREADS_BACKEND_NAME,
		iCancelled,
		iClosed
	);
	return 0;
}

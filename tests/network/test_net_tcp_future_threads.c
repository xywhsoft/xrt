#include "../test.h"



#if !defined(TEST_TCP_FUTURE_THREADS_BACKEND)
	#define TEST_TCP_FUTURE_THREADS_BACKEND XNET_PORT_SELECT
	#define TEST_TCP_FUTURE_THREADS_BACKEND_NAME "select"
#endif



#define TEST_TCP_FUTURE_THREAD_COUNT 4u
#define TEST_TCP_FUTURE_WAITER_COUNT 256u



typedef struct testtcpfuturethreads {
	xatomicptr Server;
	xatomic32 Accepted;
	xatomic32 Opened;
	xatomic32 Blocked;
	xatomic32 Release;
	xatomic32 Go;
	xatomic32 CancelStarted;
	xatomic32 Closed;
} testtcpfuturethreads;



typedef struct testtcpfuturecancel {
	testtcpfuturethreads* Context;
	xfuture** Futures;
	size_t Begin;
	size_t End;
} testtcpfuturecancel;



/* 在截止时间前等待原子状态到达目标。 */
static void testTcpFutureThreadsWait(
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



/* 接管服务端 Stream 并发布给测试线程。 */
static bool testTcpFutureThreadsAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testtcpfuturethreads* pContext = (testtcpfuturethreads*)pData;

	(void)pListener;
	testRequire(xrtNetStreamSetData(pStream, pContext),
		"threaded TCP Future accepted data setup failed");
	xrtAtomicPtrStore(&pContext->Server, pStream, XMEMORY_RELEASE);
	xrtAtomic32Store(&pContext->Accepted, 1, XMEMORY_RELEASE);
	return true;
}



/* 记录两个 Stream 已经打开。 */
static void testTcpFutureThreadsOpen(xnetstream* pStream, ptr pData)
{
	testtcpfuturethreads* pContext = (testtcpfuturethreads*)pData;

	(void)pStream;
	(void)xrtAtomic32FetchAdd(&pContext->Opened, 1, XMEMORY_RELEASE);
}



/* 记录正常关闭。 */
static void testTcpFutureThreadsClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testtcpfuturethreads* pContext = (testtcpfuturethreads*)pData;

	(void)pStream;
	testRequire((Result == XNET_RESULT_OK) && (pError == NULL),
		"threaded TCP Future close mismatch");
	(void)xrtAtomic32FetchAdd(&pContext->Closed, 1, XMEMORY_RELEASE);
}



/* 阻塞唯一 Worker，使等待登记、Close 命令和取消线程形成确定竞争窗口。 */
static void testTcpFutureThreadsBlock(xnetworker* pWorker, ptr pData)
{
	testtcpfuturethreads* pContext = (testtcpfuturethreads*)pData;

	(void)pWorker;
	xrtAtomic32Store(&pContext->Blocked, 1, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(&pContext->Release, XMEMORY_ACQUIRE) == 0 ) {
		xrtThreadYield();
	}
}



/* 每个取消线程竞争结束自己负责的一段接收等待。 */
static int32 testTcpFutureThreadsCancel(ptr pData)
{
	testtcpfuturecancel* pCancel = (testtcpfuturecancel*)pData;
	testtcpfuturethreads* pContext = pCancel->Context;

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



/* 验证并发取消与正常关闭之间只有 CANCELLED 或 CLOSED 唯一终态。 */
int main(void)
{
	testtcpfuturethreads Context;
	testtcpfuturecancel Cancel[TEST_TCP_FUTURE_THREAD_COUNT];
	xthread* Threads[TEST_TCP_FUTURE_THREAD_COUNT];
	xfuture* Futures[TEST_TCP_FUTURE_WAITER_COUNT];
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents StreamEvents;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetstream* pClient;
	xnetstream* pServer;
	xnetaddr Address;
	xfuture* pClientClose;
	xfuture* pServerClose;
	size_t iCancelled = 0;
	size_t iClosed = 0;

	memset(&Context, 0, sizeof(Context));
	memset(Threads, 0, sizeof(Threads));
	memset(Futures, 0, sizeof(Futures));
	xrtAtomicPtrInit(&Context.Server, NULL);
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&StreamEvents, 0, sizeof(StreamEvents));
	ListenerEvents.Accept = testTcpFutureThreadsAccept;
	StreamEvents.Open = testTcpFutureThreadsOpen;
	StreamEvents.Close = testTcpFutureThreadsClose;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TCP_FUTURE_THREADS_BACKEND;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"threaded TCP Future engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "threaded TCP Future listener address failed");
	ListenConfig.AcceptConcurrency = 1;
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		&StreamEvents,
		&Context
	);
	testRequire((pListener != NULL) &&
		 xrtNetListenerLocal(pListener, &Address),
		"threaded TCP Future listener start failed");
	pClient = xrtNetStreamConnect(
		pEngine,
		&Address,
		0,
		NULL,
		&StreamEvents,
		&Context
	);
	testRequire(pClient != NULL, "threaded TCP Future connect failed");
	testTcpFutureThreadsWait(&Context.Accepted, 1,
		"threaded TCP Future accept callback missing");
	testTcpFutureThreadsWait(&Context.Opened, 2,
		"threaded TCP Future open callbacks missing");
	pServer = (xnetstream*)xrtAtomicPtrLoad(
		&Context.Server,
		XMEMORY_ACQUIRE
	);
	testRequire(pServer != NULL, "threaded TCP Future server missing");

	testRequire(xrtNetEnginePost(
		pEngine,
		0,
		testTcpFutureThreadsBlock,
		&Context
	), "threaded TCP Future blocker post failed");
	testTcpFutureThreadsWait(&Context.Blocked, 1,
		"threaded TCP Future blocker did not start");
	for ( size_t i = 0; i < TEST_TCP_FUTURE_WAITER_COUNT; i++ ) {
		Futures[i] = xrtNetStreamRecvAsync(pServer, 1);
		testRequire(Futures[i] != NULL,
			"threaded TCP Future receive creation failed");
	}
	pClientClose = xrtNetStreamWaitAsync(
		pClient,
		XNET_STREAM_WAIT_CLOSE
	);
	pServerClose = xrtNetStreamWaitAsync(
		pServer,
		XNET_STREAM_WAIT_CLOSE
	);
	testRequire((pClientClose != NULL) && (pServerClose != NULL),
		"threaded TCP Future close wait creation failed");

	for ( size_t i = 0; i < TEST_TCP_FUTURE_THREAD_COUNT; i++ ) {
		Cancel[i].Context = &Context;
		Cancel[i].Futures = Futures;
		Cancel[i].Begin =
			(i * TEST_TCP_FUTURE_WAITER_COUNT) /
			TEST_TCP_FUTURE_THREAD_COUNT;
		Cancel[i].End =
			((i + 1u) * TEST_TCP_FUTURE_WAITER_COUNT) /
			TEST_TCP_FUTURE_THREAD_COUNT;
		Threads[i] = xrtThreadCreate(
			testTcpFutureThreadsCancel,
			&Cancel[i],
			0
		);
		testRequire(Threads[i] != NULL,
			"threaded TCP Future cancel thread create failed");
	}
	testTcpFutureThreadsWait(
		&Context.CancelStarted,
		TEST_TCP_FUTURE_THREAD_COUNT,
		"threaded TCP Future cancel threads did not start"
	);
	testRequire(xrtNetStreamClose(pClient) && xrtNetStreamClose(pServer),
		"threaded TCP Future close request failed");
	xrtAtomic32Store(&Context.Go, 1, XMEMORY_RELEASE);
	xrtAtomic32Store(&Context.Release, 1, XMEMORY_RELEASE);

	for ( size_t i = 0; i < TEST_TCP_FUTURE_THREAD_COUNT; i++ ) {
		testRequire(xrtThreadWait(Threads[i]) == XWAIT_OK,
			"threaded TCP Future cancel thread wait failed");
		testRequire(xrtThreadExitCode(Threads[i]) == 0,
			"threaded TCP Future cancel thread failed");
		xrtThreadDestroy(Threads[i]);
	}
	testRequire((xrtFutureWaitFor(pClientClose, 10000000u) == XWAIT_OK) &&
		 (xrtFutureState(pClientClose) == XFUTURE_RESOLVED) &&
		 (xrtFutureWaitFor(pServerClose, 10000000u) == XWAIT_OK) &&
		 (xrtFutureState(pServerClose) == XFUTURE_RESOLVED),
		"threaded TCP Future close waits failed");
	testTcpFutureThreadsWait(&Context.Closed, 2,
		"threaded TCP Future close callbacks missing");
	for ( size_t i = 0; i < TEST_TCP_FUTURE_WAITER_COUNT; i++ ) {
		testRequire(xrtFutureWaitFor(Futures[i], 10000000u) == XWAIT_OK,
			"threaded TCP Future receive did not finish");
		if ( xrtFutureState(Futures[i]) == XFUTURE_CANCELLED ) {
			iCancelled++;
		} else if ( xrtFutureState(Futures[i]) == XFUTURE_CLOSED ) {
			iClosed++;
		} else {
			testRequire(false,
				"threaded TCP Future receive reached an invalid terminal state");
		}
		xrtFutureDestroy(Futures[i]);
	}
	testRequire((iCancelled + iClosed) == TEST_TCP_FUTURE_WAITER_COUNT,
		"threaded TCP Future lost a receive terminal result");

	testRequire(xrtNetListenerClose(pListener),
		"threaded TCP Future listener close failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtFutureDestroy(pClientClose);
	xrtFutureDestroy(pServerClose);
	xrtNetStreamDestroy(pClient);
	xrtNetStreamDestroy(pServer);
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(pEngine),
		"threaded TCP Future engine destroy failed");
	printf(
		"[PASS] network TCP Future %s cancel-close threads (%zu/%zu)\n",
		TEST_TCP_FUTURE_THREADS_BACKEND_NAME,
		iCancelled,
		iClosed
	);
	return 0;
}

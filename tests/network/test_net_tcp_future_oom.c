#include "../test.h"



#define TEST_TCP_FUTURE_OOM_CREATE_LIMIT 4096u
#define TEST_TCP_FUTURE_OOM_RESULT_BYTES (128u * 1024u)



typedef struct testtcpfutureoom {
	xatomic32 Fail;
	xatomic64 Attempts;
	xatomicptr Server;
	xatomic32 Accepted;
	xatomic32 Opened;
	xatomic32 BarrierStarted;
	xatomic32 BarrierRelease;
	xatomic32 Closed;
} testtcpfutureoom;



/* 正常阶段转发系统分配，故障阶段拒绝全部新底层内存。 */
static ptr testTcpFutureOomAlloc(ptr pData, size_t iSize)
{
	testtcpfutureoom* pContext = (testtcpfutureoom*)pData;

	(void)xrtAtomic64FetchAdd(&pContext->Attempts, 1, XMEMORY_RELAXED);
	return xrtAtomic32Load(&pContext->Fail, XMEMORY_ACQUIRE) != 0 ?
		NULL : malloc(iSize);
}



/* 故障阶段拒绝扩容，正常阶段保持标准 realloc 语义。 */
static ptr testTcpFutureOomRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	testtcpfutureoom* pContext = (testtcpfutureoom*)pData;

	(void)xrtAtomic64FetchAdd(&pContext->Attempts, 1, XMEMORY_RELAXED);
	return xrtAtomic32Load(&pContext->Fail, XMEMORY_ACQUIRE) != 0 ?
		NULL : realloc(pMemory, iSize);
}



/* 释放正常阶段已经取得的底层内存。 */
static void testTcpFutureOomFree(ptr pData, ptr pMemory)
{
	(void)pData;
	free(pMemory);
}



/* 在截止时间前等待原子状态到达目标。 */
static void testTcpFutureOomWait(
	xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(iDeadline), sMessage);
		xrtThreadYield();
	}
}



/* 在截止时间前等待拉取缓冲达到指定字节数。 */
static void testTcpFutureOomAvailable(
	xnetstream* pStream,
	size_t iExpected,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	while ( xrtNetStreamAvailable(pStream) != iExpected ) {
		testRequire(!xrtDeadlineExpired(iDeadline), sMessage);
		xrtThreadYield();
	}
}



/* 接管服务端 Stream 并发布给测试线程。 */
static bool testTcpFutureOomAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testtcpfutureoom* pContext = (testtcpfutureoom*)pData;

	(void)pListener;
	testRequire(xrtNetStreamSetData(pStream, pContext),
		"TCP Future OOM accepted data setup failed");
	xrtAtomicPtrStore(&pContext->Server, pStream, XMEMORY_RELEASE);
	xrtAtomic32Store(&pContext->Accepted, 1, XMEMORY_RELEASE);
	return true;
}



/* 记录两个拉取模式 Stream 已经打开。 */
static void testTcpFutureOomOpen(xnetstream* pStream, ptr pData)
{
	testtcpfutureoom* pContext = (testtcpfutureoom*)pData;

	(void)pStream;
	(void)xrtAtomic32FetchAdd(&pContext->Opened, 1, XMEMORY_RELEASE);
}



/* 占住唯一 Worker，使等待节点先受理再进入指定故障阶段。 */
static void testTcpFutureOomBlock(xnetworker* pWorker, ptr pData)
{
	testtcpfutureoom* pContext = (testtcpfutureoom*)pData;

	(void)pWorker;
	(void)xrtAtomic32FetchAdd(
		&pContext->BarrierStarted,
		1,
		XMEMORY_RELEASE
	);
	while ( xrtAtomic32Load(
		&pContext->BarrierRelease,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
}



/* 记录正常关闭。 */
static void testTcpFutureOomClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testtcpfutureoom* pContext = (testtcpfutureoom*)pData;

	(void)pStream;
	testRequire((Result == XNET_RESULT_OK) && (pError == NULL),
		"TCP Future OOM close mismatch");
	(void)xrtAtomic32FetchAdd(&pContext->Closed, 1, XMEMORY_RELEASE);
}



/* 验证结果复制和等待创建 OOM 不丢字节、不泄漏节点且可以恢复。 */
int main(void)
{
	testtcpfutureoom Context;
	xallocator Allocator;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents StreamEvents;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetlistener* pPullListener;
	xnetstream* pClient;
	xnetstream* pServer;
	xnetaddr Address;
	xfuture* pFailed;
	xfuture* pRecovered;
	xfuture* pAcceptRecovered;
	xfuture** pPending;
	xnetlistenerstats PullStats;
	uint8* pPayload;
	size_t iPending = 0;

	memset(&Context, 0, sizeof(Context));
	xrtAtomic32Init(&Context.Fail, 0);
	xrtAtomic64Init(&Context.Attempts, 0);
	xrtAtomicPtrInit(&Context.Server, NULL);
	Allocator.Context = &Context;
	Allocator.Alloc = testTcpFutureOomAlloc;
	Allocator.Realloc = testTcpFutureOomRealloc;
	Allocator.Free = testTcpFutureOomFree;
	testRequire(xrtSetAllocator(&Allocator),
		"TCP Future OOM allocator install failed");
	pPending = (xfuture**)malloc(
		TEST_TCP_FUTURE_OOM_CREATE_LIMIT * sizeof(*pPending)
	);
	pPayload = (uint8*)malloc(TEST_TCP_FUTURE_OOM_RESULT_BYTES);
	testRequire((pPending != NULL) && (pPayload != NULL),
		"TCP Future OOM system allocation failed");
	for ( size_t i = 0; i < TEST_TCP_FUTURE_OOM_RESULT_BYTES; i++ ) {
		pPayload[i] = (uint8)(i * 31u);
	}

	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&StreamEvents, 0, sizeof(StreamEvents));
	ListenerEvents.Accept = testTcpFutureOomAccept;
	StreamEvents.Open = testTcpFutureOomOpen;
	StreamEvents.Close = testTcpFutureOomClose;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TCP Future OOM engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "TCP Future OOM listener address failed");
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
		"TCP Future OOM listener start failed");
	pClient = xrtNetStreamConnect(
		pEngine,
		&Address,
		0,
		NULL,
		&StreamEvents,
		&Context
	);
	testRequire(pClient != NULL, "TCP Future OOM connect failed");
	testTcpFutureOomWait(&Context.Accepted, 1,
		"TCP Future OOM accept callback missing");
	testTcpFutureOomWait(&Context.Opened, 2,
		"TCP Future OOM open callbacks missing");
	pServer = (xnetstream*)xrtAtomicPtrLoad(
		&Context.Server,
		XMEMORY_ACQUIRE
	);
	testRequire(pServer != NULL, "TCP Future OOM server missing");

	/* 先稳定缓存数据，再让已经受理的接收在 Worker 内复制失败。 */
	testRequire(xrtNetStreamSend(
		pClient,
		pPayload,
		TEST_TCP_FUTURE_OOM_RESULT_BYTES
	) == XNET_RESULT_OK,
		"TCP Future OOM setup send failed");
	testTcpFutureOomAvailable(
		pServer,
		TEST_TCP_FUTURE_OOM_RESULT_BYTES,
		"TCP Future OOM setup bytes did not arrive");
	testRequire(xrtNetEnginePost(
		pEngine,
		0,
		testTcpFutureOomBlock,
		&Context
	), "TCP Future OOM first blocker post failed");
	testTcpFutureOomWait(&Context.BarrierStarted, 1,
		"TCP Future OOM first blocker did not start");
	pFailed = xrtNetStreamRecvAsync(pServer, 0);
	testRequire(pFailed != NULL,
		"TCP Future OOM receive was not accepted before failure");
	xrtAtomic32Store(&Context.Fail, 1, XMEMORY_RELEASE);
	xrtAtomic32Store(&Context.BarrierRelease, 1, XMEMORY_RELEASE);
	testRequire((xrtFutureWaitFor(pFailed, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pFailed) == XFUTURE_FAILED) &&
		 (xrtErrorKind(xrtFutureError(pFailed)) == XERR_MEMORY) &&
		 (xrtNetStreamAvailable(pServer) ==
		  TEST_TCP_FUTURE_OOM_RESULT_BYTES),
		"TCP Future result OOM consumed bytes or reported the wrong state");

	/* 恢复分配后必须取得完全相同的字节。 */
	xrtAtomic32Store(&Context.Fail, 0, XMEMORY_RELEASE);
	pRecovered = xrtNetStreamRecvAsync(pServer, 0);
	testRequire((pRecovered != NULL) &&
		 (xrtFutureWaitFor(pRecovered, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pRecovered) == XFUTURE_RESOLVED),
		"TCP Future receive did not recover after result OOM");
	{
		xnetbytes* pBytes =
			(xnetbytes*)xrtFutureValue(pRecovered);
		xbytesview View = xrtNetBytesView(pBytes);

		testRequire((pBytes != NULL) &&
			 (View.Size == TEST_TCP_FUTURE_OOM_RESULT_BYTES) &&
			 (memcmp(
				View.Data,
				pPayload,
				TEST_TCP_FUTURE_OOM_RESULT_BYTES
			 ) == 0) &&
			 (xrtNetStreamAvailable(pServer) == 0),
			"TCP Future recovery bytes mismatch");
	}

	/* 耗尽已有小块缓存，验证创建链任一分配失败都完整回滚。 */
	xrtAtomic32Store(&Context.BarrierRelease, 0, XMEMORY_RELEASE);
	testRequire(xrtNetEnginePost(
		pEngine,
		0,
		testTcpFutureOomBlock,
		&Context
	), "TCP Future OOM second blocker post failed");
	testTcpFutureOomWait(&Context.BarrierStarted, 2,
		"TCP Future OOM second blocker did not start");
	xrtAtomic32Store(&Context.Fail, 1, XMEMORY_RELEASE);
	while ( iPending < TEST_TCP_FUTURE_OOM_CREATE_LIMIT ) {
		xfuture* pFuture = xrtNetStreamRecvAsync(pServer, 1);

		if ( pFuture == NULL ) {
			break;
		}
		pPending[iPending++] = pFuture;
	}
	testRequire(iPending < TEST_TCP_FUTURE_OOM_CREATE_LIMIT,
		"TCP Future creation did not reach backing OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"TCP Future creation OOM error mismatch");
	xrtClearError();
	xrtAtomic32Store(&Context.Fail, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&Context.BarrierRelease, 1, XMEMORY_RELEASE);
	for ( size_t i = 0; i < iPending; i++ ) {
		testRequire(xrtFutureCancel(pPending[i]) &&
			 (xrtFutureWaitFor(pPending[i], 5000000u) == XWAIT_OK) &&
			 (xrtFutureState(pPending[i]) == XFUTURE_CANCELLED),
			"TCP Future OOM retained a malformed pending waiter");
		xrtFutureDestroy(pPending[i]);
	}

	/* 复用同一故障分配器验证 Accept Future 部分构造完整回滚。 */
	pPullListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		NULL,
		NULL,
		NULL
	);
	testRequire(pPullListener != NULL,
		"TCP accept Future OOM listener create failed");
	iPending = 0;
	xrtAtomic32Store(&Context.Fail, 1, XMEMORY_RELEASE);
	while ( iPending < TEST_TCP_FUTURE_OOM_CREATE_LIMIT ) {
		xfuture* pFuture = xrtNetListenerAcceptAsync(pPullListener);

		if ( pFuture == NULL ) {
			break;
		}
		pPending[iPending++] = pFuture;
	}
	testRequire(iPending < TEST_TCP_FUTURE_OOM_CREATE_LIMIT,
		"TCP accept Future creation did not reach backing OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"TCP accept Future creation OOM error mismatch");
	xrtClearError();
	xrtAtomic32Store(&Context.Fail, 0, XMEMORY_RELEASE);
	for ( size_t i = 0; i < iPending; i++ ) {
		testRequire(xrtFutureCancel(pPending[i]) &&
			 (xrtFutureWaitFor(pPending[i], 5000000u) == XWAIT_OK) &&
			 (xrtFutureState(pPending[i]) == XFUTURE_CANCELLED),
			"TCP accept Future OOM retained a malformed waiter");
		xrtFutureDestroy(pPending[i]);
	}
	testRequire(xrtNetListenerStats(pPullListener, &PullStats) &&
		 (PullStats.AcceptWaiters == 0) && (PullStats.QueuedAccepts == 0),
		"TCP accept Future OOM leaked listener state");
	pAcceptRecovered = xrtNetListenerAcceptAsync(pPullListener);
	testRequire((pAcceptRecovered != NULL) &&
		 xrtFutureCancel(pAcceptRecovered) &&
		 (xrtFutureWaitFor(pAcceptRecovered, 5000000u) == XWAIT_OK) &&
		 (xrtFutureState(pAcceptRecovered) == XFUTURE_CANCELLED),
		"TCP accept Future did not recover after OOM");
	xrtFutureDestroy(pAcceptRecovered);
	testRequire(xrtNetListenerClose(pPullListener),
		"TCP accept Future OOM listener close failed");
	while ( xrtNetListenerState(pPullListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pPullListener);

	testRequire(xrtNetStreamClose(pClient) && xrtNetStreamClose(pServer),
		"TCP Future OOM close request failed");
	testTcpFutureOomWait(&Context.Closed, 2,
		"TCP Future OOM close callbacks missing");
	testRequire(xrtNetListenerClose(pListener),
		"TCP Future OOM listener close failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtFutureDestroy(pFailed);
	xrtFutureDestroy(pRecovered);
	xrtNetStreamDestroy(pClient);
	xrtNetStreamDestroy(pServer);
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP Future OOM engine destroy failed");
	testRequire(xrtAtomic64Load(
		&Context.Attempts,
		XMEMORY_ACQUIRE
	) != 0, "TCP Future OOM allocator observed no attempts");
	free(pPending);
	free(pPayload);
	printf("[PASS] network TCP Future OOM\n");
	return 0;
}

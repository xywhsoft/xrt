#include "../test.h"



#if !defined(TEST_TCP_BACKEND)
	#define TEST_TCP_BACKEND XNET_PORT_SELECT
	#define TEST_TCP_BACKEND_NAME "select"
#endif



#define TEST_TCP_SEND_THREADS 4u
#define TEST_TCP_SEND_MINIMUM 1000u



typedef struct testtcpthreads testtcpthreads;



typedef struct testtcpthreadendpoint {
	testtcpthreads* Context;
	bool Server;
} testtcpthreadendpoint;



typedef struct testtcpthreadproducer {
	testtcpthreads* Context;
	uint8 Byte;
} testtcpthreadproducer;



struct testtcpthreads {
	xnetstream* Client;
	xnetstream* Server;
	testtcpthreadendpoint ClientEndpoint;
	testtcpthreadendpoint ServerEndpoint;
	xatomic32 Accepted;
	xatomic32 Opened;
	xatomic32 Started;
	xatomic32 ProducersClosed;
	xatomic32 Failure;
	xatomic32 ClientClosed;
	xatomic32 ServerClosed;
	xatomic32 ServerEnd;
	xatomic64 Sends;
	xatomic64 Received;
};



/* 在测试截止时间前等待原子计数到达下限。 */
static void testTcpThreadsWait32(
	xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(15000000u);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(iDeadline), sMessage);
		xrtThreadYield();
	}
}



/* 在测试截止时间前等待 64 位计数到达下限。 */
static void testTcpThreadsWait64(
	xatomic64* pValue,
	uint64 iExpected,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(15000000u);

	while ( xrtAtomic64Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(iDeadline), sMessage);
		xrtThreadYield();
	}
}



/* 接管服务端 Stream，并把回调数据切换到服务端端点。 */
static bool testTcpThreadsAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testtcpthreads* pContext = (testtcpthreads*)pData;

	(void)pListener;
	testRequire(xrtNetStreamSetData(
		pStream,
		&pContext->ServerEndpoint
	), "TCP threaded accepted stream data failed");
	pContext->Server = pStream;
	xrtAtomic32Store(&pContext->Accepted, 1, XMEMORY_RELEASE);
	return true;
}



/* 记录两个端点已经公开。 */
static void testTcpThreadsOpen(xnetstream* pStream, ptr pData)
{
	testtcpthreadendpoint* pEndpoint =
		(testtcpthreadendpoint*)pData;

	(void)pStream;
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Context->Opened,
		1,
		XMEMORY_RELEASE
	);
}



/* 服务端消费全部字节并累计真实接收量。 */
static void testTcpThreadsRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	testtcpthreadendpoint* pEndpoint =
		(testtcpthreadendpoint*)pData;
	size_t iSize = xrtNetBufSize(pBuffer);

	(void)pStream;
	testRequire(pEndpoint->Server,
		"TCP threaded client received unexpected data");
	(void)xrtNetBufConsume(pBuffer, iSize);
	(void)xrtAtomic64FetchAdd(
		&pEndpoint->Context->Received,
		(uint64)iSize,
		XMEMORY_RELEASE
	);
}



/* 服务端必须在全部有序字节之后观察 FIN。 */
static void testTcpThreadsEnd(xnetstream* pStream, ptr pData)
{
	testtcpthreadendpoint* pEndpoint =
		(testtcpthreadendpoint*)pData;

	(void)pStream;
	if ( pEndpoint->Server ) {
		xrtAtomic32Store(
			&pEndpoint->Context->ServerEnd,
			1,
			XMEMORY_RELEASE
		);
	}
}



/* 两个端点都必须无错误关闭。 */
static void testTcpThreadsClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testtcpthreadendpoint* pEndpoint =
		(testtcpthreadendpoint*)pData;

	(void)pStream;
	if ( (Result != XNET_RESULT_OK) || (pError != NULL) ) {
		xrtAtomic32Store(
			&pEndpoint->Context->Failure,
			1,
			XMEMORY_RELEASE
		);
	}
	(void)xrtAtomic32FetchAdd(
		pEndpoint->Server ?
			&pEndpoint->Context->ServerClosed :
			&pEndpoint->Context->ClientClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 持续竞争发送，直到 Close 封闭入口。 */
static int32 testTcpThreadsProducer(ptr pData)
{
	testtcpthreadproducer* pProducer =
		(testtcpthreadproducer*)pData;
	testtcpthreads* pContext = pProducer->Context;

	(void)xrtAtomic32FetchAdd(
		&pContext->Started,
		1,
		XMEMORY_RELEASE
	);
	for ( ;; ) {
		xnetresult Result = xrtNetStreamSend(
			pContext->Client,
			&pProducer->Byte,
			1
		);

		if ( Result == XNET_RESULT_OK ) {
			(void)xrtAtomic64FetchAdd(
				&pContext->Sends,
				1,
				XMEMORY_RELEASE
			);
			continue;
		}
		if ( Result == XNET_RESULT_AGAIN ) {
			xrtThreadYield();
			continue;
		}
		if ( Result == XNET_RESULT_CLOSED ) {
			(void)xrtAtomic32FetchAdd(
				&pContext->ProducersClosed,
				1,
				XMEMORY_RELEASE
			);
			return 0;
		}
		xrtAtomic32Store(&pContext->Failure, 1, XMEMORY_RELEASE);
		return 1;
	}
}



/* 验证并发 Send 和 Close 之间不存在越过、丢字节或预算泄漏。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetstreamconfig StreamConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents StreamEvents;
	testtcpthreads Context;
	testtcpthreadproducer Producers[TEST_TCP_SEND_THREADS];
	xthread* Threads[TEST_TCP_SEND_THREADS];
	xnetstreamstats ClientStats;
	xnetstreamstats ServerStats;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetaddr Address;
	uint64 iSends;

	memset(&Context, 0, sizeof(Context));
	memset(Threads, 0, sizeof(Threads));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&StreamEvents, 0, sizeof(StreamEvents));
	Context.ClientEndpoint.Context = &Context;
	Context.ServerEndpoint.Context = &Context;
	Context.ServerEndpoint.Server = true;
	ListenerEvents.Accept = testTcpThreadsAccept;
	StreamEvents.Open = testTcpThreadsOpen;
	StreamEvents.Read = testTcpThreadsRead;
	StreamEvents.End = testTcpThreadsEnd;
	StreamEvents.Close = testTcpThreadsClose;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TCP_BACKEND;
	EngineConfig.Workers = 2;
	EngineConfig.CommandCapacity = 16384;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TCP threaded engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "TCP threaded listener address failed");
	ListenConfig.Stream.ReadSize = 512;
	ListenConfig.Stream.ReadMode = XNET_STREAM_READ_DIRECT;
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		&StreamEvents,
		&Context
	);
	testRequire((pListener != NULL) &&
		 xrtNetListenerLocal(pListener, &Address),
		"TCP threaded listener start failed");
	xrtNetStreamConfigInit(&StreamConfig);
	StreamConfig.WriteHighWater = 2048;
	StreamConfig.WriteLowWater = 512;
	StreamConfig.WriteLimit = 4096;
	StreamConfig.ReadMode = XNET_STREAM_READ_DIRECT;
	Context.Client = xrtNetStreamConnect(
		pEngine,
		&Address,
		1,
		&StreamConfig,
		&StreamEvents,
		&Context.ClientEndpoint
	);
	testRequire(Context.Client != NULL,
		"TCP threaded client connect failed");
	testTcpThreadsWait32(&Context.Accepted, 1,
		"TCP threaded accept callback missing");
	testTcpThreadsWait32(&Context.Opened, 2,
		"TCP threaded open callbacks missing");
	testRequire(
		(xrtNetStreamData(Context.Client) == &Context.ClientEndpoint) &&
		(xrtNetStreamData(Context.Server) == &Context.ServerEndpoint),
		"TCP threaded data snapshots mismatch"
	);

	for ( uint32 i = 0; i < TEST_TCP_SEND_THREADS; i++ ) {
		Producers[i].Context = &Context;
		Producers[i].Byte = (uint8)('A' + i);
		Threads[i] = xrtThreadCreate(
			testTcpThreadsProducer,
			&Producers[i],
			0
		);
		testRequire(Threads[i] != NULL,
			"TCP send producer create failed");
	}
	testTcpThreadsWait32(
		&Context.Started,
		TEST_TCP_SEND_THREADS,
		"TCP send producers did not start"
	);
	testTcpThreadsWait64(&Context.Sends, TEST_TCP_SEND_MINIMUM,
		"TCP send producers made no progress");
	testRequire(xrtNetStreamClose(Context.Client),
		"TCP concurrent close request failed");

	for ( uint32 i = 0; i < TEST_TCP_SEND_THREADS; i++ ) {
		testRequire(xrtThreadWait(Threads[i]) == XWAIT_OK,
			"TCP send producer wait failed");
		testRequire(xrtThreadExitCode(Threads[i]) == 0,
			"TCP send producer returned failure");
		xrtThreadDestroy(Threads[i]);
	}
	testRequire((xrtAtomic32Load(
		&Context.Failure,
		XMEMORY_ACQUIRE
	 ) == 0) && (xrtAtomic32Load(
		&Context.ProducersClosed,
		XMEMORY_ACQUIRE
	 ) == TEST_TCP_SEND_THREADS),
		"TCP send producers did not observe close");
	testTcpThreadsWait32(&Context.ClientClosed, 1,
		"TCP threaded client close callback missing");
	testTcpThreadsWait32(&Context.ServerEnd, 1,
		"TCP threaded server FIN missing");
	iSends = xrtAtomic64Load(&Context.Sends, XMEMORY_ACQUIRE);
	testTcpThreadsWait64(&Context.Received, iSends,
		"TCP concurrent close lost accepted bytes");
	testRequire(xrtAtomic64Load(
		&Context.Received,
		XMEMORY_ACQUIRE
	) == iSends, "TCP concurrent close duplicated bytes");
	testRequire(xrtNetStreamStats(Context.Client, &ClientStats) &&
		 xrtNetStreamStats(Context.Server, &ServerStats) &&
		 (ClientStats.SentBytes == iSends) &&
		 (ClientStats.QueuedBytes == 0) &&
		 (ServerStats.ReceivedBytes == iSends),
		"TCP concurrent close statistics mismatch");

	testRequire(xrtNetStreamClose(Context.Server),
		"TCP threaded server close failed");
	testTcpThreadsWait32(&Context.ServerClosed, 1,
		"TCP threaded server close callback missing");
	testRequire(xrtNetListenerClose(pListener),
		"TCP threaded listener close failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtSleep(1);
	}
	xrtNetStreamDestroy(Context.Client);
	xrtNetStreamDestroy(Context.Server);
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP threaded engine destroy failed");
	testRequire(
		(xrtAtomic32Load(
			&Context.ClientClosed,
			XMEMORY_ACQUIRE
		 ) == 1) && (xrtAtomic32Load(
			&Context.ServerClosed,
			XMEMORY_ACQUIRE
		 ) == 1),
		"TCP threaded close callback was not unique"
	);
	printf("[PASS] network TCP %s send-close threads\n",
		TEST_TCP_BACKEND_NAME);
	return 0;
}

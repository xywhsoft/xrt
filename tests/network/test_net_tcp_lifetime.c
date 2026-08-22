#include "../test.h"



#if !defined(TEST_TCP_BACKEND)
	#define TEST_TCP_BACKEND XNET_PORT_SELECT
	#define TEST_TCP_BACKEND_NAME "select"
#endif



typedef struct testtcplifetime {
	xatomic32 Open;
	xatomic32 Entered;
	xatomic32 Release;
	xatomic32 Returned;
	xatomic32 Failure;
} testtcplifetime;



typedef struct testtcpfailureorder {
	xatomic32 Open;
	xatomic32 ReadEntered;
	xatomic32 ReadRelease;
	xatomic32 Released;
	xatomic32 Closed;
	xatomic32 Failure;
	uint8 Byte;
} testtcpfailureorder;



/* 初始化一次终态回调与 Engine 销毁握手。 */
static void testTcpLifetimeInit(testtcplifetime* pContext)
{
	xrtAtomic32Init(&pContext->Open, 0);
	xrtAtomic32Init(&pContext->Entered, 0);
	xrtAtomic32Init(&pContext->Release, 0);
	xrtAtomic32Init(&pContext->Returned, 0);
	xrtAtomic32Init(&pContext->Failure, 0);
}



/* 初始化内部 I/O 失败与发送所有权顺序握手。 */
static void testTcpFailureOrderInit(testtcpfailureorder* pContext)
{
	xrtAtomic32Init(&pContext->Open, 0);
	xrtAtomic32Init(&pContext->ReadEntered, 0);
	xrtAtomic32Init(&pContext->ReadRelease, 0);
	xrtAtomic32Init(&pContext->Released, 0);
	xrtAtomic32Init(&pContext->Closed, 0);
	xrtAtomic32Init(&pContext->Failure, 0);
	pContext->Byte = 'x';
}



/* 在截止时间前等待终态握手到达目标值。 */
static void testTcpLifetimeWait(
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



/* 记录内部失败顺序测试已经完成连接。 */
static void testTcpFailureOrderOpen(xnetstream* pStream, ptr pData)
{
	testtcpfailureorder* pContext = (testtcpfailureorder*)pData;

	(void)pStream;
	xrtAtomic32Store(&pContext->Open, 1, XMEMORY_RELEASE);
}



/* 占住 Worker，使跨线程发送命令稳定排在对端复位之后。 */
static void testTcpFailureOrderRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	testtcpfailureorder* pContext = (testtcpfailureorder*)pData;
	size_t iSize = xrtNetBufSize(pBuffer);

	(void)pStream;
	if ( !xrtNetBufConsume(pBuffer, iSize) ) {
		xrtAtomic32Store(&pContext->Failure, 1, XMEMORY_RELEASE);
	}
	xrtAtomic32Store(&pContext->ReadEntered, 1, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(
		&pContext->ReadRelease,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
}



/* 标记已经归还引用发送的外部所有权。 */
static void testTcpFailureOrderRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	testtcpfailureorder* pTest = (testtcpfailureorder*)pContext;

	if ( (pData != &pTest->Byte) || (iSize != 1) ) {
		xrtAtomic32Store(&pTest->Failure, 1, XMEMORY_RELEASE);
	}
	xrtAtomic32Store(&pTest->Released, 1, XMEMORY_RELEASE);
}



/* 验证 CLOSED 发布前已经归还所有发送所有权与预算。 */
static void testTcpFailureOrderClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testtcpfailureorder* pContext = (testtcpfailureorder*)pData;
	xnetstreamstats Stats;

	if ( (Result != XNET_RESULT_ERROR) || (pError == NULL) ||
		 (xrtNetStreamState(pStream) != XNET_STREAM_CLOSED) ||
		 (xrtAtomic32Load(
			&pContext->Released,
			XMEMORY_ACQUIRE
		 ) == 0) || !xrtNetStreamStats(pStream, &Stats) ||
		 (Stats.QueuedBytes != 0) ) {
		xrtAtomic32Store(&pContext->Failure, 1, XMEMORY_RELEASE);
	}
	xrtAtomic32Store(&pContext->Closed, 1, XMEMORY_RELEASE);
}



/* 延迟放行终态回调，保证主线程已经进入 Engine 销毁。 */
static int32 testTcpLifetimeRelease(ptr pData)
{
	testtcplifetime* pContext = (testtcplifetime*)pData;

	xrtSleep(100);
	xrtAtomic32Store(&pContext->Release, 1, XMEMORY_RELEASE);
	return 0;
}



/* 等待独立放行线程并验证其正常退出。 */
static void testTcpLifetimeJoinRelease(xthread* pThread)
{
	testRequire(xrtThreadWait(pThread) == XWAIT_OK,
		"TCP lifetime release thread wait failed");
	testRequire(xrtThreadExitCode(pThread) == 0,
		"TCP lifetime release thread failed");
	xrtThreadDestroy(pThread);
}



/* 记录 Stream 已经完成连接。 */
static void testTcpLifetimeOpen(xnetstream* pStream, ptr pData)
{
	testtcplifetime* pContext = (testtcplifetime*)pData;

	(void)pStream;
	xrtAtomic32Store(&pContext->Open, 1, XMEMORY_RELEASE);
}



/* 在 Stream CLOSED 发布后阻塞回调，放大 Engine 占用释放顺序。 */
static void testTcpLifetimeStreamClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testtcplifetime* pContext = (testtcplifetime*)pData;

	if ( (xrtNetStreamState(pStream) != XNET_STREAM_CLOSED) ||
		 (Result != XNET_RESULT_CANCELLED) || (pError != NULL) ) {
		xrtAtomic32Store(&pContext->Failure, 1, XMEMORY_RELEASE);
	}
	xrtAtomic32Store(&pContext->Entered, 1, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(
		&pContext->Release,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
	xrtAtomic32Store(&pContext->Returned, 1, XMEMORY_RELEASE);
}



/* 在 Listener CLOSED 发布后阻塞回调，放大 Engine 占用释放顺序。 */
static void testTcpLifetimeListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	testtcplifetime* pContext = (testtcplifetime*)pData;

	if ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtAtomic32Store(&pContext->Failure, 1, XMEMORY_RELEASE);
	}
	xrtAtomic32Store(&pContext->Entered, 1, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(
		&pContext->Release,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
	xrtAtomic32Store(&pContext->Returned, 1, XMEMORY_RELEASE);
}



/* Listener 生命周期测试不允许收到接受错误。 */
static void testTcpLifetimeListenerError(
	xnetlistener* pListener,
	const xerror* pError,
	ptr pData
)
{
	testtcplifetime* pContext = (testtcplifetime*)pData;

	(void)pListener;
	(void)pError;
	xrtAtomic32Store(&pContext->Failure, 1, XMEMORY_RELEASE);
}



/* 验证 Stream CLOSED 已经终结其 Engine 活动占用。 */
static void testTcpLifetimeStream(void)
{
	testtcplifetime Context;
	xnetengineconfig EngineConfig;
	xnetstreamconfig StreamConfig;
	xnetstreamevents Events;
	xnetengine* pEngine;
	xnetstream* pStream;
	xnetsocket Listener;
	xnetsocket Peer = NULL;
	xnetaddr Address;
	xthread* pRelease;

	testTcpLifetimeInit(&Context);
	memset(&Events, 0, sizeof(Events));
	Events.Open = testTcpLifetimeOpen;
	Events.Close = testTcpLifetimeStreamClose;
	Listener = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		0
	);
	testRequire(Listener != NULL, "TCP lifetime raw listener open failed");
	testRequire(xrtNetAddrLoopback(
		&Address,
		XNET_FAMILY_IPV4,
		0
	) && xrtNetSocketBind(Listener, &Address) &&
		xrtNetSocketListen(Listener, 8) &&
		xrtNetSocketLocal(Listener, &Address),
		"TCP lifetime raw listener setup failed");

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TCP_BACKEND;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TCP lifetime stream engine start failed");
	xrtNetStreamConfigInit(&StreamConfig);
	pStream = xrtNetStreamConnect(
		pEngine,
		&Address,
		0,
		&StreamConfig,
		&Events,
		&Context
	);
	testRequire(pStream != NULL, "TCP lifetime stream connect failed");
	testRequire(xrtNetSocketAccept(
		Listener,
		&Peer,
		NULL
	) == XNET_RESULT_OK, "TCP lifetime raw accept failed");
	testTcpLifetimeWait(&Context.Open, 1,
		"TCP lifetime stream open callback missing");
	testRequire(xrtNetStreamAbort(pStream),
		"TCP lifetime stream abort failed");
	testTcpLifetimeWait(&Context.Entered, 1,
		"TCP lifetime stream close callback missing");
	testRequire((xrtNetStreamState(pStream) == XNET_STREAM_CLOSED) &&
		 (xrtAtomic32Load(
			&Context.Returned,
			XMEMORY_ACQUIRE
		 ) == 0), "TCP stream CLOSED was published too late");

	xrtNetStreamDestroy(pStream);
	testRequire(xrtNetSocketClose(Peer) && xrtNetSocketClose(Listener),
		"TCP lifetime raw sockets close failed");
	pRelease = xrtThreadCreate(testTcpLifetimeRelease, &Context, 0);
	testRequire(pRelease != NULL,
		"TCP lifetime stream release thread create failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP stream CLOSED retained its Engine activity");
	testTcpLifetimeJoinRelease(pRelease);
	testRequire((xrtAtomic32Load(
		&Context.Returned,
		XMEMORY_ACQUIRE
	 ) == 1) && (xrtAtomic32Load(
		&Context.Failure,
		XMEMORY_ACQUIRE
	 ) == 0), "TCP lifetime stream terminal callback mismatch");
}



/* 验证 Listener CLOSED 已经终结其 Engine 活动占用。 */
static void testTcpLifetimeListener(void)
{
	testtcplifetime Context;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents Events;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xthread* pRelease;

	testTcpLifetimeInit(&Context);
	memset(&Events, 0, sizeof(Events));
	Events.Error = testTcpLifetimeListenerError;
	Events.Close = testTcpLifetimeListenerClose;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TCP_BACKEND;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TCP lifetime listener engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "TCP lifetime listener address failed");
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&Events,
		NULL,
		&Context
	);
	testRequire(pListener != NULL, "TCP lifetime listener create failed");
	testRequire(xrtNetListenerClose(pListener),
		"TCP lifetime listener close failed");
	testTcpLifetimeWait(&Context.Entered, 1,
		"TCP lifetime listener close callback missing");
	testRequire((xrtNetListenerState(pListener) ==
		 XNET_LISTENER_CLOSED) && (xrtAtomic32Load(
			&Context.Returned,
			XMEMORY_ACQUIRE
		 ) == 0), "TCP listener CLOSED was published too late");

	xrtNetListenerDestroy(pListener);
	pRelease = xrtThreadCreate(testTcpLifetimeRelease, &Context, 0);
	testRequire(pRelease != NULL,
		"TCP lifetime listener release thread create failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP listener CLOSED retained its Engine activity");
	testTcpLifetimeJoinRelease(pRelease);
	testRequire((xrtAtomic32Load(
		&Context.Returned,
		XMEMORY_ACQUIRE
	 ) == 1) && (xrtAtomic32Load(
		&Context.Failure,
		XMEMORY_ACQUIRE
	 ) == 0), "TCP lifetime listener terminal callback mismatch");
}



/* 验证对端复位不会越过已受理的跨线程发送。 */
static void testTcpFailureOrder(void)
{
	testtcpfailureorder Context;
	xnetengineconfig EngineConfig;
	xnetstreamconfig StreamConfig;
	xnetstreamevents Events;
	xnetengine* pEngine;
	xnetstream* pStream;
	xnetsocket Listener;
	xnetsocket Peer = NULL;
	xnetaddr Address;
	size_t iSent = 0;

	testTcpFailureOrderInit(&Context);
	memset(&Events, 0, sizeof(Events));
	Events.Open = testTcpFailureOrderOpen;
	Events.Read = testTcpFailureOrderRead;
	Events.Close = testTcpFailureOrderClose;
	Listener = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		0
	);
	testRequire(Listener != NULL,
		"TCP failure-order raw listener open failed");
	testRequire(xrtNetAddrLoopback(
		&Address,
		XNET_FAMILY_IPV4,
		0
	) && xrtNetSocketBind(Listener, &Address) &&
		xrtNetSocketListen(Listener, 8) &&
		xrtNetSocketLocal(Listener, &Address),
		"TCP failure-order raw listener setup failed");

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TCP_BACKEND;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TCP failure-order engine start failed");
	xrtNetStreamConfigInit(&StreamConfig);
	StreamConfig.ReadMode = XNET_STREAM_READ_DIRECT;
	pStream = xrtNetStreamConnect(
		pEngine,
		&Address,
		0,
		&StreamConfig,
		&Events,
		&Context
	);
	testRequire(pStream != NULL,
		"TCP failure-order stream connect failed");
	testRequire(xrtNetSocketAccept(
		Listener,
		&Peer,
		NULL
	) == XNET_RESULT_OK, "TCP failure-order raw accept failed");
	testTcpLifetimeWait(&Context.Open, 1,
		"TCP failure-order open callback missing");
	testRequire((xrtNetSocketSend(
		Peer,
		&Context.Byte,
		1,
		&iSent
	) == XNET_RESULT_OK) && (iSent == 1),
		"TCP failure-order trigger send failed");
	testTcpLifetimeWait(&Context.ReadEntered, 1,
		"TCP failure-order read callback missing");
	testRequire(xrtNetStreamSendRef(
		pStream,
		&Context.Byte,
		1,
		testTcpFailureOrderRelease,
		&Context
	) == XNET_RESULT_OK, "TCP failure-order reference send failed");
	testRequire(xrtNetSocketSet(Peer, XNET_OPTION_LINGER, 0) &&
		xrtNetSocketClose(Peer), "TCP failure-order peer reset failed");
	Peer = NULL;
	xrtAtomic32Store(&Context.ReadRelease, 1, XMEMORY_RELEASE);
	testTcpLifetimeWait(&Context.Closed, 1,
		"TCP failure-order close callback missing");
	testRequire((xrtAtomic32Load(
		&Context.Released,
		XMEMORY_ACQUIRE
	 ) == 1) && (xrtAtomic32Load(
		&Context.Failure,
		XMEMORY_ACQUIRE
	 ) == 0), "TCP failure-order terminal sequence mismatch");

	xrtNetStreamDestroy(pStream);
	testRequire(xrtNetSocketClose(Listener),
		"TCP failure-order listener close failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"TCP failure-order engine destroy failed");
}



/* 验证 TCP 两种高层对象的 CLOSED 与 Engine 生命周期契约。 */
int main(void)
{
	testTcpLifetimeStream();
	testTcpLifetimeListener();
	testTcpFailureOrder();
	printf("[PASS] network TCP %s CLOSED engine lifetime\n",
		TEST_TCP_BACKEND_NAME);
	return 0;
}

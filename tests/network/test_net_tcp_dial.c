#include "../test.h"



#if !defined(TEST_TCP_DIAL_BACKEND)
	#define TEST_TCP_DIAL_BACKEND XNET_PORT_SELECT
#endif



typedef struct testdialcontext {
	xatomic32 LookupEntered;
	xatomic32 LookupGate;
	xatomic32 Accepted;
	xatomic32 Done;
	xatomic32 Open;
	xatomic32 Closed;
	xatomic32 ListenerClosed;
	xnetresult Result;
	xnetstream* Client;
	xnetstream* OpenedStream;
	xnetstream* Server;
	xnetdial* Dial;
	bool CancelAcceptedDuringOpen;
	bool DoneAfterOpen;
} testdialcontext;



/* 在测试截止时间内等待一个原子计数达到下限。 */
static void testDialWait(
	const xatomic32* pValue,
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



/* 构造 IPv6 优先、IPv4 备用的端口无关解析结果。 */
static xnetaddrlist* testDialLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	testdialcontext* pContext = (testdialcontext*)pData;
	xnetaddr Addresses[2];
	size_t iCount = 0;

	(void)sHost;
	(void)xrtAtomic32FetchAdd(
		&pContext->LookupEntered,
		1,
		XMEMORY_RELEASE
	);
	while ( xrtAtomic32Load(
		&pContext->LookupGate,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
	if ( Family != XNET_FAMILY_IPV4 ) {
		testRequire(xrtNetAddrLoopback(
			&Addresses[iCount++],
			XNET_FAMILY_IPV6,
			0
		), "dial IPv6 fixture failed");
	}
	if ( Family != XNET_FAMILY_IPV6 ) {
		testRequire(xrtNetAddrLoopback(
			&Addresses[iCount++],
			XNET_FAMILY_IPV4,
			0
		), "dial IPv4 fixture failed");
	}
	return xrtNetAddrListCreate(Addresses, iCount);
}



/* 接管 IPv4 Listener 接受的服务端连接。 */
static bool testDialAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testdialcontext* pContext = (testdialcontext*)pData;

	(void)pListener;
	testRequire(xrtNetStreamSetData(pStream, pContext),
		"dial accepted stream data setup failed");
	pContext->Server = pStream;
	(void)xrtAtomic32FetchAdd(
		&pContext->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 验证托管连接不会让失败候选提前发布用户 Open。 */
static void testDialOpen(xnetstream* pStream, ptr pData)
{
	testdialcontext* pContext = (testdialcontext*)pData;

	testRequire(xrtAtomic32Load(
		&pContext->Done,
		XMEMORY_ACQUIRE
	) == 0, "dial Done was published before Open completed");
	pContext->OpenedStream = pStream;
	pContext->CancelAcceptedDuringOpen =
		xrtNetDialCancel(pContext->Dial);
	(void)xrtAtomic32FetchAdd(
		&pContext->Open,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录两个已公开 Stream 的唯一关闭回调。 */
static void testDialClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testdialcontext* pContext = (testdialcontext*)pData;

	(void)pStream;
	testRequire((Result == XNET_RESULT_OK) && (pError == NULL),
		"dial stream normal close reported an error");
	(void)xrtAtomic32FetchAdd(
		&pContext->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 成功回调接管 Stream，并验证它尚未越过公开 Open 边界。 */
static void testDialDone(
	xnetdial* pDial,
	xnetresult Result,
	xnetstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	testdialcontext* pContext = (testdialcontext*)pData;

	testRequire(xrtNetWorkerIsCurrent(xrtNetStreamWorker(pStream)),
		"dial Done callback worker mismatch");
	testRequire((Result == XNET_RESULT_OK) && (pStream != NULL) &&
		 (pError == NULL) &&
		 (pStream == pContext->OpenedStream) &&
		 (xrtNetDialState(pDial) == XNET_DIAL_CONNECTED),
		"dial success terminal mismatch");
	pContext->Result = Result;
	pContext->Client = pStream;
	pContext->DoneAfterOpen =
		(xrtNetStreamState(pStream) == XNET_STREAM_OPEN) &&
		(xrtAtomic32Load(&pContext->Open, XMEMORY_ACQUIRE) == 1);
	(void)xrtAtomic32FetchAdd(
		&pContext->Done,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 Listener 已经释放全部接受资源。 */
static void testDialListenerClose(xnetlistener* pListener, ptr pData)
{
	testdialcontext* pContext = (testdialcontext*)pData;

	(void)pListener;
	(void)xrtAtomic32FetchAdd(
		&pContext->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证双栈交错、失败回退、发布顺序、统计和完整回收。 */
int main(void)
{
	testdialcontext Context;
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents StreamEvents;
	xnetstreamevents ServerEvents;
	xnetdialconfig DialConfig;
	xnetdialstats DialStats;
	xnetenginestats EngineStats;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	xnetdial* pDial;
	xnetaddr Address;
	xnetaddr Remote;

	memset(&Context, 0, sizeof(Context));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&StreamEvents, 0, sizeof(StreamEvents));
	memset(&ServerEvents, 0, sizeof(ServerEvents));
	ListenerEvents.Accept = testDialAccept;
	ListenerEvents.Close = testDialListenerClose;
	StreamEvents.Open = testDialOpen;
	StreamEvents.Close = testDialClose;
	ServerEvents.Close = testDialClose;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TCP_DIAL_BACKEND;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"dial engine start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Lookup = testDialLookup;
	ResolverConfig.LookupData = &Context;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL, "dial resolver create failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "dial listener address failed");
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		&ServerEvents,
		&Context
	);
	testRequire((pListener != NULL) &&
		xrtNetListenerLocal(pListener, &Address),
		"dial listener create failed");
	xrtNetDialConfigInit(&DialConfig);
	DialConfig.Affinity = 0;
	DialConfig.FallbackDelay = 1000u;
	DialConfig.MaxAttempts = 2;
	pDial = xrtNetDial(
		pEngine,
		pResolver,
		"dual.test",
		Address.Port,
		&DialConfig,
		&StreamEvents,
		&Context,
		testDialDone,
		&Context
	);
	testRequire(pDial != NULL, "managed TCP dial submit failed");
	Context.Dial = pDial;
	xrtAtomic32Store(
		&Context.LookupGate,
		1,
		XMEMORY_RELEASE
	);
	testDialWait(&Context.Done, 1, "managed TCP dial did not finish");
	testDialWait(&Context.Open, 1, "managed TCP dial did not publish Open");
	testDialWait(&Context.Accepted, 1, "managed TCP dial was not accepted");
	testRequire(Context.DoneAfterOpen &&
		!Context.CancelAcceptedDuringOpen &&
		xrtNetDialStats(pDial, &DialStats) &&
		(DialStats.State == XNET_DIAL_CONNECTED) &&
		(DialStats.Addresses == 2) &&
		(DialStats.AttemptsStarted == 2) &&
		(DialStats.AttemptsFailed == 1) &&
		(DialStats.ActiveAttempts == 0) &&
		DialStats.HasWinner && (DialStats.WinnerIndex == 1),
		"managed TCP dial statistics mismatch");
	testRequire(xrtNetEngineStats(pEngine, &EngineStats) &&
		(EngineStats.ActiveTimers == 0),
		"managed TCP dial retained a terminal timer");
	testRequire(xrtNetStreamRemote(Context.Client, &Remote) &&
		(Remote.Family == XNET_FAMILY_IPV4),
		"managed TCP dial selected the wrong address family");
	testRequire(xrtNetStreamClose(Context.Client) &&
		xrtNetStreamClose(Context.Server),
		"managed TCP dial stream close failed");
	testDialWait(&Context.Closed, 2,
		"managed TCP dial streams did not close");
	testRequire(xrtNetListenerClose(pListener),
		"managed TCP dial listener close failed");
	testDialWait(&Context.ListenerClosed, 1,
		"managed TCP dial listener did not close");
	xrtNetStreamDestroy(Context.Client);
	xrtNetStreamDestroy(Context.Server);
	xrtNetDialDestroy(pDial);
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"managed TCP dial resolver destroy failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"managed TCP dial engine destroy failed");
	printf("[PASS] managed TCP dial fallback lifecycle\n");
	return 0;
}

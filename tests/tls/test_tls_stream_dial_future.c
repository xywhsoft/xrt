#include "../fixtures/tls_server.h"



#if !defined(TEST_TLS_DIAL_FUTURE_BACKEND)
	#define TEST_TLS_DIAL_FUTURE_BACKEND XNET_PORT_SELECT
#endif



/* Future 成功路径复用真实 TLS 服务端并记录公开事件顺序。 */
typedef struct test_tls_dial_future {
	xtlsserverconfig ServerConfig;
	xtlsstreamconfig StreamConfig;
	xtlsstreamevents ClientEvents;
	xtlsstreamevents ServerEvents;
	xtlsstream* Client;
	xtlsstream* Server;
	xatomic32 Accepted;
	xatomic32 ClientOpen;
	xatomic32 ServerOpen;
	xatomic32 ClientEnd;
	xatomic32 ServerEnd;
	xatomic32 Closed;
	xatomic32 ListenerClosed;
	xatomic32 VerifiedName;
} test_tls_dial_future;



#if defined(TEST_TLS_DIAL_FUTURE_COROUTINE)

/* 协程只保存一次通用 Future 等待的输入和结果。 */
typedef struct test_tls_dial_future_await {
	xfuture* Future;
	xwaitresult Result;
} test_tls_dial_future_await;



/* 在调度协程中等待 TLS Dial Future，不引入 TLS 专用协程 API。 */
static ptr testTlsDialFutureAwaitProc(ptr pData)
{
	test_tls_dial_future_await* pAwait =
		(test_tls_dial_future_await*)pData;

	pAwait->Result = xrtFutureAwaitFor(
		pAwait->Future,
		UINT64_C(10000000)
	);
	return pAwait;
}



/* 建立一次协程调度并等待目标 Future 进入终态。 */
static void testTlsDialFutureWaitFuture(xfuture* pFuture)
{
	test_tls_dial_future_await Await;
	xcosched* pScheduler;
	xcoro* pCoroutine;

	memset(&Await, 0, sizeof(Await));
	Await.Future = pFuture;
	pScheduler = xrtCoSchedCreate();
	testRequire(pScheduler != NULL,
		"TLS Dial Future coroutine scheduler creation failed");
	pCoroutine = xrtCoSpawn(
		pScheduler,
		testTlsDialFutureAwaitProc,
		&Await,
		NULL
	);
	testRequire((pCoroutine != NULL) &&
		xrtCoSchedRun(pScheduler) &&
		(Await.Result == XWAIT_OK) &&
		(xrtCoResult(pCoroutine) == &Await),
		"TLS Dial Future coroutine await failed");
	testRequire(xrtCoDestroy(pCoroutine) &&
		xrtCoSchedDestroy(pScheduler),
		"TLS Dial Future coroutine cleanup failed");
}

#else

/* 普通线程在固定截止时间内等待目标 Future。 */
static void testTlsDialFutureWaitFuture(xfuture* pFuture)
{
	testRequire(xrtFutureWaitFor(
		pFuture,
		UINT64_C(10000000)
	) == XWAIT_OK, "TLS Dial Future wait timed out");
}

#endif



/* 在测试截止时间前等待原子计数达到给定下限。 */
static void testTlsDialFutureWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(10000000));

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}



/* 返回 IPv6 优先、IPv4 备用的确定性解析结果。 */
static xnetaddrlist* testTlsDialFutureLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Addresses[2];
	size_t iCount = 0;

	(void)pData;
	testRequire(strcmp(sHost, "future.tls.test") == 0,
		"TLS Dial Future resolver received the wrong host");
	if ( Family != XNET_FAMILY_IPV4 ) {
		testRequire(xrtNetAddrLoopback(
			&Addresses[iCount++],
			XNET_FAMILY_IPV6,
			0
		), "TLS Dial Future IPv6 fixture failed");
	}
	if ( Family != XNET_FAMILY_IPV6 ) {
		testRequire(xrtNetAddrLoopback(
			&Addresses[iCount++],
			XNET_FAMILY_IPV4,
			0
		), "TLS Dial Future IPv4 fixture failed");
	}
	return xrtNetAddrListCreate(Addresses, iCount);
}



/* 验证自动 SNI 与证书名称来自拨号主机名。 */
static xtlsverifydecision testTlsDialFutureVerify(
	const xtlspeer* pPeer,
	ptr pData
)
{
	test_tls_dial_future* pContext =
		(test_tls_dial_future*)pData;
	static const char Name[] = "future.tls.test";

	if ( (pPeer == NULL) || (pPeer->Role != XTLS_SERVER) ||
		(pPeer->CertificateCount == 0) ||
		(pPeer->Name.Size != (sizeof(Name) - 1u)) ||
		(memcmp(
			pPeer->Name.Data,
			Name,
			sizeof(Name) - 1u
		) != 0) ) {
		return XTLS_VERIFY_REJECT;
	}
	(void)xrtAtomic32FetchAdd(
		&pContext->VerifiedName,
		1,
		XMEMORY_RELEASE
	);
	return XTLS_VERIFY_ACCEPT;
}



/* 服务端 Open 只在 TLS 握手完成后发布。 */
static void testTlsDialFutureServerOpen(
	xtlsstream* pStream,
	ptr pData
)
{
	test_tls_dial_future* pContext =
		(test_tls_dial_future*)pData;

	testRequire(pStream == pContext->Server,
		"TLS Dial Future server Open stream mismatch");
	(void)xrtAtomic32FetchAdd(
		&pContext->ServerOpen,
		1,
		XMEMORY_RELEASE
	);
}



/* 客户端 Open 必须先于 Future 成功终态。 */
static void testTlsDialFutureClientOpen(
	xtlsstream* pStream,
	ptr pData
)
{
	test_tls_dial_future* pContext =
		(test_tls_dial_future*)pData;

	testRequire(xrtTlsStreamState(pStream) == XTLS_STREAM_OPEN,
		"TLS Dial Future client Open state mismatch");
	(void)xrtAtomic32FetchAdd(
		&pContext->ClientOpen,
		1,
		XMEMORY_RELEASE
	);
}



/* 收到认证 close_notify 后补齐本端认证关闭。 */
static void testTlsDialFutureEnd(xtlsstream* pStream, ptr pData)
{
	test_tls_dial_future* pContext =
		(test_tls_dial_future*)pData;
	xatomic32* pEnd = pStream == pContext->Client ?
		&pContext->ClientEnd : &pContext->ServerEnd;

	(void)xrtAtomic32FetchAdd(pEnd, 1, XMEMORY_RELEASE);
	testRequire(xrtTlsStreamClose(pStream),
		"TLS Dial Future authenticated close response failed");
}



/* 两端正常关闭都必须保留空错误根因。 */
static void testTlsDialFutureClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_tls_dial_future* pContext =
		(test_tls_dial_future*)pData;

	(void)pStream;
	testRequire((Result == XNET_RESULT_OK) && (pError == NULL),
		"TLS Dial Future normal close reported an error");
	(void)xrtAtomic32FetchAdd(
		&pContext->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* Listener 接受的 TCP Stream 由 TLS 服务端组合层接管。 */
static bool testTlsDialFutureAccept(
	xnetlistener* pListener,
	xnetstream* pTransport,
	ptr pData
)
{
	test_tls_dial_future* pContext =
		(test_tls_dial_future*)pData;
	bool bAccepted;

	(void)pListener;
	bAccepted = xrtTlsStreamAccept(
		pTransport,
		&pContext->ServerConfig,
		&pContext->StreamConfig,
		&pContext->ServerEvents,
		pContext,
		&pContext->Server
	);
	if ( bAccepted ) {
		(void)xrtAtomic32FetchAdd(
			&pContext->Accepted,
			1,
			XMEMORY_RELEASE
		);
	}
	return bAccepted;
}



/* Listener 关闭表示全部预投递 Accept 已经回收。 */
static void testTlsDialFutureListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_tls_dial_future* pContext =
		(test_tls_dial_future*)pData;

	(void)pListener;
	(void)xrtAtomic32FetchAdd(
		&pContext->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 覆盖 SNI、双栈回退、Open 顺序、Future 所有权和认证关闭。 */
int main(void)
{
	test_tls_dial_future Test;
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xtlsverifierconfig VerifierConfig;
	xtlsclientconfig ClientConfig;
	xtlsdialconfig DialConfig;
	xtlscontext* pTlsContext;
	xtlsidentity* pIdentity;
	xtlsverifier* pVerifier;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	xfuture* pFuture;
	xnetaddr Address;

	memset(&Test, 0, sizeof(Test));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	Test.ClientEvents.Open = testTlsDialFutureClientOpen;
	Test.ClientEvents.End = testTlsDialFutureEnd;
	Test.ClientEvents.Close = testTlsDialFutureClose;
	Test.ServerEvents.Open = testTlsDialFutureServerOpen;
	Test.ServerEvents.End = testTlsDialFutureEnd;
	Test.ServerEvents.Close = testTlsDialFutureClose;
	ListenerEvents.Accept = testTlsDialFutureAccept;
	ListenerEvents.Close = testTlsDialFutureListenerClose;
	pTlsContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	testRequire((pTlsContext != NULL) && (pIdentity != NULL),
		"TLS Dial Future fixture creation failed");
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsDialFutureVerify;
	VerifierConfig.Context = &Test;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(pVerifier != NULL,
		"TLS Dial Future verifier creation failed");
	xrtTlsServerConfigInit(&Test.ServerConfig);
	Test.ServerConfig.Context = pTlsContext;
	Test.ServerConfig.Identity = pIdentity;
	xrtTlsStreamConfigInit(&Test.StreamConfig);
	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.Context = pTlsContext;
	ClientConfig.Verifier = pVerifier;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TLS_DIAL_FUTURE_BACKEND;
	EngineConfig.Workers = 2u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TLS Dial Future engine start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Lookup = testTlsDialFutureLookup;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL,
		"TLS Dial Future resolver create failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "TLS Dial Future listener address setup failed");
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		NULL,
		&Test
	);
	testRequire((pListener != NULL) &&
		xrtNetListenerLocal(pListener, &Address),
		"TLS Dial Future listener creation failed");

	xrtTlsDialConfigInit(&DialConfig);
	DialConfig.Transport.Affinity = 1u;
	DialConfig.Transport.FallbackDelay = 1000u;
	DialConfig.Transport.MaxAttempts = 2u;
	DialConfig.Timeout = UINT64_C(10000000);
	pFuture = xrtTlsDialAsync(
		pEngine,
		pResolver,
		"future.tls.test",
		Address.Port,
		&ClientConfig,
		&DialConfig,
		&Test.ClientEvents,
		&Test
	);
	testRequire(pFuture != NULL,
		"TLS Dial Future submit failed");
	testTlsDialFutureWaitFuture(pFuture);
	testRequire((xrtFutureState(pFuture) == XFUTURE_RESOLVED) &&
		(xrtAtomic32Load(
			&Test.ClientOpen,
			XMEMORY_ACQUIRE
		) == 1) &&
		(xrtAtomic32Load(
			&Test.VerifiedName,
			XMEMORY_ACQUIRE
		) == 1),
		"TLS Dial Future resolved before authenticated Open");
	Test.Client = xrtTlsStreamRef(
		(xtlsstream*)xrtFutureValue(pFuture)
	);
	testRequire((Test.Client != NULL) &&
		(xrtTlsStreamState(Test.Client) == XTLS_STREAM_OPEN),
		"TLS Dial Future success value mismatch");
	xrtFutureDestroy(pFuture);
	testRequire(xrtTlsStreamState(Test.Client) == XTLS_STREAM_OPEN,
		"destroying TLS Dial Future affected retained Stream");
	testTlsDialFutureWait(&Test.Accepted, 1u,
		"TLS Dial Future server was not accepted");
	testTlsDialFutureWait(&Test.ServerOpen, 1u,
		"TLS Dial Future server Open callback missing");

	testRequire(xrtTlsStreamClose(Test.Client),
		"TLS Dial Future client close failed");
	testTlsDialFutureWait(&Test.ClientEnd, 1u,
		"TLS Dial Future client close_notify missing");
	testTlsDialFutureWait(&Test.ServerEnd, 1u,
		"TLS Dial Future server close_notify missing");
	testTlsDialFutureWait(&Test.Closed, 2u,
		"TLS Dial Future streams did not close");
	testRequire(xrtNetListenerClose(pListener),
		"TLS Dial Future listener close failed");
	testTlsDialFutureWait(&Test.ListenerClosed, 1u,
		"TLS Dial Future listener did not close");

	xrtTlsStreamDestroy(Test.Client);
	xrtTlsStreamDestroy(Test.Server);
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"TLS Dial Future resolver destroy failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"TLS Dial Future engine destroy failed");
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pTlsContext);
	printf("[PASS] TLS Dial Future lifecycle\n");
	return 0;
}

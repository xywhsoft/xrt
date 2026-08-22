#include "../fixtures/tls_server.h"



#if !defined(TEST_TLS_DIAL_BACKEND)
	#define TEST_TLS_DIAL_BACKEND XNET_PORT_SELECT
#endif



typedef struct test_tls_dial_context {
	xtlsserverconfig ServerConfig;
	xtlsstreamconfig StreamConfig;
	xtlsstreamevents ClientEvents;
	xtlsstreamevents ServerEvents;
	xtlsstream* Client;
	xtlsstream* Server;
	xatomic32 Accepted;
	xatomic32 ClientOpenEntered;
	xatomic32 ClientOpenRelease;
	xatomic32 ClientOpen;
	xatomic32 ServerOpen;
	xatomic32 Done;
	xatomic32 ClientEnd;
	xatomic32 ServerEnd;
	xatomic32 Closed;
	xatomic32 ListenerClosed;
	xatomic32 VerifiedName;
	xnetresult Result;
	bool DoneAfterOpen;
} test_tls_dial_context;



/* 在测试截止时间前等待原子计数达到给定下限。 */
static void testTlsDialWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(10000000u);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}



/* 等待服务端握手发布，并在超时时输出跨 Worker 调度状态。 */
static void testTlsDialWaitServerOpen(test_tls_dial_context* pContext)
{
	xdeadline Deadline = xrtDeadlineAfter(10000000u);

	while ( xrtAtomic32Load(
		&pContext->ServerOpen,
		XMEMORY_ACQUIRE
	) == 0 ) {
		if ( xrtDeadlineExpired(Deadline) ) {
			xnetstream* pTransport = (pContext->Server != NULL) ?
				xrtTlsStreamTransport(pContext->Server) : NULL;

			fprintf(
				stderr,
				"[DIAG] TLS dial server Open: accepted=%u client-entered=%u "
				"done=%u closed=%u server-state=%d server-worker=%u\n",
				xrtAtomic32Load(&pContext->Accepted, XMEMORY_ACQUIRE),
				xrtAtomic32Load(
					&pContext->ClientOpenEntered,
					XMEMORY_ACQUIRE
				),
				xrtAtomic32Load(&pContext->Done, XMEMORY_ACQUIRE),
				xrtAtomic32Load(&pContext->Closed, XMEMORY_ACQUIRE),
				(pContext->Server != NULL) ?
					(int)xrtTlsStreamState(pContext->Server) : -1,
				(pTransport != NULL) ?
					xrtNetWorkerIndex(xrtNetStreamWorker(pTransport)) :
					UINT32_MAX
			);
			testRequire(false, "TLS dial server Open callback missing");
		}
		xrtThreadYield();
	}
}



/* 返回 IPv6 优先、IPv4 备用的确定性解析结果。 */
static xnetaddrlist* testTlsDialLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Addresses[2];
	size_t iCount = 0;

	(void)pData;
	testRequire(strcmp(sHost, "dual.tls.test") == 0,
		"TLS dial resolver received the wrong host");
	if ( Family != XNET_FAMILY_IPV4 ) {
		testRequire(xrtNetAddrLoopback(
			&Addresses[iCount++],
			XNET_FAMILY_IPV6,
			0
		), "TLS dial IPv6 fixture failed");
	}
	if ( Family != XNET_FAMILY_IPV6 ) {
		testRequire(xrtNetAddrLoopback(
			&Addresses[iCount++],
			XNET_FAMILY_IPV4,
			0
		), "TLS dial IPv4 fixture failed");
	}
	return xrtNetAddrListCreate(Addresses, iCount);
}



/* 验证默认 ServerName 确实来自拨号主机而不是数字候选地址。 */
static xtlsverifydecision testTlsDialVerify(
	const xtlspeer* pPeer,
	ptr pData
)
{
	test_tls_dial_context* pContext =
		(test_tls_dial_context*)pData;
	static const char Name[] = "dual.tls.test";

	if ( (pPeer == NULL) || (pPeer->Role != XTLS_SERVER) ||
		(pPeer->CertificateCount == 0) ||
		(pPeer->Name.Size != (sizeof(Name) - 1u)) ||
		(memcmp(pPeer->Name.Data, Name, sizeof(Name) - 1u) != 0) ) {
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
static void testTlsDialServerOpen(xtlsstream* pStream, ptr pData)
{
	test_tls_dial_context* pContext =
		(test_tls_dial_context*)pData;

	testRequire(pStream == pContext->Server,
		"TLS dial server Open stream mismatch");
	(void)xrtAtomic32FetchAdd(
		&pContext->ServerOpen,
		1,
		XMEMORY_RELEASE
	);
}



/* 客户端 Open 必须先于受管 Dial 完成回调。 */
static void testTlsDialClientOpen(xtlsstream* pStream, ptr pData)
{
	test_tls_dial_context* pContext =
		(test_tls_dial_context*)pData;

	(void)xrtAtomic32FetchAdd(
		&pContext->ClientOpenEntered,
		1,
		XMEMORY_RELEASE
	);
	while ( xrtAtomic32Load(
		&pContext->ClientOpenRelease,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
	testRequire(xrtAtomic32Load(
		&pContext->Done,
		XMEMORY_ACQUIRE
	) == 0, "TLS dial Done was published before Open completed");
	testRequire(xrtTlsStreamState(pStream) == XTLS_STREAM_OPEN,
		"TLS dial client Open state mismatch");
	(void)xrtAtomic32FetchAdd(
		&pContext->ClientOpen,
		1,
		XMEMORY_RELEASE
	);
}



/* 收到对端 close_notify 后补齐本端认证关闭。 */
static void testTlsDialEnd(xtlsstream* pStream, ptr pData)
{
	test_tls_dial_context* pContext =
		(test_tls_dial_context*)pData;
	xatomic32* pEnd = pStream == pContext->Client ?
		&pContext->ClientEnd : &pContext->ServerEnd;

	(void)xrtAtomic32FetchAdd(pEnd, 1, XMEMORY_RELEASE);
	testRequire(xrtTlsStreamClose(pStream),
		"TLS dial authenticated close response failed");
}



/* 两端正常关闭都必须保留空错误根因。 */
static void testTlsDialClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_tls_dial_context* pContext =
		(test_tls_dial_context*)pData;

	(void)pStream;
	testRequire((Result == XNET_RESULT_OK) && (pError == NULL),
		"TLS dial stream normal close reported an error");
	(void)xrtAtomic32FetchAdd(
		&pContext->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* Listener 接受的原始 TCP Stream 必须被 TLS 服务端组合层接管。 */
static bool testTlsDialAccept(
	xnetlistener* pListener,
	xnetstream* pTransport,
	ptr pData
)
{
	test_tls_dial_context* pContext =
		(test_tls_dial_context*)pData;
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



/* TLS Dial 成功回调接管唯一客户端 Stream 引用。 */
static void testTlsDialDone(
	xtlsdial* pDial,
	xnetresult Result,
	xtlsstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	test_tls_dial_context* pContext =
		(test_tls_dial_context*)pData;

	testRequire((Result == XNET_RESULT_OK) &&
		(pStream != NULL) && (pError == NULL) &&
		(xrtTlsDialState(pDial) == XTLS_DIAL_CONNECTED),
		"TLS dial success terminal mismatch");
	pContext->Result = Result;
	pContext->Client = pStream;
	pContext->DoneAfterOpen = xrtAtomic32Load(
		&pContext->ClientOpen,
		XMEMORY_ACQUIRE
	) == 1;
	(void)xrtAtomic32FetchAdd(
		&pContext->Done,
		1,
		XMEMORY_RELEASE
	);
}



/* Listener 关闭表明全部预投递 Accept 已经回收。 */
static void testTlsDialListenerClose(xnetlistener* pListener, ptr pData)
{
	test_tls_dial_context* pContext =
		(test_tls_dial_context*)pData;

	(void)pListener;
	(void)xrtAtomic32FetchAdd(
		&pContext->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 覆盖主机名、地址回退、验证名称、发布顺序、统计和认证关闭。 */
int main(void)
{
	test_tls_dial_context Test;
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xtlsverifierconfig VerifierConfig;
	xtlsclientconfig ClientConfig;
	xtlsdialconfig DialConfig;
	xnetdialstats TransportStats;
	xtlscontext* pTlsContext;
	xtlsidentity* pIdentity;
	xtlsverifier* pVerifier;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	xtlsdial* pDial;
	xnetaddr Address;

	memset(&Test, 0, sizeof(Test));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	Test.ClientEvents.Open = testTlsDialClientOpen;
	Test.ClientEvents.End = testTlsDialEnd;
	Test.ClientEvents.Close = testTlsDialClose;
	Test.ServerEvents.Open = testTlsDialServerOpen;
	Test.ServerEvents.End = testTlsDialEnd;
	Test.ServerEvents.Close = testTlsDialClose;
	ListenerEvents.Accept = testTlsDialAccept;
	ListenerEvents.Close = testTlsDialListenerClose;
	pTlsContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	testRequire((pTlsContext != NULL) && (pIdentity != NULL),
		"TLS dial fixture creation failed");
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsDialVerify;
	VerifierConfig.Context = &Test;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(pVerifier != NULL, "TLS dial verifier creation failed");
	xrtTlsServerConfigInit(&Test.ServerConfig);
	Test.ServerConfig.Context = pTlsContext;
	Test.ServerConfig.Identity = pIdentity;
	xrtTlsStreamConfigInit(&Test.StreamConfig);
	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.Context = pTlsContext;
	ClientConfig.Verifier = pVerifier;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TLS_DIAL_BACKEND;
	EngineConfig.Workers = 2u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TLS dial engine start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Lookup = testTlsDialLookup;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL, "TLS dial resolver create failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "TLS dial listener address setup failed");
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		NULL,
		&Test
	);
	testRequire((pListener != NULL) &&
		xrtNetListenerLocal(pListener, &Address),
		"TLS dial listener creation failed");
	xrtTlsDialConfigInit(&DialConfig);
	DialConfig.Transport.Affinity = 1u;
	DialConfig.Transport.FallbackDelay = 1000u;
	DialConfig.Transport.MaxAttempts = 2u;
	DialConfig.Timeout = 10000000u;
	pDial = xrtTlsDial(
		pEngine,
		pResolver,
		"dual.tls.test",
		Address.Port,
		&ClientConfig,
		&DialConfig,
		&Test.ClientEvents,
		&Test,
		testTlsDialDone,
		&Test
	);
	testRequire(pDial != NULL, "TLS hostname dial submit failed");
	testTlsDialWait(&Test.Accepted, 1u,
		"TLS dial server was not accepted");
	testTlsDialWait(&Test.ClientOpenEntered, 1u,
		"TLS dial client Open callback did not start");
	testRequire(!xrtTlsDialCancel(pDial),
		"TLS dial accepted cancellation after secure Open won");
	xrtAtomic32Store(
		&Test.ClientOpenRelease,
		1,
		XMEMORY_RELEASE
	);
	testTlsDialWaitServerOpen(&Test);
	testTlsDialWait(&Test.Done, 1u,
		"TLS hostname dial did not finish");
	testRequire(Test.DoneAfterOpen &&
		(Test.Result == XNET_RESULT_OK) &&
		(xrtAtomic32Load(&Test.VerifiedName, XMEMORY_ACQUIRE) == 1) &&
		(xrtTlsDialError(pDial) == NULL) &&
		xrtTlsDialTransportStats(pDial, &TransportStats) &&
		(TransportStats.Addresses == 2u) &&
		(TransportStats.AttemptsStarted == 2u) &&
		(TransportStats.AttemptsFailed == 1u) &&
		TransportStats.HasWinner && (TransportStats.WinnerIndex == 1u),
		"TLS hostname dial contract mismatch");
	testRequire(xrtTlsStreamClose(Test.Client),
		"TLS dial client close failed");
	testTlsDialWait(&Test.ClientEnd, 1u,
		"TLS dial client close_notify missing");
	testTlsDialWait(&Test.ServerEnd, 1u,
		"TLS dial server close_notify missing");
	testTlsDialWait(&Test.Closed, 2u,
		"TLS dial streams did not close");
	testRequire(xrtNetListenerClose(pListener),
		"TLS dial listener close failed");
	testTlsDialWait(&Test.ListenerClosed, 1u,
		"TLS dial listener did not close");
	xrtTlsStreamDestroy(Test.Client);
	xrtTlsStreamDestroy(Test.Server);
	xrtTlsDialDestroy(pDial);
	xrtNetListenerDestroy(pListener);
	xrtNetResolverDestroy(pResolver);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TLS dial engine destroy failed");
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pTlsContext);
	printf("[PASS] TLS hostname dial lifecycle\n");
	return 0;
}

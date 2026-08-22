#include "../fixtures/http_connect_proxy.h"
#include "../fixtures/tls_server.h"



#if !defined(TEST_HTTP_CLIENT_PROXY_HTTPS_BACKEND)
	#define TEST_HTTP_CLIENT_PROXY_HTTPS_BACKEND XNET_PORT_SELECT
	#define TEST_HTTP_CLIENT_PROXY_HTTPS_BACKEND_NAME "select"
#endif



#if !defined(TEST_HTTP_CLIENT_PROXY_HTTPS_RESUME)
	#define TEST_HTTP_CLIENT_PROXY_HTTPS_RESUME 0
#endif



#if TEST_HTTP_CLIENT_PROXY_HTTPS_RESUME
	#define TEST_HTTP_CLIENT_PROXY_HTTPS_ROUNDS 2u
#else
	#define TEST_HTTP_CLIENT_PROXY_HTTPS_ROUNDS 1u
#endif



/* 夹具覆盖 CONNECT、TLS 1.3、SNI、ALPN、HTTP 和认证关闭。 */
typedef struct test_http_proxy_https {
	xnetengine* Engine;
	xnetlistener* Listener;
	xtlsstream* Server;
	xhttpclient* Client;
	xhttpcall* Call;
	xhttpresponse* Response;
	#if TEST_HTTP_CLIENT_PROXY_HTTPS_RESUME
		xtlsresume* ServerResume;
	#endif
	xtlsserverconfig ServerConfig;
	xtlsstreamconfig StreamConfig;
	xatomic32 Round;
	xatomic32 Accepted;
	xatomic32 Connected;
	xatomic32 TlsOpened;
	xatomic32 Completed;
	xatomic32 ServerClosed;
	xatomic32 ListenerClosed;
	uint16 Port;
	bool Responded;
} test_http_proxy_https;



/* 在统一截止时间内等待代理、TLS 或 HTTP Worker 发布状态。 */
static void testHttpProxyHttpsWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(10000000u);

	while ( xrtAtomic32Load(
		pValue,
		XMEMORY_ACQUIRE
	) < iExpected ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			sMessage
		);
		xrtThreadYield();
	}
}



/* 等待连接池和 TLS 认证关闭释放最后一个 Engine 对象。 */
static void testHttpProxyHttpsEngineDestroy(xnetengine* pEngine)
{
	xdeadline Deadline = xrtDeadlineAfter(10000000u);

	while ( !xrtNetEngineDestroy(pEngine) ) {
		const xerror* pError = xrtGetError();

		testRequire(
			(pError != NULL) &&
			(xrtErrorKind(pError) == XERR_STATE) &&
			(xrtErrorCode(pError) == XNET_ERROR_ENGINE_STOP),
			"HTTPS proxy engine destroy reported an unexpected error"
		);
		xrtClearError();
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTPS proxy retained an Engine object"
		);
		xrtThreadYield();
	}
}



/* 只允许解析代理端点，目标域名必须由 CONNECT 远端解析。 */
static xnetaddrlist* testHttpProxyHttpsLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)pData;
	testRequire(
		strcmp(sHost, "proxy.test") == 0,
		"HTTPS proxy unexpectedly resolved the target host"
	);
	if ( Family == XNET_FAMILY_IPV6 ) {
		return xrtNetAddrListCreate(NULL, 0);
	}
	testRequire(
		xrtNetAddrLoopback(
			&Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTPS proxy resolver fixture failed"
	);
	return xrtNetAddrListCreate(&Address, 1);
}



#if TEST_HTTP_CLIENT_PROXY_HTTPS_RESUME

/* 服务端按代理目标路由返回上一轮保存的 TLS 恢复对象。 */
static const xtlsresume* testHttpProxyHttpsResumeLookup(
	ptr pData,
	const xtlsserverresumerequest* pRequest
)
{
	test_http_proxy_https* pState =
		(test_http_proxy_https*)pData;

	testRequire(
		(pRequest != NULL) &&
		(pRequest->Ticket.Size != 0) &&
		(pState->ServerResume != NULL),
		"HTTPS proxy resume lookup request is invalid"
	);
	return pState->ServerResume;
}



/* 签发下一轮代理隧道内握手使用的单次 ticket。 */
static void testHttpProxyHttpsIssueTicket(
	test_http_proxy_https* pState,
	xtlssession* pSession
)
{
	xtlsresume* pResume = NULL;

	testRequire(
		(xrtTlsServerTicketNew(pSession, &pResume) == XTLS_OK) &&
		(pResume != NULL),
		"HTTPS proxy resume ticket issue failed"
	);
	xrtTlsResumeRelease(pState->ServerResume);
	pState->ServerResume = pResume;
}

#endif



/* TLS Open 必须保留目标 SNI 并协商 http/1.1。 */
static void testHttpProxyHttpsTlsOpen(
	xtlsstream* pStream,
	ptr pData
)
{
	test_http_proxy_https* pState =
		(test_http_proxy_https*)pData;
	xtlssession* pSession = xrtTlsStreamSession(pStream);
	xbytesview ServerName;
	xbytesview Protocol;
	#if TEST_HTTP_CLIENT_PROXY_HTTPS_RESUME
		uint32 iRound = xrtAtomic32Load(
			&pState->Round,
			XMEMORY_ACQUIRE
		);
	#endif

	testRequire(
		(pSession != NULL) &&
		xrtTlsServerName(pSession, &ServerName) &&
		(ServerName.Size == 11u) &&
		(memcmp(
			ServerName.Data,
			"example.com",
			11u
		) == 0),
		"HTTPS proxy TLS SNI mismatch"
	);
	testRequire(
		xrtTlsSessionProtocol(pSession, &Protocol) &&
		(Protocol.Size == 8u) &&
		(memcmp(
			Protocol.Data,
			"http/1.1",
			8u
		) == 0),
		"HTTPS proxy TLS ALPN mismatch"
	);
	#if TEST_HTTP_CLIENT_PROXY_HTTPS_RESUME
		testRequire(
			xrtTlsServerResumed(pSession) == (iRound != 0),
			"HTTPS proxy resume handshake type mismatch"
		);
		testHttpProxyHttpsIssueTicket(pState, pSession);
	#endif
	(void)xrtAtomic32FetchAdd(
		&pState->TlsOpened,
		1,
		XMEMORY_RELEASE
	);
}



/* 收到完整目标请求后通过真实 TLS Record 返回固定响应。 */
static void testHttpProxyHttpsTlsRead(
	xtlsstream* pStream,
	const xnetbuf* pBuffer,
	ptr pData
)
{
	static const char Response[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 6\r\n"
		"Connection: keep-alive\r\n"
		"\r\n"
		"secure";
	test_http_proxy_https* pState =
		(test_http_proxy_https*)pData;
	char Request[1024];
	size_t iSize = xrtTlsStreamAvailable(pStream);
	size_t iHeader;
	size_t iWritten = 0;

	testRequire(
		pBuffer == xrtTlsStreamBuffer(pStream),
		"HTTPS proxy plaintext buffer mismatch"
	);
	testRequire(
		(iSize != 0) && (iSize < sizeof(Request)),
		"HTTPS proxy target request overflowed"
	);
	testRequire(
		xrtNetBufPeek(
			pBuffer,
			0,
			Request,
			iSize
		) == iSize,
		"HTTPS proxy target request peek failed"
	);
	Request[iSize] = 0;
	iHeader = testHttpHeaderSize(Request, iSize);
	if ( iHeader == 0 ) {
		return;
	}
	testRequire(
		strncmp(
			Request,
			"GET /secure HTTP/1.1\r\n",
			22
		) == 0,
		"HTTPS proxy target request mismatch"
	);
	testRequire(
		(strstr(
			Request,
			"\r\nHost: example.com:"
		) != NULL) &&
		(strstr(
			Request,
			"\r\nProxy-Authorization:"
		) == NULL),
		"HTTPS proxy target Header isolation mismatch"
	);
	testRequire(
		!pState->Responded,
		"HTTPS proxy fixture sent duplicate responses"
	);
	pState->Responded = true;
	testRequire(
		xrtTlsStreamConsume(pStream, iHeader),
		"HTTPS proxy target request consume failed"
	);
	testRequire(
		(xrtTlsStreamSend(
			pStream,
			Response,
			sizeof(Response) - 1u,
			&iWritten
		) == XTLS_OK) &&
		(iWritten == (sizeof(Response) - 1u)),
		"HTTPS proxy target response failed"
	);
}



/* 对端 close_notify 到达后完成双向认证关闭。 */
static void testHttpProxyHttpsTlsEnd(
	xtlsstream* pStream,
	ptr pData
)
{
	(void)pData;
	testRequire(
		xrtTlsStreamClose(pStream),
		"HTTPS proxy server close_notify failed"
	);
}



/* 记录代理隧道内服务端 TLS 已经认证关闭。 */
static void testHttpProxyHttpsTlsClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_proxy_https* pState =
		(test_http_proxy_https*)pData;

	testRequire(
		(Result == XNET_RESULT_OK) &&
		(pError == NULL) &&
		(xrtTlsStreamState(pStream) ==
		 XTLS_STREAM_CLOSED),
		"HTTPS proxy server TLS close mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->ServerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* CONNECT 成功后在同一条已打开 TCP 上切换到服务端 TLS。 */
static void testHttpProxyHttpsConnectRead(
	xnetstream* pTransport,
	xnetbuf* pBuffer,
	ptr pData
)
{
	test_http_proxy_https* pState =
		(test_http_proxy_https*)pData;
	xtlsstreamevents Events;
	xtlssession* pSession;

	if ( !testHttpConnectProxyStep(
		pTransport,
		pBuffer,
		"example.com",
		pState->Port,
		false
	) ) {
		return;
	}
	testRequire(
		xrtAtomic32Load(
			&pState->Connected,
			XMEMORY_ACQUIRE
		) < TEST_HTTP_CLIENT_PROXY_HTTPS_ROUNDS,
		"HTTPS proxy received too many CONNECT requests"
	);
	memset(&Events, 0, sizeof(Events));
	Events.Open = testHttpProxyHttpsTlsOpen;
	Events.Read = testHttpProxyHttpsTlsRead;
	Events.End = testHttpProxyHttpsTlsEnd;
	Events.Close = testHttpProxyHttpsTlsClose;
	pSession = xrtTlsServerCreate(
		&pState->ServerConfig,
		NULL
	);
	testRequire(
		pSession != NULL,
		"HTTPS proxy server session creation failed"
	);
	if ( !xrtTlsStreamAttach(
		pTransport,
		pSession,
		&pState->StreamConfig,
		&Events,
		pState,
		&pState->Server
	) ) {
		xrtTlsSessionDestroy(pSession);
		testRequire(
			false,
			"HTTPS proxy server TLS attach failed"
		);
	}
	(void)xrtAtomic32FetchAdd(
		&pState->Connected,
		1,
		XMEMORY_RELEASE
	);
}



/* 接管唯一代理 TCP，先安装 CONNECT 明文协议事件。 */
static bool testHttpProxyHttpsAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	test_http_proxy_https* pState =
		(test_http_proxy_https*)pData;

	(void)pListener;
	(void)pStream;
	testRequire(
		xrtAtomic32Load(
			&pState->Accepted,
			XMEMORY_ACQUIRE
		) < TEST_HTTP_CLIENT_PROXY_HTTPS_ROUNDS,
		"HTTPS proxy accepted too many connections"
	);
	testRequire(
		xrtNetStreamSetData(pStream, pState),
		"HTTPS proxy stream data setup failed"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 记录代理 Listener 已经完成关闭。 */
static void testHttpProxyHttpsListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_http_proxy_https* pState =
		(test_http_proxy_https*)pData;

	(void)pListener;
	xrtAtomic32Store(
		&pState->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证代理 HTTPS 高层成功结果和缓冲正文所有权。 */
static void testHttpProxyHttpsDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	test_http_proxy_https* pState =
		(test_http_proxy_https*)pData;
	xbytesview Body;

	testRequire(
		(pCall != NULL) &&
		(pResult != NULL) &&
		(pResult->Result == XNET_RESULT_OK) &&
		(pResult->Response != NULL) &&
		(pResult->Tcp == NULL) &&
		(pResult->Tls == NULL) &&
		(pResult->Error == NULL) &&
		!pResult->Upgraded &&
		(xrtHttpCallState(pCall) ==
		 XHTTP_CALL_SUCCEEDED),
		"HTTPS proxy high-level result mismatch"
	);
	Body = xrtHttpResponseBody(pResult->Response);
	testRequire(
		(xrtHttpResponseStatus(pResult->Response) == 200) &&
		(Body.Size == 6u) &&
		(memcmp(Body.Data, "secure", 6u) == 0),
		"HTTPS proxy response mismatch"
	);
	pState->Response = pResult->Response;
	(void)xrtAtomic32FetchAdd(
		&pState->Completed,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证目标 DNS 隔离以及代理隧道内完整 TLS/HTTP 生命周期。 */
int main(void)
{
	static const xstrview Protocols[] = {
		XRT_STR_INIT("http/1.1")
	};
	test_http_proxy_https State;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents ProxyEvents;
	xhttpclientconfig ClientConfig;
	xtlsverifierconfig VerifierConfig;
	xnetproxyconfig ProxyConfig;
	xtlscontext* pContext;
	xtlsidentity* pIdentity;
	xtlsverifier* pVerifier;
	xnetproxy* pProxy;
	xnetaddr Address;
	char Url[128];
	int iLength;

	memset(&State, 0, sizeof(State));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&ProxyEvents, 0, sizeof(ProxyEvents));
	pContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	testRequire(
		(pContext != NULL) && (pIdentity != NULL),
		"HTTPS proxy TLS fixture creation failed"
	);
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(
		pVerifier != NULL,
		"HTTPS proxy verifier creation failed"
	);
	xrtTlsServerConfigInit(&State.ServerConfig);
	State.ServerConfig.Context = pContext;
	State.ServerConfig.Identity = pIdentity;
	State.ServerConfig.Protocols = Protocols;
	State.ServerConfig.ProtocolCount = 1u;
	State.ServerConfig.RequireProtocol = true;
	#if TEST_HTTP_CLIENT_PROXY_HTTPS_RESUME
		State.ServerConfig.Resume =
			testHttpProxyHttpsResumeLookup;
		State.ServerConfig.ResumeContext = &State;
	#endif
	xrtTlsStreamConfigInit(&State.StreamConfig);

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend =
		TEST_HTTP_CLIENT_PROXY_HTTPS_BACKEND;
	EngineConfig.Workers = 1;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"HTTPS proxy engine start failed"
	);
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ListenConfig.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTPS proxy listener address failed"
	);
	ListenConfig.Stream.ReadSize = 64;
	ListenConfig.Stream.ReadLimit = 4096;
	ListenerEvents.Accept = testHttpProxyHttpsAccept;
	ListenerEvents.Close =
		testHttpProxyHttpsListenerClose;
	ProxyEvents.Read = testHttpProxyHttpsConnectRead;
	State.Listener = xrtNetListen(
		State.Engine,
		&ListenConfig,
		&ListenerEvents,
		&ProxyEvents,
		&State
	);
	testRequire(
		(State.Listener != NULL) &&
		xrtNetListenerLocal(State.Listener, &Address),
		"HTTPS proxy listener creation failed"
	);
	State.Port = Address.Port;

	xrtNetProxyConfigInit(&ProxyConfig);
	ProxyConfig.Type = XNET_PROXY_HTTP_CONNECT;
	ProxyConfig.Host = XRT_STR_LITERAL("proxy.test");
	ProxyConfig.Port = State.Port;
	pProxy = xrtNetProxyCreate(&ProxyConfig);
	testRequire(
		pProxy != NULL,
		"HTTPS proxy endpoint creation failed"
	);
	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Resolver.Lookup =
		testHttpProxyHttpsLookup;
	ClientConfig.Dial.FallbackDelay = 1000u;
	ClientConfig.Dial.MaxAttempts = 1;
	ClientConfig.Proxy = pProxy;
	ClientConfig.TlsContext = pContext;
	ClientConfig.TlsVerifier = pVerifier;
	ClientConfig.SystemTrust = false;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		/* 本测试验证 TLS 认证关闭，不把连接移交给空闲池。 */
		ClientConfig.Pool.MaxIdle = 0;
	#endif
	State.Client = xrtHttpClientCreate(
		State.Engine,
		&ClientConfig
	);
	testRequire(
		State.Client != NULL,
		"HTTPS proxy client creation failed"
	);
	xrtNetProxyRelease(pProxy);

	iLength = snprintf(
		Url,
		sizeof(Url),
		"https://example.com:%u/secure",
		(unsigned)State.Port
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Url)),
		"HTTPS proxy URL fixture overflowed"
	);
	for ( uint32 iRound = 0;
		iRound < TEST_HTTP_CLIENT_PROXY_HTTPS_ROUNDS;
		iRound++ ) {
		xhttprequest* pRequest;

		xrtAtomic32Store(
			&State.Round,
			iRound,
			XMEMORY_RELEASE
		);
		State.Responded = false;
		State.Response = NULL;
		pRequest = xrtHttpRequestCreate(
			XRT_STR_LITERAL("GET"),
			(xstrview){ Url, (size_t)iLength }
		);
		testRequire(
			pRequest != NULL,
			"HTTPS proxy request creation failed"
		);
		State.Call = xrtHttpClientDo(
			State.Client,
			pRequest,
			NULL,
			testHttpProxyHttpsDone,
			&State
		);
		xrtHttpRequestDestroy(pRequest);
		testRequire(
			State.Call != NULL,
			"HTTPS proxy call submission failed"
		);
		/* 运行时必须独立保活 Call，调用方可在提交后立即释放句柄。 */
		xrtHttpCallDestroy(State.Call);
		State.Call = NULL;
		testHttpProxyHttpsWait(
			&State.Accepted,
			iRound + 1u,
			"HTTPS proxy connection was not accepted"
		);
		testHttpProxyHttpsWait(
			&State.Connected,
			iRound + 1u,
			"HTTPS proxy CONNECT did not complete"
		);
		testHttpProxyHttpsWait(
			&State.TlsOpened,
			iRound + 1u,
			"HTTPS proxy TLS did not open"
		);
		testHttpProxyHttpsWait(
			&State.Completed,
			iRound + 1u,
			"HTTPS proxy call did not complete"
		);
		testHttpProxyHttpsWait(
			&State.ServerClosed,
			iRound + 1u,
			"HTTPS proxy TLS did not authenticate close"
		);
		#if TEST_HTTP_CLIENT_PROXY_HTTPS_RESUME
			{
				xhttpresumestats Stats;

				testRequire(
					xrtHttpClientResumeStats(
						State.Client,
						&Stats
					) &&
					(Stats.Entries == 1u) &&
					(Stats.Hits == (uint64)iRound) &&
					(Stats.Misses == 1u) &&
					(Stats.Stored ==
					 ((uint64)iRound + 1u)),
					"HTTPS proxy resume cache statistics mismatch"
				);
			}
		#endif
		xrtHttpResponseDestroy(State.Response);
		xrtTlsStreamDestroy(State.Server);
		State.Response = NULL;
		State.Server = NULL;
	}
	testRequire(
		xrtNetListenerClose(State.Listener),
		"HTTPS proxy listener close failed"
	);
	testHttpProxyHttpsWait(
		&State.ListenerClosed,
		1u,
		"HTTPS proxy listener did not close"
	);

	xrtHttpClientDestroy(State.Client);
	xrtNetListenerDestroy(State.Listener);
	testHttpProxyHttpsEngineDestroy(State.Engine);
	#if TEST_HTTP_CLIENT_PROXY_HTTPS_RESUME
		xrtTlsResumeRelease(State.ServerResume);
	#endif
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	printf(
		#if TEST_HTTP_CLIENT_PROXY_HTTPS_RESUME
			"[PASS] HTTPS client resumption through CONNECT proxy (%s)\n",
		#else
			"[PASS] HTTPS client through CONNECT proxy (%s)\n",
		#endif
		TEST_HTTP_CLIENT_PROXY_HTTPS_BACKEND_NAME
	);
	return 0;
}



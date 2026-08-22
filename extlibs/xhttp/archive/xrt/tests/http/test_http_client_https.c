#include "../fixtures/tls_server.h"



#if !defined(TEST_HTTP_CLIENT_HTTPS_FUTURE)
	#define TEST_HTTP_CLIENT_HTTPS_FUTURE 0
#endif



#if !defined(TEST_HTTP_CLIENT_HTTPS_BACKEND)
	#define TEST_HTTP_CLIENT_HTTPS_BACKEND XNET_PORT_SELECT
	#define TEST_HTTP_CLIENT_HTTPS_BACKEND_NAME "select"
#endif



#if !defined(TEST_HTTP_CLIENT_HTTPS_IP)
	#define TEST_HTTP_CLIENT_HTTPS_IP 0
#endif



#if TEST_HTTP_CLIENT_HTTPS_IP
	#define TEST_HTTP_CLIENT_HTTPS_HOST "127.0.0.1"
#else
	#define TEST_HTTP_CLIENT_HTTPS_HOST "example.com"
#endif



/* HTTPS 夹具保存真实 TLS 服务端和高层客户端的跨 Worker 终态。 */
typedef struct test_http_client_https {
	xnetengine* Engine;
	xnetlistener* Listener;
	xtlsstream* Server;
	xhttpclient* Client;
	xhttpcall* Call;
	xhttpcall* CallbackCall;
	xhttpresponse* Response;
	xtlsserverconfig ServerConfig;
	xtlsstreamconfig StreamConfig;
	xatomic32 Accepted;
	xatomic32 ServerOpened;
	xatomic32 Completed;
	xatomic32 ServerClosed;
	xatomic32 ListenerClosed;
	#if TEST_HTTP_CLIENT_HTTPS_FUTURE
		xtlsstream* Upgrade;
		xatomic32 ClientClosed;
	#endif
	bool Responded;
} test_http_client_https;



/* 在截止时间前等待 TLS 或 HTTP Worker 发布终态。 */
static void testHttpClientHttpsWait(
	const xatomic32* pValue,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(10000000u);

	while ( xrtAtomic32Load(
		pValue,
		XMEMORY_ACQUIRE
	) == 0 ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			sMessage
		);
		xrtThreadYield();
	}
}



/* 等待 HTTP 完成后仍在进行的 TLS 认证关闭释放最后一个 Engine 对象。 */
static void testHttpClientHttpsEngineDestroy(
	xnetengine* pEngine
)
{
	xdeadline Deadline = xrtDeadlineAfter(
		UINT64_C(10000000)
	);

	while ( !xrtNetEngineDestroy(pEngine) ) {
		const xerror* pError = xrtGetError();

		testRequire(
			(pError != NULL) &&
			(xrtErrorKind(pError) == XERR_STATE) &&
			(xrtErrorCode(pError) == XNET_ERROR_ENGINE_STOP),
			"HTTPS client engine destroy reported an unexpected error"
		);
		xrtClearError();
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTPS client retained an Engine object"
		);
		xrtThreadYield();
	}
}



#if TEST_HTTP_CLIENT_HTTPS_FUTURE

/* 接管协议收到对端 close_notify 后完成客户端认证关闭。 */
static void testHttpClientHttpsUpgradeEnd(
	xtlsstream* pStream,
	ptr pData
)
{
	(void)pData;
	testRequire(
		xrtTlsStreamClose(pStream),
		"HTTPS Future Upgrade close response failed"
	);
}



/* 记录接管后的客户端 TLS Stream 已完成双向认证关闭。 */
static void testHttpClientHttpsUpgradeClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_client_https* pState =
		(test_http_client_https*)pData;

	testRequire(
		(Result == XNET_RESULT_OK) &&
		(pError == NULL) &&
		(xrtTlsStreamState(pStream) ==
		 XTLS_STREAM_CLOSED),
		"HTTPS Future Upgrade close result mismatch"
	);
	xrtAtomic32Store(
		&pState->ClientClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 在 TLS Stream 所属 Worker 上接管事件、消费协议余量并发起认证关闭。 */
static void testHttpClientHttpsUpgradeTakeover(
	xnetworker* pWorker,
	ptr pData
)
{
	test_http_client_https* pState =
		(test_http_client_https*)pData;
	xtlsstreamevents Events;
	char Raw[3];
	size_t iRead = 0;

	(void)pWorker;
	memset(&Events, 0, sizeof(Events));
	Events.End = testHttpClientHttpsUpgradeEnd;
	Events.Close = testHttpClientHttpsUpgradeClose;
	testRequire(
		xrtTlsStreamSetEvents(
			pState->Upgrade,
			&Events,
			pState
		),
		"HTTPS Future Upgrade event takeover failed"
	);
	testRequire(
		(xrtTlsStreamRead(
			pState->Upgrade,
			Raw,
			sizeof(Raw),
			&iRead
		) == XTLS_OK) &&
		(iRead == sizeof(Raw)) &&
		(memcmp(Raw, "RAW", sizeof(Raw)) == 0),
		"HTTPS Future Upgrade buffered bytes mismatch"
	);
	testRequire(
		xrtTlsStreamClose(pState->Upgrade),
		"HTTPS Future Upgrade close failed"
	);
}

#endif



/* 验证目标身份独立于线路 SNI，并接管测试证书的信任决策。 */
static xtlsverifydecision testHttpClientHttpsVerify(
	const xtlspeer* pPeer,
	ptr pData
)
{
	(void)pData;
	testRequire(
		(pPeer != NULL) &&
		(pPeer->Name.Size ==
		 (sizeof(TEST_HTTP_CLIENT_HTTPS_HOST) - 1u)) &&
		(memcmp(
			pPeer->Name.Data,
			TEST_HTTP_CLIENT_HTTPS_HOST,
			pPeer->Name.Size
		) == 0),
		"HTTPS client verified the wrong target identity"
	);
	return testTlsServerAccept(pPeer, NULL);
}



/* 为目标名称返回本机地址，使验证身份与测试 Listener 解耦。 */
static xnetaddrlist* testHttpClientHttpsLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)pData;
	testRequire(
		strcmp(sHost, TEST_HTTP_CLIENT_HTTPS_HOST) == 0,
		"HTTPS client resolved an unexpected target host"
	);
	if ( Family == XNET_FAMILY_IPV6 ) {
		return xrtNetAddrListCreate(NULL, 0);
	}
	testRequire(xrtNetAddrLoopback(
		&Address,
		XNET_FAMILY_IPV4,
		0
	), "HTTPS resolver fixture failed");
	return xrtNetAddrListCreate(&Address, 1);
}



/* TLS 打开后验证 DNS 名发送 SNI，而 IP 字面量不发送 SNI。 */
static void testHttpClientHttpsOpen(
	xtlsstream* pStream,
	ptr pData
)
{
	test_http_client_https* pState =
		(test_http_client_https*)pData;
	xbytesview Name;
	bool bName = xrtTlsServerName(
		xrtTlsStreamSession(pStream),
		&Name
	);

	#if TEST_HTTP_CLIENT_HTTPS_IP
		testRequire(
			!bName,
			"HTTPS client sent an IP literal in SNI"
		);
	#else
		testRequire(
			bName &&
			(Name.Size == sizeof(TEST_HTTP_CLIENT_HTTPS_HOST) - 1u) &&
			(memcmp(
				Name.Data,
				TEST_HTTP_CLIENT_HTTPS_HOST,
				Name.Size
			) == 0),
			"HTTPS client SNI mismatch"
		);
	#endif
	xrtAtomic32Store(
		&pState->ServerOpened,
		1,
		XMEMORY_RELEASE
	);
}



/* 服务端收到完整 HTTP 请求后通过真实 TLS Record 返回响应。 */
static void testHttpClientHttpsRead(
	xtlsstream* pStream,
	const xnetbuf* pBuffer,
	ptr pData
)
{
	#if TEST_HTTP_CLIENT_HTTPS_FUTURE
	static const char Response[] =
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Connection: Upgrade\r\n"
		"Upgrade: xrt-test\r\n"
		"\r\n"
		"RAW";
	#else
	static const char Response[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 2\r\n"
		"Connection: keep-alive\r\n"
		"\r\n"
		"OK";
	#endif
	test_http_client_https* pState =
		(test_http_client_https*)pData;
	char Request[1024];
	size_t iSize = xrtTlsStreamAvailable(pStream);
	size_t iWritten = 0;
	size_t i;
	bool bComplete = false;

	testRequire(
		pBuffer == xrtTlsStreamBuffer(pStream),
		"HTTPS server plaintext buffer mismatch"
	);
	testRequire(
		(iSize != 0) && (iSize < sizeof(Request)),
		"HTTPS request exceeded fixture capacity"
	);
	testRequire(xrtNetBufPeek(
		pBuffer,
		0,
		Request,
		iSize
	) == iSize, "HTTPS request peek failed");
	for ( i = 3; i < iSize; ++i ) {
		if ( (Request[i - 3] == '\r') &&
			(Request[i - 2] == '\n') &&
			(Request[i - 1] == '\r') &&
			(Request[i] == '\n') ) {
			bComplete = true;
			break;
		}
	}
	if ( !bComplete ) {
		return;
	}
	testRequire(
		memcmp(Request, "GET /secure HTTP/1.1\r\n", 22) == 0,
		"HTTPS client emitted the wrong request target"
	);
	testRequire(
		strstr(
			Request,
			#if TEST_HTTP_CLIENT_HTTPS_IP
				"\r\nHost: 127.0.0.1:"
			#else
				"\r\nHost: example.com:"
			#endif
		) != NULL,
		"HTTPS client omitted the effective Host port"
	);
	testRequire(
		!pState->Responded,
		"HTTPS fixture sent duplicate responses"
	);
	pState->Responded = true;
	testRequire(
		xrtTlsStreamConsume(pStream, iSize),
		"HTTPS request consume failed"
	);
	testRequire(
		(xrtTlsStreamSend(
			pStream,
			Response,
			sizeof(Response) - 1u,
			&iWritten
		) == XTLS_OK) &&
		(iWritten == (sizeof(Response) - 1u)),
		"HTTPS response send failed"
	);
}



/* 对端发送 close_notify 后回送认证关闭。 */
static void testHttpClientHttpsEnd(
	xtlsstream* pStream,
	ptr pData
)
{
	(void)pData;
	testRequire(
		xrtTlsStreamClose(pStream),
		"HTTPS server close_notify failed"
	);
}



/* 记录服务端 TLS Stream 已完成双向认证关闭。 */
static void testHttpClientHttpsClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_client_https* pState =
		(test_http_client_https*)pData;

	testRequire(
		(Result == XNET_RESULT_OK) &&
		(pError == NULL) &&
		(xrtTlsStreamState(pStream) ==
		 XTLS_STREAM_CLOSED),
		"HTTPS server close result mismatch"
	);
	xrtAtomic32Store(
		&pState->ServerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 接管 TCP Stream 并启动要求 http/1.1 ALPN 的 TLS 服务端。 */
static bool testHttpClientHttpsAccept(
	xnetlistener* pListener,
	xnetstream* pTransport,
	ptr pData
)
{
	test_http_client_https* pState =
		(test_http_client_https*)pData;
	xtlsstreamevents Events;

	(void)pListener;
	memset(&Events, 0, sizeof(Events));
	Events.Open = testHttpClientHttpsOpen;
	Events.Read = testHttpClientHttpsRead;
	Events.End = testHttpClientHttpsEnd;
	Events.Close = testHttpClientHttpsClose;
	testRequire(xrtTlsStreamAccept(
		pTransport,
		&pState->ServerConfig,
		&pState->StreamConfig,
		&Events,
		pState,
		&pState->Server
	), "HTTPS server TLS accept failed");
	xrtAtomic32Store(
		&pState->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 记录 HTTPS Listener 已经完成关闭。 */
static void testHttpClientHttpsListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_http_client_https* pState =
		(test_http_client_https*)pData;

	(void)pListener;
	xrtAtomic32Store(
		&pState->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



#if !TEST_HTTP_CLIENT_HTTPS_FUTURE

/* 验证高层 HTTPS 成功结果不暴露普通可复用 TLS Stream。 */
static void testHttpClientHttpsDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	test_http_client_https* pState =
		(test_http_client_https*)pData;

	testRequire(
		(pCall != NULL) &&
		(pResult != NULL) &&
		(pResult->Result == XNET_RESULT_OK) &&
		(pResult->Response != NULL) &&
		(pResult->Tcp == NULL) &&
		(pResult->Tls == NULL) &&
		(pResult->Error == NULL) &&
		(pResult->Buffered == 0) &&
		!pResult->Upgraded &&
		(xrtHttpCallState(pCall) ==
		 XHTTP_CALL_SUCCEEDED) &&
		(xrtHttpCallError(pCall) == NULL),
		"high-level HTTPS result mismatch"
	);
	testRequire(
		(xrtHttpResponseStatus(pResult->Response) == 200) &&
		(xrtHttpResponseBody(pResult->Response).Size == 2u) &&
		(memcmp(
			xrtHttpResponseBody(
				pResult->Response
			).Data,
			"OK",
			2u
		) == 0),
		"high-level HTTPS response mismatch"
	);
	pState->CallbackCall = pCall;
	pState->Response = pResult->Response;
	xrtAtomic32Store(
		&pState->Completed,
		1,
		XMEMORY_RELEASE
	);
}

#endif



/* 验证 DNS、TCP、TLS 1.3、SNI、ALPN、HTTP 和认证关闭的完整链路。 */
int main(void)
{
	static const xstrview Protocols[] = {
		XRT_STR_INIT("http/1.1")
	};
	test_http_client_https State;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xhttpclientconfig ClientConfig;
	xtlsverifierconfig VerifierConfig;
	xtlscontext* pContext;
	xtlsidentity* pIdentity;
	xtlsverifier* pVerifier;
	xhttprequest* pRequest;
	xnetaddr Address;
	#if TEST_HTTP_CLIENT_HTTPS_FUTURE
		xfuture* pFuture;
		xhttpresult* pResult;
		xhttpresponse* pResponse;
		xtlsstream* pUpgrade;
	#endif
	char Url[128];
	int iLength;

	memset(&State, 0, sizeof(State));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	xrtAtomic32Init(&State.Accepted, 0);
	xrtAtomic32Init(&State.ServerOpened, 0);
	xrtAtomic32Init(&State.Completed, 0);
	xrtAtomic32Init(&State.ServerClosed, 0);
	xrtAtomic32Init(&State.ListenerClosed, 0);
	#if TEST_HTTP_CLIENT_HTTPS_FUTURE
		xrtAtomic32Init(&State.ClientClosed, 0);
	#endif

	pContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	testRequire(
		(pContext != NULL) && (pIdentity != NULL),
		"HTTPS TLS fixture creation failed"
	);
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testHttpClientHttpsVerify;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(
		pVerifier != NULL,
		"HTTPS verifier creation failed"
	);
	xrtTlsServerConfigInit(&State.ServerConfig);
	State.ServerConfig.Context = pContext;
	State.ServerConfig.Identity = pIdentity;
	State.ServerConfig.Protocols = Protocols;
	State.ServerConfig.ProtocolCount = 1u;
	State.ServerConfig.RequireProtocol = true;
	xrtTlsStreamConfigInit(&State.StreamConfig);

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_HTTP_CLIENT_HTTPS_BACKEND;
	EngineConfig.Workers = 2;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"HTTPS client engine start failed"
	);
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "HTTPS listener address failed");
	ListenConfig.AcceptConcurrency = 4;
	ListenerEvents.Accept = testHttpClientHttpsAccept;
	ListenerEvents.Close =
		testHttpClientHttpsListenerClose;
	State.Listener = xrtNetListen(
		State.Engine,
		&ListenConfig,
		&ListenerEvents,
		NULL,
		&State
	);
	testRequire(
		(State.Listener != NULL) &&
		xrtNetListenerLocal(State.Listener, &Address),
		"HTTPS listener creation failed"
	);

	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Resolver.Lookup =
		testHttpClientHttpsLookup;
	ClientConfig.Dial.FallbackDelay = 1000u;
	ClientConfig.Dial.MaxAttempts = 1;
	ClientConfig.TlsContext = pContext;
	ClientConfig.TlsVerifier = pVerifier;
	ClientConfig.SystemTrust = false;
	#if defined(XRT_FEATURE_HTTP_CLIENT_POOL)
		/* 普通 HTTPS 分支验证认证关闭，连接池复用由独立套件覆盖。 */
		ClientConfig.Pool.MaxIdle = 0;
	#endif
	State.Client = xrtHttpClientCreate(
		State.Engine,
		&ClientConfig
	);
	testRequire(
		State.Client != NULL,
		"high-level HTTPS client creation failed"
	);
	iLength = snprintf(
		Url,
		sizeof(Url),
		"https://" TEST_HTTP_CLIENT_HTTPS_HOST ":%u/secure",
		(unsigned int)Address.Port
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Url)),
		"HTTPS fixture URL overflowed"
	);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		(xstrview){ Url, (size_t)iLength }
	);
	testRequire(
		pRequest != NULL,
		"HTTPS request creation failed"
	);
	#if TEST_HTTP_CLIENT_HTTPS_FUTURE
		testRequire(
			xrtHttpRequestSetHeader(
				pRequest,
				XRT_STR_LITERAL("Connection"),
				XRT_STR_LITERAL("Upgrade")
			) &&
			xrtHttpRequestSetHeader(
				pRequest,
				XRT_STR_LITERAL("Upgrade"),
				XRT_STR_LITERAL("xrt-test")
			),
			"HTTPS Future Upgrade headers failed"
		);
		pFuture = xrtHttpClientDoAsync(
			State.Client,
			pRequest,
			NULL
		);
	#else
	State.Call = xrtHttpClientDo(
		State.Client,
		pRequest,
		NULL,
		testHttpClientHttpsDone,
		&State
	);
	#endif
	xrtHttpRequestDestroy(pRequest);
	#if TEST_HTTP_CLIENT_HTTPS_FUTURE
		testRequire(
			pFuture != NULL,
			"high-level HTTPS Future submission failed"
		);
	#else
	testRequire(
		State.Call != NULL,
		"high-level HTTPS call submission failed"
	);
	#endif
	testHttpClientHttpsWait(
		&State.Accepted,
		"HTTPS connection was not accepted"
	);
	testHttpClientHttpsWait(
		&State.ServerOpened,
		"HTTPS TLS server did not open"
	);
	#if TEST_HTTP_CLIENT_HTTPS_FUTURE
		testRequire(
			(xrtFutureWaitFor(
				pFuture,
				UINT64_C(10000000)
			) == XWAIT_OK) &&
			(xrtFutureState(pFuture) ==
			 XFUTURE_RESOLVED),
			"high-level HTTPS Future did not complete"
		);
		pResult = (xhttpresult*)xrtFutureValue(pFuture);
		testRequire(
			(pResult != NULL) &&
			xrtHttpResultUpgraded(pResult) &&
			(xrtHttpResultTcp(pResult) == NULL) &&
			(xrtHttpResultTls(pResult) != NULL) &&
			(xrtHttpResultBuffered(pResult) == 3u) &&
			(xrtHttpResponseStatus(
				xrtHttpResultResponse(pResult)
			) == 101),
			"high-level HTTPS Future Upgrade result mismatch"
		);
		pResponse = xrtHttpResultTakeResponse(pResult);
		pUpgrade = xrtHttpResultTakeTls(pResult);
		testRequire(
			(pResponse != NULL) &&
			(pUpgrade != NULL) &&
			(xrtHttpResultResponse(pResult) == NULL) &&
			(xrtHttpResultTls(pResult) == NULL),
			"high-level HTTPS Future ownership transfer failed"
		);
		xrtFutureDestroy(pFuture);
		State.Upgrade = pUpgrade;
		testRequire(
			xrtNetEnginePost(
				State.Engine,
				xrtNetWorkerIndex(xrtNetStreamWorker(
					xrtTlsStreamTransport(pUpgrade)
				)),
				testHttpClientHttpsUpgradeTakeover,
				&State
			),
			"HTTPS Future Upgrade takeover post failed"
		);
	#else
	testHttpClientHttpsWait(
		&State.Completed,
		"high-level HTTPS call did not complete"
	);
	testRequire(
		State.CallbackCall == State.Call,
		"high-level HTTPS callback identity mismatch"
	);
	#endif
	testHttpClientHttpsWait(
		&State.ServerClosed,
		"high-level HTTPS transport did not authenticate close"
	);
	#if TEST_HTTP_CLIENT_HTTPS_FUTURE
		testHttpClientHttpsWait(
			&State.ClientClosed,
			"HTTPS Future Upgrade client did not close"
		);
	#endif
	testRequire(
		xrtNetListenerClose(State.Listener),
		"HTTPS listener close failed"
	);
	testHttpClientHttpsWait(
		&State.ListenerClosed,
		"HTTPS listener did not close"
	);

	#if TEST_HTTP_CLIENT_HTTPS_FUTURE
		xrtTlsStreamDestroy(pUpgrade);
		xrtHttpResponseDestroy(pResponse);
	#else
	xrtHttpResponseDestroy(State.Response);
	xrtHttpCallDestroy(State.Call);
	#endif
	xrtHttpClientDestroy(State.Client);
	xrtTlsStreamDestroy(State.Server);
	xrtNetListenerDestroy(State.Listener);
	testHttpClientHttpsEngineDestroy(State.Engine);
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	printf(
		#if TEST_HTTP_CLIENT_HTTPS_FUTURE
		"[PASS] high-level HTTPS Future Upgrade lifecycle (%s)\n",
		#else
		"[PASS] high-level HTTPS client lifecycle (%s)\n",
		#endif
		TEST_HTTP_CLIENT_HTTPS_BACKEND_NAME
	);
	return 0;
}

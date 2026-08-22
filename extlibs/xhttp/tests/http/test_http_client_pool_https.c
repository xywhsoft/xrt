#include "../fixtures/tls_server.h"



#if !defined(TEST_HTTP_POOL_HTTPS_BACKEND)
	#define TEST_HTTP_POOL_HTTPS_BACKEND XNET_PORT_SELECT
	#define TEST_HTTP_POOL_HTTPS_BACKEND_NAME "select"
#endif



typedef struct test_http_pool_https test_http_pool_https;



/* 每次 HTTPS 完成上下文保存独立的调用方结果引用。 */
typedef struct test_http_pool_https_call {
	test_http_pool_https* State;
	xhttpcall* Call;
	xhttpresponse* Response;
	xatomic32 Completed;
} test_http_pool_https_call;



/* 真实 TLS 夹具只允许一条连接承载两个串行 HTTP 请求。 */
struct test_http_pool_https {
	xnetengine* Engine;
	xnetlistener* Listener;
	xtlsstream* Server;
	xhttpclient* Client;
	test_http_pool_https_call Calls[2];
	xtlsserverconfig ServerConfig;
	xtlsstreamconfig StreamConfig;
	xatomic32 Accepted;
	xatomic32 Requests;
	xatomic32 ServerClosed;
	xatomic32 ListenerClosed;
};



/* 等待 TLS 或 HTTP Worker 发布指定计数。 */
static void testHttpPoolHttpsWait(
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



/* 等待客户端空闲 TLS 关闭回调释放最后一个池内对象。 */
static void testHttpPoolHttpsWaitClosed(
	xhttpclient* pClient
)
{
	xdeadline Deadline = xrtDeadlineAfter(10000000u);
	xhttpclientstats Stats;

	for ( ;; ) {
		testRequire(
			xrtHttpClientStats(pClient, &Stats),
			"HTTPS pool stats query failed"
		);
		if ( Stats.ClosingConnections == 0 ) {
			return;
		}
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTPS pool client close did not drain"
		);
		xrtThreadYield();
	}
}



/* 为真实 SNI 名称返回本机 Listener 地址。 */
static xnetaddrlist* testHttpPoolHttpsLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)pData;
	testRequire(
		strcmp(sHost, "example.com") == 0,
		"HTTPS pool resolved an unexpected SNI host"
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
		"HTTPS pool resolver address failed"
	);
	return xrtNetAddrListCreate(&Address, 1);
}



/* 每个完整请求通过同一 TLS 会话返回一条保持连接的响应。 */
static void testHttpPoolHttpsRead(
	xtlsstream* pStream,
	const xnetbuf* pBuffer,
	ptr pData
)
{
	static const char Response[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 2\r\n"
		"Connection: keep-alive\r\n"
		"\r\n"
		"OK";
	test_http_pool_https* pState =
		(test_http_pool_https*)pData;
	char Request[2048];
	size_t iSize = xrtTlsStreamAvailable(pStream);
	size_t iHeader = 0;
	size_t iWritten = 0;

	testRequire(
		pBuffer == xrtTlsStreamBuffer(pStream),
		"HTTPS pool plaintext buffer mismatch"
	);
	testRequire(
		(iSize >= 4u) && (iSize < sizeof(Request)),
		"HTTPS pool request exceeded fixture capacity"
	);
	testRequire(
		xrtNetBufPeek(
			pBuffer,
			0,
			Request,
			iSize
		) == iSize,
		"HTTPS pool request peek failed"
	);
	Request[iSize] = 0;
	for ( size_t i = 3; i < iSize; i++ ) {
		if ( (Request[i - 3u] == '\r') &&
			(Request[i - 2u] == '\n') &&
			(Request[i - 1u] == '\r') &&
			(Request[i] == '\n') ) {
			iHeader = i + 1u;
			break;
		}
	}
	if ( iHeader == 0 ) {
		return;
	}
	testRequire(
		(memcmp(Request, "GET /", 5u) == 0) &&
		(strstr(Request, " HTTP/1.1\r\n") != NULL) &&
		(strstr(
			Request,
			"\r\nHost: example.com:"
		) != NULL),
		"HTTPS pool emitted an invalid request"
	);
	testRequire(
		xrtTlsStreamConsume(pStream, iHeader),
		"HTTPS pool request consume failed"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Requests,
		1,
		XMEMORY_ACQ_REL
	);
	testRequire(
		(xrtTlsStreamSend(
			pStream,
			Response,
			sizeof(Response) - 1u,
			&iWritten
		) == XTLS_OK) &&
		(iWritten == (sizeof(Response) - 1u)),
		"HTTPS pool response send failed"
	);
}



/* 对端 close_notify 到达后完成服务端认证关闭。 */
static void testHttpPoolHttpsEnd(
	xtlsstream* pStream,
	ptr pData
)
{
	(void)pData;
	testRequire(
		xrtTlsStreamClose(pStream),
		"HTTPS pool server close_notify failed"
	);
}



/* 记录唯一 TLS 会话已经完成认证关闭。 */
static void testHttpPoolHttpsClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_pool_https* pState =
		(test_http_pool_https*)pData;

	testRequire(
		(Result == XNET_RESULT_OK) &&
		(pError == NULL) &&
		(xrtTlsStreamState(pStream) ==
		 XTLS_STREAM_CLOSED),
		"HTTPS pool server close result mismatch"
	);
	xrtAtomic32Store(
		&pState->ServerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 只建立第一条真实 TLS 1.3 会话，额外 TCP 连接立即拒绝。 */
static bool testHttpPoolHttpsAccept(
	xnetlistener* pListener,
	xnetstream* pTransport,
	ptr pData
)
{
	test_http_pool_https* pState =
		(test_http_pool_https*)pData;
	xtlsstreamevents Events;
	uint32 iAccepted;

	(void)pListener;
	iAccepted = xrtAtomic32FetchAdd(
		&pState->Accepted,
		1,
		XMEMORY_ACQ_REL
	);
	if ( iAccepted != 0 ) {
		return false;
	}
	memset(&Events, 0, sizeof(Events));
	Events.Read = testHttpPoolHttpsRead;
	Events.End = testHttpPoolHttpsEnd;
	Events.Close = testHttpPoolHttpsClose;
	testRequire(
		xrtTlsStreamAccept(
			pTransport,
			&pState->ServerConfig,
			&pState->StreamConfig,
			&Events,
			pState,
			&pState->Server
		),
		"HTTPS pool server TLS accept failed"
	);
	return true;
}



/* 记录 HTTPS Listener 已排空 Accept。 */
static void testHttpPoolHttpsListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_http_pool_https* pState =
		(test_http_pool_https*)pData;

	(void)pListener;
	xrtAtomic32Store(
		&pState->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证普通 HTTPS 成功结果不泄露池内 TLS Stream。 */
static void testHttpPoolHttpsDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	test_http_pool_https_call* pContext =
		(test_http_pool_https_call*)pData;

	testRequire(
		(pCall == pContext->Call) &&
		(pResult != NULL) &&
		(pResult->Result == XNET_RESULT_OK) &&
		(pResult->Response != NULL) &&
		(pResult->Tcp == NULL) &&
		(pResult->Tls == NULL) &&
		(pResult->Error == NULL) &&
		!pResult->Upgraded &&
		(xrtHttpCallState(pCall) ==
		 XHTTP_CALL_SUCCEEDED),
		"HTTPS pool completion mismatch"
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
		"HTTPS pool response mismatch"
	);
	pContext->Response = pResult->Response;
	xrtAtomic32Store(
		&pContext->Completed,
		1,
		XMEMORY_RELEASE
	);
}



/* 构造并提交一个 HTTPS GET Call。 */
static void testHttpPoolHttpsCall(
	test_http_pool_https* pState,
	size_t iCall,
	xnetaddr Address,
	cstr sTarget
)
{
	test_http_pool_https_call* pContext =
		&pState->Calls[iCall];
	xhttprequest* pRequest;
	char Url[160];
	int iLength;

	iLength = snprintf(
		Url,
		sizeof(Url),
		"https://example.com:%u/%s",
		(unsigned int)Address.Port,
		sTarget
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Url)),
		"HTTPS pool URL overflowed"
	);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		(xstrview){ Url, (size_t)iLength }
	);
	testRequire(
		pRequest != NULL,
		"HTTPS pool request creation failed"
	);
	pContext->Call = xrtHttpClientDo(
		pState->Client,
		pRequest,
		NULL,
		testHttpPoolHttpsDone,
		pContext
	);
	xrtHttpRequestDestroy(pRequest);
	testRequire(
		pContext->Call != NULL,
		"HTTPS pool call submission failed"
	);
}



/* 验证一次 TLS 握手承载两个请求并以 close_notify 回收空闲项。 */
int main(void)
{
	static const xstrview Protocols[] = {
		XRT_STR_INIT("http/1.1")
	};
	test_http_pool_https State;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xhttpclientconfig ClientConfig;
	xtlsverifierconfig VerifierConfig;
	xhttpclientstats Stats;
	xtlscontext* pContext;
	xtlsidentity* pIdentity;
	xtlsverifier* pVerifier;
	xnetaddr Address;

	memset(&State, 0, sizeof(State));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	xrtAtomic32Init(&State.Accepted, 0);
	xrtAtomic32Init(&State.Requests, 0);
	xrtAtomic32Init(&State.ServerClosed, 0);
	xrtAtomic32Init(&State.ListenerClosed, 0);
	for ( size_t i = 0; i < 2u; i++ ) {
		State.Calls[i].State = &State;
		xrtAtomic32Init(
			&State.Calls[i].Completed,
			0
		);
	}

	pContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	testRequire(
		(pContext != NULL) && (pIdentity != NULL),
		"HTTPS pool TLS fixture creation failed"
	);
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(
		pVerifier != NULL,
		"HTTPS pool verifier creation failed"
	);
	xrtTlsServerConfigInit(&State.ServerConfig);
	State.ServerConfig.Context = pContext;
	State.ServerConfig.Identity = pIdentity;
	State.ServerConfig.Protocols = Protocols;
	State.ServerConfig.ProtocolCount = 1u;
	State.ServerConfig.RequireProtocol = true;
	xrtTlsStreamConfigInit(&State.StreamConfig);

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_HTTP_POOL_HTTPS_BACKEND;
	EngineConfig.Workers = 2;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"HTTPS pool engine start failed"
	);
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ListenConfig.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTPS pool listener address failed"
	);
	ListenConfig.AcceptConcurrency = 4;
	ListenerEvents.Accept = testHttpPoolHttpsAccept;
	ListenerEvents.Close =
		testHttpPoolHttpsListenerClose;
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
		"HTTPS pool listener creation failed"
	);

	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Resolver.Lookup =
		testHttpPoolHttpsLookup;
	ClientConfig.Dial.FallbackDelay = 1000u;
	ClientConfig.Dial.MaxAttempts = 1;
	ClientConfig.TlsContext = pContext;
	ClientConfig.TlsVerifier = pVerifier;
	ClientConfig.SystemTrust = false;
	ClientConfig.Pool.MaxConnections = 1;
	ClientConfig.Pool.MaxConnectionsPerOrigin = 1;
	ClientConfig.Pool.MaxIdle = 1;
	ClientConfig.Pool.MaxIdlePerOrigin = 1;
	ClientConfig.Pool.IdleTimeout = 0;
	State.Client = xrtHttpClientCreate(
		State.Engine,
		&ClientConfig
	);
	testRequire(
		State.Client != NULL,
		"HTTPS pool client creation failed"
	);

	testHttpPoolHttpsCall(&State, 0, Address, "one");
	testHttpPoolHttpsWait(
		&State.Calls[0].Completed,
		1,
		"HTTPS pool first call did not complete"
	);
	testHttpPoolHttpsCall(&State, 1, Address, "two");
	testHttpPoolHttpsWait(
		&State.Calls[1].Completed,
		1,
		"HTTPS pool reused call did not complete"
	);
	testHttpPoolHttpsWait(
		&State.Requests,
		2,
		"HTTPS pool server did not receive two requests"
	);
	testRequire(
		xrtHttpClientStats(State.Client, &Stats) &&
		(Stats.ActiveConnections == 0u) &&
		(Stats.IdleConnections == 1u) &&
		(Stats.WaitingCalls == 0u) &&
		(Stats.RequestsStarted == 2u) &&
		(Stats.RequestsCompleted == 2u) &&
		(Stats.ConnectionsOpened == 1u) &&
		(Stats.ConnectionsReused == 1u) &&
		(Stats.ConnectionsClosed == 0u) &&
		(xrtAtomic32Load(
			&State.Accepted,
			XMEMORY_ACQUIRE
		 ) == 1u),
		"HTTPS pool reuse statistics mismatch"
	);
	testRequire(
		xrtHttpClientCloseIdle(State.Client) == 1u,
		"HTTPS pool did not close its idle TLS stream"
	);
	testHttpPoolHttpsWait(
		&State.ServerClosed,
		1,
		"HTTPS pool did not complete close_notify"
	);
	testHttpPoolHttpsWaitClosed(State.Client);
	testRequire(
		xrtNetListenerClose(State.Listener),
		"HTTPS pool listener close failed"
	);
	testHttpPoolHttpsWait(
		&State.ListenerClosed,
		1,
		"HTTPS pool listener did not close"
	);

	for ( size_t i = 0; i < 2u; i++ ) {
		xrtHttpResponseDestroy(State.Calls[i].Response);
		xrtHttpCallDestroy(State.Calls[i].Call);
	}
	xrtHttpClientDestroy(State.Client);
	xrtTlsStreamDestroy(State.Server);
	xrtNetListenerDestroy(State.Listener);
	testRequire(
		xrtNetEngineDestroy(State.Engine),
		"HTTPS pool engine destroy failed"
	);
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	printf(
		"[PASS] HTTPS client connection pool (%s)\n",
		TEST_HTTP_POOL_HTTPS_BACKEND_NAME
	);
	return 0;
}


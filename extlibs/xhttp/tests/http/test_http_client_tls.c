#include "../fixtures/tls_server.h"



#if !defined(TEST_HTTP_CLIENT_TLS_BACKEND)
	#define TEST_HTTP_CLIENT_TLS_BACKEND XNET_PORT_SELECT
	#define TEST_HTTP_CLIENT_TLS_BACKEND_NAME "select"
#endif



/* HTTPS 夹具持有配置、网络对象和跨 Worker 观察结果。 */
typedef struct test_http_client_tls {
	xnetengine* Engine;
	xnetlistener* Listener;
	xtlsstream* Server;
	xtlsstream* Returned;
	xhttp1exchange* Exchange;
	xhttp1call* Call;
	xhttpresponse* Response;
	xtlsserverconfig ServerConfig;
	xtlsclientconfig ClientConfig;
	xtlsstreamconfig StreamConfig;
	xatomic32 Accepted;
	xatomic32 Started;
	xatomic32 RequestReady;
	xatomic32 Completed;
	xatomic32 ServerClosed;
	xatomic32 CancelPhase;
	bool Responded;
} test_http_client_tls;



/* 在截止时间前等待一个跨 Worker 终态。 */
static void testHttpClientTlsWait(
	xatomic32* pValue,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(10000000u);

	while ( xrtAtomic32Load(
		pValue,
		XMEMORY_ACQUIRE
	) == 0 ) {
		testRequire(
			!xrtDeadlineExpired(iDeadline),
			sMessage
		);
		xrtThreadYield();
	}
}



/* 服务端收到完整请求头后发送一个可复用的固定响应。 */
static void testHttpClientTlsServerRead(
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
	test_http_client_tls* pState =
		(test_http_client_tls*)pData;
	char Request[1024];
	size_t iSize = xrtTlsStreamAvailable(pStream);
	size_t i;
	size_t iWritten = 0;
	bool bComplete = false;

	testRequire(
		pBuffer == xrtTlsStreamBuffer(pStream),
		"HTTPS server plaintext buffer mismatch"
	);
	testRequire(
		(iSize != 0) && (iSize < sizeof(Request)),
		"HTTPS request exceeded the fixture limit"
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
		xrtTlsStreamConsume(pStream, iSize),
		"HTTPS server request consume failed"
	);
	if ( xrtAtomic32Load(
		&pState->CancelPhase,
		XMEMORY_ACQUIRE
	) ) {
		xrtAtomic32Store(
			&pState->RequestReady,
			1,
			XMEMORY_RELEASE
		);
		return;
	}
	testRequire(
		!pState->Responded,
		"HTTPS server sent more than one response"
	);
	pState->Responded = true;
	testRequire(
		(xrtTlsStreamSend(
			pStream,
			Response,
			sizeof(Response) - 1u,
			&iWritten
		) == XTLS_OK) &&
		(iWritten == (sizeof(Response) - 1u)),
		"HTTPS server response send failed"
	);
}



/* 对端认证关闭后由服务端回送 close_notify。 */
static void testHttpClientTlsServerEnd(
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



/* 记录服务端 TLS Stream 已经完成认证关闭。 */
static void testHttpClientTlsServerClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_client_tls* pState =
		(test_http_client_tls*)pData;

	if ( xrtAtomic32Load(
		&pState->CancelPhase,
		XMEMORY_ACQUIRE
	) ) {
		testRequire(
			(xrtTlsStreamState(pStream) ==
			 XTLS_STREAM_CLOSED) ||
			(xrtTlsStreamState(pStream) ==
			 XTLS_STREAM_FAILED),
			"cancelled HTTPS server TLS Stream is not terminal"
		);
		xrtAtomic32Store(
			&pState->ServerClosed,
			1,
			XMEMORY_RELEASE
		);
		(void)Result;
		(void)pError;
		return;
	}
	testRequire(
		(Result == XNET_RESULT_OK) &&
		(pError == NULL) &&
		(xrtTlsStreamState(pStream) == XTLS_STREAM_CLOSED),
		"HTTPS server TLS close result mismatch"
	);
	xrtAtomic32Store(
		&pState->ServerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* TCP Accept 后转移传输引用并启动服务端 TLS 握手。 */
static bool testHttpClientTlsAccept(
	xnetlistener* pListener,
	xnetstream* pTransport,
	ptr pData
)
{
	test_http_client_tls* pState =
		(test_http_client_tls*)pData;
	xtlsstreamevents Events;

	(void)pListener;
	memset(&Events, 0, sizeof(Events));
	Events.Read = testHttpClientTlsServerRead;
	Events.End = testHttpClientTlsServerEnd;
	Events.Close = testHttpClientTlsServerClose;
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



/* 接收响应对象和归还的可复用 TLS Stream 引用。 */
static void testHttpClientTlsDone(
	xhttp1call* pCall,
	const xhttp1callresult* pResult,
	ptr pData
)
{
	test_http_client_tls* pState =
		(test_http_client_tls*)pData;

	if ( xrtAtomic32Load(
		&pState->CancelPhase,
		XMEMORY_ACQUIRE
	) ) {
		testRequire(
			(pCall == pState->Call) &&
			(pResult != NULL) &&
			(pResult->Result ==
			 XNET_RESULT_CANCELLED) &&
			(pResult->Response == NULL) &&
			(pResult->Tcp == NULL) &&
			(pResult->Tls == NULL) &&
			(pResult->Error != NULL) &&
			(xrtErrorKind(pResult->Error) ==
			 XERR_CANCELLED) &&
			(xrtHttp1CallState(pCall) ==
			 XHTTP1_CALL_CANCELLED),
			"cancelled HTTPS call result mismatch"
		);
		xrtAtomic32Store(
			&pState->Completed,
			1,
			XMEMORY_RELEASE
		);
		return;
	}
	testRequire(
		(pCall == pState->Call) &&
		(pResult != NULL) &&
		(pResult->Result == XNET_RESULT_OK) &&
		(pResult->Response != NULL) &&
		(pResult->Tcp == NULL) &&
		(pResult->Tls != NULL) &&
		(pResult->Error == NULL) &&
		(pResult->Buffered == 0) &&
		pResult->Reusable &&
		!pResult->Upgraded,
		"HTTPS call result mismatch"
	);
	pState->Response = pResult->Response;
	pState->Returned = pResult->Tls;
	xrtAtomic32Store(
		&pState->Completed,
		1,
		XMEMORY_RELEASE
	);
}



/* 客户端握手完成后把 TLS Stream 和 Exchange 转移给 HTTP 调用。 */
static void testHttpClientTlsOpen(
	xtlsstream* pStream,
	ptr pData
)
{
	test_http_client_tls* pState =
		(test_http_client_tls*)pData;
	xhttp1callevents Events;

	xrtHttp1CallEventsInit(&Events);
	Events.Done = testHttpClientTlsDone;
	Events.Data = pState;
	pState->Call = xrtHttp1CallTls(
		pStream,
		pState->Exchange,
		NULL,
		&Events
	);
	testRequire(
		pState->Call != NULL,
		"HTTPS call creation failed"
	);
	pState->Exchange = NULL;
	xrtAtomic32Store(
		&pState->Started,
		1,
		XMEMORY_RELEASE
	);
}



/* 创建一条无正文 GET Exchange。 */
static xhttp1exchange* testHttpClientTlsExchange(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.com/test")
	);
	xhttp1requestplan* pPlan;
	xhttp1exchange* pExchange;

	if ( pRequest == NULL ) {
		return NULL;
	}
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	xrtHttpRequestDestroy(pRequest);
	if ( pPlan == NULL ) {
		return NULL;
	}
	pExchange = xrtHttp1ExchangeCreate(
		pPlan,
		NULL,
		NULL
	);
	if ( pExchange == NULL ) {
		xrtHttp1RequestPlanDestroy(pPlan);
	}
	return pExchange;
}



/* 验证真实 TLS 1.3 上的 HTTP/1 生命周期与跨 Worker 取消。 */
int main(void)
{
	static const xstrview Protocols[] = {
		XRT_STR_INIT("http/1.1")
	};
	test_http_client_tls State;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xtlsstreamevents ClientEvents;
	xtlsverifierconfig VerifierConfig;
	xtlscontext* pContext;
	xtlsidentity* pIdentity;
	xtlsverifier* pVerifier;
	xtlsstream* pClient;
	xnetaddr Address;
	xdeadline iDeadline;

	memset(&State, 0, sizeof(State));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&ClientEvents, 0, sizeof(ClientEvents));
	xrtAtomic32Init(&State.Accepted, 0);
	xrtAtomic32Init(&State.Started, 0);
	xrtAtomic32Init(&State.RequestReady, 0);
	xrtAtomic32Init(&State.Completed, 0);
	xrtAtomic32Init(&State.ServerClosed, 0);
	xrtAtomic32Init(&State.CancelPhase, 0);
	State.Exchange = testHttpClientTlsExchange();
	testRequire(
		State.Exchange != NULL,
		"HTTPS Exchange creation failed"
	);

	pContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	testRequire(
		(pContext != NULL) && (pIdentity != NULL),
		"HTTPS TLS fixture creation failed"
	);
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
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
	xrtTlsClientConfigInit(&State.ClientConfig);
	State.ClientConfig.Context = pContext;
	State.ClientConfig.ServerName =
		XRT_STR_LITERAL("example.com");
	State.ClientConfig.Protocols = Protocols;
	State.ClientConfig.ProtocolCount = 1u;
	State.ClientConfig.Verifier = pVerifier;
	xrtTlsStreamConfigInit(&State.StreamConfig);

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_HTTP_CLIENT_TLS_BACKEND;
	EngineConfig.Workers = 2u;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"HTTPS Engine start failed"
	);
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "HTTPS loopback address failed");
	ListenConfig.AcceptConcurrency = 4u;
	ListenerEvents.Accept = testHttpClientTlsAccept;
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
		"HTTPS Listener creation failed"
	);

	ClientEvents.Open = testHttpClientTlsOpen;
	pClient = xrtTlsStreamConnect(
		State.Engine,
		&Address,
		1u,
		NULL,
		&State.ClientConfig,
		&State.StreamConfig,
		&ClientEvents,
		&State
	);
	testRequire(
		pClient != NULL,
		"HTTPS client TLS Stream creation failed"
	);
	testHttpClientTlsWait(
		&State.Accepted,
		"HTTPS server did not accept the connection"
	);
	testHttpClientTlsWait(
		&State.Completed,
		"HTTPS call did not complete"
	);
	testRequire(
		(xrtHttpResponseStatus(State.Response) == 200) &&
		(xrtHttpResponseBody(State.Response).Size == 2u) &&
		(memcmp(
			xrtHttpResponseBody(State.Response).Data,
			"OK",
			2u
		) == 0) &&
		(xrtHttp1CallState(State.Call) ==
		 XHTTP1_CALL_SUCCEEDED),
		"HTTPS response mismatch"
	);

	testRequire(
		xrtTlsStreamClose(State.Returned),
		"HTTPS returned TLS Stream close failed"
	);
	testHttpClientTlsWait(
		&State.ServerClosed,
		"HTTPS server TLS Stream did not close"
	);
	iDeadline = xrtDeadlineAfter(10000000u);
	while ( (xrtTlsStreamState(State.Returned) !=
		XTLS_STREAM_CLOSED) ) {
		testRequire(
			!xrtDeadlineExpired(iDeadline),
			"HTTPS reusable TLS Stream did not close"
		);
		xrtThreadYield();
	}

	xrtHttpResponseDestroy(State.Response);
	xrtHttp1CallDestroy(State.Call);
	xrtTlsStreamDestroy(State.Returned);
	xrtTlsStreamDestroy(State.Server);
	State.Response = NULL;
	State.Call = NULL;
	State.Returned = NULL;
	State.Server = NULL;
	State.Responded = false;

	/*
		复用同一组真实 TLS 配置建立第二条连接。
		服务端确认收到完整请求后保持静默，由主线程跨 Worker 取消调用。
	*/
	xrtAtomic32Store(&State.Accepted, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&State.Started, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&State.RequestReady, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&State.Completed, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&State.ServerClosed, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&State.CancelPhase, 1, XMEMORY_RELEASE);
	State.Exchange = testHttpClientTlsExchange();
	testRequire(
		State.Exchange != NULL,
		"cancelled HTTPS Exchange creation failed"
	);
	pClient = xrtTlsStreamConnect(
		State.Engine,
		&Address,
		1u,
		NULL,
		&State.ClientConfig,
		&State.StreamConfig,
		&ClientEvents,
		&State
	);
	testRequire(
		pClient != NULL,
		"cancelled HTTPS client TLS Stream creation failed"
	);
	testHttpClientTlsWait(
		&State.Accepted,
		"cancelled HTTPS server did not accept the connection"
	);
	testHttpClientTlsWait(
		&State.Started,
		"cancelled HTTPS call did not start"
	);
	testHttpClientTlsWait(
		&State.RequestReady,
		"cancelled HTTPS request did not reach the server"
	);
	testRequire(
		xrtHttp1CallCancel(State.Call) &&
		!xrtHttp1CallCancel(State.Call),
		"HTTPS call cancellation acceptance mismatch"
	);
	testHttpClientTlsWait(
		&State.Completed,
		"cancelled HTTPS call did not complete"
	);
	testHttpClientTlsWait(
		&State.ServerClosed,
		"cancelled HTTPS server TLS Stream did not close"
	);
	testRequire(
		(xrtHttp1CallState(State.Call) ==
		 XHTTP1_CALL_CANCELLED) &&
		(xrtHttp1CallError(State.Call) != NULL) &&
		(xrtErrorKind(
			xrtHttp1CallError(State.Call)
		) == XERR_CANCELLED),
		"cancelled HTTPS call state mismatch"
	);
	xrtHttp1CallDestroy(State.Call);
	xrtTlsStreamDestroy(State.Server);

	testRequire(
		xrtNetListenerClose(State.Listener),
		"HTTPS Listener close failed"
	);
	iDeadline = xrtDeadlineAfter(10000000u);
	while ( xrtNetListenerState(State.Listener) !=
		XNET_LISTENER_CLOSED ) {
		testRequire(
			!xrtDeadlineExpired(iDeadline),
			"HTTPS Listener did not close"
		);
		xrtThreadYield();
	}
	xrtNetListenerDestroy(State.Listener);
	testRequire(
		xrtNetEngineDestroy(State.Engine),
		"HTTPS Engine destroy failed"
	);
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	printf(
		"[PASS] HTTP/1 TLS call lifecycle and cancellation (%s)\n",
		TEST_HTTP_CLIENT_TLS_BACKEND_NAME
	);
	return 0;
}

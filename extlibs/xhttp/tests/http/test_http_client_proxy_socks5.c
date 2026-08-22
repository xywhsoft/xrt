#include "../fixtures/http_connect_proxy.h"
#include "../fixtures/socks5_proxy.h"



#if !defined(TEST_HTTP_CLIENT_PROXY_SOCKS5_BACKEND)
	#define TEST_HTTP_CLIENT_PROXY_SOCKS5_BACKEND XNET_PORT_SELECT
	#define TEST_HTTP_CLIENT_PROXY_SOCKS5_BACKEND_NAME "select"
#endif



/* 代理服务端阶段覆盖认证、远端域名 CONNECT 和隧道内 HTTP。 */
typedef enum test_http_socks5_stage {
	TEST_HTTP_SOCKS5_GREETING = 0,
	TEST_HTTP_SOCKS5_AUTH,
	TEST_HTTP_SOCKS5_CONNECT,
	TEST_HTTP_SOCKS5_HTTP,
	TEST_HTTP_SOCKS5_STOPPED
} test_http_socks5_stage;



/* 夹具保存跨 Worker 发布后仍然有效的连接和协议事实。 */
typedef struct test_http_socks5 {
	xnetengine* Engine;
	xnetlistener* Listener;
	xnetstream* Server;
	xhttpclient* Client;
	xhttpcall* Call;
	xhttpcall* CallbackCall;
	xhttpresponse* Response;
	xatomic32 Accepted;
	xatomic32 Authenticated;
	xatomic32 Connected;
	xatomic32 Requested;
	xatomic32 Completed;
	xatomic32 ServerClosed;
	xatomic32 ListenerClosed;
	test_http_socks5_stage Stage;
	test_socks5_target Target;
	uint16 ProxyPort;
} test_http_socks5;



/* 在统一截止时间内等待网络 Worker 发布状态。 */
static void testHttpSocks5Wait(
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



/*
	只允许解析代理端点。
	若高层错误地在本机解析目标域名，测试立即失败。
*/
static xnetaddrlist* testHttpSocks5Lookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)pData;
	testRequire(
		strcmp(sHost, "proxy.test") == 0,
		"HTTP SOCKS5 client resolved the target locally"
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
		"HTTP SOCKS5 resolver fixture failed"
	);
	return xrtNetAddrListCreate(&Address, 1);
}



/* 验证隧道内应用请求并发送固定响应。 */
static bool testHttpSocks5Request(
	test_http_socks5* pState,
	xnetstream* pStream,
	xnetbuf* pBuffer
)
{
	static const char Response[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 5\r\n"
		"Connection: close\r\n"
		"\r\n"
		"SOCKS";
	char Request[2048];
	size_t iSize = xrtNetBufSize(pBuffer);
	size_t iHeader;

	if ( iSize == 0 ) {
		return false;
	}
	testRequire(
		iSize < sizeof(Request),
		"HTTP SOCKS5 request overflowed"
	);
	testRequire(
		xrtNetBufPeek(
			pBuffer,
			0,
			Request,
			iSize
		) == iSize,
		"HTTP SOCKS5 request peek failed"
	);
	Request[iSize] = 0;
	iHeader = testHttpHeaderSize(Request, iSize);
	if ( iHeader == 0 ) {
		return false;
	}
	testRequire(
		strncmp(
			Request,
			"GET /through-socks HTTP/1.1\r\n",
			29
		) == 0,
		"HTTP SOCKS5 request target was not origin-form"
	);
	testRequire(
		strstr(
			Request,
			"\r\nHost: origin.test:8443\r\n"
		) != NULL,
		"HTTP SOCKS5 target Host mismatch"
	);
	testRequire(
		strstr(
			Request,
			"\r\nProxy-Authorization:"
		) == NULL,
		"HTTP SOCKS5 credentials leaked into HTTP Headers"
	);
	(void)xrtNetBufConsume(pBuffer, iHeader);
	pState->Stage = TEST_HTTP_SOCKS5_STOPPED;
	xrtAtomic32Store(
		&pState->Requested,
		1,
		XMEMORY_RELEASE
	);
	testRequire(
		xrtNetStreamSend(
			pStream,
			Response,
			sizeof(Response) - 1u
		) == XNET_RESULT_OK,
		"HTTP SOCKS5 response failed"
	);
	return true;
}



/* 按共享 SOCKS5 夹具增量推进代理握手和应用协议。 */
static void testHttpSocks5Read(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	test_http_socks5* pState =
		(test_http_socks5*)pData;
	bool bProgress;

	do {
		bProgress = false;
		switch ( pState->Stage ) {
			case TEST_HTTP_SOCKS5_GREETING:
				bProgress =
					testSocks5ProxyGreetingRequest(
						pBuffer,
						0x02
					);
				if ( bProgress ) {
					pState->Stage =
						TEST_HTTP_SOCKS5_AUTH;
					testSocks5ProxyMethodReply(
						pStream,
						0x05,
						0x02
					);
				}
				break;
			case TEST_HTTP_SOCKS5_AUTH:
				bProgress = testSocks5ProxyAuthRequest(
					pBuffer,
					XRT_BYTES_LITERAL("user"),
					XRT_BYTES_LITERAL("password")
				);
				if ( bProgress ) {
					pState->Stage =
						TEST_HTTP_SOCKS5_CONNECT;
					xrtAtomic32Store(
						&pState->Authenticated,
						1,
						XMEMORY_RELEASE
					);
					testSocks5ProxyAuthReply(
						pStream,
						0
					);
				}
				break;
			case TEST_HTTP_SOCKS5_CONNECT:
				bProgress =
					testSocks5ProxyConnectRequest(
						pBuffer,
						&pState->Target
					);
				if ( bProgress ) {
					pState->Stage =
						TEST_HTTP_SOCKS5_HTTP;
					xrtAtomic32Store(
						&pState->Connected,
						1,
						XMEMORY_RELEASE
					);
					testSocks5ProxyConnectReply(
						pStream,
						XNET_SOCKS5_SUCCEEDED,
						(xbytesview){ NULL, 0 }
					);
				}
				break;
			case TEST_HTTP_SOCKS5_HTTP:
				bProgress = testHttpSocks5Request(
					pState,
					pStream,
					pBuffer
				);
				break;
			default:
				return;
		}
	} while ( bProgress && !xrtNetBufEmpty(pBuffer) );
}



/* 对端结束写方向后完成服务端正常关闭。 */
static void testHttpSocks5End(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	(void)xrtNetStreamClose(pStream);
}



/* 发布代理服务端连接关闭。 */
static void testHttpSocks5Close(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_socks5* pState =
		(test_http_socks5*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	xrtAtomic32Store(
		&pState->ServerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 接管唯一代理连接并安装显式 Stream 数据。 */
static bool testHttpSocks5Accept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	test_http_socks5* pState =
		(test_http_socks5*)pData;

	(void)pListener;
	testRequire(
		pState->Server == NULL,
		"HTTP SOCKS5 opened an unexpected second connection"
	);
	testRequire(
		xrtNetStreamSetData(pStream, pState),
		"HTTP SOCKS5 stream data setup failed"
	);
	pState->Server = pStream;
	xrtAtomic32Store(
		&pState->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 发布代理 Listener 关闭。 */
static void testHttpSocks5ListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_http_socks5* pState =
		(test_http_socks5*)pData;

	(void)pListener;
	xrtAtomic32Store(
		&pState->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证高层成功终态并接管响应所有权。 */
static void testHttpSocks5Done(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	test_http_socks5* pState =
		(test_http_socks5*)pData;
	xbytesview Body;
	bool bSuccess;

	bSuccess =
		(pCall != NULL) &&
		(pResult != NULL) &&
		(pResult->Result == XNET_RESULT_OK) &&
		(pResult->Response != NULL) &&
		(pResult->Tcp == NULL) &&
		(pResult->Error == NULL) &&
		!pResult->Upgraded &&
		(xrtHttpCallState(pCall) ==
		 XHTTP_CALL_SUCCEEDED);
	if ( !bSuccess ) {
		const xerror* pCause = pResult != NULL ?
			pResult->Error : NULL;

		fprintf(
			stderr,
			"[INFO] result=%d response=%p tcp=%p error=%p "
			"upgrade=%d state=%d code=%d message=%s\n",
			pResult != NULL ? (int)pResult->Result : -1,
			pResult != NULL ?
				(void*)pResult->Response : NULL,
			pResult != NULL ? (void*)pResult->Tcp : NULL,
			pResult != NULL ? (void*)pResult->Error : NULL,
			pResult != NULL ?
				(int)pResult->Upgraded : -1,
			pCall != NULL ?
				(int)xrtHttpCallState(pCall) : -1,
			(pResult != NULL) &&
			(pResult->Error != NULL) ?
				(int)xrtErrorCode(pResult->Error) : 0,
			(pResult != NULL) &&
			(pResult->Error != NULL) ?
				xrtErrorMessage(pResult->Error) : ""
		);
		while ( pCause != NULL ) {
			fprintf(
				stderr,
				"[CAUSE] %s/%d kind=%d operation=%s "
				"message=%s\n",
				xrtErrorDomain(pCause),
				(int)xrtErrorCode(pCause),
				(int)xrtErrorKind(pCause),
				xrtErrorOperation(pCause),
				xrtErrorMessage(pCause)
			);
			pCause = xrtErrorCause(pCause);
		}
	}
	testRequire(
		bSuccess,
		"HTTP SOCKS5 completion mismatch"
	);
	pState->CallbackCall = pCall;
	Body = xrtHttpResponseBody(pResult->Response);
	testRequire(
		(xrtHttpResponseStatus(pResult->Response) == 200) &&
		(Body.Size == 5u) &&
		(memcmp(Body.Data, "SOCKS", 5u) == 0),
		"HTTP SOCKS5 response mismatch"
	);
	pState->Response = pResult->Response;
	xrtAtomic32Store(
		&pState->Completed,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证高层 HTTP 通过认证 SOCKS5 使用远端 DNS。 */
int main(void)
{
	test_http_socks5 State;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents StreamEvents;
	xhttpclientconfig ClientConfig;
	xhttpcalloptions Options;
	xnetenginestats EngineStats;
	xnetproxyconfig ProxyConfig;
	xnetproxy* pProxy;
	xhttprequest* pRequest;
	xnetaddr Address;

	memset(&State, 0, sizeof(State));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&StreamEvents, 0, sizeof(StreamEvents));
	xrtAtomic32Init(&State.Accepted, 0);
	xrtAtomic32Init(&State.Authenticated, 0);
	xrtAtomic32Init(&State.Connected, 0);
	xrtAtomic32Init(&State.Requested, 0);
	xrtAtomic32Init(&State.Completed, 0);
	xrtAtomic32Init(&State.ServerClosed, 0);
	xrtAtomic32Init(&State.ListenerClosed, 0);
	State.Stage = TEST_HTTP_SOCKS5_GREETING;
	ListenerEvents.Accept = testHttpSocks5Accept;
	ListenerEvents.Close = testHttpSocks5ListenerClose;
	StreamEvents.Read = testHttpSocks5Read;
	StreamEvents.End = testHttpSocks5End;
	StreamEvents.Close = testHttpSocks5Close;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend =
		TEST_HTTP_CLIENT_PROXY_SOCKS5_BACKEND;
	EngineConfig.Workers = 1;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"HTTP SOCKS5 engine start failed"
	);
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ListenConfig.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP SOCKS5 listener address failed"
	);
	ListenConfig.Stream.ReadSize = 32;
	ListenConfig.Stream.ReadLimit = 4096;
	State.Listener = xrtNetListen(
		State.Engine,
		&ListenConfig,
		&ListenerEvents,
		&StreamEvents,
		&State
	);
	testRequire(
		(State.Listener != NULL) &&
		xrtNetListenerLocal(State.Listener, &Address),
		"HTTP SOCKS5 listener creation failed"
	);
	State.ProxyPort = Address.Port;

	xrtNetProxyConfigInit(&ProxyConfig);
	ProxyConfig.Type = XNET_PROXY_SOCKS5;
	ProxyConfig.Host = XRT_STR_LITERAL("proxy.test");
	ProxyConfig.Port = State.ProxyPort;
	ProxyConfig.Username = XRT_BYTES_LITERAL("user");
	ProxyConfig.Password = XRT_BYTES_LITERAL("password");
	pProxy = xrtNetProxyCreate(&ProxyConfig);
	testRequire(
		pProxy != NULL,
		"HTTP SOCKS5 endpoint creation failed"
	);
	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Resolver.Lookup = testHttpSocks5Lookup;
	ClientConfig.Resolver.LookupData = &State;
	ClientConfig.Dial.FallbackDelay = 1000u;
	ClientConfig.Dial.MaxAttempts = 1;
	ClientConfig.Proxy = pProxy;
	State.Client = xrtHttpClientCreate(
		State.Engine,
		&ClientConfig
	);
	xrtNetProxyRelease(pProxy);
	testRequire(
		State.Client != NULL,
		"HTTP SOCKS5 client creation failed"
	);

	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL(
			"http://origin.test:8443/through-socks"
		)
	);
	testRequire(
		pRequest != NULL,
		"HTTP SOCKS5 request creation failed"
	);
	xrtHttpCallOptionsInit(&Options);
	Options.Timeout = 5000000u;
	State.Call = xrtHttpClientDo(
		State.Client,
		pRequest,
		&Options,
		testHttpSocks5Done,
		&State
	);
	xrtHttpRequestDestroy(pRequest);
	testRequire(
		State.Call != NULL,
		"HTTP SOCKS5 Call submission failed"
	);

	testHttpSocks5Wait(
		&State.Completed,
		"HTTP SOCKS5 Call did not complete"
	);
	testRequire(
		(xrtAtomic32Load(
			&State.Accepted,
			XMEMORY_ACQUIRE
		) == 1) &&
		(xrtAtomic32Load(
			&State.Authenticated,
			XMEMORY_ACQUIRE
		) == 1) &&
		(xrtAtomic32Load(
			&State.Connected,
			XMEMORY_ACQUIRE
		) == 1) &&
		(xrtAtomic32Load(
			&State.Requested,
			XMEMORY_ACQUIRE
		) == 1) &&
		(strcmp(State.Target.Host, "origin.test") == 0) &&
		(State.Target.Port == 8443) &&
		(State.CallbackCall == State.Call) &&
		!xrtHttpCallCancel(State.Call),
		"HTTP SOCKS5 composition facts mismatch"
	);
	testHttpSocks5Wait(
		&State.ServerClosed,
		"HTTP SOCKS5 transport did not close"
	);
	testRequire(
		xrtNetListenerClose(State.Listener),
		"HTTP SOCKS5 listener close failed"
	);
	testHttpSocks5Wait(
		&State.ListenerClosed,
		"HTTP SOCKS5 listener did not close"
	);

	xrtHttpResponseDestroy(State.Response);
	xrtHttpCallDestroy(State.Call);
	xrtHttpClientDestroy(State.Client);
	xrtNetStreamDestroy(State.Server);
	xrtNetListenerDestroy(State.Listener);
	testRequire(
		xrtNetEngineStats(State.Engine, &EngineStats) &&
		(EngineStats.LiveObjects == 0) &&
		xrtNetEngineDestroy(State.Engine),
		"HTTP SOCKS5 engine did not drain"
	);
	printf(
		"[PASS] HTTP client SOCKS5 composition (%s)\n",
		TEST_HTTP_CLIENT_PROXY_SOCKS5_BACKEND_NAME
	);
	return 0;
}



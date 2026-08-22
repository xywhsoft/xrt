#include "../fixtures/http_connect_proxy.h"



#if !defined(TEST_HTTP_CLIENT_PROXY_BACKEND)
	#define TEST_HTTP_CLIENT_PROXY_BACKEND XNET_PORT_SELECT
	#define TEST_HTTP_CLIENT_PROXY_BACKEND_NAME "select"
#endif



typedef enum test_http_proxy_peer_mode {
	TEST_HTTP_PROXY_UNKNOWN = 0,
	TEST_HTTP_PROXY_TUNNEL,
	TEST_HTTP_PROXY_DIRECT
} test_http_proxy_peer_mode;



typedef struct test_http_proxy test_http_proxy;



/* 每条服务端 TCP 连接独立记录它是代理隧道还是直连。 */
typedef struct test_http_proxy_peer {
	test_http_proxy* Owner;
	xnetstream* Stream;
	test_http_proxy_peer_mode Mode;
} test_http_proxy_peer;



/* 夹具保存代理、直连和连接池复用的全部可观察事实。 */
struct test_http_proxy {
	xnetengine* Engine;
	xnetlistener* Listener;
	xhttpclient* Client;
	test_http_proxy_peer Peers[2];
	xhttpcall* Calls[3];
	xatomic32 Accepted;
	xatomic32 Connects;
	xatomic32 Proxied;
	xatomic32 Direct;
	xatomic32 Completed;
	xatomic32 Closed;
	xatomic32 ListenerClosed;
	uint16 Port;
};



/* 每个完成上下文声明本次响应必须来自哪条路由。 */
typedef struct test_http_proxy_call {
	test_http_proxy* Owner;
	cstr ExpectedBody;
} test_http_proxy_call;



/* 在统一截止时间内等待跨 Worker 计数达到期望值。 */
static void testHttpProxyWait(
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



/* 代理名和目标名都解析到同一个双角色 Listener。 */
static xnetaddrlist* testHttpProxyLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)pData;
	testRequire(
		(strcmp(sHost, "proxy.test") == 0) ||
		(strcmp(sHost, "origin.test") == 0),
		"HTTP proxy resolved an unexpected host"
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
		"HTTP proxy resolver fixture failed"
	);
	return xrtNetAddrListCreate(&Address, 1);
}



/* 验证 CONNECT 目标与凭据，随后把连接切换为隧道模式。 */
static void testHttpProxyConnect(
	test_http_proxy_peer* pPeer,
	xnetstream* pStream,
	xnetbuf* pBuffer
)
{
	testRequire(
		testHttpConnectProxyStep(
			pStream,
			pBuffer,
			"origin.test",
			pPeer->Owner->Port,
			true
		),
		"HTTP client CONNECT request remained incomplete"
	);
	pPeer->Mode = TEST_HTTP_PROXY_TUNNEL;
	(void)xrtAtomic32FetchAdd(
		&pPeer->Owner->Connects,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证应用请求没有泄漏代理凭据，并返回路由特定正文。 */
static void testHttpProxyRequest(
	test_http_proxy_peer* pPeer,
	xnetstream* pStream,
	const char* pRequest,
	size_t iHeader
)
{
	static const char ProxyResponse[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 5\r\n"
		"Connection: keep-alive\r\n"
		"\r\n"
		"proxy";
	static const char DirectResponse[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 6\r\n"
		"Connection: keep-alive\r\n"
		"\r\n"
		"direct";
	test_http_proxy* pState = pPeer->Owner;
	cstr sResponse;
	size_t iResponse;

	(void)iHeader;
	testRequire(
		strncmp(pRequest, "GET /", 5) == 0,
		"HTTP proxy fixture received a non-origin-form request"
	);
	testRequire(
		strstr(pRequest, "\r\nProxy-Authorization:") == NULL,
		"HTTP client leaked proxy credentials into the target request"
	);
	if ( pPeer->Mode == TEST_HTTP_PROXY_UNKNOWN ) {
		testRequire(
			strncmp(
				pRequest,
				"GET /direct HTTP/1.1\r\n",
				22
			) == 0,
			"HTTP direct override used the wrong request target"
		);
		pPeer->Mode = TEST_HTTP_PROXY_DIRECT;
	}
	if ( pPeer->Mode == TEST_HTTP_PROXY_TUNNEL ) {
		uint32 iRequest = xrtAtomic32FetchAdd(
			&pState->Proxied,
			1,
			XMEMORY_ACQ_REL
		);

		testRequire(
			((iRequest == 0) &&
			 (strncmp(
				pRequest,
				"GET /proxied HTTP/1.1\r\n",
				23
			 ) == 0)) ||
			((iRequest == 1) &&
			 (strncmp(
				pRequest,
				"GET /proxied-again HTTP/1.1\r\n",
				29
			 ) == 0)),
			"HTTP proxy tunnel request order mismatch"
		);
		sResponse = ProxyResponse;
		iResponse = sizeof(ProxyResponse) - 1u;
	} else {
		(void)xrtAtomic32FetchAdd(
			&pState->Direct,
			1,
			XMEMORY_RELEASE
		);
		sResponse = DirectResponse;
		iResponse = sizeof(DirectResponse) - 1u;
	}
	testRequire(
		xrtNetStreamSend(
			pStream,
			sResponse,
			iResponse
		) == XNET_RESULT_OK,
		"HTTP proxy fixture response failed"
	);
}



/* 增量处理 CONNECT 与同连接上的一个或多个 HTTP 请求。 */
static void testHttpProxyRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	test_http_proxy_peer* pPeer =
		(test_http_proxy_peer*)pData;
	char Request[2048];
	size_t iSize;
	size_t iHeader;

	for ( ;; ) {
		iSize = xrtNetBufSize(pBuffer);
		if ( iSize == 0 ) {
			return;
		}
		testRequire(
			iSize < sizeof(Request),
			"HTTP proxy fixture input overflowed"
		);
		testRequire(
			xrtNetBufPeek(
				pBuffer,
				0,
				Request,
				iSize
			) == iSize,
			"HTTP proxy fixture input peek failed"
		);
		Request[iSize] = 0;
		iHeader = testHttpHeaderSize(
			Request,
			iSize
		);
		if ( iHeader == 0 ) {
			return;
		}
		if ( (pPeer->Mode == TEST_HTTP_PROXY_UNKNOWN) &&
			(strncmp(Request, "CONNECT ", 8) == 0) ) {
			testHttpProxyConnect(
				pPeer,
				pStream,
				pBuffer
			);
			continue;
		} else {
			testHttpProxyRequest(
				pPeer,
				pStream,
				Request,
				iHeader
			);
		}
		(void)xrtNetBufConsume(pBuffer, iHeader);
	}
}



/* 对端半关闭时完成服务端正常关闭。 */
static void testHttpProxyEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	(void)xrtNetStreamClose(pStream);
}



/* 记录每条代理或直连 TCP 已经释放底层套接字。 */
static void testHttpProxyClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_proxy_peer* pPeer =
		(test_http_proxy_peer*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pPeer->Owner->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 为两条预期连接分配独立角色状态。 */
static bool testHttpProxyAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	test_http_proxy* pState = (test_http_proxy*)pData;
	uint32 iIndex = xrtAtomic32FetchAdd(
		&pState->Accepted,
		1,
		XMEMORY_ACQ_REL
	);
	test_http_proxy_peer* pPeer;

	(void)pListener;
	testRequire(
		iIndex < 2u,
		"HTTP proxy opened more connections than expected"
	);
	pPeer = &pState->Peers[iIndex];
	pPeer->Owner = pState;
	pPeer->Stream = pStream;
	pPeer->Mode = TEST_HTTP_PROXY_UNKNOWN;
	testRequire(
		xrtNetStreamSetData(pStream, pPeer),
		"HTTP proxy peer data setup failed"
	);
	return true;
}



/* 记录 Listener 唯一关闭事件。 */
static void testHttpProxyListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_http_proxy* pState = (test_http_proxy*)pData;

	(void)pListener;
	xrtAtomic32Store(
		&pState->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证高层结果正文并立即释放响应所有权。 */
static void testHttpProxyDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	test_http_proxy_call* pExpected =
		(test_http_proxy_call*)pData;
	xbytesview Body;
	size_t iExpected = strlen(pExpected->ExpectedBody);

	(void)pCall;
	testRequire(
		(pResult != NULL) &&
		(pResult->Result == XNET_RESULT_OK) &&
		(pResult->Response != NULL) &&
		(pResult->Error == NULL),
		"HTTP proxy call failed"
	);
	Body = xrtHttpResponseBody(pResult->Response);
	testRequire(
		(Body.Size == iExpected) &&
		(memcmp(
			Body.Data,
			pExpected->ExpectedBody,
			iExpected
		) == 0),
		"HTTP proxy response body mismatch"
	);
	xrtHttpResponseDestroy(pResult->Response);
	(void)xrtAtomic32FetchAdd(
		&pExpected->Owner->Completed,
		1,
		XMEMORY_RELEASE
	);
}



/* 提交一条默认代理或显式直连请求。 */
static xhttpcall* testHttpProxySubmit(
	test_http_proxy* pState,
	cstr sPath,
	xhttpproxymode Mode,
	test_http_proxy_call* pExpected
)
{
	xhttpcalloptions Options;
	xhttprequest* pRequest;
	xhttpcall* pCall;
	char Url[256];
	int iLength;

	iLength = snprintf(
		Url,
		sizeof(Url),
		"http://origin.test:%u%s",
		(unsigned)pState->Port,
		sPath
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Url)),
		"HTTP proxy URL fixture overflowed"
	);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		(xstrview){ Url, (size_t)iLength }
	);
	testRequire(
		pRequest != NULL,
		"HTTP proxy request creation failed"
	);
	xrtHttpCallOptionsInit(&Options);
	Options.Proxy.Mode = Mode;
	if ( Mode == XHTTP_PROXY_EXPLICIT ) {
		Options.Proxy.Proxy =
			xrtHttpClientProxy(pState->Client);
	}
	pCall = xrtHttpClientDo(
		pState->Client,
		pRequest,
		&Options,
		testHttpProxyDone,
		pExpected
	);
	xrtHttpRequestDestroy(pRequest);
	testRequire(
		pCall != NULL,
		"HTTP proxy call submission failed"
	);
	return pCall;
}



/* 验证代理与直连池键隔离，并验证默认和显式选择能够复用同一代理。 */
int main(void)
{
	test_http_proxy State;
	test_http_proxy_call Expected[3];
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents StreamEvents;
	xhttpclientconfig ClientConfig;
	xhttpclientstats Stats;
	xnetproxyconfig ProxyConfig;
	xnetproxy* pProxy;
	xnetaddr Address;

	memset(&State, 0, sizeof(State));
	memset(&Expected, 0, sizeof(Expected));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&StreamEvents, 0, sizeof(StreamEvents));
	ListenerEvents.Accept = testHttpProxyAccept;
	ListenerEvents.Close = testHttpProxyListenerClose;
	StreamEvents.Read = testHttpProxyRead;
	StreamEvents.End = testHttpProxyEnd;
	StreamEvents.Close = testHttpProxyClose;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_HTTP_CLIENT_PROXY_BACKEND;
	EngineConfig.Workers = 1;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"HTTP proxy engine start failed"
	);
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ListenConfig.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP proxy listener address failed"
	);
	ListenConfig.Stream.ReadSize = 64;
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
		"HTTP proxy listener creation failed"
	);
	State.Port = Address.Port;

	xrtNetProxyConfigInit(&ProxyConfig);
	ProxyConfig.Type = XNET_PROXY_HTTP_CONNECT;
	ProxyConfig.Host = XRT_STR_LITERAL("proxy.test");
	ProxyConfig.Port = State.Port;
	ProxyConfig.Username = XRT_BYTES_LITERAL("user");
	ProxyConfig.Password = XRT_BYTES_LITERAL("password");
	pProxy = xrtNetProxyCreate(&ProxyConfig);
	testRequire(
		pProxy != NULL,
		"HTTP proxy endpoint creation failed"
	);
	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Resolver.Lookup = testHttpProxyLookup;
	ClientConfig.Dial.FallbackDelay = 1000u;
	ClientConfig.Dial.MaxAttempts = 1;
	ClientConfig.Proxy = pProxy;
	ClientConfig.Pool.MaxConnections = 2;
	ClientConfig.Pool.MaxConnectionsPerOrigin = 1;
	ClientConfig.Pool.MaxIdle = 2;
	ClientConfig.Pool.MaxIdlePerOrigin = 1;
	State.Client = xrtHttpClientCreate(
		State.Engine,
		&ClientConfig
	);
	testRequire(
		State.Client != NULL,
		"HTTP proxy client creation failed"
	);
	xrtNetProxyRelease(pProxy);

	Expected[0].Owner = &State;
	Expected[0].ExpectedBody = "proxy";
	State.Calls[0] = testHttpProxySubmit(
		&State,
		"/proxied",
		XHTTP_PROXY_DEFAULT,
		&Expected[0]
	);
	testHttpProxyWait(
		&State.Completed,
		1,
		"HTTP proxied call did not complete"
	);

	Expected[1].Owner = &State;
	Expected[1].ExpectedBody = "direct";
	State.Calls[1] = testHttpProxySubmit(
		&State,
		"/direct",
		XHTTP_PROXY_DIRECT,
		&Expected[1]
	);
	testHttpProxyWait(
		&State.Completed,
		2,
		"HTTP direct override did not complete"
	);

	Expected[2].Owner = &State;
	Expected[2].ExpectedBody = "proxy";
	State.Calls[2] = testHttpProxySubmit(
		&State,
		"/proxied-again",
		XHTTP_PROXY_EXPLICIT,
		&Expected[2]
	);
	testHttpProxyWait(
		&State.Completed,
		3,
		"HTTP proxy reuse call did not complete"
	);
	testRequire(
		xrtHttpClientStats(State.Client, &Stats) &&
		(xrtAtomic32Load(
			&State.Accepted,
			XMEMORY_ACQUIRE
		) == 2) &&
		(xrtAtomic32Load(
			&State.Connects,
			XMEMORY_ACQUIRE
		) == 1) &&
		(xrtAtomic32Load(
			&State.Proxied,
			XMEMORY_ACQUIRE
		) == 2) &&
		(xrtAtomic32Load(
			&State.Direct,
			XMEMORY_ACQUIRE
		) == 1) &&
		(Stats.ConnectionsOpened == 2) &&
		(Stats.ConnectionsReused == 1),
		"HTTP proxy pool route identity mismatch"
	);

	for ( size_t i = 0; i < 3u; i++ ) {
		xrtHttpCallDestroy(State.Calls[i]);
	}
	testRequire(
		xrtHttpClientCloseIdle(State.Client) == 2u,
		"HTTP proxy idle close count mismatch"
	);
	testHttpProxyWait(
		&State.Closed,
		2,
		"HTTP proxy transports did not close"
	);
	xrtHttpClientDestroy(State.Client);
	testRequire(
		xrtNetListenerClose(State.Listener),
		"HTTP proxy listener close failed"
	);
	testHttpProxyWait(
		&State.ListenerClosed,
		1,
		"HTTP proxy listener did not close"
	);
	for ( size_t i = 0; i < 2u; i++ ) {
		xrtNetStreamDestroy(State.Peers[i].Stream);
	}
	xrtNetListenerDestroy(State.Listener);
	testRequire(
		xrtNetEngineDestroy(State.Engine),
		"HTTP proxy engine destroy failed"
	);
	printf(
		"[PASS] HTTP client CONNECT proxy and pool identity (%s)\n",
		TEST_HTTP_CLIENT_PROXY_BACKEND_NAME
	);
	return 0;
}

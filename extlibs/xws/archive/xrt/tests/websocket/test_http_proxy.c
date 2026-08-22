#include "../fixtures/http_connect_proxy.h"



#if !defined(TEST_WS_HTTP_PROXY_BACKEND)
	#define TEST_WS_HTTP_PROXY_BACKEND XNET_PORT_SELECT
	#define TEST_WS_HTTP_PROXY_BACKEND_NAME "select"
#endif

#define TEST_WS_HTTP_PROXY_TEXT "proxy-websocket"



typedef enum test_ws_proxy_mode {
	TEST_WS_PROXY_CONNECT = 0,
	TEST_WS_PROXY_HANDSHAKE,
	TEST_WS_PROXY_WEBSOCKET
} test_ws_proxy_mode;



typedef struct test_ws_proxy_endpoint {
	uint8 Opcode;
	size_t Size;
	uint8 Data[64];
	xwsconnclose Close;
} test_ws_proxy_endpoint;



typedef struct test_ws_proxy test_ws_proxy;



typedef struct test_ws_proxy_peer {
	test_ws_proxy* Owner;
	xnetstream* Stream;
	test_ws_proxy_mode Mode;
} test_ws_proxy_peer;



/* 保存代理握手、WebSocket 会话和跨 Worker 可观察状态。 */
struct test_ws_proxy {
	xnetengine* Engine;
	xnetlistener* Listener;
	xhttpclient* Client;
	xhttpcall* Call;
	xhttpresponse* Response;
	test_ws_proxy_peer Peer;
	test_ws_proxy_endpoint ClientEndpoint;
	test_ws_proxy_endpoint ServerEndpoint;
	xwsconnevents WsEvents;
	xatomicptr ClientConnection;
	xatomicptr ServerConnection;
	xatomic32 Accepted;
	xatomic32 Connects;
	xatomic32 Upgraded;
	xatomic32 ClientDone;
	xatomic32 ClientMessages;
	xatomic32 ServerMessages;
	xatomic32 ClientClosed;
	xatomic32 ServerClosed;
	xatomic32 ListenerClosed;
	xatomic32 Errors;
	uint16 Port;
};



/* 比较借用文本视图与编译期字符串。 */
static bool testWsProxyTextEqual(
	xstrview Text,
	cstr sExpected
)
{
	size_t iExpected = strlen(sExpected);

	return (Text.Size == iExpected) &&
		((iExpected == 0) ||
		 (memcmp(Text.Data, sExpected, iExpected) == 0));
}



/* 在统一截止时间内等待跨 Worker 计数达到期望值。 */
static void testWsProxyWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(10000000));

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



/* 代理名和目标名都解析到本地双角色 Listener。 */
static xnetaddrlist* testWsProxyLookup(
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
		"WebSocket proxy resolved an unexpected host"
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
		"WebSocket proxy resolver fixture failed"
	);
	return xrtNetAddrListCreate(&Address, 1);
}



/* 按连接角色返回独立消息暂存。 */
static test_ws_proxy_endpoint* testWsProxyEndpoint(
	test_ws_proxy* pState,
	xwsconn* pConnection
)
{
	return xrtWsConnRole(pConnection) == XWS_ROLE_CLIENT ?
		&pState->ClientEndpoint : &pState->ServerEndpoint;
}



/* 开始一条数据消息并重置对应暂存。 */
static void testWsProxyMessageBegin(
	xwsconn* pConnection,
	const xwsmessageinfo* pInfo,
	ptr pData
)
{
	test_ws_proxy* pState = (test_ws_proxy*)pData;
	test_ws_proxy_endpoint* pEndpoint =
		testWsProxyEndpoint(pState, pConnection);

	testRequire(
		pInfo != NULL,
		"WebSocket proxy message info is null"
	);
	pEndpoint->Opcode = pInfo->Opcode;
	pEndpoint->Size = 0;
}



/* 逐块保存有界测试消息，不依赖一次 Read 交付完整帧。 */
static void testWsProxyMessageData(
	xwsconn* pConnection,
	xbytesview Data,
	ptr pData
)
{
	test_ws_proxy* pState = (test_ws_proxy*)pData;
	test_ws_proxy_endpoint* pEndpoint =
		testWsProxyEndpoint(pState, pConnection);

	testRequire(
		Data.Size <=
			(sizeof(pEndpoint->Data) - pEndpoint->Size),
		"WebSocket proxy message exceeded fixture storage"
	);
	if ( Data.Size != 0 ) {
		memcpy(
			pEndpoint->Data + pEndpoint->Size,
			Data.Data,
			Data.Size
		);
	}
	pEndpoint->Size += Data.Size;
}



/* 服务端回显消息，客户端收到回显后发起正常关闭。 */
static void testWsProxyMessageEnd(
	xwsconn* pConnection,
	ptr pData
)
{
	test_ws_proxy* pState = (test_ws_proxy*)pData;
	test_ws_proxy_endpoint* pEndpoint =
		testWsProxyEndpoint(pState, pConnection);

	testRequire(
		(pEndpoint->Opcode == XWS_OPCODE_TEXT) &&
		(pEndpoint->Size ==
		 (sizeof(TEST_WS_HTTP_PROXY_TEXT) - 1u)) &&
		(memcmp(
			pEndpoint->Data,
			TEST_WS_HTTP_PROXY_TEXT,
			pEndpoint->Size
		 ) == 0),
		"WebSocket proxy message payload mismatch"
	);
	if ( xrtWsConnRole(pConnection) == XWS_ROLE_SERVER ) {
		(void)xrtAtomic32FetchAdd(
			&pState->ServerMessages,
			1,
			XMEMORY_RELEASE
		);
		testRequire(
			xrtWsConnText(
				pConnection,
				XRT_STR_LITERAL(TEST_WS_HTTP_PROXY_TEXT)
			) == XNET_RESULT_OK,
			"WebSocket proxy server echo failed"
		);
	} else {
		(void)xrtAtomic32FetchAdd(
			&pState->ClientMessages,
			1,
			XMEMORY_RELEASE
		);
		testRequire(
			xrtWsConnClose(
				pConnection,
				XWS_CLOSE_NORMAL,
				XRT_STR_LITERAL("done")
			) == XNET_RESULT_OK,
			"WebSocket proxy client Close failed"
		);
	}
}



/* 协议和传输错误必须独立计数，不由正常 Close 掩盖。 */
static void testWsProxyError(
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_ws_proxy* pState = (test_ws_proxy*)pData;

	(void)pConnection;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pState->Errors,
		1,
		XMEMORY_RELEASE
	);
}



/* 保存两端 Close 终态，并发布对应完成计数。 */
static void testWsProxyClose(
	xwsconn* pConnection,
	const xwsconnclose* pClose,
	ptr pData
)
{
	test_ws_proxy* pState = (test_ws_proxy*)pData;
	test_ws_proxy_endpoint* pEndpoint =
		testWsProxyEndpoint(pState, pConnection);
	xatomic32* pClosed =
		xrtWsConnRole(pConnection) == XWS_ROLE_CLIENT ?
			&pState->ClientClosed : &pState->ServerClosed;

	testRequire(
		pClose != NULL,
		"WebSocket proxy Close snapshot is null"
	);
	pEndpoint->Close = *pClose;
	(void)xrtAtomic32FetchAdd(
		pClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 解析隧道内 Upgrade，写出 101 并把当前 Stream 接管为服务端 Connection。 */
static bool testWsProxyUpgrade(
	test_ws_proxy_peer* pPeer,
	xnetstream* pStream,
	xnetbuf* pBuffer
)
{
	test_ws_proxy* pState = pPeer->Owner;
	char Request[4096];
	char Accept[XWS_ACCEPT_CAPACITY];
	char Response[512];
	xhttpfield Fields[16];
	xhttp1head Head;
	const xhttpfield* pKey;
	xwsconnconfig Config;
	xwsconn* pConnection;
	size_t iSize = xrtNetBufSize(pBuffer);
	int iLength;

	testRequire(
		iSize < sizeof(Request),
		"WebSocket proxy Upgrade input overflowed"
	);
	testRequire(
		xrtNetBufPeek(
			pBuffer,
			0,
			Request,
			iSize
		) == iSize,
		"WebSocket proxy Upgrade input peek failed"
	);
	xrtHttp1HeadInit(
		&Head,
		Fields,
		sizeof(Fields) / sizeof(Fields[0])
	);
	switch ( xrtHttp1RequestParse(
		(xbytesview) { (cbytes)Request, iSize },
		&Head,
		NULL,
		NULL
	) ) {
		case XHTTP1_MORE:
			return false;
		case XHTTP1_READY:
			break;
		default:
			testRequire(
				false,
				"WebSocket proxy Upgrade request is invalid"
			);
			return false;
	}
	testRequire(
		testWsProxyTextEqual(Head.Method, "GET") &&
		testWsProxyTextEqual(Head.Target, "/chat"),
		"WebSocket proxy Upgrade target mismatch"
	);
	pKey = xrtHttp1Field(
		&Head,
		XRT_STR_LITERAL("Sec-WebSocket-Key")
	);
	testRequire(
		(pKey != NULL) &&
		xrtWsAccept(pKey->Value, Accept, sizeof(Accept)),
		"WebSocket proxy Accept calculation failed"
	);
	iLength = snprintf(
		Response,
		sizeof(Response),
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Accept: %s\r\n"
		"\r\n",
		Accept
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Response)) &&
		(xrtNetStreamSend(
			pStream,
			Response,
			(size_t)iLength
		 ) == XNET_RESULT_OK) &&
		(xrtNetBufConsume(pBuffer, Head.Bytes) ==
		 Head.Bytes),
		"WebSocket proxy 101 response failed"
	);
	xrtWsConnConfigInit(&Config);
	Config.Role = XWS_ROLE_SERVER;
	pConnection = xrtWsConnAttach(
		pStream,
		&Config,
		&pState->WsEvents,
		pState
	);
	testRequire(
		pConnection != NULL,
		"WebSocket proxy server attach failed"
	);
	pPeer->Stream = NULL;
	pPeer->Mode = TEST_WS_PROXY_WEBSOCKET;
	xrtAtomicPtrStore(
		&pState->ServerConnection,
		pConnection,
		XMEMORY_RELEASE
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Upgraded,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 在同一连接上依次处理 CONNECT 和隧道内 WebSocket Upgrade。 */
static void testWsProxyRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	test_ws_proxy_peer* pPeer =
		(test_ws_proxy_peer*)pData;

	for ( ;; ) {
		if ( pPeer->Mode == TEST_WS_PROXY_CONNECT ) {
			if ( !testHttpConnectProxyStep(
				pStream,
				pBuffer,
				"origin.test",
				pPeer->Owner->Port,
				true
			) ) {
				return;
			}
			pPeer->Mode = TEST_WS_PROXY_HANDSHAKE;
			(void)xrtAtomic32FetchAdd(
				&pPeer->Owner->Connects,
				1,
				XMEMORY_RELEASE
			);
			continue;
		}
		if ( pPeer->Mode == TEST_WS_PROXY_HANDSHAKE ) {
			(void)testWsProxyUpgrade(
				pPeer,
				pStream,
				pBuffer
			);
		}
		return;
	}
}



/* 对端在 Upgrade 前半关闭时收敛原始代理 Stream。 */
static void testWsProxyEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	(void)xrtNetStreamClose(pStream);
}



/* 接管唯一代理连接，并把后续 Stream 事件绑定到 Peer。 */
static bool testWsProxyAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	test_ws_proxy* pState = (test_ws_proxy*)pData;
	uint32 iAccepted = xrtAtomic32FetchAdd(
		&pState->Accepted,
		1,
		XMEMORY_ACQ_REL
	);

	(void)pListener;
	testRequire(
		iAccepted == 0,
		"WebSocket proxy opened more than one connection"
	);
	pState->Peer.Owner = pState;
	pState->Peer.Stream = pStream;
	pState->Peer.Mode = TEST_WS_PROXY_CONNECT;
	testRequire(
		xrtNetStreamSetData(pStream, &pState->Peer),
		"WebSocket proxy peer data setup failed"
	);
	return true;
}



/* 发布 Listener 唯一关闭事件。 */
static void testWsProxyListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_ws_proxy* pState = (test_ws_proxy*)pData;

	(void)pListener;
	xrtAtomic32Store(
		&pState->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* WebSocket 客户端握手完成后发送一条文本消息。 */
static void testWsProxyConnected(
	xhttpcall* pCall,
	xnetresult Result,
	xwsconn* pConnection,
	xhttpresponse* pResponse,
	const xerror* pError,
	ptr pData
)
{
	test_ws_proxy* pState = (test_ws_proxy*)pData;
	const xerror* pCause = pError;
	bool bValid =
		(Result == XNET_RESULT_OK) &&
		(pConnection != NULL) &&
		(pResponse != NULL) &&
		(pError == NULL) &&
		(xrtHttpResponseStatus(pResponse) ==
		 XHTTP_STATUS_SWITCHING_PROTOCOLS);

	(void)pCall;
	/* 失败时保留完整原因链，便于定位 CONNECT 与 Upgrade 的组合边界。 */
	if ( !bValid ) {
		fprintf(
			stderr,
			"websocket proxy connect: result=%d status=%u connection=%p response=%p\n",
			(int)Result,
			(unsigned)(pResponse != NULL ?
				xrtHttpResponseStatus(pResponse) : 0),
			(void*)pConnection,
			(void*)pResponse
		);
		while ( pCause != NULL ) {
			fprintf(
				stderr,
				"  %s/%d %s: %s\n",
				xrtErrorDomain(pCause),
				(int)xrtErrorCode(pCause),
				xrtErrorOperation(pCause),
				xrtErrorMessage(pCause)
			);
			pCause = xrtErrorCause(pCause);
		}
	}
	testRequire(
		bValid,
		"WebSocket proxy client handshake failed"
	);
	pState->Response = pResponse;
	xrtAtomicPtrStore(
		&pState->ClientConnection,
		pConnection,
		XMEMORY_RELEASE
	);
	testRequire(
		xrtWsConnText(
			pConnection,
			XRT_STR_LITERAL(TEST_WS_HTTP_PROXY_TEXT)
		) == XNET_RESULT_OK,
		"WebSocket proxy client send failed"
	);
	xrtAtomic32Store(
		&pState->ClientDone,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证 HTTP CONNECT 隧道与 WebSocket Upgrade、消息和关闭完整组合。 */
int main(void)
{
	test_ws_proxy State;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents StreamEvents;
	xhttpclientconfig ClientConfig;
	xnetproxyconfig ProxyConfig;
	xwsclientconfig WsConfig;
	xnetproxy* pProxy;
	xnetaddr Address;
	xwsconn* pClient;
	xwsconn* pServer;
	char Url[256];
	int iLength;

	memset(&State, 0, sizeof(State));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&StreamEvents, 0, sizeof(StreamEvents));
	memset(&State.WsEvents, 0, sizeof(State.WsEvents));
	xrtAtomicPtrInit(&State.ClientConnection, NULL);
	xrtAtomicPtrInit(&State.ServerConnection, NULL);
	xrtAtomic32Init(&State.Accepted, 0);
	xrtAtomic32Init(&State.Connects, 0);
	xrtAtomic32Init(&State.Upgraded, 0);
	xrtAtomic32Init(&State.ClientDone, 0);
	xrtAtomic32Init(&State.ClientMessages, 0);
	xrtAtomic32Init(&State.ServerMessages, 0);
	xrtAtomic32Init(&State.ClientClosed, 0);
	xrtAtomic32Init(&State.ServerClosed, 0);
	xrtAtomic32Init(&State.ListenerClosed, 0);
	xrtAtomic32Init(&State.Errors, 0);
	State.WsEvents.MessageBegin = testWsProxyMessageBegin;
	State.WsEvents.MessageData = testWsProxyMessageData;
	State.WsEvents.MessageEnd = testWsProxyMessageEnd;
	State.WsEvents.Error = testWsProxyError;
	State.WsEvents.Close = testWsProxyClose;
	ListenerEvents.Accept = testWsProxyAccept;
	ListenerEvents.Close = testWsProxyListenerClose;
	StreamEvents.Read = testWsProxyRead;
	StreamEvents.End = testWsProxyEnd;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_WS_HTTP_PROXY_BACKEND;
	EngineConfig.Workers = 1;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"WebSocket proxy engine start failed"
	);
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ListenConfig.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"WebSocket proxy listener address failed"
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
		"WebSocket proxy listener creation failed"
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
		"WebSocket proxy endpoint creation failed"
	);
	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Resolver.Lookup = testWsProxyLookup;
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
		"WebSocket proxy HTTP client creation failed"
	);

	iLength = snprintf(
		Url,
		sizeof(Url),
		"ws://origin.test:%u/chat",
		(unsigned)State.Port
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Url)),
		"WebSocket proxy URL fixture overflowed"
	);
	xrtWsClientConfigInit(&WsConfig);
	State.Call = xrtWsConnect(
		State.Client,
		(xstrview) { Url, (size_t)iLength },
		&WsConfig,
		&State.WsEvents,
		&State,
		testWsProxyConnected,
		&State
	);
	testRequire(
		State.Call != NULL,
		"WebSocket proxy Connect submission failed"
	);
	testWsProxyWait(
		&State.ClientDone,
		1,
		"WebSocket proxy client did not open"
	);
	testWsProxyWait(
		&State.Upgraded,
		1,
		"WebSocket proxy server did not upgrade"
	);
	testWsProxyWait(
		&State.ServerMessages,
		1,
		"WebSocket proxy server message missing"
	);
	testWsProxyWait(
		&State.ClientMessages,
		1,
		"WebSocket proxy client echo missing"
	);
	testWsProxyWait(
		&State.ClientClosed,
		1,
		"WebSocket proxy client Close missing"
	);
	testWsProxyWait(
		&State.ServerClosed,
		1,
		"WebSocket proxy server Close missing"
	);
	pClient = (xwsconn*)xrtAtomicPtrLoad(
		&State.ClientConnection,
		XMEMORY_ACQUIRE
	);
	pServer = (xwsconn*)xrtAtomicPtrLoad(
		&State.ServerConnection,
		XMEMORY_ACQUIRE
	);
	testRequire(
		(pClient != NULL) &&
		(pServer != NULL) &&
		(xrtAtomic32Load(
			&State.Accepted,
			XMEMORY_ACQUIRE
		 ) == 1) &&
		(xrtAtomic32Load(
			&State.Connects,
			XMEMORY_ACQUIRE
		 ) == 1) &&
		(xrtAtomic32Load(
			&State.Upgraded,
			XMEMORY_ACQUIRE
		 ) == 1) &&
		(xrtAtomic32Load(
			&State.Errors,
			XMEMORY_ACQUIRE
		 ) == 0) &&
		((State.ClientEndpoint.Close.Flags &
		  XWS_CONN_CLOSE_CLEAN) != 0) &&
		((State.ServerEndpoint.Close.Flags &
		  XWS_CONN_CLOSE_CLEAN) != 0),
		"WebSocket proxy final contract mismatch"
	);

	xrtWsConnDestroy(pClient);
	xrtWsConnDestroy(pServer);
	xrtHttpResponseDestroy(State.Response);
	xrtHttpCallDestroy(State.Call);
	xrtHttpClientDestroy(State.Client);
	testRequire(
		xrtNetListenerClose(State.Listener),
		"WebSocket proxy listener close failed"
	);
	testWsProxyWait(
		&State.ListenerClosed,
		1,
		"WebSocket proxy listener did not close"
	);
	if ( State.Peer.Stream != NULL ) {
		xrtNetStreamDestroy(State.Peer.Stream);
	}
	xrtNetListenerDestroy(State.Listener);
	testRequire(
		xrtNetEngineDestroy(State.Engine),
		"WebSocket proxy engine destroy failed"
	);
	printf(
		"[PASS] WebSocket HTTP CONNECT proxy (%s)\n",
		TEST_WS_HTTP_PROXY_BACKEND_NAME
	);
	return 0;
}

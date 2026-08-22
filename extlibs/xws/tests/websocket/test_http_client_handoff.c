#include "../test.h"



#ifndef TEST_WS_HTTP_CLIENT_FUTURE
	#define TEST_WS_HTTP_CLIENT_FUTURE 0
#endif



/* 记录 HTTP Client 到 WebSocket Client 的缓冲交接与资源终态。 */
typedef struct test_ws_http_client_handoff {
	xnetengine* Engine;
	xnetlistener* Listener;
	xnetstream* Server;
	xhttpclient* Client;
	xhttpcall* Call;
	xhttpresponse* Response;
	xwsconn* Connection;
	xatomic32 Accepted;
	xatomic32 Connected;
	xatomic32 Message;
	xatomic32 WsClosed;
	xatomic32 ServerClosed;
	xatomic32 ListenerClosed;
	xatomic32 WsErrors;
	xwsopcode Opcode;
	size_t Buffered;
	size_t Size;
	uint8 Data[16];
	bool Responded;
} test_ws_http_client_handoff;



/* 在截止时间前等待异步状态达到目标值。 */
static void testWsHttpClientHandoffWait(
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
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}



/* 等待 HTTP Client 完成独立关闭。 */
static void testWsHttpClientHandoffWaitClient(
	const xhttpclient* pClient
)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(10000000));

	while ( xrtHttpClientState(pClient) !=
		XHTTP_CLIENT_CLOSED ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"WebSocket HTTP client handoff drain timed out"
		);
		xrtThreadYield();
	}
}



/* 测试域名固定解析到 IPv4 Loopback。 */
static xnetaddrlist* testWsHttpClientHandoffLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)pData;
	testRequire(
		strcmp(sHost, "handoff-client.test") == 0,
		"WebSocket HTTP client handoff resolved an unexpected host"
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
		"WebSocket HTTP client handoff resolver failed"
	);
	return xrtNetAddrListCreate(&Address, 1);
}



/* 收到完整请求后一次发送 101 与首个未掩码文本帧。 */
static void testWsHttpClientHandoffRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	test_ws_http_client_handoff* pState =
		(test_ws_http_client_handoff*)pData;
	static const char KeyPrefix[] =
		"\r\nSec-WebSocket-Key: ";
	static const uint8 Frame[] = {
		0x81, 0x05, 'e', 'a', 'r', 'l', 'y'
	};
	char Request[2048];
	uint8 Response[1024];
	char Accept[XWS_ACCEPT_CAPACITY];
	const char* pKey;
	size_t iSize = xrtNetBufSize(pBuffer);
	int iHead;

	testRequire(
		(iSize != 0) && (iSize < sizeof(Request)),
		"WebSocket HTTP client handoff request exceeded storage"
	);
	testRequire(
		xrtNetBufPeek(pBuffer, 0, Request, iSize) == iSize,
		"WebSocket HTTP client handoff request peek failed"
	);
	Request[iSize] = '\0';
	if ( strstr(Request, "\r\n\r\n") == NULL ) {
		return;
	}
	testRequire(
		!pState->Responded,
		"WebSocket HTTP client handoff sent a duplicate response"
	);
	pKey = strstr(Request, KeyPrefix);
	testRequire(
		(pKey != NULL) &&
		(pKey[sizeof(KeyPrefix) - 1u + XWS_KEY_SIZE] == '\r'),
		"WebSocket HTTP client handoff key missing"
	);
	pKey += sizeof(KeyPrefix) - 1u;
	testRequire(
		xrtWsAccept(
			(xstrview) { pKey, XWS_KEY_SIZE },
			Accept,
			sizeof(Accept)
		),
		"WebSocket HTTP client handoff Accept failed"
	);
	iHead = snprintf(
		(char*)Response,
		sizeof(Response),
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Accept: %s\r\n"
		"\r\n",
		Accept
	);
	testRequire(
		(iHead > 0) &&
		(((size_t)iHead + sizeof(Frame)) <= sizeof(Response)),
		"WebSocket HTTP client handoff response overflowed"
	);
	memcpy(Response + (size_t)iHead, Frame, sizeof(Frame));
	testRequire(
		(xrtNetBufConsume(pBuffer, iSize) == iSize) &&
		(xrtNetStreamSend(
			pStream,
			Response,
			(size_t)iHead + sizeof(Frame)
		 ) == XNET_RESULT_OK),
		"WebSocket HTTP client handoff response send failed"
	);
	pState->Responded = true;
}



/* 客户端结束时关闭原始服务端传输。 */
static void testWsHttpClientHandoffEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	(void)xrtNetStreamClose(pStream);
}



/* 记录原始服务端传输终态。 */
static void testWsHttpClientHandoffServerClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_ws_http_client_handoff* pState =
		(test_ws_http_client_handoff*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	xrtAtomic32Store(&pState->ServerClosed, 1, XMEMORY_RELEASE);
}



/* 接管 Listener 交付的本地原始 HTTP 服务端 Stream。 */
static bool testWsHttpClientHandoffAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	test_ws_http_client_handoff* pState =
		(test_ws_http_client_handoff*)pData;
	xnetstreamevents Events;

	(void)pListener;
	memset(&Events, 0, sizeof(Events));
	Events.Read = testWsHttpClientHandoffRead;
	Events.End = testWsHttpClientHandoffEnd;
	Events.Close = testWsHttpClientHandoffServerClose;
	testRequire(
		xrtNetStreamSetEvents(pStream, &Events, pState),
		"WebSocket HTTP client handoff server takeover failed"
	);
	pState->Server = pStream;
	xrtAtomic32Store(&pState->Accepted, 1, XMEMORY_RELEASE);
	return true;
}



/* 记录本地原始 Listener 终态。 */
static void testWsHttpClientHandoffListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_ws_http_client_handoff* pState =
		(test_ws_http_client_handoff*)pData;

	(void)pListener;
	xrtAtomic32Store(
		&pState->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



#if !TEST_WS_HTTP_CLIENT_FUTURE

/* 记录 HTTP Client 成功交付的 WebSocket 及其缓冲余量。 */
static void testWsHttpClientHandoffDone(
	xhttpcall* pCall,
	xnetresult Result,
	xwsconn* pConnection,
	xhttpresponse* pResponse,
	const xerror* pError,
	ptr pData
)
{
	test_ws_http_client_handoff* pState =
		(test_ws_http_client_handoff*)pData;
	xnetstream* pTcp;

	testRequire(
		(pCall == pState->Call) &&
		(Result == XNET_RESULT_OK) &&
		(pConnection != NULL) &&
		(pResponse != NULL) &&
		(pError == NULL) &&
		(xrtHttpCallState(pCall) == XHTTP_CALL_SUCCEEDED),
		"WebSocket HTTP client handoff completion mismatch"
	);
	pTcp = xrtWsConnTcp(pConnection);
	testRequire(
		pTcp != NULL,
		"WebSocket HTTP client handoff did not retain TCP"
	);
	pState->Connection = pConnection;
	pState->Response = pResponse;
	pState->Buffered = xrtNetStreamAvailable(pTcp);
	xrtAtomic32Store(&pState->Connected, 1, XMEMORY_RELEASE);
}

#endif



/* 开始接收响应余量中的第一条文本消息。 */
static void testWsHttpClientHandoffMessageBegin(
	xwsconn* pConnection,
	const xwsmessageinfo* pInfo,
	ptr pData
)
{
	test_ws_http_client_handoff* pState =
		(test_ws_http_client_handoff*)pData;

	testRequire(
		(pConnection != NULL) &&
		#if !TEST_WS_HTTP_CLIENT_FUTURE
			(pConnection == pState->Connection) &&
		#endif
		(pInfo != NULL) &&
		(pInfo->Opcode == XWS_OPCODE_TEXT) &&
		(pState->Size == 0),
		"WebSocket HTTP client handoff message begin mismatch"
	);
	pState->Opcode = (xwsopcode)pInfo->Opcode;
}



/* 复制客户端收到的早到消息分块。 */
static void testWsHttpClientHandoffMessageData(
	xwsconn* pConnection,
	xbytesview Data,
	ptr pData
)
{
	test_ws_http_client_handoff* pState =
		(test_ws_http_client_handoff*)pData;

	testRequire(
		(pConnection != NULL) &&
		#if !TEST_WS_HTTP_CLIENT_FUTURE
			(pConnection == pState->Connection) &&
		#endif
		(Data.Size <= (sizeof(pState->Data) - pState->Size)),
		"WebSocket HTTP client handoff message exceeded storage"
	);
	if ( Data.Size != 0 ) {
		memcpy(
			pState->Data + pState->Size,
			Data.Data,
			Data.Size
		);
	}
	pState->Size += Data.Size;
}



/* 核对客户端完整消费了 101 后的首帧。 */
static void testWsHttpClientHandoffMessageEnd(
	xwsconn* pConnection,
	ptr pData
)
{
	test_ws_http_client_handoff* pState =
		(test_ws_http_client_handoff*)pData;

	testRequire(
		(pConnection != NULL) &&
		#if !TEST_WS_HTTP_CLIENT_FUTURE
			(pConnection == pState->Connection) &&
		#endif
		(pState->Opcode == XWS_OPCODE_TEXT) &&
		(pState->Size == 5) &&
		(memcmp(pState->Data, "early", 5) == 0),
		"WebSocket HTTP client handoff lost the early frame"
	);
	xrtAtomic32Store(&pState->Message, 1, XMEMORY_RELEASE);
}



/* 记录连接关闭前发布的结构化错误。 */
static void testWsHttpClientHandoffWsError(
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_ws_http_client_handoff* pState =
		(test_ws_http_client_handoff*)pData;

	(void)pConnection;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pState->WsErrors,
		1,
		XMEMORY_ACQ_REL
	);
}



/* 记录 WebSocket Client 传输终态。 */
static void testWsHttpClientHandoffWsClose(
	xwsconn* pConnection,
	const xwsconnclose* pClose,
	ptr pData
)
{
	test_ws_http_client_handoff* pState =
		(test_ws_http_client_handoff*)pData;

	(void)pClose;
	testRequire(
		pConnection == pState->Connection,
		"WebSocket HTTP client handoff close identity mismatch"
	);
	xrtAtomic32Store(&pState->WsClosed, 1, XMEMORY_RELEASE);
}



/* 验证客户端 HTTP Upgrade 保留并消费同一响应块内的早到帧。 */
int main(void)
{
	test_ws_http_client_handoff State;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xhttpclientconfig ClientConfig;
	xwsclientconfig WsConfig;
	xwsconnevents WsEvents;
	xnetaddr Address;
	#if TEST_WS_HTTP_CLIENT_FUTURE
		xfuture* pFuture;
		xwsopenresult* pResult;
	#endif
	char Url[160];
	int iLength;

	memset(&State, 0, sizeof(State));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&WsEvents, 0, sizeof(WsEvents));
	xrtAtomic32Init(&State.Accepted, 0);
	xrtAtomic32Init(&State.Connected, 0);
	xrtAtomic32Init(&State.Message, 0);
	xrtAtomic32Init(&State.WsClosed, 0);
	xrtAtomic32Init(&State.ServerClosed, 0);
	xrtAtomic32Init(&State.ListenerClosed, 0);
	xrtAtomic32Init(&State.WsErrors, 0);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"WebSocket HTTP client handoff engine start failed"
	);
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ListenConfig.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"WebSocket HTTP client handoff listener address failed"
	);
	ListenConfig.Stream.ReadSize = 4096;
	ListenConfig.Stream.ReadLimit = 8192;
	ListenerEvents.Accept = testWsHttpClientHandoffAccept;
	ListenerEvents.Close = testWsHttpClientHandoffListenerClose;
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
		"WebSocket HTTP client handoff listener start failed"
	);
	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Resolver.Lookup =
		testWsHttpClientHandoffLookup;
	ClientConfig.Dial.FallbackDelay = 1000;
	ClientConfig.Dial.MaxAttempts = 1;
	State.Client = xrtHttpClientCreate(
		State.Engine,
		&ClientConfig
	);
	testRequire(
		State.Client != NULL,
		"WebSocket HTTP client handoff client creation failed"
	);
	iLength = snprintf(
		Url,
		sizeof(Url),
		"ws://handoff-client.test:%u/chat",
		(unsigned int)Address.Port
	);
	testRequire(
		(iLength > 0) && ((size_t)iLength < sizeof(Url)),
		"WebSocket HTTP client handoff URL overflowed"
	);
	xrtWsClientConfigInit(&WsConfig);
	WsEvents.MessageBegin =
		testWsHttpClientHandoffMessageBegin;
	WsEvents.MessageData =
		testWsHttpClientHandoffMessageData;
	WsEvents.MessageEnd =
		testWsHttpClientHandoffMessageEnd;
	WsEvents.Error = testWsHttpClientHandoffWsError;
	WsEvents.Close = testWsHttpClientHandoffWsClose;
	#if TEST_WS_HTTP_CLIENT_FUTURE
		pFuture = xrtWsConnectAsync(
			State.Client,
			(xstrview) { Url, (size_t)iLength },
			&WsConfig,
			&WsEvents,
			&State
		);
		testRequire(
			pFuture != NULL,
			"WebSocket HTTP client Future submission failed"
		);
	#else
		State.Call = xrtWsConnect(
			State.Client,
			(xstrview) { Url, (size_t)iLength },
			&WsConfig,
			&WsEvents,
			&State,
			testWsHttpClientHandoffDone,
			&State
		);
		testRequire(
			State.Call != NULL,
			"WebSocket HTTP client handoff submission failed"
		);
	#endif
	testWsHttpClientHandoffWait(
		&State.Accepted,
		1,
		"WebSocket HTTP client handoff was not accepted"
	);
	#if TEST_WS_HTTP_CLIENT_FUTURE
		testRequire(
			(xrtFutureWaitFor(
				pFuture,
				UINT64_C(10000000)
			 ) == XWAIT_OK) &&
			(xrtFutureState(pFuture) == XFUTURE_RESOLVED),
			"WebSocket HTTP client Future did not resolve"
		);
		pResult = (xwsopenresult*)xrtFutureValue(pFuture);
		testRequire(
			(pResult != NULL) &&
			(xrtWsOpenResultConnection(pResult) != NULL) &&
			(xrtWsOpenResultResponse(pResult) != NULL),
			"WebSocket HTTP client Future result is incomplete"
		);
		State.Connection = xrtWsOpenResultTakeConnection(
			pResult
		);
		State.Response = xrtWsOpenResultTakeResponse(pResult);
		testRequire(
			(State.Connection != NULL) &&
			(State.Response != NULL) &&
			(xrtWsOpenResultConnection(pResult) == NULL) &&
			(xrtWsOpenResultResponse(pResult) == NULL),
			"WebSocket HTTP client Future Take mismatch"
		);
		xrtFutureDestroy(pFuture);
		xrtAtomic32Store(
			&State.Connected,
			1,
			XMEMORY_RELEASE
		);
	#endif
	testWsHttpClientHandoffWait(
		&State.Connected,
		1,
		"WebSocket HTTP client handoff callback missing"
	);
	testWsHttpClientHandoffWait(
		&State.Message,
		1,
		"WebSocket HTTP client handoff early message missing"
	);
	testRequire(
		#if !TEST_WS_HTTP_CLIENT_FUTURE
			(State.Buffered >= 7) &&
		#endif
		(xrtAtomic32Load(
			&State.WsErrors,
			XMEMORY_ACQUIRE
		 ) == 0),
		"WebSocket HTTP client handoff did not preserve buffered bytes"
	);
	testRequire(
		xrtWsConnAbort(State.Connection),
		"WebSocket HTTP client handoff abort failed"
	);
	testWsHttpClientHandoffWait(
		&State.WsClosed,
		1,
		"WebSocket HTTP client handoff close missing"
	);
	testWsHttpClientHandoffWait(
		&State.ServerClosed,
		1,
		"WebSocket HTTP client handoff server close missing"
	);
	testRequire(
		xrtHttpClientDrain(State.Client),
		"WebSocket HTTP client handoff client drain failed"
	);
	testWsHttpClientHandoffWaitClient(State.Client);
	testRequire(
		xrtNetListenerClose(State.Listener),
		"WebSocket HTTP client handoff listener close failed"
	);
	testWsHttpClientHandoffWait(
		&State.ListenerClosed,
		1,
		"WebSocket HTTP client handoff listener terminal missing"
	);
	xrtHttpResponseDestroy(State.Response);
	#if !TEST_WS_HTTP_CLIENT_FUTURE
		xrtHttpCallDestroy(State.Call);
	#endif
	xrtHttpClientDestroy(State.Client);
	xrtWsConnDestroy(State.Connection);
	xrtNetStreamDestroy(State.Server);
	xrtNetListenerDestroy(State.Listener);
	testRequire(
		xrtNetEngineDestroy(State.Engine),
		"WebSocket HTTP client handoff engine destroy failed"
	);
	printf(
		"[PASS] WebSocket HTTP client buffered handoff (%s)\n",
		TEST_WS_HTTP_CLIENT_FUTURE ? "Future" : "callback"
	);
	return 0;
}

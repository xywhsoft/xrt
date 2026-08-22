#include "../test.h"
#include "../../../xhttp/src/internal/xrt_http_client.h"
#include "../../../xhttp/src/internal/xrt_http_client_runtime.h"



#ifndef TEST_WS_CLIENT_INVALID_BACKEND
	#define TEST_WS_CLIENT_INVALID_BACKEND XNET_PORT_SELECT
#endif

#ifndef TEST_WS_CLIENT_INVALID_BACKEND_NAME
	#define TEST_WS_CLIENT_INVALID_BACKEND_NAME "select"
#endif



typedef enum test_ws_client_invalid_case {
	TEST_WS_CLIENT_STATUS = 0,
	TEST_WS_CLIENT_ACCEPT,
	TEST_WS_CLIENT_PROTOCOL,
	TEST_WS_CLIENT_EXTENSION,
	TEST_WS_CLIENT_BODY,
	TEST_WS_CLIENT_DUPLICATE_ACCEPT,
	TEST_WS_CLIENT_CONNECTION,
	TEST_WS_CLIENT_UPGRADE
} test_ws_client_invalid_case;



typedef struct test_ws_client_invalid {
	xnetengine* Engine;
	xnetlistener* Listener;
	xnetstream* Server;
	xhttpclient* Client;
	xhttpcall* Call;
	xhttpcall* CallbackCall;
	xhttpresponse* Response;
	xatomic32 Accepted;
	xatomic32 Completed;
	xatomic32 ServerClosed;
	xatomic32 ListenerClosed;
	test_ws_client_invalid_case Scenario;
	xwshandshakeerror Expected;
	bool Responded;
} test_ws_client_invalid;



/* 在截止时间前等待一个跨 Worker 终态。 */
static void testWsClientInvalidWait(
	const xatomic32* pValue,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(
		UINT64_C(10000000)
	);

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



/* 测试域名固定解析到 IPv4 Loopback。 */
static xnetaddrlist* testWsClientInvalidLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)pData;
	testRequire(
		strcmp(sHost, "invalid-websocket.test") == 0,
		"invalid WebSocket test resolved an unexpected host"
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
		"invalid WebSocket resolver fixture failed"
	);
	return xrtNetAddrListCreate(&Address, 1);
}



/* 从客户端请求中取得 nonce 并构造本场景响应。 */
static size_t testWsClientInvalidResponse(
	test_ws_client_invalid* pTest,
	cstr sRequest,
	char* sResponse,
	size_t iCapacity
)
{
	static const char Prefix[] =
		"\r\nSec-WebSocket-Key: ";
	const char* pKey = strstr(sRequest, Prefix);
	char sAccept[XWS_ACCEPT_CAPACITY];
	int iLength;

	testRequire(
		(pKey != NULL) &&
		(pKey[sizeof(Prefix) - 1u + XWS_KEY_SIZE] ==
		 '\r'),
		"invalid WebSocket fixture did not receive a key"
	);
	pKey += sizeof(Prefix) - 1u;
	testRequire(
		xrtWsAccept(
			(xstrview) { pKey, XWS_KEY_SIZE },
			sAccept,
			sizeof(sAccept)
		),
		"invalid WebSocket fixture Accept failed"
	);
	switch ( pTest->Scenario ) {
	case TEST_WS_CLIENT_STATUS:
		iLength = snprintf(
			sResponse,
			iCapacity,
			"HTTP/1.1 200 OK\r\n"
			"Content-Length: 0\r\n"
			"Connection: close\r\n\r\n"
		);
		break;
	case TEST_WS_CLIENT_ACCEPT:
		iLength = snprintf(
			sResponse,
			iCapacity,
			"HTTP/1.1 101 Switching Protocols\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Accept: "
			"AAAAAAAAAAAAAAAAAAAAAAAAAAAA\r\n\r\n"
		);
		break;
	case TEST_WS_CLIENT_PROTOCOL:
		iLength = snprintf(
			sResponse,
			iCapacity,
			"HTTP/1.1 101 Switching Protocols\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Accept: %s\r\n"
			"Sec-WebSocket-Protocol: other\r\n\r\n",
			sAccept
		);
		break;
	case TEST_WS_CLIENT_EXTENSION:
		iLength = snprintf(
			sResponse,
			iCapacity,
			"HTTP/1.1 101 Switching Protocols\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Accept: %s\r\n"
			"Sec-WebSocket-Extensions: "
			"permessage-deflate\r\n\r\n",
			sAccept
		);
		break;
	case TEST_WS_CLIENT_BODY:
		iLength = snprintf(
			sResponse,
			iCapacity,
			"HTTP/1.1 101 Switching Protocols\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Accept: %s\r\n"
			"Content-Length: 0\r\n\r\n",
			sAccept
		);
		break;
	case TEST_WS_CLIENT_DUPLICATE_ACCEPT:
		iLength = snprintf(
			sResponse,
			iCapacity,
			"HTTP/1.1 101 Switching Protocols\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Accept: %s\r\n"
			"Sec-WebSocket-Accept: %s\r\n\r\n",
			sAccept,
			sAccept
		);
		break;
	case TEST_WS_CLIENT_CONNECTION:
		iLength = snprintf(
			sResponse,
			iCapacity,
			"HTTP/1.1 101 Switching Protocols\r\n"
			"Upgrade: websocket\r\n"
			"Connection: keep-alive\r\n"
			"Sec-WebSocket-Accept: %s\r\n\r\n",
			sAccept
		);
		break;
	default:
		iLength = snprintf(
			sResponse,
			iCapacity,
			"HTTP/1.1 101 Switching Protocols\r\n"
			"Upgrade: h2c\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Accept: %s\r\n\r\n",
			sAccept
		);
		break;
	}
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < iCapacity),
		"invalid WebSocket response fixture overflowed"
	);
	return (size_t)iLength;
}



/* 收到完整请求后发送本场景的错误握手响应。 */
static void testWsClientInvalidRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	test_ws_client_invalid* pTest =
		(test_ws_client_invalid*)pData;
	char Request[2048];
	char Response[1024];
	size_t iSize = xrtNetBufSize(pBuffer);
	size_t iResponse;

	testRequire(
		(iSize != 0) &&
		(iSize < sizeof(Request)),
		"invalid WebSocket request exceeded fixture storage"
	);
	testRequire(
		xrtNetBufPeek(
			pBuffer,
			0,
			Request,
			iSize
		) == iSize,
		"invalid WebSocket request peek failed"
	);
	Request[iSize] = '\0';
	if ( strstr(Request, "\r\n\r\n") == NULL ) {
		return;
	}
	testRequire(
		!pTest->Responded,
		"invalid WebSocket fixture sent a duplicate response"
	);
	pTest->Responded = true;
	testRequire(
		xrtNetBufConsume(pBuffer, iSize) == iSize,
		"invalid WebSocket request consume failed"
	);
	iResponse = testWsClientInvalidResponse(
		pTest,
		Request,
		Response,
		sizeof(Response)
	);
	testRequire(
		xrtNetStreamSend(
			pStream,
			Response,
			iResponse
		) == XNET_RESULT_OK,
		"invalid WebSocket response send failed"
	);
	if ( pTest->Scenario == TEST_WS_CLIENT_STATUS ) {
		testRequire(
			xrtNetStreamClose(pStream),
			"invalid WebSocket status response close failed"
		);
	}
}



/* 对端结束时完成服务端半关闭。 */
static void testWsClientInvalidEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	testRequire(
		xrtNetStreamClose(pStream),
		"invalid WebSocket server half-close failed"
	);
}



/* 记录服务端传输终态。 */
static void testWsClientInvalidServerClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_ws_client_invalid* pTest =
		(test_ws_client_invalid*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	xrtAtomic32Store(
		&pTest->ServerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 接管 Listener 交付的服务端 Stream。 */
static bool testWsClientInvalidAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	test_ws_client_invalid* pTest =
		(test_ws_client_invalid*)pData;
	xnetstreamevents Events;

	(void)pListener;
	memset(&Events, 0, sizeof(Events));
	Events.Read = testWsClientInvalidRead;
	Events.End = testWsClientInvalidEnd;
	Events.Close = testWsClientInvalidServerClose;
	testRequire(
		xrtNetStreamSetEvents(
			pStream,
			&Events,
			pTest
		),
		"invalid WebSocket server event takeover failed"
	);
	pTest->Server = pStream;
	xrtAtomic32Store(
		&pTest->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 记录 Listener 已关闭。 */
static void testWsClientInvalidListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_ws_client_invalid* pTest =
		(test_ws_client_invalid*)pData;

	(void)pListener;
	xrtAtomic32Store(
		&pTest->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证 HTTP 成功与 WebSocket 握手失败是两层独立终态。 */
static void testWsClientInvalidDone(
	xhttpcall* pCall,
	xnetresult Result,
	xwsconn* pConnection,
	xhttpresponse* pResponse,
	const xerror* pError,
	ptr pData
)
{
	test_ws_client_invalid* pTest =
		(test_ws_client_invalid*)pData;
	bool bHttpRejected =
		(pTest->Scenario == TEST_WS_CLIENT_BODY) ||
		(pTest->Scenario == TEST_WS_CLIENT_CONNECTION);
	bool bValid;

	if ( bHttpRejected ) {
		bValid =
			(pCall != NULL) &&
			(Result == XNET_RESULT_ERROR) &&
			(pConnection == NULL) &&
			(pResponse == NULL) &&
			(pError != NULL) &&
			(xrtErrorKind(pError) == XERR_PROTOCOL) &&
			(xrtErrorCode(pError) ==
			 XHTTP_CLIENT_ERROR_PROTOCOL) &&
			(strcmp(
				xrtErrorDomain(pError),
				"xrt.http.client"
			 ) == 0) &&
			(xrtHttpCallState(pCall) ==
			 XHTTP_CALL_FAILED) &&
			(xrtHttpCallError(pCall) == pError);
	} else {
		bValid =
			(pCall != NULL) &&
			(Result == XNET_RESULT_ERROR) &&
			(pConnection == NULL) &&
			(pResponse != NULL) &&
			(pError != NULL) &&
			(xrtErrorKind(pError) == XERR_PROTOCOL) &&
			(xrtErrorCode(pError) ==
			 (int32)pTest->Expected) &&
			(strcmp(
				xrtErrorDomain(pError),
				"xrt.websocket.handshake"
			 ) == 0) &&
			(xrtHttpCallState(pCall) ==
			 XHTTP_CALL_SUCCEEDED) &&
			(xrtHttpCallError(pCall) == NULL);
	}

	if ( !bValid ) {
		fprintf(
			stderr,
			"[INFO] scenario=%d result=%d conn=%p response=%p "
			"error=%p kind=%d code=%d domain=%s "
			"call-state=%d call-error=%p\n",
			(int)pTest->Scenario,
			(int)Result,
			(void*)pConnection,
			(void*)pResponse,
			(void*)pError,
			pError != NULL ?
				(int)xrtErrorKind(pError) : -1,
			pError != NULL ?
				(int)xrtErrorCode(pError) : -1,
			pError != NULL ?
				xrtErrorDomain(pError) : "(null)",
			pCall != NULL ?
				(int)xrtHttpCallState(pCall) : -1,
			pCall != NULL ?
				(void*)xrtHttpCallError(pCall) : NULL
		);
	}
	testRequire(
		bValid,
		"invalid WebSocket completion classification mismatch"
	);
	pTest->CallbackCall = pCall;
	pTest->Response = pResponse;
	xrtAtomic32Store(
		&pTest->Completed,
		1,
		XMEMORY_RELEASE
	);
}



/* 执行一个独立畸形响应场景。 */
static void testWsClientInvalidRun(
	test_ws_client_invalid_case Scenario,
	xwshandshakeerror Expected
)
{
	test_ws_client_invalid Test;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xhttpclientconfig ClientConfig;
	xwsclientconfig WsConfig;
	xnetaddr Address;
	char Url[160];
	int iLength;

	memset(&Test, 0, sizeof(Test));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	Test.Scenario = Scenario;
	Test.Expected = Expected;
	xrtAtomic32Init(&Test.Accepted, 0);
	xrtAtomic32Init(&Test.Completed, 0);
	xrtAtomic32Init(&Test.ServerClosed, 0);
	xrtAtomic32Init(&Test.ListenerClosed, 0);

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend =
		TEST_WS_CLIENT_INVALID_BACKEND;
	EngineConfig.Workers = 2;
	Test.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(Test.Engine != NULL) &&
		xrtNetEngineStart(Test.Engine),
		"invalid WebSocket engine start failed"
	);
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ListenConfig.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"invalid WebSocket listener address failed"
	);
	ListenerEvents.Accept =
		testWsClientInvalidAccept;
	ListenerEvents.Close =
		testWsClientInvalidListenerClose;
	Test.Listener = xrtNetListen(
		Test.Engine,
		&ListenConfig,
		&ListenerEvents,
		NULL,
		&Test
	);
	testRequire(
		(Test.Listener != NULL) &&
		xrtNetListenerLocal(
			Test.Listener,
			&Address
		),
		"invalid WebSocket listener start failed"
	);
	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Resolver.Lookup =
		testWsClientInvalidLookup;
	ClientConfig.Dial.FallbackDelay = 1000;
	ClientConfig.Dial.MaxAttempts = 1;
	Test.Client = xrtHttpClientCreate(
		Test.Engine,
		&ClientConfig
	);
	testRequire(
		Test.Client != NULL,
		"invalid WebSocket client creation failed"
	);
	iLength = snprintf(
		Url,
		sizeof(Url),
		"ws://invalid-websocket.test:%u/chat",
		(unsigned int)Address.Port
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Url)),
		"invalid WebSocket URL fixture overflowed"
	);
	xrtWsClientConfigInit(&WsConfig);
	WsConfig.Protocols =
		XRT_STR_LITERAL("chat.v1");
	Test.Call = xrtWsConnect(
		Test.Client,
		(xstrview) { Url, (size_t)iLength },
		&WsConfig,
		NULL,
		NULL,
		testWsClientInvalidDone,
		&Test
	);
	testRequire(
		Test.Call != NULL,
		"invalid WebSocket call submission failed"
	);
	testWsClientInvalidWait(
		&Test.Accepted,
		"invalid WebSocket connection was not accepted"
	);
	testWsClientInvalidWait(
		&Test.Completed,
		"invalid WebSocket callback did not complete"
	);
	testRequire(
		Test.CallbackCall == Test.Call,
		"invalid WebSocket callback Call identity mismatch"
	);
	testWsClientInvalidWait(
		&Test.ServerClosed,
		"invalid WebSocket transport did not close"
	);
	testRequire(
		xrtNetListenerClose(Test.Listener),
		"invalid WebSocket listener close failed"
	);
	testWsClientInvalidWait(
		&Test.ListenerClosed,
		"invalid WebSocket listener terminal missing"
	);

	xrtHttpResponseDestroy(Test.Response);
	xrtHttpCallDestroy(Test.Call);
	xrtHttpClientDestroy(Test.Client);
	xrtNetStreamDestroy(Test.Server);
	xrtNetListenerDestroy(Test.Listener);
	testRequire(
		xrtNetEngineDestroy(Test.Engine),
		"invalid WebSocket engine destroy failed"
	);
}



/* 创建一个带标准必要字段的拥有型 101 响应。 */
static xhttpresponse* testWsClientCheckResponse(void)
{
	xhttpresponse* pResponse =
		__xrtHttpResponseCreate(
			XHTTP_VERSION_1_1,
			XHTTP_STATUS_SWITCHING_PROTOCOLS,
			XRT_STR_LITERAL("Switching Protocols"),
			NULL
		);

	testRequire(
		(pResponse != NULL) &&
		__xrtHttpResponseAddHeader(
			pResponse,
			XRT_STR_LITERAL("Upgrade"),
			XRT_STR_LITERAL("websocket")
		) &&
		__xrtHttpResponseAddHeader(
			pResponse,
			XRT_STR_LITERAL("Connection"),
			XRT_STR_LITERAL("Upgrade")
		) &&
		__xrtHttpResponseAddHeader(
			pResponse,
			XRT_STR_LITERAL("Sec-WebSocket-Accept"),
			XRT_STR_LITERAL(
				"s3pPLMBiTxaQ9kYGzzhZRbK+xOo="
			)
		),
		"WebSocket client response fixture creation failed"
	);
	return pResponse;
}



/* 即使 HTTP 层通常先拒绝，纯检查器仍独立压实正文边界。 */
static void testWsClientCheckBodies(void)
{
	xhttpresponse* pResponse =
		testWsClientCheckResponse();
	xwsclientconfig Config;
	xwsclienthandshake Handshake;
	xwsclienthandshake Before;

	xrtWsClientConfigInit(&Config);
	Config.Protocols = XRT_STR_LITERAL("chat.v1");
	memset(&Handshake, 0xA5, sizeof(Handshake));
	Before = Handshake;
	testRequire(
		__xrtHttpResponseAddHeader(
			pResponse,
			XRT_STR_LITERAL("Content-Length"),
			XRT_STR_LITERAL("0")
		) &&
		!xrtWsClientCheck(
			pResponse,
			XRT_STR_LITERAL(
				"dGhlIHNhbXBsZSBub25jZQ=="
			),
			&Config,
			&Handshake
		) &&
		(memcmp(
			&Handshake,
			&Before,
			sizeof(Handshake)
		 ) == 0) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_HANDSHAKE_ERROR_BODY),
		"WebSocket client accepted 101 Content-Length"
	);
	xrtClearError();
	xrtHttpResponseDestroy(pResponse);

	pResponse = testWsClientCheckResponse();
	testRequire(
		__xrtHttpResponseAppendBody(
			pResponse,
			XRT_BYTES_LITERAL("x")
		) &&
		!xrtWsClientCheck(
			pResponse,
			XRT_STR_LITERAL(
				"dGhlIHNhbXBsZSBub25jZQ=="
			),
			&Config,
			&Handshake
		) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_HANDSHAKE_ERROR_BODY),
		"WebSocket client accepted a 101 response body"
	);
	xrtClearError();
	xrtHttpResponseDestroy(pResponse);

	pResponse = testWsClientCheckResponse();
	testRequire(
		xrtHttpHeadersSet(
			pResponse->Headers,
			XRT_STR_LITERAL("Connection"),
			XRT_STR_LITERAL("keep-alive")
		) &&
		!xrtWsClientCheck(
			pResponse,
			XRT_STR_LITERAL(
				"dGhlIHNhbXBsZSBub25jZQ=="
			),
			&Config,
			&Handshake
		) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_HANDSHAKE_ERROR_CONNECTION),
		"WebSocket client accepted an invalid Connection field"
	);
	xrtClearError();
	xrtHttpResponseDestroy(pResponse);

	pResponse = testWsClientCheckResponse();
	testRequire(
		xrtHttpHeadersSet(
			pResponse->Headers,
			XRT_STR_LITERAL("Upgrade"),
			XRT_STR_LITERAL("h2c")
		) &&
		!xrtWsClientCheck(
			pResponse,
			XRT_STR_LITERAL(
				"dGhlIHNhbXBsZSBub25jZQ=="
			),
			&Config,
			&Handshake
		) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_HANDSHAKE_ERROR_UPGRADE),
		"WebSocket client accepted an invalid Upgrade field"
	);
	xrtClearError();
	xrtHttpResponseDestroy(pResponse);
}



/* 覆盖状态、nonce、子协议、扩展、正文和连接字段。 */
/* 验证范围失败发布统一的 WebSocket 握手参数错误。 */
static void testWsClientArgumentError(cstr sMessage)
{
	const xerror* pError = xrtGetError();

	testRequire(
		(pError != NULL) &&
		(xrtErrorKind(pError) == XERR_ARGUMENT) &&
		(xrtErrorCode(pError) ==
		 XWS_HANDSHAKE_ERROR_ARGUMENT) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.websocket.handshake"
		 ) == 0),
		sMessage
	);
	xrtClearError();
}



/* 无效异步入口测试只验证提交前契约，不接管任何连接。 */
static void testWsClientRangeDone(
	xhttpcall* pCall,
	xnetresult Result,
	xwsconn* pConnection,
	xhttpresponse* pResponse,
	const xerror* pError,
	ptr pData
)
{
	(void)pCall;
	(void)Result;
	(void)pConnection;
	(void)pResponse;
	(void)pError;
	(void)pData;
}



/* 压实非对齐快照、地址回绕、输出重叠和提交前参数检查。 */
static void testWsClientRanges(void)
{
	uint8 ConfigStorage[sizeof(xwsclientconfig) + 2u];
	uint8 HandshakeStorage[sizeof(xwsclienthandshake) + 2u];
	char KeyStorage[XWS_KEY_CAPACITY + 2u];
	xwsclientconfig* pConfig =
		(xwsclientconfig*)(void*)(ConfigStorage + 1u);
	xwsclienthandshake* pHandshake =
		(xwsclienthandshake*)(void*)(HandshakeStorage + 1u);
	char* sKey = KeyStorage + 1u;
	const xwsclientconfig* pWrappingConfig =
		(const xwsclientconfig*)(uintptr_t)(UINTPTR_MAX - 1u);
	const xwsconnevents* pWrappingEvents =
		(const xwsconnevents*)(uintptr_t)(UINTPTR_MAX - 1u);
	const xhttprequest* pWrappingRequest =
		(const xhttprequest*)(uintptr_t)(UINTPTR_MAX - 1u);
	const xhttpresponse* pWrappingResponse =
		(const xhttpresponse*)(uintptr_t)(UINTPTR_MAX - 1u);
	xhttpclient* pWrappingClient =
		(xhttpclient*)(uintptr_t)(UINTPTR_MAX - 1u);
	xwsclientconfig Config;
	xwsclienthandshake Handshake;
	xwsclienthandshake Before;
	xhttpclient Client;
	xhttprequest* pRequest;
	xhttprequest* pPrepared;
	xhttpresponse* pResponse;
	char StableKey[XWS_KEY_CAPACITY];

	memset(ConfigStorage, 0xC3, sizeof(ConfigStorage));
	memset(HandshakeStorage, 0xD4, sizeof(HandshakeStorage));
	memset(KeyStorage, 0xE5, sizeof(KeyStorage));
	xrtWsClientConfigInit(pConfig);
	memcpy(&Config, pConfig, sizeof(Config));
	testRequire(
		(ConfigStorage[0] == UINT8_C(0xC3)) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] ==
		 UINT8_C(0xC3)) &&
		(Config.Connection.Role == XWS_ROLE_CLIENT),
		"WebSocket client rejected an unaligned configuration"
	);
	pPrepared = xrtWsClientRequestCreate(
		XRT_STR_LITERAL("http://example.test/chat"),
		pConfig,
		sKey
	);
	testRequire(
		(pPrepared != NULL) &&
		(KeyStorage[0] == (char)0xE5) &&
		(KeyStorage[sizeof(KeyStorage) - 1u] == (char)0xE5) &&
		xrtWsKeyValid((xstrview) { sKey, XWS_KEY_SIZE }),
		"WebSocket client rejected unaligned request inputs"
	);
	xrtHttpRequestDestroy(pPrepared);

	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://example.test/chat")
	);
	testRequire(
		pRequest != NULL,
		"WebSocket client range request fixture failed"
	);
	pPrepared = xrtWsClientRequestClone(
		pRequest,
		pConfig,
		sKey
	);
	testRequire(
		pPrepared != NULL,
		"WebSocket client rejected an unaligned clone configuration"
	);
	xrtHttpRequestDestroy(pPrepared);

	pResponse = testWsClientCheckResponse();
	testRequire(
		xrtWsClientCheck(
			pResponse,
			XRT_STR_LITERAL("dGhlIHNhbXBsZSBub25jZQ=="),
			pConfig,
			pHandshake
		) &&
		(HandshakeStorage[0] == UINT8_C(0xD4)) &&
		(HandshakeStorage[sizeof(HandshakeStorage) - 1u] ==
		 UINT8_C(0xD4)),
		"WebSocket client rejected an unaligned handshake output"
	);

	testRequire(
		!xrtWsClientCheck(
			pResponse,
			XRT_STR_LITERAL("dGhlIHNhbXBsZSBub25jZQ=="),
			pConfig,
			(xwsclienthandshake*)(void*)pResponse
		),
		"WebSocket client accepted response/output overlap"
	);
	testWsClientArgumentError(
		"WebSocket client response overlap error mismatch"
	);
	testRequire(
		!xrtWsClientCheck(
			pResponse,
			XRT_STR_LITERAL("dGhlIHNhbXBsZSBub25jZQ=="),
			pConfig,
			(xwsclienthandshake*)(void*)pConfig
		),
		"WebSocket client accepted configuration/output overlap"
	);
	testWsClientArgumentError(
		"WebSocket client configuration overlap error mismatch"
	);
	memcpy(
		StableKey,
		"dGhlIHNhbXBsZSBub25jZQ==",
		XWS_KEY_CAPACITY
	);
	testRequire(
		!xrtWsClientCheck(
			pResponse,
			(xstrview) { StableKey, XWS_KEY_SIZE },
			pConfig,
			(xwsclienthandshake*)(void*)StableKey
		),
		"WebSocket client accepted key/output overlap"
	);
	testWsClientArgumentError(
		"WebSocket client key overlap error mismatch"
	);

	xrtWsClientConfigInit(
		(xwsclientconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testWsClientArgumentError(
		"WebSocket client initialized a wrapping configuration"
	);
	memset(&Handshake, 0xA5, sizeof(Handshake));
	Before = Handshake;
	testRequire(
		!xrtWsClientCheck(
			pWrappingResponse,
			XRT_STR_LITERAL("dGhlIHNhbXBsZSBub25jZQ=="),
			NULL,
			&Handshake
		) &&
		(memcmp(&Handshake, &Before, sizeof(Handshake)) == 0),
		"WebSocket client accepted a wrapping response range"
	);
	testWsClientArgumentError(
		"WebSocket client response range error mismatch"
	);
	testRequire(
		!xrtWsClientCheck(
			pResponse,
			XRT_STR_LITERAL("dGhlIHNhbXBsZSBub25jZQ=="),
			pWrappingConfig,
			&Handshake
		) &&
		(memcmp(&Handshake, &Before, sizeof(Handshake)) == 0),
		"WebSocket client accepted a wrapping configuration range"
	);
	testWsClientArgumentError(
		"WebSocket client configuration range error mismatch"
	);
	testRequire(
		!xrtWsClientCheck(
			pResponse,
			(xstrview) {
				(cstr)(uintptr_t)(UINTPTR_MAX - 1u),
				4u
			},
			NULL,
			&Handshake
		) &&
		(memcmp(&Handshake, &Before, sizeof(Handshake)) == 0),
		"WebSocket client accepted a wrapping key range"
	);
	testWsClientArgumentError(
		"WebSocket client key range error mismatch"
	);
	testRequire(
		!xrtWsClientCheck(
			pResponse,
			XRT_STR_LITERAL("dGhlIHNhbXBsZSBub25jZQ=="),
			NULL,
			(xwsclienthandshake*)(uintptr_t)(UINTPTR_MAX - 1u)
		),
		"WebSocket client accepted a wrapping handshake output"
	);
	testWsClientArgumentError(
		"WebSocket client output range error mismatch"
	);

	memset(StableKey, 0xA5, sizeof(StableKey));
	testRequire(
		(xrtWsClientRequestCreate(
			(xstrview) {
				(cstr)(uintptr_t)(UINTPTR_MAX - 1u),
				4u
			},
			NULL,
			StableKey
		 ) == NULL) &&
		(StableKey[0] == (char)0xA5),
		"WebSocket client accepted a wrapping URL range"
	);
	testWsClientArgumentError(
		"WebSocket client URL range error mismatch"
	);
	testRequire(
		xrtWsClientRequestCreate(
			XRT_STR_LITERAL("http://example.test/chat"),
			pWrappingConfig,
			StableKey
		) == NULL,
		"WebSocket client request accepted a wrapping configuration"
	);
	testWsClientArgumentError(
		"WebSocket client request configuration error mismatch"
	);
	testRequire(
		xrtWsClientRequestCreate(
			XRT_STR_LITERAL("http://example.test/chat"),
			NULL,
			(char*)(uintptr_t)(UINTPTR_MAX - 1u)
		) == NULL,
		"WebSocket client request accepted a wrapping key output"
	);
	testWsClientArgumentError(
		"WebSocket client request key error mismatch"
	);
	testRequire(
		xrtWsClientRequestClone(
			pWrappingRequest,
			NULL,
			StableKey
		) == NULL,
		"WebSocket client clone accepted a wrapping request"
	);
	testWsClientArgumentError(
		"WebSocket client clone request error mismatch"
	);
	testRequire(
		xrtWsClientRequestClone(
			pRequest,
			pWrappingConfig,
			StableKey
		) == NULL,
		"WebSocket client clone accepted a wrapping configuration"
	);
	testWsClientArgumentError(
		"WebSocket client clone configuration error mismatch"
	);
	testRequire(
		xrtWsClientRequestClone(
			pRequest,
			NULL,
			(char*)(uintptr_t)(UINTPTR_MAX - 1u)
		) == NULL,
		"WebSocket client clone accepted a wrapping key output"
	);
	testWsClientArgumentError(
		"WebSocket client clone key error mismatch"
	);

	memset(&Client, 0, sizeof(Client));
	testRequire(
		xrtWsConnect(
			&Client,
			XRT_STR_LITERAL("http://example.test/chat"),
			pWrappingConfig,
			NULL,
			NULL,
			testWsClientRangeDone,
			NULL
		) == NULL,
		"WebSocket Connect accepted a wrapping configuration"
	);
	testWsClientArgumentError(
		"WebSocket Connect configuration error mismatch"
	);
	testRequire(
		xrtWsConnect(
			&Client,
			XRT_STR_LITERAL("http://example.test/chat"),
			NULL,
			pWrappingEvents,
			NULL,
			testWsClientRangeDone,
			NULL
		) == NULL,
		"WebSocket Connect accepted a wrapping event table"
	);
	testWsClientArgumentError(
		"WebSocket Connect event error mismatch"
	);
	testRequire(
		xrtWsConnect(
			pWrappingClient,
			XRT_STR_LITERAL("http://example.test/chat"),
			NULL,
			NULL,
			NULL,
			testWsClientRangeDone,
			NULL
		) == NULL,
		"WebSocket Connect accepted a wrapping HTTP client"
	);
	testWsClientArgumentError(
		"WebSocket Connect client error mismatch"
	);
	testRequire(
		xrtWsConnect(
			&Client,
			XRT_STR_LITERAL("http://example.test/chat"),
			NULL,
			NULL,
			NULL,
			NULL,
			NULL
		) == NULL,
		"WebSocket Connect accepted a null callback"
	);
	testWsClientArgumentError(
		"WebSocket Connect callback error mismatch"
	);
	testRequire(
		xrtWsConnectRequest(
			&Client,
			pRequest,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL
		) == NULL,
		"WebSocket ConnectRequest accepted a null callback"
	);
	testWsClientArgumentError(
		"WebSocket ConnectRequest callback error mismatch"
	);
	testRequire(
		xrtWsConnectRequest(
			pWrappingClient,
			pRequest,
			NULL,
			NULL,
			NULL,
			testWsClientRangeDone,
			NULL
		) == NULL,
		"WebSocket ConnectRequest accepted a wrapping client"
	);
	testWsClientArgumentError(
		"WebSocket ConnectRequest client error mismatch"
	);
	testRequire(
		xrtWsConnectRequest(
			&Client,
			pWrappingRequest,
			NULL,
			NULL,
			NULL,
			testWsClientRangeDone,
			NULL
		) == NULL,
		"WebSocket ConnectRequest accepted a wrapping request"
	);
	testWsClientArgumentError(
		"WebSocket ConnectRequest request error mismatch"
	);

	xrtHttpResponseDestroy(pResponse);
	xrtHttpRequestDestroy(pRequest);
}



int main(void)
{
	static const struct {
		test_ws_client_invalid_case Scenario;
		xwshandshakeerror Error;
	} Cases[] = {
		{
			TEST_WS_CLIENT_STATUS,
			XWS_HANDSHAKE_ERROR_STATUS
		},
		{
			TEST_WS_CLIENT_ACCEPT,
			XWS_HANDSHAKE_ERROR_ACCEPT
		},
		{
			TEST_WS_CLIENT_PROTOCOL,
			XWS_HANDSHAKE_ERROR_PROTOCOL
		},
		{
			TEST_WS_CLIENT_EXTENSION,
			XWS_HANDSHAKE_ERROR_EXTENSION
		},
		{
			TEST_WS_CLIENT_BODY,
			XWS_HANDSHAKE_ERROR_BODY
		},
		{
			TEST_WS_CLIENT_DUPLICATE_ACCEPT,
			XWS_HANDSHAKE_ERROR_ACCEPT
		},
		{
			TEST_WS_CLIENT_CONNECTION,
			XWS_HANDSHAKE_ERROR_CONNECTION
		},
		{
			TEST_WS_CLIENT_UPGRADE,
			XWS_HANDSHAKE_ERROR_UPGRADE
		}
	};

	testWsClientCheckBodies();
	testWsClientRanges();
	for ( size_t i = 0;
		i < (sizeof(Cases) / sizeof(Cases[0]));
		i++ ) {
		testWsClientInvalidRun(
			Cases[i].Scenario,
			Cases[i].Error
		);
	}
	printf(
		"[PASS] WebSocket client invalid responses (%s)\n",
		TEST_WS_CLIENT_INVALID_BACKEND_NAME
	);
	return 0;
}

#ifndef TEST_WS_HTTP_TLS
	#define TEST_WS_HTTP_TLS 0
#endif

#ifndef TEST_WS_HTTP_DEFLATE
	#define TEST_WS_HTTP_DEFLATE 0
#endif

#if TEST_WS_HTTP_DEFLATE
	#define TEST_WS_HTTP_MESSAGES 2u
#else
	#define TEST_WS_HTTP_MESSAGES 1u
#endif

#if TEST_WS_HTTP_TLS
	#include "../fixtures/tls_server.h"
#else
	#include "../test.h"
#endif



#ifndef TEST_WS_HTTP_BACKEND
	#define TEST_WS_HTTP_BACKEND XNET_PORT_SELECT
#endif

#ifndef TEST_WS_HTTP_BACKEND_NAME
	#define TEST_WS_HTTP_BACKEND_NAME "select"
#endif



typedef struct test_ws_http test_ws_http;



typedef struct test_ws_http_endpoint {
	test_ws_http* Test;
	xwsrole Role;
	xatomic32 Messages;
	xatomic32 Ping;
	xatomic32 Pong;
	xatomic32 Closed;
	xwsopcode Opcode;
	size_t Size;
	uint8 Data[32];
	xwsconnclose Close;
} test_ws_http_endpoint;



struct test_ws_http {
	xnetengine* Engine;
	xhttpserver* Server;
	xhttpclient* Client;
	xhttpcall* Call;
	xhttpcall* CallbackCall;
	xhttpresponse* Response;
	xatomicptr ClientConnection;
	xatomicptr ServerConnection;
	xatomic32 ClientDone;
	xatomic32 ServerDone;
	xatomic32 HttpClose;
	xatomic32 Errors;
	xatomic32 Shutdown;
	#if TEST_WS_HTTP_TLS
		xatomic32 ServerAttachFailed;
		xatomic32 ClientAttachFinished;
		xatomic32 TlsSni;
		xatomic32 TlsVerifyName;
	#endif
	bool ServerSubmitting;
	test_ws_http_endpoint ClientEndpoint;
	test_ws_http_endpoint ServerEndpoint;
	xwsconnevents WsEvents;
};



/* 测试中按大小写敏感规则比较两个文本视图。 */
static bool testWsHttpTextEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 在测试截止时间前等待跨 Worker 发布的状态。 */
static void testWsHttpWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(
		UINT64_C(10000000)
	);

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



/* 按角色返回当前 Connection 对应的测试端点。 */
static test_ws_http_endpoint* testWsHttpEndpoint(
	xwsconn* pConnection,
	ptr pData
)
{
	test_ws_http* pTest = (test_ws_http*)pData;

	return xrtWsConnRole(pConnection) ==
		XWS_ROLE_CLIENT ?
			&pTest->ClientEndpoint :
			&pTest->ServerEndpoint;
}



/* 确认 HTTP 适配器按当前测试模式接管了正确的传输类型。 */
static bool testWsHttpTransport(xwsconn* pConnection)
{
	#if TEST_WS_HTTP_TLS
		return (xrtWsConnTls(pConnection) != NULL) &&
			(xrtWsConnTcp(pConnection) == NULL);
	#else
		return (xrtWsConnTcp(pConnection) != NULL);
	#endif
}



/* 按当前测试模式发送 Text，压缩路径仍保留同一业务断言。 */
static xnetresult testWsHttpTextSend(
	xwsconn* pConnection,
	xstrview Text
)
{
	#if TEST_WS_HTTP_DEFLATE
		return xrtWsConnTextCompressed(
			pConnection,
			Text
		);
	#else
		return xrtWsConnText(pConnection, Text);
	#endif
}



/* 按当前测试模式发送 Binary 回显。 */
static xnetresult testWsHttpBinarySend(
	xwsconn* pConnection,
	xbytesview Data
)
{
	#if TEST_WS_HTTP_DEFLATE
		return xrtWsConnBinaryCompressed(
			pConnection,
			Data
		);
	#else
		return xrtWsConnBinary(pConnection, Data);
	#endif
}



/* 开始一条消息并清空端点暂存。 */
static void testWsHttpMessageBegin(
	xwsconn* pConnection,
	const xwsmessageinfo* pInfo,
	ptr pData
)
{
	test_ws_http_endpoint* pEndpoint =
		testWsHttpEndpoint(pConnection, pData);

	testRequire(
		pInfo != NULL,
		"WebSocket HTTP message info is null"
	);
	pEndpoint->Opcode = (xwsopcode)pInfo->Opcode;
	pEndpoint->Size = 0;
}



/* 复制流式消息分块，供 End 回调检查并回显。 */
static void testWsHttpMessageData(
	xwsconn* pConnection,
	xbytesview Data,
	ptr pData
)
{
	test_ws_http_endpoint* pEndpoint =
		testWsHttpEndpoint(pConnection, pData);

	testRequire(
		Data.Size <=
			(sizeof(pEndpoint->Data) -
			 pEndpoint->Size),
		"WebSocket HTTP message exceeded fixture storage"
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



/* 服务端回显 Binary，客户端核对完整逻辑消息。 */
static void testWsHttpMessageEnd(
	xwsconn* pConnection,
	ptr pData
)
{
	test_ws_http_endpoint* pEndpoint =
		testWsHttpEndpoint(pConnection, pData);

	if ( pEndpoint->Role == XWS_ROLE_SERVER ) {
		testRequire(
			(pEndpoint->Opcode == XWS_OPCODE_TEXT) &&
			(pEndpoint->Size == 5) &&
			(memcmp(
				pEndpoint->Data,
				"hello",
				5
			 ) == 0) &&
			(testWsHttpBinarySend(
				pConnection,
				(xbytesview) {
					pEndpoint->Data,
					pEndpoint->Size
				}
			 ) == XNET_RESULT_OK),
			"WebSocket HTTP server message or echo mismatch"
		);
	} else {
		testRequire(
			(pEndpoint->Opcode == XWS_OPCODE_BINARY) &&
			(pEndpoint->Size == 5) &&
			(memcmp(
				pEndpoint->Data,
				"hello",
				5
			 ) == 0),
			"WebSocket HTTP client echo mismatch"
		);
	}
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Messages,
		1,
		XMEMORY_RELEASE
	);
}



/* 服务端观察客户端 Ping；自动 Pong 由 Connection 负责。 */
static void testWsHttpPing(
	xwsconn* pConnection,
	xbytesview Payload,
	ptr pData
)
{
	test_ws_http_endpoint* pEndpoint =
		testWsHttpEndpoint(pConnection, pData);

	testRequire(
		(pEndpoint->Role == XWS_ROLE_SERVER) &&
		(Payload.Size == 5) &&
		(memcmp(Payload.Data, "probe", 5) == 0),
		"WebSocket HTTP Ping mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Ping,
		1,
		XMEMORY_RELEASE
	);
}



/* 客户端观察服务端自动 Pong。 */
static void testWsHttpPong(
	xwsconn* pConnection,
	xbytesview Payload,
	ptr pData
)
{
	test_ws_http_endpoint* pEndpoint =
		testWsHttpEndpoint(pConnection, pData);

	testRequire(
		(pEndpoint->Role == XWS_ROLE_CLIENT) &&
		(Payload.Size == 5) &&
		(memcmp(Payload.Data, "probe", 5) == 0),
		"WebSocket HTTP Pong mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Pong,
		1,
		XMEMORY_RELEASE
	);
}



/* 正常测试路径不允许发布 WebSocket 会话错误。 */
static void testWsHttpError(
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_ws_http* pTest = (test_ws_http*)pData;

	(void)pConnection;
	testRequire(
		pError != NULL,
		"WebSocket HTTP error omitted its cause"
	);
	(void)xrtAtomic32FetchAdd(
		&pTest->Errors,
		1,
		XMEMORY_RELEASE
	);
}



/* 保存完整、干净的 1000 Close 终态。 */
static void testWsHttpClose(
	xwsconn* pConnection,
	const xwsconnclose* pClose,
	ptr pData
)
{
	test_ws_http_endpoint* pEndpoint =
		testWsHttpEndpoint(pConnection, pData);

	testRequire(
		(pClose != NULL) &&
		(pClose->Transport == XNET_RESULT_OK) &&
		((pClose->Flags & XWS_CONN_CLOSE_SENT) != 0) &&
		((pClose->Flags & XWS_CONN_CLOSE_RECEIVED) != 0) &&
		((pClose->Flags & XWS_CONN_CLOSE_CLEAN) != 0) &&
		(pClose->LocalCode == XWS_CLOSE_NORMAL) &&
		(pClose->RemoteCode == XWS_CLOSE_NORMAL),
		"WebSocket HTTP Close snapshot mismatch"
	);
	pEndpoint->Close = *pClose;
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* HTTP Upgrade 完成后接管服务端 Connection 引用。 */
static void testWsHttpServerDone(
	xhttpconn* pHttp,
	xnetresult Result,
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_ws_http* pTest = (test_ws_http*)pData;

	testRequire(
		!pTest->ServerSubmitting,
		"WebSocket server Upgrade reentered submit stack"
	);
	testRequire(
		(pHttp != NULL) &&
		(Result == XNET_RESULT_OK) &&
		(pConnection != NULL) &&
		(pError == NULL) &&
		testWsHttpTransport(pConnection),
		"WebSocket server Upgrade completion mismatch"
	);
	xrtAtomicPtrStore(
		&pTest->ServerConnection,
		pConnection,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pTest->ServerDone,
		1,
		XMEMORY_RELEASE
	);
}



#if TEST_WS_HTTP_TLS

/* 确认 HTTPS 适配器按目标 DNS 主机发送 SNI，并只协商 HTTP/1.1。 */
static bool testWsHttpTlsSelect(
	ptr pContext,
	const xtlsserverrequest* pRequest,
	xtlsserverchoice* pChoice
)
{
	test_ws_http* pTest = (test_ws_http*)pContext;
	xbytesview Http11 = XRT_BYTES_LITERAL("http/1.1");
	bool bName;
	bool bProtocol;

	bName = (pRequest != NULL) &&
		(pRequest->ServerName.Size ==
		 (sizeof("websocket.test") - 1u)) &&
		(memcmp(
			pRequest->ServerName.Data,
			"websocket.test",
			pRequest->ServerName.Size
		 ) == 0);
	bProtocol = (pRequest != NULL) &&
		(xrtTlsProtocolFind(
			pRequest->Protocols,
			Http11
		 ) == XTLS_ITEM_VALUE);
	if ( bName && bProtocol ) {
		xrtAtomic32Store(
			&pTest->TlsSni,
			1,
			XMEMORY_RELEASE
		);
	}
	return (pChoice != NULL) &&
		(pChoice->Identity != NULL) &&
		bName && bProtocol;
}



/* 确认证书验证使用目标主机名，而不是解析后的回环地址。 */
static xtlsverifydecision testWsHttpTlsVerify(
	const xtlspeer* pPeer,
	ptr pContext
)
{
	test_ws_http* pTest = (test_ws_http*)pContext;
	bool bValid;

	bValid = (pPeer != NULL) &&
		(pPeer->Role == XTLS_SERVER) &&
		(pPeer->Name.Size ==
		 (sizeof("websocket.test") - 1u)) &&
		(memcmp(
			pPeer->Name.Data,
			"websocket.test",
			pPeer->Name.Size
		 ) == 0) &&
		(pPeer->CertificateCount != 0) &&
		(((uintptr_t)pPeer->Certificates %
		  TEST_ALIGNOF(xx509cert)) == 0);
	if ( bValid ) {
		xrtAtomic32Store(
			&pTest->TlsVerifyName,
			1,
			XMEMORY_RELEASE
		);
	}
	return bValid ? XTLS_VERIFY_ACCEPT : XTLS_VERIFY_REJECT;
}



/* TLS 线路预算不相容时，Upgrade 回调必须收到唯一结构化失败。 */
static void testWsHttpServerAttachFailed(
	xhttpconn* pHttp,
	xnetresult Result,
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_ws_http* pTest = (test_ws_http*)pData;

	testRequire(
		!pTest->ServerSubmitting,
		"WebSocket TLS attach failure reentered submit stack"
	);
	testRequire(
		(pHttp != NULL) &&
		(Result == XNET_RESULT_ERROR) &&
		(pConnection == NULL) &&
		(pError != NULL) &&
		(xrtErrorKind(pError) == XERR_RANGE) &&
		(xrtErrorCode(pError) == XWS_CONN_ERROR_CONFIG),
		"WebSocket TLS attach failure mismatch"
	);
	xrtAtomic32Store(
		&pTest->ServerAttachFailed,
		1,
		XMEMORY_RELEASE
	);
}

#endif



/* 对完整 HTTP 请求执行标准 WebSocket Upgrade。 */
static void testWsHttpRequest(
	xhttpserver* pServer,
	xhttpconn* pHttp,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_ws_http* pTest = (test_ws_http*)pData;
	xwsserverconfig Config;
	xwsserverhandshake Handshake;
	xhttpheaders* pHeaders;
	xwsupgradeproc pDone = testWsHttpServerDone;

	(void)pServer;
	testRequire(
		(xrtHttpServerRequestFlags(pRequest) &
		 XHTTP_SERVER_REQUEST_UPGRADE) != 0 &&
		(xrtHttpConnSecure(pHttp) ==
		 (TEST_WS_HTTP_TLS != 0)),
		"WebSocket HTTP request omitted Upgrade flag"
	);
	xrtWsServerConfigInit(&Config);
	Config.Protocols =
		XRT_STR_LITERAL("chat.v2, chat.v1");
	#if TEST_WS_HTTP_DEFLATE
		Config.EnableDeflate = true;
		Config.RequireDeflate = true;
		Config.Connection.Inflater.Retain = true;
		Config.Connection.Deflater.Retain = true;
	#endif
	#if TEST_WS_HTTP_TLS
		if ( testWsHttpTextEqual(
			xrtHttpServerRequestTarget(pRequest),
			XRT_STR_LITERAL("/attach-fail")
		) ) {
			Config.Connection.ControlReserve =
				(2u + XWS_CLOSE_PAYLOAD_MAX) * 3u;
			pDone = testWsHttpServerAttachFailed;
		}
	#endif
	testRequire(
		xrtWsServerCheck(pRequest, &Config, &Handshake),
		"WebSocket server handshake check failed"
	);
	pHeaders = xrtHttpHeadersCreate(NULL);
	testRequire(
		(pHeaders != NULL) &&
		xrtHttpHeadersAdd(
			pHeaders,
			XRT_STR_LITERAL("Sec-WebSocket-Accept"),
			XRT_STR_LITERAL("forbidden")
		),
		"WebSocket application Header creation failed"
	);
	testRequire(
		(xrtWsUpgradeAccept(
			pHttp,
			&Config,
			&Handshake,
			pHeaders,
			&pTest->WsEvents,
			pTest,
			pDone,
			pTest
		 ) == XNET_RESULT_ERROR) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_HANDSHAKE_ERROR_OUTPUT),
		"WebSocket reserved application Header was accepted"
	);
	xrtClearError();
	xrtHttpHeadersClear(pHeaders);
	testRequire(
		xrtHttpHeadersAdd(
			pHeaders,
			XRT_STR_LITERAL("X-Xrt-WebSocket"),
			XRT_STR_LITERAL("accepted")
		),
		"WebSocket custom application Header creation failed"
	);
	pTest->ServerSubmitting = true;
	testRequire(
		xrtWsUpgradeAccept(
			pHttp,
			&Config,
			&Handshake,
			pHeaders,
			&pTest->WsEvents,
			pTest,
			pDone,
			pTest
		) == XNET_RESULT_OK,
		"WebSocket server Upgrade submission failed"
	);
	xrtHttpHeadersDestroy(pHeaders);
	testRequire(
		xrtAtomic32Load(
			&pTest->ServerDone,
			XMEMORY_ACQUIRE
		) == 0,
		"WebSocket server Upgrade completed synchronously"
	);
	pTest->ServerSubmitting = false;
}



/* Upgrade 后 HTTP 层不得再发布传输 Close。 */
static void testWsHttpHttpClose(
	xhttpserver* pServer,
	xhttpconn* pHttp,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_ws_http* pTest = (test_ws_http*)pData;

	(void)pServer;
	(void)pHttp;
	(void)Result;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pTest->HttpClose,
		1,
		XMEMORY_RELEASE
	);
}



/* 正常握手路径不允许 HTTP Server 发布错误。 */
static void testWsHttpHttpError(
	xhttpserver* pServer,
	xhttpconn* pHttp,
	const xerror* pError,
	ptr pData
)
{
	test_ws_http* pTest = (test_ws_http*)pData;

	(void)pServer;
	(void)pHttp;
	testRequire(
		pError != NULL,
		"WebSocket HTTP server error omitted its cause"
	);
	(void)xrtAtomic32FetchAdd(
		&pTest->Errors,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 HTTP Server 已排空所有仍归自身所有的连接。 */
static void testWsHttpShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_ws_http* pTest = (test_ws_http*)pData;

	testRequire(
		xrtHttpServerState(pServer) ==
			XHTTP_SERVER_CLOSED,
		"WebSocket HTTP server shutdown state mismatch"
	);
	xrtAtomic32Store(
		&pTest->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



/* 测试域名固定解析到本机，端口仍由 URL 覆盖。 */
static xnetaddrlist* testWsHttpLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)pData;
	testRequire(
		strcmp(sHost, "websocket.test") == 0,
		"WebSocket client resolved an unexpected host"
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
		"WebSocket resolver fixture failed"
	);
	return xrtNetAddrListCreate(&Address, 1);
}



/* 客户端握手完成后接管 Connection 并发送消息与 Ping。 */
static void testWsHttpClientDone(
	xhttpcall* pCall,
	xnetresult Result,
	xwsconn* pConnection,
	xhttpresponse* pResponse,
	const xerror* pError,
	ptr pData
)
{
	test_ws_http* pTest = (test_ws_http*)pData;
	const xhttpfield* pApplicationHeader;
	xnetresult First;
	xnetresult Second = XNET_RESULT_OK;
	#if TEST_WS_HTTP_DEFLATE
		uint8 RejectedData[1024];
		uint32 iRandom = UINT32_C(0xD1B54A35);
		size_t iPending;
		xnetresult Rejected;
	#endif

	pApplicationHeader = xrtHttpResponseHeader(
		pResponse,
		XRT_STR_LITERAL("X-Xrt-WebSocket")
	);
	testRequire(
		(pCall != NULL) &&
		(Result == XNET_RESULT_OK) &&
		(pConnection != NULL) &&
		(pResponse != NULL) &&
		(pError == NULL) &&
		testWsHttpTransport(pConnection) &&
		(xrtHttpResponseStatus(pResponse) ==
		 XHTTP_STATUS_SWITCHING_PROTOCOLS) &&
		(pApplicationHeader != NULL) &&
		testWsHttpTextEqual(
			pApplicationHeader->Value,
			XRT_STR_LITERAL("accepted")
		),
		"WebSocket client completion mismatch"
	);
	pTest->CallbackCall = pCall;
	pTest->Response = pResponse;
	xrtAtomicPtrStore(
		&pTest->ClientConnection,
		pConnection,
		XMEMORY_RELEASE
	);
	First = testWsHttpTextSend(
		pConnection,
		XRT_STR_LITERAL("hello")
	);
	#if TEST_WS_HTTP_DEFLATE
		for ( size_t i = 0;
			i < sizeof(RejectedData);
			i++ ) {
			iRandom ^= iRandom << 13;
			iRandom ^= iRandom >> 17;
			iRandom ^= iRandom << 5;
			RejectedData[i] = (uint8)iRandom;
		}
		iPending = xrtWsConnPending(pConnection);
		Rejected = xrtWsConnBinaryCompressed(
			pConnection,
			(xbytesview) {
				RejectedData,
				sizeof(RejectedData)
			}
		);
		testRequire(
			(Rejected == XNET_RESULT_ERROR) &&
			(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
			(xrtErrorCode(xrtGetError()) ==
			 XWS_CONN_ERROR_LIMIT) &&
			(xrtWsConnPending(pConnection) == iPending) &&
			(xrtWsConnError(pConnection) == NULL),
			"WebSocket compressed capacity rejection changed state"
		);
		xrtClearError();
		Second = testWsHttpTextSend(
			pConnection,
			XRT_STR_LITERAL("hello")
		);
	#endif
	testRequire(
		(First == XNET_RESULT_OK) &&
		(Second == XNET_RESULT_OK) &&
		(xrtWsConnPing(
			pConnection,
			XRT_BYTES_LITERAL("probe")
		 ) == XNET_RESULT_OK),
		"WebSocket client initial send failed"
	);
	xrtAtomic32Store(
		&pTest->ClientDone,
		1,
		XMEMORY_RELEASE
	);
}



#if TEST_WS_HTTP_TLS

/* 对端在 101 后中止 TLS 时，客户端必须终结且归还所有晚到对象。 */
static void testWsHttpTlsAttachClientDone(
	xhttpcall* pCall,
	xnetresult Result,
	xwsconn* pConnection,
	xhttpresponse* pResponse,
	const xerror* pError,
	ptr pData
)
{
	test_ws_http* pTest = (test_ws_http*)pData;
	bool bOpened = (Result == XNET_RESULT_OK) &&
		(pConnection != NULL) &&
		(pResponse != NULL) &&
		(pError == NULL) &&
		(xrtHttpResponseStatus(pResponse) ==
		 XHTTP_STATUS_SWITCHING_PROTOCOLS);
	bool bFailed = (Result != XNET_RESULT_OK) &&
		(pConnection == NULL) &&
		(pError != NULL);

	testRequire(
		(pCall != NULL) && (bOpened || bFailed),
		"WebSocket TLS attach-failure client terminal mismatch"
	);
	if ( pConnection != NULL ) {
		(void)xrtWsConnAbort(pConnection);
		xrtWsConnDestroy(pConnection);
	}
	xrtHttpResponseDestroy(pResponse);
	xrtAtomic32Store(
		&pTest->ClientAttachFinished,
		1,
		XMEMORY_RELEASE
	);
}

#endif



/* 在客户端 Worker 上发起标准关闭握手。 */
static void testWsHttpCloseTask(
	xnetworker* pWorker,
	ptr pData
)
{
	test_ws_http* pTest = (test_ws_http*)pData;
	xwsconn* pConnection = (xwsconn*)xrtAtomicPtrLoad(
		&pTest->ClientConnection,
		XMEMORY_ACQUIRE
	);

	testRequire(
		(pConnection != NULL) &&
		(pWorker == xrtWsConnWorker(pConnection)) &&
		(xrtWsConnClose(
			pConnection,
			XWS_CLOSE_NORMAL,
			XRT_STR_LITERAL("done")
		 ) == XNET_RESULT_OK),
		"WebSocket HTTP client Close failed"
	);
}



/* 覆盖 URL 映射、Header 替换和失败原子性。 */
static void testWsHttpRequestBuilder(void)
{
	char sKey[XWS_KEY_CAPACITY];
	char sFailed[XWS_KEY_CAPACITY];
	xwsclientconfig Config;
	xhttprequest* pRequest;
	xhttprequest* pFragment;
	xhttprequest* pSource;
	const xhttpfield* pField;

	xrtWsClientConfigInit(&Config);
	Config.Protocols =
		XRT_STR_LITERAL("chat.v1, chat.v2");
	pRequest = xrtWsRequestCreate(
		XRT_STR_LITERAL("wss://example.test/chat")
	);
	testRequire(
		(pRequest != NULL) && testWsHttpTextEqual(
			xrtHttpRequestMethod(pRequest),
			XRT_STR_LITERAL("GET")
		) && testWsHttpTextEqual(
			xrtHttpRequestUrlText(pRequest),
			XRT_STR_LITERAL("https://example.test/chat")
		) && (xrtHttpRequestHeader(
			pRequest,
			XRT_STR_LITERAL("Upgrade")
		 ) == NULL),
		"WebSocket base request mapping or field isolation mismatch"
	);
	xrtHttpRequestDestroy(pRequest);
	testRequire(
		(xrtWsRequestCreate(
			XRT_STR_LITERAL(
				"ws://user:secret@example.test/chat"
			)
		 ) == NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_HANDSHAKE_ERROR_ARGUMENT),
		"WebSocket base request accepted URL userinfo"
	);
	xrtClearError();

	memset(sKey, 0, sizeof(sKey));
	pRequest = xrtWsClientRequestCreate(
		XRT_STR_LITERAL("wss://example.test/chat"),
		&Config,
		sKey
	);
	testRequire(
		(pRequest != NULL) &&
		xrtWsKeyValid(
			(xstrview) { sKey, XWS_KEY_SIZE }
		) &&
		testWsHttpTextEqual(
			xrtHttpRequestUrlText(pRequest),
			XRT_STR_LITERAL(
				"https://example.test/chat"
			)
		),
		"WebSocket request URL mapping or key mismatch"
	);
	pField = xrtHttpRequestHeader(
		pRequest,
		XRT_STR_LITERAL("Sec-WebSocket-Protocol")
	);
	testRequire(
		(pField != NULL) &&
		testWsHttpTextEqual(
			pField->Value,
			XRT_STR_LITERAL("chat.v1, chat.v2")
		),
		"WebSocket request protocol field mismatch"
	);
	xrtHttpRequestDestroy(pRequest);

	/* URI fragment 不属于 WebSocket resource-name，连显式空值也必须拒绝。 */
	memset(sFailed, 'x', sizeof(sFailed));
	testRequire(
		(xrtWsClientRequestCreate(
			XRT_STR_LITERAL(
				"wss://example.test/chat#section"
			),
			&Config,
			sFailed
		 ) == NULL) &&
		(memcmp(
			sFailed,
			"xxxxxxxxxxxxxxxxxxxxxxxxx",
			sizeof(sFailed)
		 ) == 0) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_HANDSHAKE_ERROR_ARGUMENT),
		"WebSocket request accepted a URL fragment"
	);
	xrtClearError();
	memset(sFailed, 'x', sizeof(sFailed));
	testRequire(
		(xrtWsClientRequestCreate(
			XRT_STR_LITERAL("ws://example.test/chat#"),
			&Config,
			sFailed
		 ) == NULL) &&
		(memcmp(
			sFailed,
			"xxxxxxxxxxxxxxxxxxxxxxxxx",
			sizeof(sFailed)
		 ) == 0),
		"WebSocket request accepted an empty URL fragment"
	);
	xrtClearError();

	/* URI userinfo 不能隐式转化为 WebSocket 认证信息。 */
	memset(sFailed, 'x', sizeof(sFailed));
	testRequire(
		(xrtWsClientRequestCreate(
			XRT_STR_LITERAL(
				"wss://user:secret@example.test/chat"
			),
			&Config,
			sFailed
		 ) == NULL) &&
		(memcmp(
			sFailed,
			"xxxxxxxxxxxxxxxxxxxxxxxxx",
			sizeof(sFailed)
		 ) == 0) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_HANDSHAKE_ERROR_ARGUMENT),
		"WebSocket request accepted URL userinfo"
	);
	xrtClearError();

	/* 自定义 HTTP 请求也必须经过同一 URI 约束。 */
	pFragment = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/chat#section")
	);
	memset(sFailed, 'x', sizeof(sFailed));
	testRequire(
		(pFragment != NULL) &&
		(xrtWsClientRequestClone(
			pFragment,
			&Config,
			sFailed
		 ) == NULL) &&
		(memcmp(
			sFailed,
			"xxxxxxxxxxxxxxxxxxxxxxxxx",
			sizeof(sFailed)
		 ) == 0) &&
		testWsHttpTextEqual(
			xrtHttpRequestUrlText(pFragment),
			XRT_STR_LITERAL(
				"https://example.test/chat#section"
			)
		),
		"WebSocket custom request fragment failure was not atomic"
	);
	xrtHttpRequestDestroy(pFragment);
	xrtClearError();

	pSource = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("http://example.test/chat")
	);
	testRequire(
		(pSource != NULL) &&
		xrtHttpRequestSetHeader(
			pSource,
			XRT_STR_LITERAL("X-Test"),
			XRT_STR_LITERAL("retained")
		),
		"WebSocket source request setup failed"
	);
	memset(sFailed, 'x', sizeof(sFailed));
	testRequire(
		(xrtWsClientRequestClone(
			pSource,
			&Config,
			sFailed
		 ) == NULL) &&
		(memcmp(
			sFailed,
			"xxxxxxxxxxxxxxxxxxxxxxxxx",
			sizeof(sFailed)
		 ) == 0) &&
		testWsHttpTextEqual(
			xrtHttpRequestMethod(pSource),
			XRT_STR_LITERAL("POST")
		) &&
		(xrtHttpRequestHeader(
			pSource,
			XRT_STR_LITERAL("X-Test")
		 ) != NULL),
		"WebSocket request builder failure was not atomic"
	);
	xrtHttpRequestDestroy(pSource);
	xrtClearError();
}



/* 覆盖完整 HTTP/1.1 Upgrade、会话和所有权生命周期。 */
int main(void)
{
	#if TEST_WS_HTTP_TLS
		static const xstrview TlsProtocols[] = {
			XRT_STR_INIT("http/1.1")
		};
	#endif
	test_ws_http Test;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents ServerEvents;
	xhttpclientconfig ClientConfig;
	xwsclientconfig WsConfig;
	#if TEST_WS_HTTP_TLS
		xhttpservertlsconfig ServerTls;
		xtlsverifierconfig VerifierConfig;
		xtlscontext* pTlsContext;
		xtlsidentity* pTlsIdentity;
		xtlsverifier* pTlsVerifier;
		xhttpcall* pAttachFailure;
		char AttachFailureUrl[128];
		int iAttachFailureLength;
	#endif
	xhttpserverstats Stats;
	xnetaddr Address;
	xwsconn* pClient;
	xwsconn* pServer;
	#if TEST_WS_HTTP_DEFLATE
		xwsdeflate ClientDeflate;
		xwsdeflate ServerDeflate;
		const xhttpfield* pExtensions;
	#endif
	char Url[128];
	int iLength;

	testWsHttpRequestBuilder();
	#if TEST_WS_HTTP_TLS
		pTlsContext = testTlsServerContext();
		pTlsIdentity = testTlsServerIdentity();
		testRequire(
			(pTlsContext != NULL) &&
			(pTlsIdentity != NULL),
			"WebSocket HTTPS TLS fixture creation failed"
		);
		xrtTlsVerifierConfigInit(&VerifierConfig);
		VerifierConfig.Verify = testWsHttpTlsVerify;
		VerifierConfig.Context = &Test;
		pTlsVerifier = xrtTlsVerifierCreate(
			&VerifierConfig
		);
		testRequire(
			pTlsVerifier != NULL,
			"WebSocket HTTPS verifier creation failed"
		);
	#endif
	memset(&Test, 0, sizeof(Test));
	xrtAtomicPtrInit(&Test.ClientConnection, NULL);
	xrtAtomicPtrInit(&Test.ServerConnection, NULL);
	xrtAtomic32Init(&Test.ClientDone, 0);
	xrtAtomic32Init(&Test.ServerDone, 0);
	xrtAtomic32Init(&Test.HttpClose, 0);
	xrtAtomic32Init(&Test.Errors, 0);
	xrtAtomic32Init(&Test.Shutdown, 0);
	#if TEST_WS_HTTP_TLS
		xrtAtomic32Init(&Test.ServerAttachFailed, 0);
		xrtAtomic32Init(&Test.ClientAttachFinished, 0);
		xrtAtomic32Init(&Test.TlsSni, 0);
		xrtAtomic32Init(&Test.TlsVerifyName, 0);
	#endif
	Test.ClientEndpoint.Test = &Test;
	Test.ClientEndpoint.Role = XWS_ROLE_CLIENT;
	Test.ServerEndpoint.Test = &Test;
	Test.ServerEndpoint.Role = XWS_ROLE_SERVER;
	xrtAtomic32Init(&Test.ClientEndpoint.Messages, 0);
	xrtAtomic32Init(&Test.ClientEndpoint.Ping, 0);
	xrtAtomic32Init(&Test.ClientEndpoint.Pong, 0);
	xrtAtomic32Init(&Test.ClientEndpoint.Closed, 0);
	xrtAtomic32Init(&Test.ServerEndpoint.Messages, 0);
	xrtAtomic32Init(&Test.ServerEndpoint.Ping, 0);
	xrtAtomic32Init(&Test.ServerEndpoint.Pong, 0);
	xrtAtomic32Init(&Test.ServerEndpoint.Closed, 0);
	Test.WsEvents.MessageBegin = testWsHttpMessageBegin;
	Test.WsEvents.MessageData = testWsHttpMessageData;
	Test.WsEvents.MessageEnd = testWsHttpMessageEnd;
	Test.WsEvents.Ping = testWsHttpPing;
	Test.WsEvents.Pong = testWsHttpPong;
	Test.WsEvents.Error = testWsHttpError;
	Test.WsEvents.Close = testWsHttpClose;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_WS_HTTP_BACKEND;
	EngineConfig.Workers = 2;
	Test.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(Test.Engine != NULL) &&
		xrtNetEngineStart(Test.Engine),
		"WebSocket HTTP engine start failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"WebSocket HTTP server address failed"
	);
	ServerConfig.WriteSize = 3;
	ServerConfig.Network.Listen.Stream.ReadSize = 2;
	xrtHttpServerEventsInit(&ServerEvents);
	ServerEvents.Request = testWsHttpRequest;
	ServerEvents.Close = testWsHttpHttpClose;
	ServerEvents.Error = testWsHttpHttpError;
	ServerEvents.Shutdown = testWsHttpShutdown;
	ServerEvents.Data = &Test;
	#if TEST_WS_HTTP_TLS
		xrtHttpServerTlsConfigInit(&ServerTls);
		ServerTls.Handshake.Context = pTlsContext;
		ServerTls.Handshake.Identity = pTlsIdentity;
		ServerTls.Handshake.Protocols = TlsProtocols;
		ServerTls.Handshake.ProtocolCount =
			sizeof(TlsProtocols) /
			sizeof(TlsProtocols[0]);
		ServerTls.Handshake.Select = testWsHttpTlsSelect;
		ServerTls.Handshake.SelectContext = &Test;
		ServerTls.Handshake.RequireProtocol = true;
		Test.Server = xrtHttpServerStartTls(
			Test.Engine,
			&ServerConfig,
			&ServerTls,
			&ServerEvents
		);
	#else
		Test.Server = xrtHttpServerStart(
			Test.Engine,
			&ServerConfig,
			&ServerEvents
		);
	#endif
	testRequire(
		(Test.Server != NULL) &&
		xrtHttpServerLocal(Test.Server, 0, &Address),
		"WebSocket HTTP server start failed"
	);

	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Resolver.Lookup = testWsHttpLookup;
	ClientConfig.Dial.FallbackDelay = 1000;
	ClientConfig.Dial.MaxAttempts = 1;
	#if TEST_WS_HTTP_TLS
		ClientConfig.TlsContext = pTlsContext;
		ClientConfig.TlsVerifier = pTlsVerifier;
		ClientConfig.SystemTrust = false;
	#endif
	Test.Client = xrtHttpClientCreate(
		Test.Engine,
		&ClientConfig
	);
	testRequire(
		Test.Client != NULL,
		"WebSocket HTTP client creation failed"
	);
	iLength = snprintf(
		Url,
		sizeof(Url),
		#if TEST_WS_HTTP_TLS
			"wss://websocket.test:%u/chat",
		#else
			"ws://websocket.test:%u/chat",
		#endif
		(unsigned int)Address.Port
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Url)),
		"WebSocket HTTP URL fixture overflowed"
	);
	xrtWsClientConfigInit(&WsConfig);
	WsConfig.Protocols =
		XRT_STR_LITERAL("chat.v1, chat.v2");
	#if TEST_WS_HTTP_DEFLATE
		WsConfig.EnableDeflate = true;
		WsConfig.RequireDeflate = true;
		WsConfig.Deflate.Flags =
			XWS_DEFLATE_CLIENT_MAX_WINDOW |
			XWS_DEFLATE_CLIENT_MAX_WINDOW_ANY;
		WsConfig.Connection.Inflater.Retain = true;
		WsConfig.Connection.Deflater.Retain = true;
		WsConfig.Connection.SendLimit = 1024;
		WsConfig.Connection.ControlReserve =
			XWS_CONN_CONTROL_RESERVE_DEFAULT;
		{
			xwsclientconfig InvalidConfig = WsConfig;
			xhttpserverstats Before;

			InvalidConfig.Connection.Deflater.Level = 11;
			testRequire(
				(xrtWsConnect(
					Test.Client,
					(xstrview) {
						Url,
						(size_t)iLength
					},
					&InvalidConfig,
					&Test.WsEvents,
					&Test,
					testWsHttpClientDone,
					&Test
				 ) == NULL) &&
				(xrtErrorKind(xrtGetError()) ==
				 XERR_VALUE) &&
				(xrtErrorCode(xrtGetError()) ==
				 XWS_HANDSHAKE_ERROR_ARGUMENT) &&
				xrtHttpServerStats(
					Test.Server,
					&Before
				) &&
				(Before.Accepted == 0) &&
				(Before.Requests == 0) &&
				(xrtAtomic32Load(
					&Test.ClientDone,
					XMEMORY_ACQUIRE
				 ) == 0),
				"WebSocket client submitted invalid Deflate configuration"
			);
			xrtClearError();
		}
	#endif
	#if TEST_WS_HTTP_TLS
		iAttachFailureLength = snprintf(
			AttachFailureUrl,
			sizeof(AttachFailureUrl),
			"wss://websocket.test:%u/attach-fail",
			(unsigned int)Address.Port
		);
		testRequire(
			(iAttachFailureLength > 0) &&
			((size_t)iAttachFailureLength <
			 sizeof(AttachFailureUrl)),
			"WebSocket TLS attach-failure URL overflowed"
		);
		pAttachFailure = xrtWsConnect(
			Test.Client,
			(xstrview) {
				AttachFailureUrl,
				(size_t)iAttachFailureLength
			},
			&WsConfig,
			NULL,
			NULL,
			testWsHttpTlsAttachClientDone,
			&Test
		);
		testRequire(
			pAttachFailure != NULL,
			"WebSocket TLS attach-failure call submission failed"
		);
		testWsHttpWait(
			&Test.ServerAttachFailed,
			1,
			"WebSocket TLS server attach failure missing"
		);
		testWsHttpWait(
			&Test.ClientAttachFinished,
			1,
			"WebSocket TLS attach-failure client did not finish"
		);
		xrtHttpCallDestroy(pAttachFailure);
	#endif
	Test.Call = xrtWsConnect(
		Test.Client,
		(xstrview) { Url, (size_t)iLength },
		&WsConfig,
		&Test.WsEvents,
		&Test,
		testWsHttpClientDone,
		&Test
	);
	testRequire(
		Test.Call != NULL,
		"WebSocket client handshake submission failed"
	);
	testWsHttpWait(
		&Test.ClientDone,
		1,
		"WebSocket client handshake did not complete"
	);
	testWsHttpWait(
		&Test.ServerDone,
		1,
		"WebSocket server Upgrade did not complete"
	);
	pClient = (xwsconn*)xrtAtomicPtrLoad(
		&Test.ClientConnection,
		XMEMORY_ACQUIRE
	);
	pServer = (xwsconn*)xrtAtomicPtrLoad(
		&Test.ServerConnection,
		XMEMORY_ACQUIRE
	);
	#if TEST_WS_HTTP_DEFLATE
		pExtensions = xrtHttpResponseHeader(
			Test.Response,
			XRT_STR_LITERAL(
				"Sec-WebSocket-Extensions"
			)
		);
	#endif
	testRequire(
		(Test.CallbackCall == Test.Call) &&
		(pClient != NULL) &&
		(pServer != NULL) &&
		testWsHttpTextEqual(
			xrtWsConnProtocol(pClient),
			XRT_STR_LITERAL("chat.v1")
		) &&
		testWsHttpTextEqual(
			xrtWsConnProtocol(pServer),
			XRT_STR_LITERAL("chat.v1")
		)
		#if TEST_WS_HTTP_DEFLATE
			&& (pExtensions != NULL) &&
			xrtWsConnDeflate(
				pClient,
				&ClientDeflate
			) &&
			xrtWsConnDeflate(
				pServer,
				&ServerDeflate
			) &&
			(memcmp(
				&ClientDeflate,
				&ServerDeflate,
				sizeof(ClientDeflate)
			 ) == 0)
		#endif
		,
		"WebSocket negotiated protocol or Call identity mismatch"
	);
	testWsHttpWait(
		&Test.ServerEndpoint.Messages,
		TEST_WS_HTTP_MESSAGES,
		"WebSocket server message missing"
	);
	testWsHttpWait(
		&Test.ClientEndpoint.Messages,
		TEST_WS_HTTP_MESSAGES,
		"WebSocket client echo missing"
	);
	testWsHttpWait(
		&Test.ServerEndpoint.Ping,
		1,
		"WebSocket server Ping missing"
	);
	testWsHttpWait(
		&Test.ClientEndpoint.Pong,
		1,
		"WebSocket client Pong missing"
	);
	testRequire(
		xrtNetEnginePost(
			Test.Engine,
			xrtNetWorkerIndex(
				xrtWsConnWorker(pClient)
			),
			testWsHttpCloseTask,
			&Test
		),
		"WebSocket HTTP Close task post failed"
	);
	testWsHttpWait(
		&Test.ClientEndpoint.Closed,
		1,
		"WebSocket HTTP client Close missing"
	);
	testWsHttpWait(
		&Test.ServerEndpoint.Closed,
		1,
		"WebSocket HTTP server Close missing"
	);
	testRequire(
		xrtHttpServerDrain(Test.Server),
		"WebSocket HTTP server drain failed"
	);
	testWsHttpWait(
		&Test.Shutdown,
		1,
		"WebSocket HTTP server shutdown missing"
	);
	testRequire(
		xrtHttpServerStats(Test.Server, &Stats) &&
		(Stats.Accepted == (TEST_WS_HTTP_TLS ? 2u : 1u)) &&
		(Stats.Requests == (TEST_WS_HTTP_TLS ? 2u : 1u)) &&
		(Stats.Responses == (TEST_WS_HTTP_TLS ? 2u : 1u)) &&
		(Stats.Upgraded == (TEST_WS_HTTP_TLS ? 2u : 1u)) &&
		(Stats.Connections == 0) &&
		(xrtAtomic32Load(
			&Test.HttpClose,
			XMEMORY_ACQUIRE
		 ) == 0) &&
		(xrtAtomic32Load(
			&Test.Errors,
			XMEMORY_ACQUIRE
		 ) == 0)
		#if TEST_WS_HTTP_TLS
			&& (xrtAtomic32Load(
				&Test.TlsSni,
				XMEMORY_ACQUIRE
			 ) != 0) &&
			(xrtAtomic32Load(
				&Test.TlsVerifyName,
				XMEMORY_ACQUIRE
			 ) != 0)
		#endif
		&&
		((Test.ClientEndpoint.Close.Flags &
		  XWS_CONN_CLOSE_REMOTE) == 0) &&
		((Test.ServerEndpoint.Close.Flags &
		  XWS_CONN_CLOSE_REMOTE) != 0),
		"WebSocket HTTP ownership or lifecycle mismatch"
	);

	xrtWsConnDestroy(pClient);
	xrtWsConnDestroy(pServer);
	xrtHttpResponseDestroy(Test.Response);
	xrtHttpCallDestroy(Test.Call);
	xrtHttpClientDestroy(Test.Client);
	xrtHttpServerDestroy(Test.Server);
	#if TEST_WS_HTTP_TLS
		xrtTlsVerifierRelease(pTlsVerifier);
		xrtTlsIdentityRelease(pTlsIdentity);
		xrtTlsContextRelease(pTlsContext);
	#endif
	testRequire(
		xrtNetEngineDestroy(Test.Engine),
		"WebSocket HTTP engine destroy failed"
	);
	printf(
		"[PASS] WebSocket HTTP client/server (%s)\n",
		TEST_WS_HTTP_BACKEND_NAME
	);
	return 0;
}

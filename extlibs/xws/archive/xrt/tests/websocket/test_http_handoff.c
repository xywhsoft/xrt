#include "../test.h"



/* 记录 HTTP 到 WebSocket 交接期间的缓冲、消息和关闭终态。 */
typedef struct test_ws_http_handoff {
	xatomic32 Upgraded;
	xatomic32 Rejected;
	xatomic32 Message;
	xatomic32 Closed;
	xatomic32 Shutdown;
	xatomic32 HttpErrors;
	xatomic32 WsErrors;
	#if defined(XRT_FEATURE_WEBSOCKET_SERVER_FUTURE)
		xatomicptr Future;
		xatomic32 FutureDropped;
		xatomic32 FutureEvent;
		xwsconnevents FutureEvents;
	#endif
	xwsconn* Connection;
	xwsopcode Opcode;
	size_t Buffered;
	size_t Size;
	uint8 Data[16];
	xwsconnevents WsEvents;
} test_ws_http_handoff;



/* 在截止时间前等待 Worker 发布指定终态。 */
static void testWsHttpHandoffWait(
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



/* 完整发送一段本地原始协议数据。 */
static void testWsHttpHandoffSend(
	xnetsocket Socket,
	cbytes pData,
	size_t iSize
)
{
	size_t iOffset = 0;

	while ( iOffset < iSize ) {
		size_t iSent = 0;

		testRequire(
			(xrtNetSocketSend(
				Socket,
				pData + iOffset,
				iSize - iOffset,
				&iSent
			 ) == XNET_RESULT_OK) &&
			(iSent != 0),
			"WebSocket HTTP handoff send failed"
		);
		iOffset += iSent;
	}
}



/* 比较服务端借用的 request-target 与固定测试路径。 */
static bool testWsHttpHandoffTarget(
	xstrview Target,
	cstr sExpected
)
{
	size_t iExpected = strlen(sExpected);

	return (Target.Size == iExpected) &&
		(memcmp(Target.Data, sExpected, iExpected) == 0);
}



/* 建立本地原始客户端并完整提交一份 HTTP 请求。 */
static xnetsocket testWsHttpHandoffClient(
	const xnetaddr* pAddress,
	cstr sRequest
)
{
	xnetsocket Socket = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		0
	);

	testRequire(
		(Socket != NULL) &&
		(xrtNetSocketConnect(Socket, pAddress) ==
		 XNET_RESULT_OK),
		"WebSocket HTTP raw client connect failed"
	);
	testWsHttpHandoffSend(
		Socket,
		(cbytes)sRequest,
		strlen(sRequest)
	);
	return Socket;
}



/* 发送一次独立请求并读取完整响应头。 */
static void testWsHttpHandoffResponse(
	const xnetaddr* pAddress,
	cstr sRequest,
	cstr sStatus,
	cstr sHeader1,
	cstr sHeader2
)
{
	char sResponse[1024];
	size_t iSize = 0;
	xnetsocket Socket = testWsHttpHandoffClient(
		pAddress,
		sRequest
	);
	sResponse[0] = '\0';
	while ( strstr(sResponse, "\r\n\r\n") == NULL ) {
		size_t iRead = 0;
		xnetresult Result;

		testRequire(
			iSize < (sizeof(sResponse) - 1u),
			"WebSocket HTTP rejection response exceeded storage"
		);
		Result = xrtNetSocketRecv(
			Socket,
			sResponse + iSize,
			sizeof(sResponse) - iSize - 1u,
			&iRead
		);
		testRequire(
			(Result == XNET_RESULT_OK) &&
			(iRead != 0),
			"WebSocket HTTP rejection response receive failed"
		);
		iSize += iRead;
		sResponse[iSize] = '\0';
	}
	testRequire(
		(strstr(sResponse, sStatus) != NULL) &&
		((sHeader1 == NULL) ||
		 (strstr(sResponse, sHeader1) != NULL)) &&
		((sHeader2 == NULL) ||
		 (strstr(sResponse, sHeader2) != NULL)),
		"WebSocket HTTP rejection response mismatch"
	);
	testRequire(
		xrtNetSocketClose(Socket),
		"WebSocket HTTP rejection client close failed"
	);
}



#if defined(XRT_FEATURE_WEBSOCKET_SERVER_FUTURE)

/* 等待 Request Worker 发布当前服务端 Upgrade Future。 */
static xfuture* testWsHttpHandoffFutureTake(
	test_ws_http_handoff* pState
)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	xfuture* pFuture;

	do {
		pFuture = (xfuture*)xrtAtomicPtrExchange(
			&pState->Future,
			NULL,
			XMEMORY_ACQ_REL
		);
		if ( pFuture == NULL ) {
			testRequire(
				!xrtDeadlineExpired(Deadline),
				"WebSocket server Future was not published"
			);
			xrtThreadYield();
		}
	} while ( pFuture == NULL );
	return pFuture;
}



/* 等待已取走的服务端 Connection 释放底层传输。 */
static void testWsHttpHandoffFutureClosed(
	xwsconn* pConnection
)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(10000000));

	while ( xrtWsConnState(pConnection) != XWS_CONN_CLOSED ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"WebSocket server Future connection did not close"
		);
		xrtThreadYield();
	}
}



/* 验证服务端 Future 的 Connection 事件收到独立应用数据。 */
static void testWsHttpHandoffFuturePing(
	xwsconn* pConnection,
	xbytesview Payload,
	ptr pData
)
{
	test_ws_http_handoff* pState =
		(test_ws_http_handoff*)pData;

	testRequire(
		(pState != NULL) &&
		(pConnection != NULL) &&
		(Payload.Size == 4) &&
		(memcmp(Payload.Data, "ping", 4) == 0),
		"WebSocket server Future event data mismatch"
	);
	xrtAtomic32Store(
		&pState->FutureEvent,
		1,
		XMEMORY_RELEASE
	);
}

#endif



/* 开始接收交接后的第一条文本消息。 */
static void testWsHttpHandoffMessageBegin(
	xwsconn* pConnection,
	const xwsmessageinfo* pInfo,
	ptr pData
)
{
	test_ws_http_handoff* pState =
		(test_ws_http_handoff*)pData;

	testRequire(
		(pConnection == pState->Connection) &&
		(pInfo != NULL) &&
		(pInfo->Opcode == XWS_OPCODE_TEXT) &&
		(pState->Size == 0),
		"WebSocket HTTP handoff message begin mismatch"
	);
	pState->Opcode = (xwsopcode)pInfo->Opcode;
}



/* 复制早到消息分块，验证 HTTP 缓冲没有被丢弃。 */
static void testWsHttpHandoffMessageData(
	xwsconn* pConnection,
	xbytesview Data,
	ptr pData
)
{
	test_ws_http_handoff* pState =
		(test_ws_http_handoff*)pData;

	testRequire(
		(pConnection == pState->Connection) &&
		(Data.Size <= (sizeof(pState->Data) - pState->Size)),
		"WebSocket HTTP handoff message exceeded storage"
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



/* 核对完整早到消息，并发布消费终态。 */
static void testWsHttpHandoffMessageEnd(
	xwsconn* pConnection,
	ptr pData
)
{
	test_ws_http_handoff* pState =
		(test_ws_http_handoff*)pData;

	testRequire(
		(pConnection == pState->Connection) &&
		(pState->Opcode == XWS_OPCODE_TEXT) &&
		(pState->Size == 5) &&
		(memcmp(pState->Data, "early", 5) == 0),
		"WebSocket HTTP handoff lost the early frame"
	);
	xrtAtomic32Store(&pState->Message, 1, XMEMORY_RELEASE);
}



/* 记录 WebSocket 结构化错误。 */
static void testWsHttpHandoffWsError(
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_ws_http_handoff* pState =
		(test_ws_http_handoff*)pData;

	(void)pConnection;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pState->WsErrors,
		1,
		XMEMORY_ACQ_REL
	);
}



/* 验证对端标准 Close 经过交接后的 Connection 正常发布。 */
static void testWsHttpHandoffClose(
	xwsconn* pConnection,
	const xwsconnclose* pClose,
	ptr pData
)
{
	test_ws_http_handoff* pState =
		(test_ws_http_handoff*)pData;

	testRequire(
		(pConnection == pState->Connection) &&
		(pClose != NULL) &&
		(pClose->RemoteCode == XWS_CLOSE_NORMAL),
		"WebSocket HTTP handoff close mismatch"
	);
	xrtAtomic32Store(&pState->Closed, 1, XMEMORY_RELEASE);
}



/* 接管成功的 WebSocket，并记录交接时仍位于 TCP 中的早到字节。 */
static void testWsHttpHandoffUpgraded(
	xhttpconn* pHttp,
	xnetresult Result,
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_ws_http_handoff* pState =
		(test_ws_http_handoff*)pData;
	xnetstream* pTcp;

	(void)pHttp;
	testRequire(
		(Result == XNET_RESULT_OK) &&
		(pConnection != NULL) &&
		(pError == NULL),
		"WebSocket HTTP handoff Upgrade failed"
	);
	pTcp = xrtWsConnTcp(pConnection);
	testRequire(
		pTcp != NULL,
		"WebSocket HTTP handoff did not retain TCP"
	);
	pState->Connection = pConnection;
	pState->Buffered = xrtNetStreamAvailable(pTcp);
	xrtAtomic32Store(&pState->Upgraded, 1, XMEMORY_RELEASE);
}



/* 用公开服务端 Helper 把当前 HTTP 请求升级为 WebSocket。 */
static void testWsHttpHandoffRequest(
	xhttpserver* pServer,
	xhttpconn* pHttp,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_ws_http_handoff* pState =
		(test_ws_http_handoff*)pData;
	xwsserverconfig Config;
	xstrview Target = xrtHttpServerRequestTarget(pRequest);
	xnetresult Result;

	(void)pServer;
	#if defined(XRT_FEATURE_WEBSOCKET_SERVER_FUTURE)
		if ( testWsHttpHandoffTarget(Target, "/future") ||
			testWsHttpHandoffTarget(Target, "/future-cancel") ||
			testWsHttpHandoffTarget(Target, "/future-drop") ) {
			xfuture* pFuture;
			ptr pPrevious;
			bool bDrop = testWsHttpHandoffTarget(
				Target,
				"/future-drop"
			);

			testRequire(
				(xrtHttpServerRequestFlags(pRequest) &
				 XHTTP_SERVER_REQUEST_UPGRADE) != 0,
				"WebSocket server Future request omitted Upgrade"
			);
			xrtWsServerConfigInit(&Config);
			pFuture = xrtWsUpgradeAsync(
				pHttp,
				&Config,
				&pState->FutureEvents,
				pState
			);
			testRequire(
				pFuture != NULL,
				"WebSocket server Future submission failed"
			);
			if ( testWsHttpHandoffTarget(
				Target,
				"/future-cancel"
			) ) {
				testRequire(
					xrtFutureCancel(pFuture),
					"WebSocket server Future cancel failed"
				);
			}
			if ( bDrop ) {
				xrtFutureDestroy(pFuture);
				(void)xrtAtomic32FetchAdd(
					&pState->FutureDropped,
					1,
					XMEMORY_RELEASE
				);
				return;
			}
			pPrevious = xrtAtomicPtrExchange(
				&pState->Future,
				pFuture,
				XMEMORY_ACQ_REL
			);
			testRequire(
				pPrevious == NULL,
				"WebSocket server Future slot was occupied"
			);
			return;
		}
	#endif
	if ( !testWsHttpHandoffTarget(Target, "/handoff") ) {
		const xerror* pError = NULL;

		if ( !testWsHttpHandoffTarget(Target, "/internal") ) {
			xwsserverhandshake Handshake;

			xrtWsServerConfigInit(&Config);
			testRequire(
				!xrtWsServerCheck(
					pRequest,
					&Config,
					&Handshake
				),
				"WebSocket rejection request unexpectedly passed"
			);
			pError = xrtGetError();
		}
		Result = xrtWsServerReject(pHttp, pError);
		testRequire(
			Result == XNET_RESULT_OK,
			"WebSocket rejection response submission failed"
		);
		xrtClearError();
		(void)xrtAtomic32FetchAdd(
			&pState->Rejected,
			1,
			XMEMORY_RELEASE
		);
		return;
	}
	testRequire(
		(xrtHttpServerRequestFlags(pRequest) &
		 XHTTP_SERVER_REQUEST_UPGRADE) != 0,
		"WebSocket HTTP handoff request omitted Upgrade"
	);
	xrtWsServerConfigInit(&Config);
	Result = xrtWsUpgrade(
		pHttp,
		&Config,
		&pState->WsEvents,
		pState,
		testWsHttpHandoffUpgraded,
		pState
	);
	testRequire(
		Result == XNET_RESULT_OK,
		"WebSocket HTTP handoff submission failed"
	);
}



/* HTTP Connection 不应在已经转移所有权后发布普通关闭或错误。 */
static void testWsHttpHandoffHttpClose(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_ws_http_handoff* pState =
		(test_ws_http_handoff*)pData;

	(void)pServer;
	(void)pConnection;
	(void)Result;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pState->HttpErrors,
		1,
		XMEMORY_ACQ_REL
	);
}



/* 记录 HTTP Server 独立于转交会话的关闭终态。 */
static void testWsHttpHandoffShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_ws_http_handoff* pState =
		(test_ws_http_handoff*)pData;

	testRequire(
		xrtHttpServerState(pServer) == XHTTP_SERVER_CLOSED,
		"WebSocket HTTP handoff server state mismatch"
	);
	xrtAtomic32Store(&pState->Shutdown, 1, XMEMORY_RELEASE);
}



/* 验证服务端 HTTP Upgrade 保留并消费同一接收块内的早到帧。 */
int main(void)
{
	#if defined(XRT_FEATURE_WEBSOCKET_SERVER_FUTURE)
		static const char FutureRequest[] =
			"GET /future HTTP/1.1\r\n"
			"Host: handoff.test\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Version: 13\r\n"
			"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n";
		static const char CancelRequest[] =
			"GET /future-cancel HTTP/1.1\r\n"
			"Host: handoff.test\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Version: 13\r\n"
			"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n";
		static const char DropRequest[] =
			"GET /future-drop HTTP/1.1\r\n"
			"Host: handoff.test\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Version: 13\r\n"
			"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n";
	#endif
	static const char MethodRequest[] =
		"POST /reject HTTP/1.1\r\n"
		"Host: handoff.test\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade, close\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
		"Content-Length: 0\r\n\r\n";
	static const char VersionRequest[] =
		"GET /reject HTTP/1.1\r\n"
		"Host: handoff.test\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade, close\r\n"
		"Sec-WebSocket-Version: 12\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n";
	static const char InvalidRequest[] =
		"GET /reject HTTP/1.1\r\n"
		"Host: handoff.test\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade, close\r\n"
		"Sec-WebSocket-Version: 13\r\n\r\n";
	static const char InternalRequest[] =
		"GET /internal HTTP/1.1\r\n"
		"Host: handoff.test\r\n"
		"Connection: close\r\n\r\n";
	static const uint8 Packet[] =
		"GET /handoff HTTP/1.1\r\n"
		"Host: handoff.test\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
		"\r\n"
		"\x81\x85\x01\x02\x03\x04"
		"\x64\x63\x71\x68\x78";
	static const uint8 CloseFrame[] = {
		0x88, 0x82, 0x05, 0x06, 0x07, 0x08, 0x06, 0xee
	};
	#if defined(XRT_FEATURE_WEBSOCKET_SERVER_FUTURE)
		static const uint8 FuturePingFrame[] = {
			0x89, 0x84, 0x01, 0x02, 0x03, 0x04,
			0x71, 0x6b, 0x6d, 0x63
		};
	#endif
	test_ws_http_handoff State;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents HttpEvents;
	xnetengine* pEngine;
	xhttpserver* pServer;
	xnetaddr Address;
	xnetsocket Client;
	#if defined(XRT_FEATURE_WEBSOCKET_SERVER_FUTURE)
		xnetsocket FutureClient;
		xfuture* pFuture;
		xwsopenresult* pFutureResult;
		xwsconn* pFutureConnection;
	#endif

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.Upgraded, 0);
	xrtAtomic32Init(&State.Rejected, 0);
	xrtAtomic32Init(&State.Message, 0);
	xrtAtomic32Init(&State.Closed, 0);
	xrtAtomic32Init(&State.Shutdown, 0);
	xrtAtomic32Init(&State.HttpErrors, 0);
	xrtAtomic32Init(&State.WsErrors, 0);
	#if defined(XRT_FEATURE_WEBSOCKET_SERVER_FUTURE)
		xrtAtomicPtrInit(&State.Future, NULL);
		xrtAtomic32Init(&State.FutureDropped, 0);
		xrtAtomic32Init(&State.FutureEvent, 0);
		State.FutureEvents.Ping =
			testWsHttpHandoffFuturePing;
	#endif
	State.WsEvents.MessageBegin = testWsHttpHandoffMessageBegin;
	State.WsEvents.MessageData = testWsHttpHandoffMessageData;
	State.WsEvents.MessageEnd = testWsHttpHandoffMessageEnd;
	State.WsEvents.Error = testWsHttpHandoffWsError;
	State.WsEvents.Close = testWsHttpHandoffClose;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) && xrtNetEngineStart(pEngine),
		"WebSocket HTTP handoff engine start failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"WebSocket HTTP handoff address setup failed"
	);
	ServerConfig.Network.Listen.Stream.ReadSize = 4096;
	ServerConfig.Network.Listen.Stream.ReadLimit = 8192;
	xrtHttpServerEventsInit(&HttpEvents);
	HttpEvents.Request = testWsHttpHandoffRequest;
	HttpEvents.Close = testWsHttpHandoffHttpClose;
	HttpEvents.Shutdown = testWsHttpHandoffShutdown;
	HttpEvents.Data = &State;
	pServer = xrtHttpServerStart(
		pEngine,
		&ServerConfig,
		&HttpEvents
	);
	testRequire(
		(pServer != NULL) &&
		xrtHttpServerLocal(pServer, 0, &Address),
		"WebSocket HTTP handoff server start failed"
	);
	Client = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		0
	);
	testRequire(
		(Client != NULL) &&
		(xrtNetSocketConnect(Client, &Address) ==
		 XNET_RESULT_OK),
		"WebSocket HTTP handoff client connect failed"
	);
	testWsHttpHandoffSend(
		Client,
		Packet,
		sizeof(Packet) - 1u
	);
	testWsHttpHandoffWait(
		&State.Upgraded,
		1,
		"WebSocket HTTP handoff callback missing"
	);
	testWsHttpHandoffWait(
		&State.Message,
		1,
		"WebSocket HTTP handoff early message missing"
	);
	testRequire(
		(State.Buffered >= 11) &&
		(xrtAtomic32Load(
			&State.HttpErrors,
			XMEMORY_ACQUIRE
		 ) == 0) &&
		(xrtAtomic32Load(
			&State.WsErrors,
			XMEMORY_ACQUIRE
		 ) == 0),
		"WebSocket HTTP handoff did not preserve buffered bytes"
	);
	testWsHttpHandoffResponse(
		&Address,
		MethodRequest,
		"HTTP/1.1 405 Method Not Allowed",
		"Allow: GET",
		NULL
	);
	testWsHttpHandoffResponse(
		&Address,
		VersionRequest,
		"HTTP/1.1 426 Upgrade Required",
		"Upgrade: websocket",
		"Sec-WebSocket-Version: 13"
	);
	testWsHttpHandoffResponse(
		&Address,
		InvalidRequest,
		"HTTP/1.1 400 Bad Request",
		NULL,
		NULL
	);
	testWsHttpHandoffResponse(
		&Address,
		InternalRequest,
		"HTTP/1.1 500 Internal Server Error",
		NULL,
		NULL
	);
	testWsHttpHandoffWait(
		&State.Rejected,
		4,
		"WebSocket HTTP rejection callbacks missing"
	);
	#if defined(XRT_FEATURE_WEBSOCKET_SERVER_FUTURE)
		FutureClient = testWsHttpHandoffClient(
			&Address,
			FutureRequest
		);
		testWsHttpHandoffSend(
			FutureClient,
			FuturePingFrame,
			sizeof(FuturePingFrame)
		);
		pFuture = testWsHttpHandoffFutureTake(&State);
		testRequire(
			(xrtFutureWaitFor(
				pFuture,
				UINT64_C(10000000)
			 ) == XWAIT_OK) &&
			(xrtFutureState(pFuture) == XFUTURE_RESOLVED),
			"WebSocket server Future did not resolve"
		);
		pFutureResult = (xwsopenresult*)xrtFutureValue(
			pFuture
		);
		pFutureConnection = xrtWsOpenResultTakeConnection(
			pFutureResult
		);
		testRequire(
			(pFutureResult != NULL) &&
			(pFutureConnection != NULL) &&
			(xrtWsOpenResultConnection(pFutureResult) == NULL),
			"WebSocket server Future ownership mismatch"
		);
		testWsHttpHandoffWait(
			&State.FutureEvent,
			1,
			"WebSocket server Future event data missing"
		);
		xrtFutureDestroy(pFuture);
		testWsHttpHandoffSend(
			FutureClient,
			CloseFrame,
			sizeof(CloseFrame)
		);
		testRequire(
			xrtNetSocketClose(FutureClient),
			"WebSocket server Future client close failed"
		);
		testWsHttpHandoffFutureClosed(pFutureConnection);
		xrtWsConnDestroy(pFutureConnection);

		FutureClient = testWsHttpHandoffClient(
			&Address,
			CancelRequest
		);
		pFuture = testWsHttpHandoffFutureTake(&State);
		testRequire(
			(xrtFutureWaitFor(
				pFuture,
				UINT64_C(10000000)
			 ) == XWAIT_OK) &&
			(xrtFutureState(pFuture) == XFUTURE_CANCELLED),
			"WebSocket server Future cancellation mismatch"
		);
		xrtFutureDestroy(pFuture);
		testRequire(
			xrtNetSocketClose(FutureClient),
			"WebSocket cancelled Future client close failed"
		);

		FutureClient = testWsHttpHandoffClient(
			&Address,
			DropRequest
		);
		testWsHttpHandoffWait(
			&State.FutureDropped,
			1,
			"WebSocket dropped server Future was not released"
		);
		testRequire(
			xrtNetSocketClose(FutureClient),
			"WebSocket dropped Future client close failed"
		);
	#endif
	testRequire(
		xrtHttpServerDrain(pServer),
		"WebSocket HTTP handoff server drain failed"
	);
	testWsHttpHandoffWait(
		&State.Shutdown,
		1,
		"WebSocket HTTP handoff server shutdown missing"
	);
	testRequire(
		xrtWsConnState(State.Connection) == XWS_CONN_OPEN,
		"HTTP server retained transferred WebSocket lifetime"
	);
	testWsHttpHandoffSend(
		Client,
		CloseFrame,
		sizeof(CloseFrame)
	);
	testWsHttpHandoffWait(
		&State.Closed,
		1,
		"WebSocket HTTP handoff close missing"
	);
	testRequire(
		(xrtAtomic32Load(
			&State.WsErrors,
			XMEMORY_ACQUIRE
		 ) == 0) &&
		xrtNetSocketClose(Client),
		"WebSocket HTTP handoff close failed"
	);
	xrtWsConnDestroy(State.Connection);
	xrtHttpServerDestroy(pServer);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"WebSocket HTTP handoff engine destroy failed"
	);
	printf("[PASS] WebSocket HTTP buffered handoff (select)\n");
	return 0;
}

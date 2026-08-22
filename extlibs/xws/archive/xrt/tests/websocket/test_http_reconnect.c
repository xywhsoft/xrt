#include "../test.h"



#ifndef TEST_WS_RECONNECT_BACKEND
	#define TEST_WS_RECONNECT_BACKEND XNET_PORT_SELECT
#endif

#ifndef TEST_WS_RECONNECT_BACKEND_NAME
	#define TEST_WS_RECONNECT_BACKEND_NAME "select"
#endif

#ifndef TEST_WS_RECONNECT_ROUNDS
	#define TEST_WS_RECONNECT_ROUNDS 100u
#endif

#define TEST_WS_RECONNECT_PROTOCOL "xrt.stress"
#define TEST_WS_RECONNECT_LIMIT ((size_t)1024u)
#define TEST_WS_RECONNECT_OVERSIZE ((size_t)1536u)
#define TEST_WS_RECONNECT_TIMEOUT UINT64_C(10000000)



typedef struct test_ws_reconnect_endpoint {
	xwsrole Role;
	xatomic32 Opened;
	xatomic32 Messages;
	xatomic32 LimitErrors;
	xatomic32 Closed;
	xwsopcode Opcode;
	size_t Size;
	uint8 Data[256];
	xwsconnclose Close;
} test_ws_reconnect_endpoint;



typedef struct test_ws_reconnect_round {
	bool Limit;
	char Text[128];
	size_t TextSize;
	uint8 Oversized[TEST_WS_RECONNECT_OVERSIZE];
	xatomicptr ClientConnection;
	xatomicptr ServerConnection;
	xhttpcall* Call;
	xhttpresponse* Response;
	test_ws_reconnect_endpoint Client;
	test_ws_reconnect_endpoint Server;
} test_ws_reconnect_round;



typedef struct test_ws_reconnect {
	xnetengine* Engine;
	xhttpserver* Server;
	xhttpclient* Client;
	xatomicptr Current;
	xatomic32 HttpClose;
	xatomic32 Shutdown;
	xwsconnevents Events;
} test_ws_reconnect;



/* 比较借用文本视图和以零结尾的常量。 */
static bool testWsReconnectTextEqual(
	xstrview Text,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		((iSize == 0) ||
		 (memcmp(Text.Data, sExpected, iSize) == 0));
}



/* 在截止时间前等待一次跨 Worker 状态发布。 */
static void testWsReconnectWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(
		TEST_WS_RECONNECT_TIMEOUT
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



/* 按角色返回当前 Connection 的独立消息暂存。 */
static test_ws_reconnect_endpoint* testWsReconnectEndpoint(
	test_ws_reconnect_round* pRound,
	xwsconn* pConnection
)
{
	return xrtWsConnRole(pConnection) == XWS_ROLE_CLIENT ?
		&pRound->Client : &pRound->Server;
}



/* 初始化一轮的原子状态、消息内容和超限噪声。 */
static void testWsReconnectRoundInit(
	test_ws_reconnect_round* pRound,
	uint32 iRound
)
{
	uint32 iRandom = iRound + UINT32_C(1);
	int iLength;

	memset(pRound, 0, sizeof(*pRound));
	pRound->Limit = ((iRound + 1u) % 10u) == 0u;
	iLength = snprintf(
		pRound->Text,
		sizeof(pRound->Text),
		"round-%u-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
		(unsigned int)iRound
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(pRound->Text)),
		"WebSocket reconnect text fixture overflowed"
	);
	pRound->TextSize = (size_t)iLength;
	for ( size_t i = 0; i < sizeof(pRound->Oversized); i++ ) {
		iRandom ^= iRandom << 13u;
		iRandom ^= iRandom >> 17u;
		iRandom ^= iRandom << 5u;
		pRound->Oversized[i] = (uint8)(iRandom >> 24u);
	}
	xrtAtomicPtrInit(&pRound->ClientConnection, NULL);
	xrtAtomicPtrInit(&pRound->ServerConnection, NULL);
	pRound->Client.Role = XWS_ROLE_CLIENT;
	pRound->Server.Role = XWS_ROLE_SERVER;
	xrtAtomic32Init(&pRound->Client.Opened, 0);
	xrtAtomic32Init(&pRound->Client.Messages, 0);
	xrtAtomic32Init(&pRound->Client.LimitErrors, 0);
	xrtAtomic32Init(&pRound->Client.Closed, 0);
	xrtAtomic32Init(&pRound->Server.Opened, 0);
	xrtAtomic32Init(&pRound->Server.Messages, 0);
	xrtAtomic32Init(&pRound->Server.LimitErrors, 0);
	xrtAtomic32Init(&pRound->Server.Closed, 0);
}



/* 开始一条逻辑消息并清空对应端点的有界暂存。 */
static void testWsReconnectMessageBegin(
	xwsconn* pConnection,
	const xwsmessageinfo* pInfo,
	ptr pData
)
{
	test_ws_reconnect_round* pRound =
		(test_ws_reconnect_round*)pData;
	test_ws_reconnect_endpoint* pEndpoint =
		testWsReconnectEndpoint(pRound, pConnection);

	testRequire(
		pInfo != NULL,
		"WebSocket reconnect message info is null"
	);
	pEndpoint->Opcode = (xwsopcode)pInfo->Opcode;
	pEndpoint->Size = 0;
}



/* 汇总流式消息分块，普通回显不得超过固定测试容量。 */
static void testWsReconnectMessageData(
	xwsconn* pConnection,
	xbytesview Data,
	ptr pData
)
{
	test_ws_reconnect_round* pRound =
		(test_ws_reconnect_round*)pData;
	test_ws_reconnect_endpoint* pEndpoint =
		testWsReconnectEndpoint(pRound, pConnection);

	testRequire(
		Data.Size <=
			(sizeof(pEndpoint->Data) -
			 pEndpoint->Size),
		"WebSocket reconnect message exceeded fixture storage"
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



/* 服务端压缩回显，客户端收到完整消息后发起正常关闭。 */
static void testWsReconnectMessageEnd(
	xwsconn* pConnection,
	ptr pData
)
{
	test_ws_reconnect_round* pRound =
		(test_ws_reconnect_round*)pData;
	test_ws_reconnect_endpoint* pEndpoint =
		testWsReconnectEndpoint(pRound, pConnection);

	testRequire(
		!pRound->Limit &&
		(pEndpoint->Opcode == XWS_OPCODE_TEXT) &&
		(pEndpoint->Size == pRound->TextSize) &&
		(memcmp(
			pEndpoint->Data,
			pRound->Text,
			pRound->TextSize
		 ) == 0),
		"WebSocket reconnect text payload mismatch"
	);
	xrtAtomic32Store(
		&pEndpoint->Messages,
		1,
		XMEMORY_RELEASE
	);
	if ( pEndpoint->Role == XWS_ROLE_SERVER ) {
		testRequire(
			xrtWsConnTextCompressed(
				pConnection,
				(xstrview) {
					pRound->Text,
					pRound->TextSize
				}
			) == XNET_RESULT_OK,
			"WebSocket reconnect server echo failed"
		);
	} else {
		testRequire(
			xrtWsConnClose(
				pConnection,
				XWS_CLOSE_NORMAL,
				XRT_STR_LITERAL("stress")
			) == XNET_RESULT_OK,
			"WebSocket reconnect normal Close failed"
		);
	}
}



/* 超限轮次只允许服务端发布消息上限错误。 */
static void testWsReconnectError(
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_ws_reconnect_round* pRound =
		(test_ws_reconnect_round*)pData;
	test_ws_reconnect_endpoint* pEndpoint =
		testWsReconnectEndpoint(pRound, pConnection);

	testRequire(
		pRound->Limit &&
		(pEndpoint->Role == XWS_ROLE_SERVER) &&
		(pError != NULL) &&
		(xrtErrorKind(pError) == XERR_RANGE) &&
		(xrtErrorCode(pError) == XWS_CONN_ERROR_MESSAGE),
		"WebSocket reconnect published unexpected session error"
	);
	xrtAtomic32Store(
		&pEndpoint->LimitErrors,
		1,
		XMEMORY_RELEASE
	);
}



/* 保存本轮两端独立的关闭快照。 */
static void testWsReconnectClose(
	xwsconn* pConnection,
	const xwsconnclose* pClose,
	ptr pData
)
{
	test_ws_reconnect_round* pRound =
		(test_ws_reconnect_round*)pData;
	test_ws_reconnect_endpoint* pEndpoint =
		testWsReconnectEndpoint(pRound, pConnection);

	testRequire(
		(pClose != NULL) &&
		(xrtWsConnState(pConnection) == XWS_CONN_CLOSED),
		"WebSocket reconnect Close snapshot missing"
	);
	pEndpoint->Close = *pClose;
	xrtAtomic32Store(
		&pEndpoint->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 服务端 Upgrade 完成后验证子协议与压缩并发布 Connection。 */
static void testWsReconnectServerDone(
	xhttpconn* pHttp,
	xnetresult Result,
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_ws_reconnect_round* pRound =
		(test_ws_reconnect_round*)pData;
	xwsdeflate Deflate;

	(void)pHttp;
	(void)pError;
	testRequire(
		(Result == XNET_RESULT_OK) &&
		(pConnection != NULL) &&
		testWsReconnectTextEqual(
			xrtWsConnProtocol(pConnection),
			TEST_WS_RECONNECT_PROTOCOL
		) && xrtWsConnDeflate(pConnection, &Deflate),
		"WebSocket reconnect server negotiation failed"
	);
	xrtAtomicPtrStore(
		&pRound->ServerConnection,
		pConnection,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pRound->Server.Opened,
		1,
		XMEMORY_RELEASE
	);
}



/* 当前轮次的完整 HTTP 请求升级为受限 WebSocket 会话。 */
static void testWsReconnectRequest(
	xhttpserver* pServer,
	xhttpconn* pHttp,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_ws_reconnect* pTest =
		(test_ws_reconnect*)pData;
	test_ws_reconnect_round* pRound =
		(test_ws_reconnect_round*)xrtAtomicPtrLoad(
			&pTest->Current,
			XMEMORY_ACQUIRE
		);
	xwsserverconfig Config;

	(void)pServer;
	testRequire(
		(pRound != NULL) &&
		testWsReconnectTextEqual(
			xrtHttpServerRequestTarget(pRequest),
			"/stress"
		),
		"WebSocket reconnect request target mismatch"
	);
	xrtWsServerConfigInit(&Config);
	Config.Protocols =
		XRT_STR_LITERAL(TEST_WS_RECONNECT_PROTOCOL);
	Config.EnableDeflate = true;
	Config.RequireDeflate = true;
	Config.Connection.MessageLimit =
		TEST_WS_RECONNECT_LIMIT;
	Config.Connection.FrameLimit =
		TEST_WS_RECONNECT_OVERSIZE;
	Config.Connection.CloseTimeout = UINT64_C(1000000);
	testRequire(
		xrtWsUpgrade(
		pHttp,
		&Config,
		&pTest->Events,
		pRound,
		testWsReconnectServerDone,
		pRound
		) == XNET_RESULT_OK,
		"WebSocket reconnect server Upgrade failed"
	);
}



/* Upgrade 后 HTTP 层不得再发布 Connection Close。 */
static void testWsReconnectHttpClose(
	xhttpserver* pServer,
	xhttpconn* pHttp,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_ws_reconnect* pTest =
		(test_ws_reconnect*)pData;

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



/* 压力路径不允许 HTTP Server 发布错误。 */
static void testWsReconnectHttpError(
	xhttpserver* pServer,
	xhttpconn* pHttp,
	const xerror* pError,
	ptr pData
)
{
	(void)pServer;
	(void)pHttp;
	(void)pData;
	testRequire(
		pError == NULL,
		"WebSocket reconnect HTTP server error"
	);
}



/* HTTP Server 完成排空后发布销毁门禁。 */
static void testWsReconnectShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_ws_reconnect* pTest =
		(test_ws_reconnect*)pData;

	(void)pServer;
	xrtAtomic32Store(
		&pTest->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



/* 客户端握手完成后发送普通压缩消息或超限二进制消息。 */
static void testWsReconnectClientDone(
	xhttpcall* pCall,
	xnetresult Result,
	xwsconn* pConnection,
	xhttpresponse* pResponse,
	const xerror* pError,
	ptr pData
)
{
	test_ws_reconnect_round* pRound =
		(test_ws_reconnect_round*)pData;
	xwsdeflate Deflate;
	xnetresult Send;

	(void)pCall;
	(void)pError;
	testRequire(
		(Result == XNET_RESULT_OK) &&
		(pConnection != NULL) &&
		(pResponse != NULL) &&
		(xrtHttpResponseStatus(pResponse) ==
		 XHTTP_STATUS_SWITCHING_PROTOCOLS) &&
		testWsReconnectTextEqual(
			xrtWsConnProtocol(pConnection),
			TEST_WS_RECONNECT_PROTOCOL
		) && xrtWsConnDeflate(pConnection, &Deflate),
		"WebSocket reconnect client negotiation failed"
	);
	pRound->Response = pResponse;
	xrtAtomicPtrStore(
		&pRound->ClientConnection,
		pConnection,
		XMEMORY_RELEASE
	);
	Send = pRound->Limit ?
		xrtWsConnBinary(
			pConnection,
			(xbytesview) {
				pRound->Oversized,
				sizeof(pRound->Oversized)
			}
		) :
		xrtWsConnTextCompressed(
			pConnection,
			(xstrview) {
				pRound->Text,
				pRound->TextSize
			}
		);
	testRequire(
		Send == XNET_RESULT_OK,
		"WebSocket reconnect client send failed"
	);
	xrtAtomic32Store(
		&pRound->Client.Opened,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证一轮正常关闭或 1009 超限关闭的完整契约。 */
static void testWsReconnectRoundCheck(
	test_ws_reconnect_round* pRound
)
{
	xwsconn* pClient = (xwsconn*)xrtAtomicPtrLoad(
		&pRound->ClientConnection,
		XMEMORY_ACQUIRE
	);
	xwsconn* pServer = (xwsconn*)xrtAtomicPtrLoad(
		&pRound->ServerConnection,
		XMEMORY_ACQUIRE
	);

	testRequire(
		(pClient != NULL) &&
		(pServer != NULL),
		"WebSocket reconnect lost a Connection reference"
	);
	if ( pRound->Limit ) {
		bool bValid =
			(xrtAtomic32Load(
				&pRound->Server.LimitErrors,
				XMEMORY_ACQUIRE
			 ) == 1) &&
			(pRound->Server.Close.LocalCode ==
			 XWS_CLOSE_TOO_BIG) &&
			(pRound->Server.Close.RemoteCode == 0) &&
			(pRound->Server.Close.Transport ==
			 XNET_RESULT_OK) &&
			((pRound->Server.Close.Flags &
			  XWS_CONN_CLOSE_SENT) != 0) &&
			((pRound->Server.Close.Flags &
			  XWS_CONN_CLOSE_RECEIVED) == 0) &&
			((pRound->Server.Close.Flags &
			  XWS_CONN_CLOSE_CLEAN) == 0) &&
			(pRound->Client.Close.RemoteCode ==
			 XWS_CLOSE_TOO_BIG) &&
			((pRound->Client.Close.Flags &
			  XWS_CONN_CLOSE_CLEAN) != 0) &&
			(xrtAtomic32Load(
				&pRound->Client.Messages,
				XMEMORY_ACQUIRE
			 ) == 0) &&
			(xrtAtomic32Load(
				&pRound->Server.Messages,
				XMEMORY_ACQUIRE
			 ) == 0);

		/* 失败时打印两端完整终态，便于定位高负载下的关闭竞态。 */
		if ( !bValid ) {
			fprintf(
				stderr,
				"reconnect 1009: error=%u server(flags=%u transport=%d local=%u remote=%u messages=%u) client(flags=%u transport=%d local=%u remote=%u messages=%u)\n",
				(unsigned int)xrtAtomic32Load(
					&pRound->Server.LimitErrors,
					XMEMORY_ACQUIRE
				),
				(unsigned int)pRound->Server.Close.Flags,
				(int)pRound->Server.Close.Transport,
				(unsigned int)pRound->Server.Close.LocalCode,
				(unsigned int)pRound->Server.Close.RemoteCode,
				(unsigned int)xrtAtomic32Load(
					&pRound->Server.Messages,
					XMEMORY_ACQUIRE
				),
				(unsigned int)pRound->Client.Close.Flags,
				(int)pRound->Client.Close.Transport,
				(unsigned int)pRound->Client.Close.LocalCode,
				(unsigned int)pRound->Client.Close.RemoteCode,
				(unsigned int)xrtAtomic32Load(
					&pRound->Client.Messages,
					XMEMORY_ACQUIRE
				)
			);
		}
		testRequire(
			bValid,
			"WebSocket reconnect 1009 contract mismatch"
		);
	} else {
		testRequire(
			(xrtAtomic32Load(
				&pRound->Client.Messages,
				XMEMORY_ACQUIRE
			 ) == 1) &&
			(xrtAtomic32Load(
				&pRound->Server.Messages,
				XMEMORY_ACQUIRE
			 ) == 1) &&
			(pRound->Client.Close.LocalCode ==
			 XWS_CLOSE_NORMAL) &&
			(pRound->Server.Close.RemoteCode ==
			 XWS_CLOSE_NORMAL) &&
			((pRound->Client.Close.Flags &
			  XWS_CONN_CLOSE_CLEAN) != 0) &&
			((pRound->Server.Close.Flags &
			  XWS_CONN_CLOSE_CLEAN) != 0),
			"WebSocket reconnect normal contract mismatch"
		);
	}
	xrtWsConnDestroy(pClient);
	xrtWsConnDestroy(pServer);
	xrtHttpResponseDestroy(pRound->Response);
	xrtHttpCallDestroy(pRound->Call);
}



/* 执行可配置轮数的 HTTP Upgrade 重连与超限压力。 */
static void testWsReconnectRun(uint32 iRounds)
{
	test_ws_reconnect Test;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents ServerEvents;
	xhttpclientconfig ClientConfig;
	xhttpserverstats Stats;
	xnetaddr Address;
	char sUrl[128];
	int iLength;

	memset(&Test, 0, sizeof(Test));
	xrtAtomicPtrInit(&Test.Current, NULL);
	xrtAtomic32Init(&Test.HttpClose, 0);
	xrtAtomic32Init(&Test.Shutdown, 0);
	memset(&Test.Events, 0, sizeof(Test.Events));
	Test.Events.MessageBegin = testWsReconnectMessageBegin;
	Test.Events.MessageData = testWsReconnectMessageData;
	Test.Events.MessageEnd = testWsReconnectMessageEnd;
	Test.Events.Error = testWsReconnectError;
	Test.Events.Close = testWsReconnectClose;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_WS_RECONNECT_BACKEND;
	EngineConfig.Workers = 2;
	Test.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(Test.Engine != NULL) &&
		xrtNetEngineStart(Test.Engine),
		"WebSocket reconnect engine start failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"WebSocket reconnect listen address failed"
	);
	xrtHttpServerEventsInit(&ServerEvents);
	ServerEvents.Request = testWsReconnectRequest;
	ServerEvents.Close = testWsReconnectHttpClose;
	ServerEvents.Error = testWsReconnectHttpError;
	ServerEvents.Shutdown = testWsReconnectShutdown;
	ServerEvents.Data = &Test;
	Test.Server = xrtHttpServerStart(
		Test.Engine,
		&ServerConfig,
		&ServerEvents
	);
	testRequire(
		(Test.Server != NULL) &&
		xrtHttpServerLocal(Test.Server, 0, &Address),
		"WebSocket reconnect server start failed"
	);
	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Dial.MaxAttempts = 1;
	Test.Client = xrtHttpClientCreate(
		Test.Engine,
		&ClientConfig
	);
	testRequire(
		Test.Client != NULL,
		"WebSocket reconnect client create failed"
	);
	iLength = snprintf(
		sUrl,
		sizeof(sUrl),
		"ws://127.0.0.1:%u/stress",
		(unsigned int)Address.Port
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(sUrl)),
		"WebSocket reconnect URL overflowed"
	);

	for ( uint32 iRound = 0; iRound < iRounds; iRound++ ) {
		test_ws_reconnect_round Round;
		xwsclientconfig WsConfig;

		testWsReconnectRoundInit(&Round, iRound);
		xrtAtomicPtrStore(
			&Test.Current,
			&Round,
			XMEMORY_RELEASE
		);
		xrtWsClientConfigInit(&WsConfig);
		WsConfig.Protocols =
			XRT_STR_LITERAL(TEST_WS_RECONNECT_PROTOCOL);
		WsConfig.EnableDeflate = true;
		WsConfig.RequireDeflate = true;
		WsConfig.Deflate.Flags =
			XWS_DEFLATE_SERVER_NO_CONTEXT;
		WsConfig.Connection.CloseTimeout =
			UINT64_C(1000000);
		Round.Call = xrtWsConnect(
			Test.Client,
			(xstrview) { sUrl, (size_t)iLength },
			&WsConfig,
			&Test.Events,
			&Round,
			testWsReconnectClientDone,
			&Round
		);
		testRequire(
			Round.Call != NULL,
			"WebSocket reconnect client submit failed"
		);
		testWsReconnectWait(
			&Round.Client.Opened,
			1,
			"WebSocket reconnect client did not open"
		);
		testWsReconnectWait(
			&Round.Server.Opened,
			1,
			"WebSocket reconnect server did not open"
		);
		testWsReconnectWait(
			&Round.Client.Closed,
			1,
			"WebSocket reconnect client did not close"
		);
		testWsReconnectWait(
			&Round.Server.Closed,
			1,
			"WebSocket reconnect server did not close"
		);
		testWsReconnectRoundCheck(&Round);
		xrtAtomicPtrStore(
			&Test.Current,
			NULL,
			XMEMORY_RELEASE
		);
		testRequire(
			xrtHttpServerStats(Test.Server, &Stats) &&
			(Stats.Accepted == (uint64)iRound + 1u) &&
			(Stats.Requests == (uint64)iRound + 1u) &&
			(Stats.Responses == (uint64)iRound + 1u) &&
			(Stats.Upgraded == (uint64)iRound + 1u) &&
			(Stats.Connections == 0),
			"WebSocket reconnect HTTP ownership leaked"
		);
	}

	testRequire(
		xrtHttpServerDrain(Test.Server),
		"WebSocket reconnect server drain failed"
	);
	testWsReconnectWait(
		&Test.Shutdown,
		1,
		"WebSocket reconnect server did not shut down"
	);
	testRequire(
		(xrtAtomic32Load(
			&Test.HttpClose,
			XMEMORY_ACQUIRE
		 ) == 0) &&
		xrtHttpServerStats(Test.Server, &Stats) &&
		(Stats.State == XHTTP_SERVER_CLOSED) &&
		(Stats.Accepted == iRounds) &&
		(Stats.Upgraded == iRounds) &&
		(Stats.Connections == 0),
		"WebSocket reconnect final server state mismatch"
	);
	xrtHttpClientDestroy(Test.Client);
	xrtHttpServerDestroy(Test.Server);
	testRequire(
		xrtNetEngineDestroy(Test.Engine),
		"WebSocket reconnect engine destroy failed"
	);
}



/* 普通回归运行固定轮数，命令行可提高发布前压力规模。 */
int main(int iArgumentCount, char** pArguments)
{
	uint32 iRounds = TEST_WS_RECONNECT_ROUNDS;

	if ( iArgumentCount > 1 ) {
		char* pEnd = NULL;
		unsigned long iValue = strtoul(
			pArguments[1],
			&pEnd,
			10
		);

		if ( (pEnd == NULL) || (*pEnd != '\0') ||
			(iValue == 0) || (iValue > 100000u) ) {
			fprintf(stderr, "invalid reconnect round count\n");
			return 2;
		}
		iRounds = (uint32)iValue;
	}
	testWsReconnectRun(iRounds);
	printf(
		"[PASS] WebSocket HTTP reconnect (%s, %u rounds)\n",
		TEST_WS_RECONNECT_BACKEND_NAME,
		(unsigned int)iRounds
	);
	return 0;
}

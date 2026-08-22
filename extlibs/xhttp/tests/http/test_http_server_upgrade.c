#include "../test.h"



#ifndef TEST_HTTP_SERVER_UPGRADE_BACKEND
	#define TEST_HTTP_SERVER_UPGRADE_BACKEND XNET_PORT_SELECT
#endif

#ifndef TEST_HTTP_SERVER_UPGRADE_BACKEND_NAME
	#define TEST_HTTP_SERVER_UPGRADE_BACKEND_NAME "select"
#endif

#ifndef TEST_HTTP_SERVER_UPGRADE_SUBMIT
	#define TEST_HTTP_SERVER_UPGRADE_SUBMIT 0
#endif

#ifndef TEST_HTTP_SERVER_UPGRADE_SUBMIT_NAME
	#define TEST_HTTP_SERVER_UPGRADE_SUBMIT_NAME "Reply"
#endif



static const uint8 __g_TestHttpServerUpgradePayload[] =
	"upgrade-early-payload";



/* 保存 HTTP 所有权转移和新协议 Stream 的终态。 */
typedef struct test_http_server_upgrade {
	xatomic32 Requests;
	xatomic32 EarlyRejected;
	xatomic32 Upgraded;
	xatomic32 StreamClosed;
	xatomic32 HttpClosed;
	xatomic32 Errors;
	xatomic32 Shutdown;
	xnetstream* Tcp;
	size_t Buffered;
	bool Submitting;
} test_http_server_upgrade;



/* 回显当前已经留在新协议明文缓冲中的全部数据。 */
static void testHttpServerUpgradeEcho(xnetstream* pStream)
{
	uint8 Buffer[256];

	while ( xrtNetStreamAvailable(pStream) != 0 ) {
		size_t iRead = xrtNetStreamRead(
			pStream,
			Buffer,
			sizeof(Buffer)
		);

		testRequire(
			(iRead != 0) &&
			(xrtNetStreamSend(
				pStream,
				Buffer,
				iRead
			 ) == XNET_RESULT_OK),
			"upgraded HTTP Stream echo failed"
		);
	}
}



/* 新协议收到数据后直接回显，不再经过 HTTP Parser。 */
static void testHttpServerUpgradeRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	(void)pBuffer;
	(void)pData;
	testHttpServerUpgradeEcho(pStream);
}



/* 对端写半关闭后由新协议拥有者结束传输。 */
static void testHttpServerUpgradeEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	testRequire(
		xrtNetStreamClose(pStream),
		"upgraded HTTP Stream close failed"
	);
}



/* 记录由新协议事件表观察到的传输终态。 */
static void testHttpServerUpgradeStreamClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_upgrade* pState =
		(test_http_server_upgrade*)pData;

	(void)pStream;
	testRequire(
		(Result == XNET_RESULT_OK) &&
		(pError == NULL),
		"upgraded HTTP Stream terminal mismatch"
	);
	xrtAtomic32Store(
		&pState->StreamClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 接管 HTTP 已经摘除事件的 TCP Stream，并显式处理缓冲余量。 */
static void testHttpServerUpgradeComplete(
	xhttpconn* pConnection,
	xnetresult Result,
	xhttpupgrade Upgrade,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_upgrade* pState =
		(test_http_server_upgrade*)pData;
	xnetstreamevents Events;

	testRequire(
		!pState->Submitting,
		"HTTP Upgrade completion reentered submit stack"
	);
	testRequire(
		(Result == XNET_RESULT_OK) &&
		(pError == NULL) &&
		(Upgrade.Tcp != NULL) &&
		(Upgrade.Tls == NULL) &&
		(xrtHttpConnState(pConnection) ==
		 XHTTP_CONN_UPGRADED),
		"HTTP Upgrade completion mismatch"
	);
	memset(&Events, 0, sizeof(Events));
	Events.Read = testHttpServerUpgradeRead;
	Events.End = testHttpServerUpgradeEnd;
	Events.Close = testHttpServerUpgradeStreamClose;
	testRequire(
		xrtNetStreamSetEvents(
			Upgrade.Tcp,
			&Events,
			pState
		),
		"upgraded HTTP Stream event install failed"
	);
	pState->Tcp = Upgrade.Tcp;
	pState->Buffered = Upgrade.Buffered;
	testHttpServerUpgradeEcho(Upgrade.Tcp);
	testRequire(
		xrtNetStreamResume(Upgrade.Tcp),
		"upgraded HTTP Stream resume failed"
	);
	xrtAtomic32Store(
		&pState->Upgraded,
		1,
		XMEMORY_RELEASE
	);
}



/* 按当前测试层级构建 101 响应并尝试登记传输接管。 */
static xnetresult testHttpServerUpgradeSubmit(
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	test_http_server_upgrade* pState
)
{
	#if TEST_HTTP_SERVER_UPGRADE_SUBMIT != 2
	xhttpreply* pReply;
	#endif
	xnetresult Result;

	#if TEST_HTTP_SERVER_UPGRADE_SUBMIT != 1
		(void)pRequest;
	#endif
	#if TEST_HTTP_SERVER_UPGRADE_SUBMIT == 2
		Result = xrtHttpConnUpgradeRaw(
			pConnection,
			XRT_BYTES_LITERAL(
				"HTTP/1.1 101 Switching Protocols\r\n"
				"Connection: Upgrade\r\n"
				"Upgrade: xrt-test\r\n"
				"\r\n"
			),
			testHttpServerUpgradeComplete,
			pState
		);
	#else
		pReply = xrtHttpReplyCreate(101);
		testRequire(
			(pReply != NULL) &&
			xrtHttpReplySetHeader(
				pReply,
				XRT_STR_LITERAL("Connection"),
				XRT_STR_LITERAL("Upgrade")
			) &&
			xrtHttpReplySetHeader(
				pReply,
				XRT_STR_LITERAL("Upgrade"),
				XRT_STR_LITERAL("xrt-test")
			),
			"HTTP Upgrade Reply creation failed"
		);
		#if TEST_HTTP_SERVER_UPGRADE_SUBMIT == 1
			{
				xhttp1serverresponse* pResponse =
					xrtHttp1ServerResponseCreate(
						pRequest,
						pReply
					);

				testRequire(
					pResponse != NULL,
					"HTTP Upgrade response plan creation failed"
				);
				Result = xrtHttpConnUpgradeResponse(
					pConnection,
					pResponse,
					testHttpServerUpgradeComplete,
					pState
				);
			}
		#else
			Result = xrtHttpConnUpgrade(
				pConnection,
				pReply,
				testHttpServerUpgradeComplete,
				pState
			);
		#endif
		xrtHttpReplyDestroy(pReply);
	#endif
	return Result;
}



/* Header 完成不等于请求完成，三个接管入口都必须同步拒绝。 */
static xhttpserverbodypolicy testHttpServerUpgradeHeaders(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_http_server_upgrade* pState =
		(test_http_server_upgrade*)pData;
	xnetresult Result;

	(void)pServer;
	testRequire(
		((xrtHttpServerRequestFlags(pRequest) &
		  XHTTP_SERVER_REQUEST_UPGRADE) != 0) &&
		((xrtHttpServerRequestFlags(pRequest) &
		  XHTTP_SERVER_REQUEST_COMPLETE) == 0),
		"HTTP Upgrade Headers request state mismatch"
	);
	pState->Submitting = true;
	Result = testHttpServerUpgradeSubmit(
		pConnection,
		pRequest,
		pState
	);
	pState->Submitting = false;
	testRequire(
		(Result == XNET_RESULT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP_SERVER_ERROR_STATE),
		"incomplete HTTP Upgrade request was accepted"
	);
	xrtClearError();
	(void)xrtAtomic32FetchAdd(
		&pState->EarlyRejected,
		1,
		XMEMORY_RELEASE
	);
	return XHTTP_SERVER_BODY_BUFFER;
}



/* 请求完整后，使用同一个公开入口登记传输接管。 */
static void testHttpServerUpgradeRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_http_server_upgrade* pState =
		(test_http_server_upgrade*)pData;
	xnetresult Result;

	(void)pServer;
	testRequire(
		((xrtHttpServerRequestFlags(pRequest) &
		  XHTTP_SERVER_REQUEST_UPGRADE) != 0) &&
		((xrtHttpServerRequestFlags(pRequest) &
		  XHTTP_SERVER_REQUEST_COMPLETE) != 0) &&
		(xrtAtomic32Load(
			&pState->EarlyRejected,
			XMEMORY_ACQUIRE
		 ) == 1),
		"HTTP Upgrade complete request state mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Requests,
		1,
		XMEMORY_RELEASE
	);
	#if TEST_HTTP_SERVER_UPGRADE_SUBMIT == 2
		testRequire(
			(xrtHttpConnUpgradeRaw(
				pConnection,
				(xbytesview){
					(cbytes)(uintptr_t)(UINTPTR_MAX - 1u),
					4u
				},
				testHttpServerUpgradeComplete,
				pState
			 ) == XNET_RESULT_ERROR) &&
			(xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
			(xrtErrorCode(xrtGetError()) ==
			 XHTTP_SERVER_ERROR_ARGUMENT),
			"Raw HTTP Upgrade accepted a wrapping byte range"
		);
		xrtClearError();
	#endif
	pState->Submitting = true;
	Result = testHttpServerUpgradeSubmit(
		pConnection,
		pRequest,
		pState
	);
	testRequire(
		Result == XNET_RESULT_OK,
		"HTTP Upgrade submission failed"
	);
	testRequire(
		xrtAtomic32Load(
			&pState->Upgraded,
			XMEMORY_ACQUIRE
		) == 0,
		"HTTP Upgrade completed inside submit call"
	);
	pState->Submitting = false;
}



/* Upgrade 成功后 HTTP 层不应再发布 Connection Close。 */
static void testHttpServerUpgradeHttpClose(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_upgrade* pState =
		(test_http_server_upgrade*)pData;

	(void)pServer;
	(void)pConnection;
	(void)Result;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pState->HttpClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 测试路径不允许 HTTP 层发布错误。 */
static void testHttpServerUpgradeError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_upgrade* pState =
		(test_http_server_upgrade*)pData;

	(void)pServer;
	(void)pConnection;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pState->Errors,
		1,
		XMEMORY_RELEASE
	);
}



/* Server 排空只等待仍归 HTTP 所有的连接。 */
static void testHttpServerUpgradeShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_http_server_upgrade* pState =
		(test_http_server_upgrade*)pData;

	testRequire(
		xrtHttpServerState(pServer) ==
			XHTTP_SERVER_CLOSED,
		"HTTP Upgrade server shutdown state mismatch"
	);
	xrtAtomic32Store(
		&pState->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



/* 在截止时间前等待 Worker 发布一个状态。 */
static void testHttpServerUpgradeWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline =
		xrtDeadlineAfter(UINT64_C(10000000));

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



/* 完整发送请求和紧随其后的新协议数据。 */
static void testHttpServerUpgradeSend(
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
			"HTTP Upgrade client send failed"
		);
		iOffset += iSent;
	}
}



/* 读取到 101 Header 和完整回显载荷。 */
static size_t testHttpServerUpgradeReceive(
	xnetsocket Socket,
	char* pOutput,
	size_t iCapacity
)
{
	size_t iUsed = 0;

	while ( iUsed < iCapacity ) {
		size_t iRead = 0;
		xnetresult Result = xrtNetSocketRecv(
			Socket,
			pOutput + iUsed,
			iCapacity - iUsed,
			&iRead
		);

		testRequire(
			(Result == XNET_RESULT_OK) &&
			(iRead != 0),
			"HTTP Upgrade client receive failed"
		);
		iUsed += iRead;
		if ( (iUsed >=
			  sizeof(__g_TestHttpServerUpgradePayload) - 1u) &&
			(memcmp(
				pOutput + iUsed -
					(sizeof(__g_TestHttpServerUpgradePayload) - 1u),
				__g_TestHttpServerUpgradePayload,
				sizeof(__g_TestHttpServerUpgradePayload) - 1u
			 ) == 0) ) {
			return iUsed;
		}
	}
	testRequire(false, "HTTP Upgrade response exceeded buffer");
	return 0;
}



/* 验证延迟回调、余量交付、事件接管和 Server 排空边界。 */
int main(void)
{
	static const uint8 Request[] =
		"GET /chat HTTP/1.1\r\n"
		"Host: upgrade.test\r\n"
		"Connection: Upgrade\r\n"
		"Upgrade: xrt-test\r\n"
		"\r\n"
		"upgrade-early-payload";
	test_http_server_upgrade State;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents Events;
	xhttpserverstats Stats;
	xnetengine* pEngine;
	xhttpserver* pServer;
	xnetaddr Address;
	xnetsocket Client;
	char Response[512];
	size_t iResponse;

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.Requests, 0);
	xrtAtomic32Init(&State.EarlyRejected, 0);
	xrtAtomic32Init(&State.Upgraded, 0);
	xrtAtomic32Init(&State.StreamClosed, 0);
	xrtAtomic32Init(&State.HttpClosed, 0);
	xrtAtomic32Init(&State.Errors, 0);
	xrtAtomic32Init(&State.Shutdown, 0);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_HTTP_SERVER_UPGRADE_BACKEND;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) &&
		xrtNetEngineStart(pEngine),
		"HTTP Upgrade engine start failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP Upgrade address setup failed"
	);
	ServerConfig.WriteSize = 5;
	xrtHttpServerEventsInit(&Events);
	Events.Headers = testHttpServerUpgradeHeaders;
	Events.Request = testHttpServerUpgradeRequest;
	Events.Close = testHttpServerUpgradeHttpClose;
	Events.Error = testHttpServerUpgradeError;
	Events.Shutdown = testHttpServerUpgradeShutdown;
	Events.Data = &State;
	pServer = xrtHttpServerStart(
		pEngine,
		&ServerConfig,
		&Events
	);
	testRequire(
		(pServer != NULL) &&
		xrtHttpServerLocal(pServer, 0, &Address),
		"HTTP Upgrade server start failed"
	);
	Client = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		0
	);
	testRequire(
		(Client != NULL) &&
		(xrtNetSocketConnect(
			Client,
			&Address
		 ) == XNET_RESULT_OK),
		"HTTP Upgrade client connect failed"
	);
	testHttpServerUpgradeSend(
		Client,
		Request,
		sizeof(Request) - 1u
	);
	testHttpServerUpgradeWait(
		&State.Upgraded,
		1,
		"HTTP Upgrade completion missing"
	);
	testRequire(
		xrtHttpServerDrain(pServer),
		"HTTP Upgrade server drain failed"
	);
	testHttpServerUpgradeWait(
		&State.Shutdown,
		1,
		"HTTP Upgrade server waited for transferred Stream"
	);
	iResponse = testHttpServerUpgradeReceive(
		Client,
		Response,
		sizeof(Response) - 1u
	);
	Response[iResponse] = '\0';
	testRequire(
		(iResponse >
		 sizeof(__g_TestHttpServerUpgradePayload) - 1u) &&
		(memcmp(
			Response,
			"HTTP/1.1 101 Switching Protocols\r\n",
			sizeof("HTTP/1.1 101 Switching Protocols\r\n") - 1u
		 ) == 0) &&
		(strstr(Response, "\r\n\r\n") != NULL),
		"HTTP Upgrade response bytes mismatch"
	);
	testRequire(
		xrtNetSocketClose(Client),
		"HTTP Upgrade client close failed"
	);
	testHttpServerUpgradeWait(
		&State.StreamClosed,
		1,
		"upgraded HTTP Stream close event missing"
	);
	testRequire(
		xrtHttpServerStats(pServer, &Stats) &&
		(Stats.Accepted == 1) &&
		(Stats.Requests == 1) &&
		(Stats.Responses == 1) &&
		(Stats.Upgraded == 1) &&
		(Stats.Connections == 0) &&
		(xrtAtomic32Load(
			&State.EarlyRejected,
			XMEMORY_ACQUIRE
		 ) == 1) &&
		(xrtAtomic32Load(
			&State.HttpClosed,
			XMEMORY_ACQUIRE
		 ) == 0) &&
		(xrtAtomic32Load(
			&State.Errors,
			XMEMORY_ACQUIRE
		 ) == 0),
		"HTTP Upgrade statistics or ownership mismatch"
	);
	xrtNetStreamDestroy(State.Tcp);
	xrtHttpServerDestroy(pServer);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"HTTP Upgrade engine destroy failed"
	);
	printf(
		"[PASS] HTTP server Upgrade ownership "
		"(%s, %s, buffered=%zu)\n",
		TEST_HTTP_SERVER_UPGRADE_BACKEND_NAME,
		TEST_HTTP_SERVER_UPGRADE_SUBMIT_NAME,
		State.Buffered
	);
	return 0;
}

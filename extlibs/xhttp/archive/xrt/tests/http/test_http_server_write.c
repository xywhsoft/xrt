#include "../test.h"



#ifndef TEST_HTTP_SERVER_BACKEND
	#define TEST_HTTP_SERVER_BACKEND XNET_PORT_SELECT
#endif

#ifndef TEST_HTTP_SERVER_BACKEND_NAME
	#define TEST_HTTP_SERVER_BACKEND_NAME "select"
#endif



typedef struct test_http_server_write {
	xbytesview Body;
	xatomic32 Requested;
	xatomic32 Errors;
	xatomic32 Closed;
	xatomic32 Shutdown;
	xerrkind ErrorKind;
	int32 ErrorCode;
	char ErrorOperation[64];
} test_http_server_write;



/* 在截止时间前等待 Worker 发布指定计数。 */
static void testHttpServerWriteWait(
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



/* 等待 Close 回调返回后的连接清理和 Server 统计收敛。 */
static void testHttpServerWriteWaitConnections(
	xhttpserver* pServer,
	size_t iExpected
)
{
	xdeadline Deadline = xrtDeadlineAfter(
		UINT64_C(10000000)
	);

	for ( ;; ) {
		xhttpserverstats Stats;

		testRequire(
			xrtHttpServerStats(pServer, &Stats),
			"HTTP server slow-reader stats query failed"
		);
		if ( Stats.Connections == iExpected ) {
			return;
		}
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP server slow-reader stats did not settle"
		);
		xrtThreadYield();
	}
}



/* 完整发送阻塞客户端的短请求。 */
static void testHttpServerWriteSend(
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
			"HTTP server slow-reader request send failed"
		);
		iOffset += iSent;
	}
}



/* 提交足够填满对端窗口的大正文。 */
static void testHttpServerWriteRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_http_server_write* pState =
		(test_http_server_write*)pData;

	(void)pServer;
	testRequire(
		xrtHttpServerRequestTarget(pRequest).Size == 6,
		"HTTP server slow-reader request target mismatch"
	);
	testRequire(
		xrtHttpConnReply(
			pConnection,
			XHTTP_STATUS_OK,
			XRT_STR_LITERAL(
				"application/octet-stream"
			),
			pState->Body
		) == XNET_RESULT_OK,
		"HTTP server large response submission failed"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Requested,
		1,
		XMEMORY_RELEASE
	);
}



/* 保存唯一写超时的稳定分类。 */
static void testHttpServerWriteError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_write* pState =
		(test_http_server_write*)pData;
	cstr sOperation;
	size_t iSize;

	(void)pServer;
	(void)pConnection;
	testRequire(
		pError != NULL,
		"HTTP server slow-reader error is null"
	);
	pState->ErrorKind = xrtErrorKind(pError);
	pState->ErrorCode = xrtErrorCode(pError);
	sOperation = xrtErrorOperation(pError);
	iSize = strlen(sOperation);
	testRequire(
		iSize < sizeof(pState->ErrorOperation),
		"HTTP server slow-reader operation is too long"
	);
	memcpy(pState->ErrorOperation, sOperation, iSize + 1);
	(void)xrtAtomic32FetchAdd(
		&pState->Errors,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录连接写超时后的唯一终态。 */
static void testHttpServerWriteClose(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_write* pState =
		(test_http_server_write*)pData;

	(void)pServer;
	(void)pConnection;
	(void)Result;
	testRequire(
		(pError != NULL) &&
		(xrtErrorCode(pError) ==
		 XHTTP_SERVER_ERROR_TIMEOUT_WRITE),
		"HTTP server slow-reader close cause mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 Server 排空终态。 */
static void testHttpServerWriteShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_http_server_write* pState =
		(test_http_server_write*)pData;

	testRequire(
		xrtHttpServerState(pServer) ==
		XHTTP_SERVER_CLOSED,
		"HTTP server slow-reader shutdown state mismatch"
	);
	xrtAtomic32Store(
		&pState->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



int main(void)
{
	static const char sRequest[] =
		"GET /large HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"Connection: close\r\n"
		"\r\n";
	test_http_server_write State;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents Events;
	xhttpserverstats Stats;
	xnetengine* pEngine;
	xhttpserver* pServer;
	xnetsocket Client;
	xnetaddr Address;
	bytes pBody;
	size_t iBodySize = 16u * 1024u * 1024u;

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.Requested, 0);
	xrtAtomic32Init(&State.Errors, 0);
	xrtAtomic32Init(&State.Closed, 0);
	xrtAtomic32Init(&State.Shutdown, 0);
	pBody = (bytes)xrtMalloc(iBodySize);
	testRequire(
		pBody != NULL,
		"HTTP server slow-reader body allocation failed"
	);
	memset(pBody, 'W', iBodySize);
	State.Body = (xbytesview){ pBody, iBodySize };

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_HTTP_SERVER_BACKEND;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) &&
		xrtNetEngineStart(pEngine),
		"HTTP server slow-reader engine start failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP server slow-reader address setup failed"
	);
	ServerConfig.Network.Listen.Stream.WriteHighWater =
		32u * 1024u;
	ServerConfig.Network.Listen.Stream.WriteLowWater =
		16u * 1024u;
	ServerConfig.Network.Listen.Stream.WriteLimit =
		64u * 1024u;
	ServerConfig.WriteSize = 16u * 1024u;
	ServerConfig.WriteTimeout = UINT64_C(100000);
	ServerConfig.RequestTimeout = UINT64_C(2000000);
	xrtHttpServerEventsInit(&Events);
	Events.Request = testHttpServerWriteRequest;
	Events.Error = testHttpServerWriteError;
	Events.Close = testHttpServerWriteClose;
	Events.Shutdown = testHttpServerWriteShutdown;
	Events.Data = &State;
	pServer = xrtHttpServerStart(
		pEngine,
		&ServerConfig,
		&Events
	);
	testRequire(
		(pServer != NULL) &&
		xrtHttpServerLocal(pServer, 0, &Address),
		"HTTP server slow-reader start failed"
	);

	Client = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		0
	);
	testRequire(
		(Client != NULL) &&
		xrtNetSocketSet(
			Client,
			XNET_OPTION_RECEIVE_BUFFER,
			1024
		) &&
		(xrtNetSocketConnect(
			Client,
			&Address
		 ) == XNET_RESULT_OK),
		"HTTP server slow-reader client connect failed"
	);
	testHttpServerWriteSend(
		Client,
		(cbytes)sRequest,
		sizeof(sRequest) - 1
	);
	testHttpServerWriteWait(
		&State.Requested,
		1,
		"HTTP server slow-reader request callback missing"
	);
	testHttpServerWriteWait(
		&State.Errors,
		1,
		"HTTP server slow-reader write timeout missing"
	);
	testHttpServerWriteWait(
		&State.Closed,
		1,
		"HTTP server slow-reader connection did not close"
	);
	testHttpServerWriteWaitConnections(pServer, 0);
	testRequire(
		(State.ErrorKind == XERR_TIMEOUT) &&
		(State.ErrorCode ==
		 XHTTP_SERVER_ERROR_TIMEOUT_WRITE) &&
		(strcmp(
			State.ErrorOperation,
			"write-http-response"
		 ) == 0),
		"HTTP server slow-reader error contract mismatch"
	);
	testRequire(
		xrtHttpServerStats(pServer, &Stats) &&
		(Stats.Accepted == 1) &&
		(Stats.Requests == 1) &&
		(Stats.Responses == 0) &&
		(Stats.ProtocolErrors == 0) &&
		(Stats.Timeouts == 1) &&
		(Stats.Connections == 0),
		"HTTP server slow-reader statistics mismatch"
	);
	testRequire(
		xrtNetSocketClose(Client),
		"HTTP server slow-reader client close failed"
	);
	testRequire(
		xrtHttpServerDrain(pServer),
		"HTTP server slow-reader drain failed"
	);
	testHttpServerWriteWait(
		&State.Shutdown,
		1,
		"HTTP server slow-reader shutdown missing"
	);
	xrtHttpServerDestroy(pServer);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"HTTP server slow-reader engine destroy failed"
	);
	xrtFree(pBody);
	printf(
		"[PASS] HTTP server slow-reader write timeout (%s)\n",
		TEST_HTTP_SERVER_BACKEND_NAME
	);
	return 0;
}

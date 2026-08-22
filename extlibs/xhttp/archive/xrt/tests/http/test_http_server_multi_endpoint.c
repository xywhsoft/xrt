#include "../test.h"



typedef struct test_http_multi_server {
	xhttpserver* Server;
	xatomic32 Endpoint0;
	xatomic32 Endpoint1;
	xatomic32 ConnectionsClosed;
	xatomic32 Shutdown;
} test_http_multi_server;



typedef struct test_http_multi_client {
	xatomic32 Closed;
} test_http_multi_client;



/* 在截止时间前等待原子计数达到目标。 */
static void testHttpMultiWait(
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
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}



/* 记录连接所属的逻辑监听端点。 */
static void testHttpMultiOpen(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	ptr pData
)
{
	test_http_multi_server* pState =
		(test_http_multi_server*)pData;
	size_t iEndpoint = xrtHttpConnEndpoint(pConnection);

	testRequire(
		(pServer == pState->Server) && (iEndpoint < 2),
		"HTTP multi-endpoint connection index mismatch"
	);
	if ( iEndpoint == 0 ) {
		(void)xrtAtomic32FetchAdd(
			&pState->Endpoint0,
			1,
			XMEMORY_RELEASE
		);
	} else {
		(void)xrtAtomic32FetchAdd(
			&pState->Endpoint1,
			1,
			XMEMORY_RELEASE
		);
	}
}



/* 使用直接响应 Helper 完成每个端点上的请求。 */
static void testHttpMultiRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	(void)pServer;
	(void)pRequest;
	(void)pData;
	testRequire(
		xrtHttpConnReply(
			pConnection,
			200,
			XRT_STR_LITERAL("application/json; charset=utf-8"),
			XRT_BYTES_LITERAL("{\"code\":200}")
		) == XNET_RESULT_OK,
		"HTTP multi-endpoint response failed"
	);
}



/* 记录每个 HTTP 连接的正常关闭。 */
static void testHttpMultiConnectionClose(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_multi_server* pState =
		(test_http_multi_server*)pData;

	(void)pServer;
	(void)pConnection;
	testRequire(
		(Result == XNET_RESULT_OK) && (pError == NULL),
		"HTTP multi-endpoint connection close mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->ConnectionsClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 聚合网络和全部连接退出后只记录一次终态。 */
static void testHttpMultiShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_http_multi_server* pState =
		(test_http_multi_server*)pData;

	testRequire(
		xrtHttpServerState(pServer) == XHTTP_SERVER_CLOSED,
		"HTTP multi-endpoint shutdown state mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



/* TCP Client 打开后发送一个关闭连接的 HTTP 请求。 */
static void testHttpMultiClientOpen(
	xnetstream* pStream,
	ptr pData
)
{
	static const char sRequest[] =
		"GET / HTTP/1.1\r\n"
		"Host: multi.test\r\n"
		"Connection: close\r\n"
		"\r\n";

	(void)pData;
	testRequire(
		xrtNetStreamSend(
			pStream,
			sRequest,
			sizeof(sRequest) - 1u
		) == XNET_RESULT_OK,
		"HTTP multi-endpoint client request failed"
	);
}



/* 及时消费响应，避免测试本身制造背压。 */
static void testHttpMultiClientRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	(void)pStream;
	(void)pData;
	(void)xrtNetBufConsume(pBuffer, xrtNetBufSize(pBuffer));
}



/* HTTP 对端结束写方向后关闭 Client。 */
static void testHttpMultiClientEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	(void)xrtNetStreamClose(pStream);
}



/* 记录 Client 正常终止。 */
static void testHttpMultiClientClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_multi_client* pClient =
		(test_http_multi_client*)pData;

	(void)pStream;
	testRequire(
		(Result == XNET_RESULT_OK) && (pError == NULL),
		"HTTP multi-endpoint client close mismatch"
	);
	xrtAtomic32Store(&pClient->Closed, 1, XMEMORY_RELEASE);
}



/* 验证双栈共享端口、端点传播、底层访问和聚合关闭契约。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents ServerEvents;
	xnetlistenconfig Additional;
	xnetstreamevents ClientEvents;
	test_http_multi_server State;
	test_http_multi_client Client4;
	test_http_multi_client Client6;
	xhttpserverstats HttpStats;
	xnetserverstats NetStats;
	xnetengine* pEngine;
	xnetserver* pNetwork;
	xnetstream* pClient4;
	xnetstream* pClient6;
	xnetaddr Local4;
	xnetaddr Local6;

	memset(&State, 0, sizeof(State));
	memset(&Client4, 0, sizeof(Client4));
	memset(&Client6, 0, sizeof(Client6));
	xrtAtomic32Init(&State.Endpoint0, 0);
	xrtAtomic32Init(&State.Endpoint1, 0);
	xrtAtomic32Init(&State.ConnectionsClosed, 0);
	xrtAtomic32Init(&State.Shutdown, 0);
	xrtAtomic32Init(&Client4.Closed, 0);
	xrtAtomic32Init(&Client6.Closed, 0);

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) && xrtNetEngineStart(pEngine),
		"HTTP multi-endpoint engine start failed"
	);

	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV6,
			0
		),
		"HTTP multi-endpoint IPv6 address failed"
	);
	ServerConfig.Network.Listen.Affinity = 0;
	ServerConfig.Network.Listen.Distribution = XNET_ACCEPT_LOCAL;
	ServerConfig.Network.Listen.IPv6Only = true;
	xrtNetListenConfigInit(&Additional);
	testRequire(
		xrtNetAddrLoopback(
			&Additional.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP multi-endpoint IPv4 address failed"
	);
	Additional.Affinity = 1;
	Additional.Distribution = XNET_ACCEPT_LOCAL;
	ServerConfig.Network.Additional = &Additional;
	ServerConfig.Network.AdditionalCount = 1;
	ServerConfig.Network.SharedPort = true;

	xrtHttpServerEventsInit(&ServerEvents);
	ServerEvents.Open = testHttpMultiOpen;
	ServerEvents.Request = testHttpMultiRequest;
	ServerEvents.Close = testHttpMultiConnectionClose;
	ServerEvents.Shutdown = testHttpMultiShutdown;
	ServerEvents.Data = &State;
	State.Server = xrtHttpServerStart(
		pEngine,
		&ServerConfig,
		&ServerEvents
	);
	testRequire(
		(State.Server != NULL) &&
		(xrtHttpServerEndpointCount(State.Server) == 2) &&
		(xrtHttpServerListenerCount(State.Server) == 2) &&
		xrtHttpServerLocal(State.Server, 0, &Local6) &&
		xrtHttpServerLocal(State.Server, 1, &Local4) &&
		(Local6.Family == XNET_FAMILY_IPV6) &&
		(Local4.Family == XNET_FAMILY_IPV4) &&
		(Local6.Port != 0) && (Local6.Port == Local4.Port),
		"HTTP multi-endpoint topology mismatch"
	);
	pNetwork = xrtHttpServerNetwork(State.Server);
	testRequire(
		(pNetwork != NULL) &&
		(xrtNetServerData(pNetwork) == State.Server) &&
		xrtNetServerStats(pNetwork, &NetStats) &&
		(NetStats.Endpoints == 2) && (NetStats.Listeners == 2) &&
		(xrtHttpServerError(State.Server) == NULL),
		"HTTP multi-endpoint network access mismatch"
	);

	memset(&ClientEvents, 0, sizeof(ClientEvents));
	ClientEvents.Open = testHttpMultiClientOpen;
	ClientEvents.Read = testHttpMultiClientRead;
	ClientEvents.End = testHttpMultiClientEnd;
	ClientEvents.Close = testHttpMultiClientClose;
	pClient6 = xrtNetStreamConnect(
		pEngine,
		&Local6,
		0,
		NULL,
		&ClientEvents,
		&Client6
	);
	pClient4 = xrtNetStreamConnect(
		pEngine,
		&Local4,
		1,
		NULL,
		&ClientEvents,
		&Client4
	);
	testRequire(
		(pClient6 != NULL) && (pClient4 != NULL),
		"HTTP multi-endpoint clients failed"
	);
	testHttpMultiWait(
		&Client6.Closed,
		1,
		"HTTP multi-endpoint IPv6 client did not close"
	);
	testHttpMultiWait(
		&Client4.Closed,
		1,
		"HTTP multi-endpoint IPv4 client did not close"
	);
	testHttpMultiWait(
		&State.ConnectionsClosed,
		2,
		"HTTP multi-endpoint connections did not close"
	);
	testRequire(
		(xrtAtomic32Load(&State.Endpoint0, XMEMORY_ACQUIRE) == 1) &&
		(xrtAtomic32Load(&State.Endpoint1, XMEMORY_ACQUIRE) == 1) &&
		xrtHttpServerStats(State.Server, &HttpStats) &&
		(HttpStats.Accepted == 2) && (HttpStats.Rejected == 0) &&
		(HttpStats.Requests == 2) && (HttpStats.Responses == 2) &&
		(HttpStats.Endpoints == 2) && (HttpStats.Listeners == 2),
		"HTTP multi-endpoint runtime statistics mismatch"
	);

	testRequire(
		xrtHttpServerDrain(State.Server),
		"HTTP multi-endpoint drain failed"
	);
	testHttpMultiWait(
		&State.Shutdown,
		1,
		"HTTP multi-endpoint shutdown did not finish"
	);
	testRequire(
		xrtAtomic32Load(&State.Shutdown, XMEMORY_ACQUIRE) == 1,
		"HTTP multi-endpoint shutdown repeated"
	);
	xrtNetStreamDestroy(pClient4);
	xrtNetStreamDestroy(pClient6);
	xrtNetServerDestroy(pNetwork);
	xrtHttpServerDestroy(State.Server);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"HTTP multi-endpoint engine destroy failed"
	);
	printf("[PASS] HTTP server multi-endpoint runtime\n");
	return 0;
}

#include "../test.h"
#include "../../src/internal/xrt_http_server_runtime.h"



typedef struct test_http_server_lifecycle {
	xnetengine* Engine;
	xhttpserver* Server;
	xhttpconn* Connection;
	xatomic32 Request;
	xatomic32 Close;
	xatomic32 Shutdown;
	xatomic32 Errors;
} test_http_server_lifecycle;



typedef struct test_http_server_lifecycle_client {
	xnetstream* Stream;
	xatomic32 Open;
	xatomic32 Close;
} test_http_server_lifecycle_client;



typedef struct test_http_server_shutdown_order {
	xnetengine* Engine;
	xhttpserver* Server;
	xatomic32 Open;
	xatomic32 Request;
	xatomic32 Cleanup;
	xatomic32 ContinueCleanup;
	xatomic32 Shutdown;
	xatomic32 Errors;
} test_http_server_shutdown_order;



/* 等待异步生命周期状态达到下限。 */
static void testHttpServerLifecycleWait(
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



/* 保留活动请求 Connection 供主线程验证跨线程边界。 */
static void testHttpServerLifecycleRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_http_server_lifecycle* pState =
		(test_http_server_lifecycle*)pData;

	(void)pServer;
	testRequire(
		(pState->Connection == NULL) &&
		(xrtHttpServerRequestBody(pRequest).Size == 0),
		"HTTP lifecycle received an unexpected request"
	);
	pState->Connection = xrtHttpConnRef(pConnection);
	testRequire(
		pState->Connection != NULL,
		"HTTP lifecycle could not retain its connection"
	);
	xrtAtomic32Store(
		&pState->Request,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录活动 Connection 的唯一关闭回调。 */
static void testHttpServerLifecycleClose(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_lifecycle* pState =
		(test_http_server_lifecycle*)pData;

	(void)pServer;
	testRequire(
		(pConnection == pState->Connection) &&
		(Result == XNET_RESULT_CANCELLED) &&
		(pError == NULL),
		"HTTP lifecycle abort close mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Close,
		1,
		XMEMORY_RELEASE
	);
}



/* 生命周期测试不应产生协议或传输错误。 */
static void testHttpServerLifecycleError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_lifecycle* pState =
		(test_http_server_lifecycle*)pData;

	(void)pServer;
	(void)pConnection;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pState->Errors,
		1,
		XMEMORY_RELEASE
	);
}



/* 发布 Server 关闭终态。 */
static void testHttpServerLifecycleShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_http_server_lifecycle* pState =
		(test_http_server_lifecycle*)pData;

	testRequire(
		xrtHttpServerState(pServer) == XHTTP_SERVER_CLOSED,
		"HTTP lifecycle Shutdown state mismatch"
	);
	xrtAtomic32Store(
		&pState->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



/* 客户端打开后发送一个保持活动的请求。 */
static void testHttpServerLifecycleClientOpen(
	xnetstream* pStream,
	ptr pData
)
{
	static const char Request[] =
		"GET /hold HTTP/1.1\r\n"
		"Host: lifecycle.test\r\n"
		"\r\n";
	test_http_server_lifecycle_client* pClient =
		(test_http_server_lifecycle_client*)pData;

	testRequire(
		xrtNetStreamSend(
			pStream,
			Request,
			sizeof(Request) - 1
		) == XNET_RESULT_OK,
		"HTTP lifecycle client request failed"
	);
	xrtAtomic32Store(
		&pClient->Open,
		1,
		XMEMORY_RELEASE
	);
}



/* 结束客户端收到的全部字节。 */
static void testHttpServerLifecycleClientRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	(void)pStream;
	(void)pData;
	(void)xrtNetBufConsume(
		pBuffer,
		xrtNetBufSize(pBuffer)
	);
}



/* 对端关闭写方向后结束客户端。 */
static void testHttpServerLifecycleClientEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	(void)xrtNetStreamClose(pStream);
}



/* 发布客户端终态。 */
static void testHttpServerLifecycleClientClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_lifecycle_client* pClient =
		(test_http_server_lifecycle_client*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	xrtAtomic32Store(
		&pClient->Close,
		1,
		XMEMORY_RELEASE
	);
}



/* 阻塞 Connection 最终适配器清理，验证 Server 终态不能越过该边界。 */
static void testHttpServerShutdownOrderCleanup(ptr pData)
{
	test_http_server_shutdown_order* pState =
		(test_http_server_shutdown_order*)pData;

	xrtAtomic32Store(
		&pState->Cleanup,
		1,
		XMEMORY_RELEASE
	);
	while ( !xrtAtomic32Load(
		&pState->ContinueCleanup,
		XMEMORY_ACQUIRE
	) ) {
		xrtThreadYield();
	}
}



/* 为顺序测试的 Connection 安装可观察的最终清理上下文。 */
static void testHttpServerShutdownOrderOpen(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	ptr pData
)
{
	test_http_server_shutdown_order* pState =
		(test_http_server_shutdown_order*)pData;

	(void)pServer;
	testRequire(
		__xrtHttpConnAdapterSet(
			pConnection,
			pState,
			testHttpServerShutdownOrderCleanup
		),
		"HTTP shutdown order could not install cleanup context"
	);
	xrtAtomic32Store(
		&pState->Open,
		1,
		XMEMORY_RELEASE
	);
}



/* 保持请求活动，使 Drain 只关闭监听器而不提前关闭 Connection。 */
static void testHttpServerShutdownOrderRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_http_server_shutdown_order* pState =
		(test_http_server_shutdown_order*)pData;

	(void)pServer;
	(void)pConnection;
	(void)pRequest;
	xrtAtomic32Store(
		&pState->Request,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录顺序测试不应出现的协议或传输错误。 */
static void testHttpServerShutdownOrderError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_shutdown_order* pState =
		(test_http_server_shutdown_order*)pData;

	(void)pServer;
	(void)pConnection;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pState->Errors,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 Connection 内部清理全部完成后的 Server 终态。 */
static void testHttpServerShutdownOrderDone(
	xhttpserver* pServer,
	ptr pData
)
{
	test_http_server_shutdown_order* pState =
		(test_http_server_shutdown_order*)pData;

	testRequire(
		xrtHttpServerState(pServer) == XHTTP_SERVER_CLOSED,
		"HTTP shutdown order state mismatch"
	);
	xrtAtomic32Store(
		&pState->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证 Shutdown 不会在 Connection 的 HTTP 运行时清理完成前发布。 */
static void testHttpServerShutdownCleanupOrder(void)
{
	test_http_server_shutdown_order State;
	test_http_server_lifecycle_client Client;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents ServerEvents;
	xnetstreamconfig StreamConfig;
	xnetstreamevents StreamEvents;
	xhttpserverstats Stats;
	xnetaddr Address;

	memset(&State, 0, sizeof(State));
	memset(&Client, 0, sizeof(Client));
	xrtAtomic32Init(&State.Open, 0);
	xrtAtomic32Init(&State.Request, 0);
	xrtAtomic32Init(&State.Cleanup, 0);
	xrtAtomic32Init(&State.ContinueCleanup, 0);
	xrtAtomic32Init(&State.Shutdown, 0);
	xrtAtomic32Init(&State.Errors, 0);
	xrtAtomic32Init(&Client.Open, 0);
	xrtAtomic32Init(&Client.Close, 0);

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"HTTP shutdown order engine start failed"
	);

	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP shutdown order loopback address failed"
	);
	ServerConfig.HeaderTimeout = UINT64_C(10000000);
	ServerConfig.RequestTimeout = UINT64_C(10000000);
	ServerConfig.IdleTimeout = UINT64_C(10000000);
	xrtHttpServerEventsInit(&ServerEvents);
	ServerEvents.Open = testHttpServerShutdownOrderOpen;
	ServerEvents.Request = testHttpServerShutdownOrderRequest;
	ServerEvents.Error = testHttpServerShutdownOrderError;
	ServerEvents.Shutdown = testHttpServerShutdownOrderDone;
	ServerEvents.Data = &State;
	State.Server = xrtHttpServerStart(
		State.Engine,
		&ServerConfig,
		&ServerEvents
	);
	testRequire(
		(State.Server != NULL) &&
		xrtHttpServerLocal(State.Server, 0, &Address),
		"HTTP shutdown order server start failed"
	);

	memset(&StreamEvents, 0, sizeof(StreamEvents));
	StreamEvents.Open = testHttpServerLifecycleClientOpen;
	StreamEvents.Read = testHttpServerLifecycleClientRead;
	StreamEvents.End = testHttpServerLifecycleClientEnd;
	StreamEvents.Close = testHttpServerLifecycleClientClose;
	xrtNetStreamConfigInit(&StreamConfig);
	Client.Stream = xrtNetStreamConnect(
		State.Engine,
		&Address,
		0,
		&StreamConfig,
		&StreamEvents,
		&Client
	);
	testRequire(
		Client.Stream != NULL,
		"HTTP shutdown order client connect failed"
	);
	testHttpServerLifecycleWait(
		&State.Request,
		1,
		"HTTP shutdown order active request missing"
	);

	testRequire(
		xrtHttpServerDrain(State.Server),
		"HTTP shutdown order drain failed"
	);
	testHttpServerLifecycleWait(
		&State.Server->NetworkClosed,
		1,
		"HTTP shutdown order listener did not close"
	);
	testRequire(
		(xrtAtomic32Load(
			&State.Shutdown,
			XMEMORY_ACQUIRE
		 ) == 0) &&
		xrtHttpServerAbort(State.Server),
		"HTTP shutdown order abort started too late"
	);
	testHttpServerLifecycleWait(
		&State.Cleanup,
		1,
		"HTTP shutdown order cleanup did not start"
	);
	testRequire(
		(xrtAtomic32Load(
			&State.Shutdown,
			XMEMORY_ACQUIRE
		 ) == 0) &&
		(xrtHttpServerState(State.Server) ==
		 XHTTP_SERVER_ABORTING),
		"HTTP shutdown escaped a live Connection cleanup"
	);

	xrtAtomic32Store(
		&State.ContinueCleanup,
		1,
		XMEMORY_RELEASE
	);
	testHttpServerLifecycleWait(
		&State.Shutdown,
		1,
		"HTTP shutdown order server did not finish"
	);
	testHttpServerLifecycleWait(
		&Client.Close,
		1,
		"HTTP shutdown order client did not close"
	);
	testRequire(
		xrtHttpServerStats(State.Server, &Stats) &&
		(Stats.State == XHTTP_SERVER_CLOSED) &&
		(Stats.Connections == 0) &&
		(xrtAtomic32Load(
			&State.Errors,
			XMEMORY_ACQUIRE
		 ) == 0),
		"HTTP shutdown order terminal facts mismatch"
	);

	xrtNetStreamDestroy(Client.Stream);
	xrtHttpServerDestroy(State.Server);
	testRequire(
		xrtNetEngineDestroy(State.Engine),
		"HTTP shutdown order engine destroy failed"
	);
}



/* 建立一个生命周期测试客户端。 */
static void testHttpServerLifecycleConnect(
	test_http_server_lifecycle* pState,
	const xnetaddr* pAddress,
	test_http_server_lifecycle_client* pClient
)
{
	xnetstreamconfig Config;
	xnetstreamevents Events;

	memset(pClient, 0, sizeof(*pClient));
	memset(&Events, 0, sizeof(Events));
	xrtAtomic32Init(&pClient->Open, 0);
	xrtAtomic32Init(&pClient->Close, 0);
	Events.Open = testHttpServerLifecycleClientOpen;
	Events.Read = testHttpServerLifecycleClientRead;
	Events.End = testHttpServerLifecycleClientEnd;
	Events.Close = testHttpServerLifecycleClientClose;
	xrtNetStreamConfigInit(&Config);
	pClient->Stream = xrtNetStreamConnect(
		pState->Engine,
		pAddress,
		0,
		&Config,
		&Events,
		pClient
	);
	testRequire(
		pClient->Stream != NULL,
		"HTTP lifecycle client connect failed"
	);
}



/* 验证公开地址和统计快照接受未对齐存储并拒绝回绕范围。 */
static void testHttpServerLifecycleOutputs(
	test_http_server_lifecycle* pState,
	const xnetaddr* pExpected
)
{
	uint8 AddressStorage[sizeof(xnetaddr) + 2u];
	uint8 ServerStorage[sizeof(xhttpserverstats) + 2u];
	uint8 ConnStorage[sizeof(xhttpconnstats) + 2u];
	xhttpserverstats ServerStats;
	xhttpconnstats ConnStats;
	xnetaddr Address;

	memset(AddressStorage, 0xa5, sizeof(AddressStorage));
	testRequire(
		xrtHttpServerLocal(
			pState->Server,
			0,
			(xnetaddr*)(void*)(AddressStorage + 1u)
		) &&
		(AddressStorage[0] == 0xa5) &&
		(AddressStorage[sizeof(AddressStorage) - 1u] == 0xa5),
		"HTTP server unaligned local output mismatch"
	);
	memcpy(&Address, AddressStorage + 1u, sizeof(Address));
	testRequire(
		memcmp(&Address, pExpected, sizeof(Address)) == 0,
		"HTTP server unaligned local address changed"
	);

	memset(ServerStorage, 0xa5, sizeof(ServerStorage));
	testRequire(
		xrtHttpServerStats(
			pState->Server,
			(xhttpserverstats*)(void*)(ServerStorage + 1u)
		) &&
		(ServerStorage[0] == 0xa5) &&
		(ServerStorage[sizeof(ServerStorage) - 1u] == 0xa5),
		"HTTP server unaligned stats output mismatch"
	);
	memcpy(&ServerStats, ServerStorage + 1u, sizeof(ServerStats));
	testRequire(
		(ServerStats.Accepted == 1u) &&
		(ServerStats.Connections == 1u),
		"HTTP server unaligned stats value mismatch"
	);

	memset(AddressStorage, 0xa5, sizeof(AddressStorage));
	testRequire(
		xrtHttpConnLocal(
			pState->Connection,
			(xnetaddr*)(void*)(AddressStorage + 1u)
		) &&
		(AddressStorage[0] == 0xa5) &&
		(AddressStorage[sizeof(AddressStorage) - 1u] == 0xa5),
		"HTTP connection unaligned local output mismatch"
	);
	memset(AddressStorage, 0xa5, sizeof(AddressStorage));
	testRequire(
		xrtHttpConnRemote(
			pState->Connection,
			(xnetaddr*)(void*)(AddressStorage + 1u)
		) &&
		(AddressStorage[0] == 0xa5) &&
		(AddressStorage[sizeof(AddressStorage) - 1u] == 0xa5),
		"HTTP connection unaligned remote output mismatch"
	);

	memset(ConnStorage, 0xa5, sizeof(ConnStorage));
	testRequire(
		xrtHttpConnStats(
			pState->Connection,
			(xhttpconnstats*)(void*)(ConnStorage + 1u)
		) &&
		(ConnStorage[0] == 0xa5) &&
		(ConnStorage[sizeof(ConnStorage) - 1u] == 0xa5),
		"HTTP connection unaligned stats output mismatch"
	);
	memcpy(&ConnStats, ConnStorage + 1u, sizeof(ConnStats));
	testRequire(
		(ConnStats.Requests == 1u) && ConnStats.RequestActive,
		"HTTP connection unaligned stats value mismatch"
	);

	xrtClearError();
	testRequire(
		!xrtHttpServerLocal(
			pState->Server,
			0,
			(xnetaddr*)(uintptr_t)(UINTPTR_MAX - 1u)
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server accepted a wrapping local output"
	);
	xrtClearError();
	testRequire(
		!xrtHttpServerStats(
			pState->Server,
			(xhttpserverstats*)(uintptr_t)(UINTPTR_MAX - 1u)
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server accepted a wrapping stats output"
	);
	xrtClearError();
	testRequire(
		!xrtHttpConnLocal(
			pState->Connection,
			(xnetaddr*)(uintptr_t)(UINTPTR_MAX - 1u)
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP connection accepted a wrapping local output"
	);
	xrtClearError();
	testRequire(
		!xrtHttpConnRemote(
			pState->Connection,
			(xnetaddr*)(uintptr_t)(UINTPTR_MAX - 1u)
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP connection accepted a wrapping remote output"
	);
	xrtClearError();
	testRequire(
		!xrtHttpConnStats(
			pState->Connection,
			(xhttpconnstats*)(uintptr_t)(UINTPTR_MAX - 1u)
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP connection accepted a wrapping stats output"
	);
	xrtClearError();

	/* 输出不能覆盖仍在运行的 Server 或 Connection 对象。 */
	testRequire(
		!xrtHttpServerLocal(
			pState->Server,
			0,
			(xnetaddr*)(void*)pState->Server
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server local output overwrote the Server"
	);
	xrtClearError();
	testRequire(
		!xrtHttpServerStats(
			pState->Server,
			(xhttpserverstats*)(void*)pState->Server
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server stats output overwrote the Server"
	);
	xrtClearError();
	testRequire(
		!xrtHttpConnLocal(
			pState->Connection,
			(xnetaddr*)(void*)pState->Connection
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP local output overwrote the Connection"
	);
	xrtClearError();
	testRequire(
		!xrtHttpConnRemote(
			pState->Connection,
			(xnetaddr*)(void*)pState->Connection
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP remote output overwrote the Connection"
	);
	xrtClearError();
	testRequire(
		!xrtHttpConnStats(
			pState->Connection,
			(xhttpconnstats*)(void*)pState->Connection
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP stats output overwrote the Connection"
	);
	xrtClearError();
}



int main(void)
{
	test_http_server_lifecycle State;
	test_http_server_lifecycle_client First;
	test_http_server_lifecycle_client Second;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents Events;
	xhttpserverstats ServerStats;
	xhttpconnstats ConnStats;
	xnetaddr Address;
	xhttpreply* pReply;

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.Request, 0);
	xrtAtomic32Init(&State.Close, 0);
	xrtAtomic32Init(&State.Shutdown, 0);
	xrtAtomic32Init(&State.Errors, 0);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"HTTP lifecycle engine start failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP lifecycle loopback address failed"
	);
	ServerConfig.MaxConnections = 1;
	ServerConfig.HeaderTimeout = UINT64_C(10000000);
	ServerConfig.RequestTimeout = UINT64_C(10000000);
	ServerConfig.IdleTimeout = UINT64_C(10000000);
	xrtHttpServerEventsInit(&Events);
	Events.Request = testHttpServerLifecycleRequest;
	Events.Close = testHttpServerLifecycleClose;
	Events.Error = testHttpServerLifecycleError;
	Events.Shutdown = testHttpServerLifecycleShutdown;
	Events.Data = &State;
	State.Server = xrtHttpServerStart(
		State.Engine,
		&ServerConfig,
		&Events
	);
	testRequire(
		(State.Server != NULL) &&
		xrtHttpServerLocal(State.Server, 0, &Address),
		"HTTP lifecycle server start failed"
	);

	testHttpServerLifecycleConnect(
		&State,
		&Address,
		&First
	);
	testHttpServerLifecycleWait(
		&State.Request,
		1,
		"HTTP lifecycle active request missing"
	);
	testHttpServerLifecycleOutputs(&State, &Address);
	testRequire(
		xrtHttpServerStats(State.Server, &ServerStats) &&
		(ServerStats.Connections == 1) &&
		(ServerStats.Accepted == 1),
		"HTTP lifecycle first connection stats mismatch"
	);
	testHttpServerLifecycleConnect(
		&State,
		&Address,
		&Second
	);
	testHttpServerLifecycleWait(
		&Second.Close,
		1,
		"HTTP lifecycle rejected client did not close"
	);
	{
		xdeadline Deadline = xrtDeadlineAfter(5000000u);

		do {
			testRequire(
				xrtHttpServerStats(
					State.Server,
					&ServerStats
				),
				"HTTP lifecycle rejection stats query failed"
			);
			if ( ServerStats.Rejected == 1 ) {
				break;
			}
			testRequire(
				!xrtDeadlineExpired(Deadline),
				"HTTP lifecycle rejection was not counted"
			);
			xrtThreadYield();
		} while ( true );
	}
	testRequire(
		(ServerStats.Accepted == 1) &&
		(ServerStats.Rejected == 1) &&
		(ServerStats.Connections == 1),
		"HTTP lifecycle connection limit mismatch"
	);

	testRequire(
		(xrtHttpConnRequest(State.Connection) == NULL) &&
		(xrtHttpConnTcp(State.Connection) == NULL) &&
		xrtHttpConnStats(State.Connection, &ConnStats) &&
		ConnStats.RequestActive,
		"HTTP lifecycle off-Worker query mismatch"
	);
	pReply = xrtHttpReplyCreate(103);
	testRequire(
		pReply != NULL,
		"HTTP lifecycle information Reply create failed"
	);
	xrtClearError();
	testRequire(
		xrtHttpConnInform(
			State.Connection,
			pReply
		) == XNET_RESULT_ERROR &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"HTTP lifecycle allowed off-Worker information"
	);
	xrtClearError();
	testRequire(
		xrtHttpConnRespond(
			State.Connection,
			pReply
		) == XNET_RESULT_ERROR &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"HTTP lifecycle allowed off-Worker response"
	);
	xrtHttpReplyDestroy(pReply);
	xrtClearError();

	testRequire(
		xrtHttpServerDrain(State.Server) &&
		(xrtHttpServerState(State.Server) ==
		 XHTTP_SERVER_DRAINING) &&
		xrtHttpServerAbort(State.Server) &&
		xrtHttpServerAbort(State.Server),
		"HTTP lifecycle Drain to Abort transition failed"
	);
	testHttpServerLifecycleWait(
		&State.Close,
		1,
		"HTTP lifecycle active connection did not abort"
	);
	testHttpServerLifecycleWait(
		&State.Shutdown,
		1,
		"HTTP lifecycle server did not shut down"
	);
	testHttpServerLifecycleWait(
		&First.Close,
		1,
		"HTTP lifecycle first client did not close"
	);
	testRequire(
		xrtHttpServerStats(State.Server, &ServerStats) &&
		(ServerStats.State == XHTTP_SERVER_CLOSED) &&
		(ServerStats.Accepted == 1) &&
		(ServerStats.Rejected == 1) &&
		(ServerStats.Requests == 1) &&
		(ServerStats.Responses == 0) &&
		(ServerStats.Connections == 0) &&
		(xrtHttpConnState(State.Connection) ==
		 XHTTP_CONN_CLOSED) &&
		(xrtAtomic32Load(
			&State.Errors,
			XMEMORY_ACQUIRE
		 ) == 0),
		"HTTP lifecycle terminal state mismatch"
	);

	xrtNetStreamDestroy(First.Stream);
	xrtNetStreamDestroy(Second.Stream);
	xrtHttpConnDestroy(State.Connection);
	xrtHttpServerDestroy(State.Server);
	testRequire(
		xrtNetEngineDestroy(State.Engine),
		"HTTP lifecycle engine destroy failed"
	);
	testHttpServerShutdownCleanupOrder();
	printf("[PASS] HTTP server lifecycle boundaries\n");
	return 0;
}

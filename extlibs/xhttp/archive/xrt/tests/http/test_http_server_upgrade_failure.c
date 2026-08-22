#include "../test.h"



/* 保存被中止 Upgrade 的唯一完成与 HTTP Close 计数。 */
typedef struct test_http_server_upgrade_failure {
	xatomic32 Completed;
	xatomic32 HttpClosed;
	xatomic32 Shutdown;
} test_http_server_upgrade_failure;



/* 已受理但未完成的 Upgrade 必须发布一次无传输失败终态。 */
static void testHttpServerUpgradeFailureComplete(
	xhttpconn* pConnection,
	xnetresult Result,
	xhttpupgrade Upgrade,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_upgrade_failure* pState =
		(test_http_server_upgrade_failure*)pData;

	testRequire(
		(Result == XNET_RESULT_CANCELLED) &&
		(Upgrade.Tcp == NULL) &&
		(Upgrade.Tls == NULL) &&
		(Upgrade.Buffered == 0) &&
		(pError == NULL) &&
		(xrtHttpConnState(pConnection) ==
		 XHTTP_CONN_CLOSED),
		"aborted HTTP Upgrade completion mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Completed,
		1,
		XMEMORY_RELEASE
	);
}



/* 用硬写队列上限把 101 留在发送中，再立即中止 Connection。 */
static void testHttpServerUpgradeFailureRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_http_server_upgrade_failure* pState =
		(test_http_server_upgrade_failure*)pData;
	xhttpreply* pReply = xrtHttpReplyCreate(101);

	(void)pServer;
	(void)pRequest;
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
		) &&
		(xrtHttpConnUpgrade(
			pConnection,
			pReply,
			testHttpServerUpgradeFailureComplete,
			pState
		 ) == XNET_RESULT_OK),
		"aborted HTTP Upgrade submission failed"
	);
	xrtHttpReplyDestroy(pReply);
	testRequire(
		xrtHttpConnState(pConnection) ==
			XHTTP_CONN_RESPONSE,
		"HTTP Upgrade unexpectedly completed inside bounded queue"
	);
	testRequire(
		xrtHttpConnAbort(pConnection),
		"pending HTTP Upgrade abort failed"
	);
}



/* 受理 Upgrade 后由完成回调独占终态，不再重复发布 HTTP Close。 */
static void testHttpServerUpgradeFailureClose(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_upgrade_failure* pState =
		(test_http_server_upgrade_failure*)pData;

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



/* 记录 Server 已经回收失败 Upgrade。 */
static void testHttpServerUpgradeFailureShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_http_server_upgrade_failure* pState =
		(test_http_server_upgrade_failure*)pData;

	testRequire(
		xrtHttpServerState(pServer) ==
			XHTTP_SERVER_CLOSED,
		"aborted HTTP Upgrade shutdown state mismatch"
	);
	xrtAtomic32Store(
		&pState->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



/* 等待异步完成或 Server 终态。 */
static void testHttpServerUpgradeFailureWait(
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



/* 完整写入触发 Upgrade 的请求头。 */
static void testHttpServerUpgradeFailureSend(
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
			"aborted HTTP Upgrade request send failed"
		);
		iOffset += iSent;
	}
}



/* 验证受理后中止仍然恰好完成一次且不泄漏 HTTP 所有权。 */
int main(void)
{
	static const uint8 Request[] =
		"GET /abort HTTP/1.1\r\n"
		"Host: upgrade.test\r\n"
		"Connection: Upgrade\r\n"
		"Upgrade: xrt-test\r\n"
		"\r\n";
	test_http_server_upgrade_failure State;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents Events;
	xhttpserverstats Stats;
	xnetengine* pEngine;
	xhttpserver* pServer;
	xnetaddr Address;
	xnetsocket Client;

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.Completed, 0);
	xrtAtomic32Init(&State.HttpClosed, 0);
	xrtAtomic32Init(&State.Shutdown, 0);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) &&
		xrtNetEngineStart(pEngine),
		"aborted HTTP Upgrade engine start failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"aborted HTTP Upgrade address setup failed"
	);
	ServerConfig.WriteSize = 5;
	ServerConfig.Network.Listen.Stream.WriteHighWater = 12;
	ServerConfig.Network.Listen.Stream.WriteLowWater = 4;
	ServerConfig.Network.Listen.Stream.WriteLimit = 16;
	xrtHttpServerEventsInit(&Events);
	Events.Request = testHttpServerUpgradeFailureRequest;
	Events.Close = testHttpServerUpgradeFailureClose;
	Events.Shutdown = testHttpServerUpgradeFailureShutdown;
	Events.Data = &State;
	pServer = xrtHttpServerStart(
		pEngine,
		&ServerConfig,
		&Events
	);
	testRequire(
		(pServer != NULL) &&
		xrtHttpServerLocal(pServer, 0, &Address),
		"aborted HTTP Upgrade server start failed"
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
		"aborted HTTP Upgrade client connect failed"
	);
	testHttpServerUpgradeFailureSend(
		Client,
		Request,
		sizeof(Request) - 1u
	);
	testHttpServerUpgradeFailureWait(
		&State.Completed,
		1,
		"aborted HTTP Upgrade completion missing"
	);
	testRequire(
		xrtHttpServerDrain(pServer),
		"aborted HTTP Upgrade server drain failed"
	);
	testHttpServerUpgradeFailureWait(
		&State.Shutdown,
		1,
		"aborted HTTP Upgrade shutdown missing"
	);
	testRequire(
		xrtNetSocketClose(Client),
		"aborted HTTP Upgrade client close failed"
	);
	testRequire(
		xrtHttpServerStats(pServer, &Stats) &&
		(Stats.Accepted == 1) &&
		(Stats.Requests == 1) &&
		(Stats.Responses == 0) &&
		(Stats.Upgraded == 0) &&
		(Stats.Connections == 0) &&
		(xrtAtomic32Load(
			&State.Completed,
			XMEMORY_ACQUIRE
		 ) == 1) &&
		(xrtAtomic32Load(
			&State.HttpClosed,
			XMEMORY_ACQUIRE
		 ) == 0),
		"aborted HTTP Upgrade ownership mismatch"
	);
	xrtHttpServerDestroy(pServer);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"aborted HTTP Upgrade engine destroy failed"
	);
	printf("[PASS] HTTP server Upgrade failure completion\n");
	return 0;
}

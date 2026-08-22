#include "../test.h"



/* 记录 Upgrade 交接回调与 Server 关闭屏障的严格顺序。 */
typedef struct test_http_server_upgrade_wait {
	xatomic32 Submitted;
	xatomic32 Entered;
	xatomic32 Release;
	xatomic32 Exited;
	xatomic32 Shutdown;
	xatomic32 StreamClosed;
	xnetstream* Tcp;
} test_http_server_upgrade_wait;



/* 在截止时间前等待 Worker 发布指定状态。 */
static void testHttpServerUpgradeWaitValue(
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



/* 对端结束后正常关闭已经转交的新协议 Stream。 */
static void testHttpServerUpgradeEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	(void)xrtNetStreamClose(pStream);
}



/* 独立记录转交后的 Stream 生命周期终态。 */
static void testHttpServerUpgradeStreamClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_upgrade_wait* pState =
		(test_http_server_upgrade_wait*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	xrtAtomic32Store(
		&pState->StreamClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 保持交接回调未返回，以验证关闭 Future 仍处于等待状态。 */
static void testHttpServerUpgradeComplete(
	xhttpconn* pConnection,
	xnetresult Result,
	xhttpupgrade Upgrade,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_upgrade_wait* pState =
		(test_http_server_upgrade_wait*)pData;
	xnetstreamevents Events;

	(void)pConnection;
	testRequire(
		(Result == XNET_RESULT_OK) &&
		(pError == NULL) &&
		(Upgrade.Tcp != NULL) &&
		(Upgrade.Tls == NULL),
		"HTTP Upgrade wait completion mismatch"
	);
	xrtAtomic32Store(&pState->Entered, 1, XMEMORY_RELEASE);
	while ( !xrtAtomic32Load(
		&pState->Release,
		XMEMORY_ACQUIRE
	) ) {
		xrtThreadYield();
	}
	memset(&Events, 0, sizeof(Events));
	Events.End = testHttpServerUpgradeEnd;
	Events.Close = testHttpServerUpgradeStreamClose;
	testRequire(
		xrtNetStreamSetEvents(Upgrade.Tcp, &Events, pState) &&
		xrtNetStreamResume(Upgrade.Tcp),
		"HTTP Upgrade wait Stream handoff failed"
	);
	pState->Tcp = Upgrade.Tcp;
	xrtAtomic32Store(&pState->Exited, 1, XMEMORY_RELEASE);
}



/* 提交最小原始 101，并让交接终态异步进入阻塞回调。 */
static void testHttpServerUpgradeRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_http_server_upgrade_wait* pState =
		(test_http_server_upgrade_wait*)pData;

	(void)pServer;
	(void)pRequest;
	testRequire(
		xrtHttpConnUpgradeRaw(
			pConnection,
			XRT_BYTES_LITERAL(
				"HTTP/1.1 101 Switching Protocols\r\n"
				"Connection: Upgrade\r\n"
				"Upgrade: xrt-wait\r\n"
				"\r\n"
			),
			testHttpServerUpgradeComplete,
			pState
		) == XNET_RESULT_OK,
		"HTTP Upgrade wait submission failed"
	);
	xrtAtomic32Store(&pState->Submitted, 1, XMEMORY_RELEASE);
}



/* Shutdown 必须晚于已经受理的交接回调返回。 */
static void testHttpServerUpgradeShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_http_server_upgrade_wait* pState =
		(test_http_server_upgrade_wait*)pData;

	testRequire(
		(xrtHttpServerState(pServer) == XHTTP_SERVER_CLOSED) &&
		(xrtAtomic32Load(
			&pState->Exited,
			XMEMORY_ACQUIRE
		 ) == 1),
		"HTTP server shutdown preceded Upgrade handoff"
	);
	xrtAtomic32Store(&pState->Shutdown, 1, XMEMORY_RELEASE);
}



/* 完整发送本地 Upgrade 请求。 */
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
			"HTTP Upgrade wait client send failed"
		);
		iOffset += iSent;
	}
}



/* 验证关闭 Future 只等待 HTTP 交接回调，不等待转移后的协议会话。 */
int main(void)
{
	static const uint8 Request[] =
		"GET /wait HTTP/1.1\r\n"
		"Host: upgrade.test\r\n"
		"Connection: Upgrade\r\n"
		"Upgrade: xrt-wait\r\n"
		"\r\n";
	test_http_server_upgrade_wait State;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents Events;
	xnetengine* pEngine;
	xhttpserver* pServer;
	xfuture* pWait;
	xnetaddr Address;
	xnetsocket Client;

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.Submitted, 0);
	xrtAtomic32Init(&State.Entered, 0);
	xrtAtomic32Init(&State.Release, 0);
	xrtAtomic32Init(&State.Exited, 0);
	xrtAtomic32Init(&State.Shutdown, 0);
	xrtAtomic32Init(&State.StreamClosed, 0);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) && xrtNetEngineStart(pEngine),
		"HTTP Upgrade wait engine start failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP Upgrade wait address setup failed"
	);
	xrtHttpServerEventsInit(&Events);
	Events.Request = testHttpServerUpgradeRequest;
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
		"HTTP Upgrade wait server start failed"
	);
	pWait = xrtHttpServerWaitAsync(pServer);
	testRequire(
		(pWait != NULL) &&
		(xrtFutureWaitFor(pWait, 0) == XWAIT_TIMEOUT),
		"HTTP Upgrade close wait did not begin pending"
	);
	Client = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		0
	);
	testRequire(
		(Client != NULL) &&
		(xrtNetSocketConnect(Client, &Address) == XNET_RESULT_OK),
		"HTTP Upgrade wait client connect failed"
	);
	testHttpServerUpgradeSend(
		Client,
		Request,
		sizeof(Request) - 1u
	);
	testHttpServerUpgradeWaitValue(
		&State.Submitted,
		1,
		"HTTP Upgrade wait request was not submitted"
	);
	testRequire(
		xrtHttpServerDrain(pServer),
		"HTTP Upgrade wait server drain failed"
	);
	testHttpServerUpgradeWaitValue(
		&State.Entered,
		1,
		"HTTP Upgrade handoff callback did not start"
	);
	testRequire(
		(xrtFutureWaitFor(
			pWait,
			UINT64_C(250000)
		 ) == XWAIT_TIMEOUT) &&
		(xrtAtomic32Load(
			&State.Shutdown,
			XMEMORY_ACQUIRE
		 ) == 0),
		"HTTP server closed while Upgrade handoff was running"
	);
	xrtAtomic32Store(&State.Release, 1, XMEMORY_RELEASE);
	testRequire(
		(xrtFutureWaitFor(
			pWait,
			UINT64_C(5000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pWait) == XFUTURE_RESOLVED) &&
		(xrtAtomic32Load(
			&State.Exited,
			XMEMORY_ACQUIRE
		 ) == 1) &&
		(xrtAtomic32Load(
			&State.Shutdown,
			XMEMORY_ACQUIRE
		 ) == 1),
		"HTTP server did not close after Upgrade handoff"
	);
	testRequire(
		(State.Tcp != NULL) &&
		(xrtNetStreamState(State.Tcp) == XNET_STREAM_OPEN),
		"HTTP close wait retained transferred Stream lifetime"
	);
	testRequire(
		xrtNetSocketClose(Client),
		"HTTP Upgrade wait client close failed"
	);
	testHttpServerUpgradeWaitValue(
		&State.StreamClosed,
		1,
		"transferred HTTP Upgrade Stream did not close"
	);
	xrtNetStreamDestroy(State.Tcp);
	xrtFutureDestroy(pWait);
	xrtHttpServerDestroy(pServer);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"HTTP Upgrade wait engine destroy failed"
	);
	printf("[PASS] HTTP Upgrade shutdown handoff barrier (select)\n");
	return 0;
}

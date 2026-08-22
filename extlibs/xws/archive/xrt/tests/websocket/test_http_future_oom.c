#include "../test.h"



/* 在截止时间前等待 Engine 回到指定对象与定时器基线。 */
static void testWsFutureOomWaitBaseline(
	xnetengine* pEngine,
	const xnetenginestats* pBefore,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(
		UINT64_C(10000000)
	);
	xnetenginestats After;

	for ( ;; ) {
		testRequire(
			xrtNetEngineStats(pEngine, &After),
			"WebSocket Future OOM stats failed"
		);
		if ( (After.LiveObjects ==
			 pBefore->LiveObjects) &&
			(After.ActiveTimers ==
			 pBefore->ActiveTimers) ) {
			return;
		}
		testRequire(
			!xrtDeadlineExpired(Deadline),
			sMessage
		);
		xrtClearError();
		xrtThreadYield();
	}
}



/* 扫描同步分配点，确认任一失败都能取消并回到提交前基线。 */
static void testWsFutureOomClient(
	xnetengine* pEngine,
	xhttpclient* pClient
)
{
	static const xstrview Url =
		XRT_STR_INIT("ws://127.0.0.1:1/oom");
	xnetenginestats Before;
	xfuture* pFuture;
	bool bSucceeded = false;
	size_t iFailures = 0;

	for ( uint32 iAllow = 0;
		iAllow < 128u;
		iAllow++ ) {
		bool bTriggered;

		testRequire(
			xrtNetEngineStats(pEngine, &Before),
			"WebSocket Future OOM baseline failed"
		);
		testRequire(
			xrtMemDebugFailAfter((uint64)iAllow),
			"WebSocket Future OOM injection failed"
		);
		pFuture = xrtWsConnectAsync(
			pClient,
			Url,
			NULL,
			NULL,
			NULL
		);
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( pFuture == NULL ) {
			testRequire(
				bTriggered &&
				(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
				"WebSocket client Future OOM mismatch"
			);
			iFailures++;
			xrtClearError();
		} else {
			testRequire(
				!bTriggered,
				"WebSocket client Future ignored an allocation fault"
			);
			(void)xrtFutureCancel(pFuture);
			testRequire(
				xrtFutureWaitFor(
					pFuture,
					UINT64_C(10000000)
				) == XWAIT_OK,
				"WebSocket Future OOM cancellation did not finish"
			);
			xrtFutureDestroy(pFuture);
			bSucceeded = true;
		}
		testWsFutureOomWaitBaseline(
			pEngine,
			&Before,
			"WebSocket client Future OOM changed Engine ownership"
		);
		if ( bSucceeded ) {
			break;
		}
	}
	testRequire(
		bSucceeded &&
		(iFailures >= 3u),
		"WebSocket client Future OOM scan missed bridge stages"
	);
}



typedef struct test_ws_server_future_oom {
	xatomic32 RequestDone;
	xatomic32 Shutdown;
} test_ws_server_future_oom;



/* 精确拒绝服务端 Future 首次分配，再证明普通响应门仍可使用。 */
static void testWsFutureOomRequest(
	xhttpserver* pServer,
	xhttpconn* pHttp,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_ws_server_future_oom* pTest =
		(test_ws_server_future_oom*)pData;
	xfuture* pFuture;
	bool bTriggered;

	(void)pServer;
	(void)pRequest;
	testRequire(
		xrtMemDebugFailAfter(0),
		"WebSocket server Future OOM injection failed"
	);
	pFuture = xrtWsUpgradeAsync(
		pHttp,
		NULL,
		NULL,
		NULL
	);
	bTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	testRequire(
		(pFuture == NULL) &&
		bTriggered &&
		(xrtErrorKind(xrtGetError()) ==
		 XERR_MEMORY),
		"WebSocket server Future OOM mismatch"
	);
	xrtClearError();
	testRequire(
		xrtHttpConnReply(
			pHttp,
			XHTTP_STATUS_SERVICE_UNAVAILABLE,
			XRT_STR_LITERAL(
				"text/plain; charset=utf-8"
			),
			XRT_BYTES_LITERAL("Unavailable")
		) == XNET_RESULT_OK,
		"WebSocket server Future OOM occupied response gate"
	);
	xrtAtomic32Store(
		&pTest->RequestDone,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 OOM 场景 HTTP Server 已排空。 */
static void testWsFutureOomShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_ws_server_future_oom* pTest =
		(test_ws_server_future_oom*)pData;

	(void)pServer;
	xrtAtomic32Store(
		&pTest->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



/* 在截止时间前等待一个原子标志。 */
static void testWsFutureOomWait(
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



/* 验证服务端 Future 分配失败不污染 HTTP 最终响应状态。 */
static void testWsFutureOomServer(
	xnetengine* pEngine,
	xhttpclient* pClient
)
{
	test_ws_server_future_oom Test;
	xhttpserverconfig ServerConfig;
	xhttpserverevents ServerEvents;
	xnetaddr Address;
	xhttpserver* pServer;
	xfuture* pFuture;
	char Url[160];
	int iLength;

	memset(&Test, 0, sizeof(Test));
	xrtAtomic32Init(&Test.RequestDone, 0);
	xrtAtomic32Init(&Test.Shutdown, 0);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"WebSocket server Future OOM address failed"
	);
	xrtHttpServerEventsInit(&ServerEvents);
	ServerEvents.Request = testWsFutureOomRequest;
	ServerEvents.Shutdown =
		testWsFutureOomShutdown;
	ServerEvents.Data = &Test;
	pServer = xrtHttpServerStart(
		pEngine,
		&ServerConfig,
		&ServerEvents
	);
	testRequire(
		(pServer != NULL) &&
		xrtHttpServerLocal(pServer, 0, &Address),
		"WebSocket server Future OOM start failed"
	);
	iLength = snprintf(
		Url,
		sizeof(Url),
		"ws://127.0.0.1:%u/oom",
		(unsigned)Address.Port
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Url)),
		"WebSocket server Future OOM URL overflowed"
	);
	pFuture = xrtWsConnectAsync(
		pClient,
		(xstrview) { Url, (size_t)iLength },
		NULL,
		NULL,
		NULL
	);
	testRequire(
		pFuture != NULL,
		"WebSocket server Future OOM client submission failed"
	);
	testWsFutureOomWait(
		&Test.RequestDone,
		"WebSocket server Future OOM request did not finish"
	);
	testRequire(
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(10000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pFuture) ==
		 XFUTURE_FAILED),
		"WebSocket server Future OOM client terminal mismatch"
	);
	(void)xrtFutureValue(pFuture);
	xrtClearError();
	xrtFutureDestroy(pFuture);
	testRequire(
		xrtHttpServerDrain(pServer),
		"WebSocket server Future OOM drain failed"
	);
	testWsFutureOomWait(
		&Test.Shutdown,
		"WebSocket server Future OOM did not drain"
	);
	xrtHttpServerDestroy(pServer);
}



/* 覆盖客户端桥接扫描、服务端首分配失败和最终资源归零。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xhttpclientconfig ClientConfig;
	xnetengine* pEngine;
	xhttpclient* pClient;
	xdeadline Deadline;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) &&
		xrtNetEngineStart(pEngine),
		"WebSocket Future OOM engine start failed"
	);
	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Dial.Family = XNET_FAMILY_IPV4;
	ClientConfig.Dial.MaxAttempts = 1;
	pClient = xrtHttpClientCreate(
		pEngine,
		&ClientConfig
	);
	testRequire(
		pClient != NULL,
		"WebSocket Future OOM client create failed"
	);

	testWsFutureOomClient(
		pEngine,
		pClient
	);
	testWsFutureOomServer(
		pEngine,
		pClient
	);
	xrtHttpClientDestroy(pClient);

	Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	while ( !xrtNetEngineDestroy(pEngine) ) {
		xrtClearError();
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"WebSocket Future OOM retained an Engine object"
		);
		xrtThreadYield();
	}
	testMemoryDebugDrain(
		"WebSocket Future OOM leaked storage"
	);
	printf("[PASS] WebSocket HTTP Future OOM recovery\n");
	return 0;
}

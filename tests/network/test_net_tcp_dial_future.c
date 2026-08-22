#include "../test.h"



#if !defined(TEST_TCP_DIAL_BACKEND)
	#define TEST_TCP_DIAL_BACKEND XNET_PORT_SELECT
#endif



typedef struct testdialfuture {
	xatomic32 Entered;
	xatomic32 Gate;
} testdialfuture;



/* 返回 IPv4 回环；slow 主机在测试放行前保持底层查询运行。 */
static xnetaddrlist* testDialFutureLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	testdialfuture* pContext = (testdialfuture*)pData;
	xnetaddr Address;

	(void)Family;
	if ( strncmp(sHost, "slow-", 5) == 0 ) {
		(void)xrtAtomic32FetchAdd(
			&pContext->Entered,
			1,
			XMEMORY_RELEASE
		);
		while ( xrtAtomic32Load(
			&pContext->Gate,
			XMEMORY_ACQUIRE
		) == 0 ) {
			xrtThreadYield();
		}
	}
	if ( !xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) ) {
		return NULL;
	}
	return xrtNetAddrListCreate(&Address, 1);
}



/* 在截止时间内轮询拉取一个已接受 Stream。 */
static xnetstream* testDialFutureAccept(xnetlistener* pListener)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);
	xnetstream* pStream;

	while ( (pStream = xrtNetListenerAccept(pListener)) == NULL ) {
		xrtClearError();
		testRequire(!xrtDeadlineExpired(iDeadline),
			"dial Future server accept timed out");
		xrtThreadYield();
	}
	return pStream;
}



/* 等待 Stream 或 Listener 到达关闭终态。 */
static void testDialFutureWaitClosed(
	const xnetstream* pStream,
	const xnetlistener* pListener,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	while ( ((pStream != NULL) &&
		  (xrtNetStreamState(pStream) != XNET_STREAM_CLOSED)) ||
		 ((pListener != NULL) &&
		  (xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED)) ) {
		testRequire(!xrtDeadlineExpired(iDeadline), sMessage);
		xrtThreadYield();
	}
}



/* 验证成功值所有权、完成时 OPEN 状态和 Future 取消传播。 */
int main(void)
{
	testdialfuture Context;
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	xnetstream* pClient;
	xnetstream* pServer;
	xfuture* pConnect;
	xfuture* pCancel;
	xnetaddr Address;
	xdeadline iDeadline;

	memset(&Context, 0, sizeof(Context));
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TCP_DIAL_BACKEND;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"dial Future engine start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 2;
	ResolverConfig.Lookup = testDialFutureLookup;
	ResolverConfig.LookupData = &Context;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL, "dial Future resolver create failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "dial Future listener address failed");
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		NULL,
		NULL,
		NULL
	);
	testRequire((pListener != NULL) &&
		xrtNetListenerLocal(pListener, &Address),
		"dial Future listener create failed");
	pConnect = xrtNetDialAsync(
		pEngine,
		pResolver,
		"future.test",
		Address.Port,
		NULL,
		NULL,
		NULL
	);
	testRequire(pConnect != NULL, "dial Future submit failed");
	testRequire(xrtFutureWaitFor(pConnect, 5000000u) == XWAIT_OK &&
		(xrtFutureState(pConnect) == XFUTURE_RESOLVED),
		"dial Future did not resolve");
	pClient = (xnetstream*)xrtFutureValue(pConnect);
	testRequire((pClient != NULL) &&
		(xrtNetStreamState(pClient) == XNET_STREAM_OPEN),
		"dial Future resolved before Stream Open");
	pServer = testDialFutureAccept(pListener);
	testRequire(xrtNetStreamClose(pClient) &&
		xrtNetStreamClose(pServer),
		"dial Future streams close failed");
	testDialFutureWaitClosed(
		pClient,
		NULL,
		"dial Future client did not close"
	);
	testDialFutureWaitClosed(
		pServer,
		NULL,
		"dial Future server did not close"
	);
	xrtFutureDestroy(pConnect);
	xrtNetStreamDestroy(pServer);

	pCancel = xrtNetDialAsync(
		pEngine,
		pResolver,
		"slow-future.test",
		Address.Port,
		NULL,
		NULL,
		NULL
	);
	testRequire(pCancel != NULL, "cancelled dial Future submit failed");
	iDeadline = xrtDeadlineAfter(5000000u);
	while ( xrtAtomic32Load(&Context.Entered, XMEMORY_ACQUIRE) == 0 ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"cancelled dial Future lookup did not start");
		xrtThreadYield();
	}
	testRequire(xrtFutureCancel(pCancel),
		"dial Future cancellation request failed");
	testRequire(xrtFutureWaitFor(pCancel, 5000000u) == XWAIT_OK &&
		(xrtFutureState(pCancel) == XFUTURE_CANCELLED),
		"dial Future cancellation was not confirmed");
	xrtAtomic32Store(&Context.Gate, 1, XMEMORY_RELEASE);
	xrtFutureDestroy(pCancel);

	testRequire(xrtNetListenerClose(pListener),
		"dial Future listener close failed");
	testDialFutureWaitClosed(
		NULL,
		pListener,
		"dial Future listener did not close"
	);
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"dial Future resolver destroy failed");
	iDeadline = xrtDeadlineAfter(5000000u);
	while ( !xrtNetEngineDestroy(pEngine) ) {
		xrtClearError();
		testRequire(!xrtDeadlineExpired(iDeadline),
			"dial Future retained an internal engine resource");
		xrtThreadYield();
	}
	printf("[PASS] managed TCP dial Future lifecycle\n");
	return 0;
}

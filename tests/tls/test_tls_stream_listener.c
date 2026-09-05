#include "../fixtures/tls_server.h"



/* 记录 push 模式下完成握手的连接与关闭事件。 */
typedef struct test_tls_listener_push {
	xtlsstream* Stream;
	xatomic32 Accepted;
	xatomic32 Closed;
	xatomic32 ListenerClosed;
} test_tls_listener_push;



/* push 回调接管已认证 Stream 引用。 */
static bool testTlsListenerPushAccept(
	xtlslistener* pListener,
	xtlsstream* pStream,
	ptr pData
)
{
	test_tls_listener_push* pState =
		(test_tls_listener_push*)pData;

	(void)pListener;
	pState->Stream = pStream;
	(void)xrtAtomic32FetchAdd(
		&pState->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 记录 TLS Stream 唯一终态。 */
static void testTlsListenerStreamClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_tls_listener_push* pState =
		(test_tls_listener_push*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pState->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 TLS Listener 完成关闭。 */
static void testTlsListenerClose(
	xtlslistener* pListener,
	ptr pData
)
{
	test_tls_listener_push* pState =
		(test_tls_listener_push*)pData;

	(void)pListener;
	(void)xrtAtomic32FetchAdd(
		&pState->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 等待原子计数达到目标值。 */
static void testTlsListenerWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(10000000));

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}



/* 等待 Listener 的活动握手数达到目标值。 */
static void testTlsListenerWaitHandshakes(
	xtlslistener* pListener,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	xtlslistenerstats Stats;

	for ( ;; ) {
		testRequire(xrtTlsListenerStats(pListener, &Stats),
			"TLS Listener stats failed while waiting");
		if ( Stats.ActiveHandshakes == iExpected ) {
			return;
		}
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}



/* 覆盖 push 快速路径、真实证书握手、配置快照、统计与独立关闭。 */
int main(void)
{
	test_tls_listener_push State;
	static const xstrview Protocols[] = {
		XRT_STR_INIT("xrt-test")
	};
	xtlslistenerconfig ListenerConfig;
	xtlslistenerevents ListenerEvents;
	xtlsstreamevents StreamEvents;
	xtlsclientconfig ClientConfig;
	xtlsverifierconfig VerifierConfig;
	xtlslistenerstats Stats;
	xnetengineconfig EngineConfig;
	xtlscontext* pContext;
	xtlsidentity* pIdentity;
	xtlsverifier* pVerifier;
	xnetengine* pEngine;
	xtlslistener* pListener;
	xtlsstream* pClient;
	xnetstream* pStalled;
	xfuture* pClientOpen;
	xnetaddr Address;

	memset(&State, 0, sizeof(State));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&StreamEvents, 0, sizeof(StreamEvents));
	ListenerEvents.Accept = testTlsListenerPushAccept;
	ListenerEvents.Close = testTlsListenerClose;
	StreamEvents.Close = testTlsListenerStreamClose;
	pContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	testRequire((pContext != NULL) && (pIdentity != NULL),
		"TLS Listener fixture creation failed");
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(pVerifier != NULL,
		"TLS Listener verifier creation failed");

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TLS Listener engine start failed");
	xrtTlsListenerConfigInit(&ListenerConfig);
	testRequire((ListenerConfig.HandshakeLimit == 128u) &&
		(ListenerConfig.AcceptQueueLimit == 1024u),
		"TLS Listener default resource budgets differ");
	testRequire(xrtNetAddrLoopback(
		&ListenerConfig.Listen.Address,
		XNET_FAMILY_IPV4,
		0
	), "TLS Listener loopback address failed");
	ListenerConfig.Listen.AcceptConcurrency = 2;
	ListenerConfig.Tls.Context = pContext;
	ListenerConfig.Tls.Identity = pIdentity;
	ListenerConfig.Tls.Protocols = Protocols;
	ListenerConfig.Tls.ProtocolCount = 1;
	ListenerConfig.Tls.RequireProtocol = true;
	pListener = xrtTlsListenerStart(
		pEngine,
		&ListenerConfig,
		&ListenerEvents,
		&StreamEvents,
		&State
	);
	testRequire((pListener != NULL) &&
		xrtTlsListenerLocal(pListener, &Address),
		"TLS Listener start failed");
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);

	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.ServerName = XRT_STR_LITERAL("example.com");
	ClientConfig.Protocols = Protocols;
	ClientConfig.ProtocolCount = 1;
	ClientConfig.Verifier = pVerifier;
	pClient = xrtTlsStreamConnect(
		pEngine,
		&Address,
		1,
		NULL,
		&ClientConfig,
		NULL,
		&StreamEvents,
		&State
	);
	testRequire(pClient != NULL,
		"TLS Listener client connect failed");
	pClientOpen = xrtTlsStreamWaitAsync(
		pClient,
		XTLS_STREAM_WAIT_OPEN
	);
	testRequire((pClientOpen != NULL) &&
		(xrtFutureWaitFor(
			pClientOpen,
			UINT64_C(10000000)
		) == XWAIT_OK) &&
		(xrtFutureState(pClientOpen) == XFUTURE_RESOLVED),
		"TLS Listener client handshake failed");
	xrtFutureDestroy(pClientOpen);
	testTlsListenerWait(
		&State.Accepted,
		1,
		"TLS Listener did not publish push accept"
	);
	testRequire(xrtTlsListenerStats(pListener, &Stats) &&
		(Stats.Handshakes == 1) &&
		(Stats.Accepted == 1) &&
		(Stats.Rejected == 0) &&
		(Stats.ActiveHandshakes == 0) &&
		(Stats.PeakHandshakes == 1),
		"TLS Listener stats mismatch");
	pStalled = xrtNetStreamConnect(
		pEngine,
		&Address,
		0,
		NULL,
		NULL,
		NULL
	);
	testRequire(pStalled != NULL,
		"TLS Listener stalled transport connect failed");
	testTlsListenerWaitHandshakes(
		pListener,
		1,
		"TLS Listener did not track stalled handshake"
	);

	testRequire(xrtTlsListenerClose(pListener),
		"TLS Listener close failed");
	testTlsListenerWait(
		&State.ListenerClosed,
		1,
		"TLS Listener close event missing"
	);
	testTlsListenerWaitHandshakes(
		pListener,
		0,
		"TLS Listener close left an active handshake"
	);
	testRequire(xrtTlsListenerStats(pListener, &Stats) &&
		(Stats.Handshakes == 2) &&
		(Stats.Accepted == 1) &&
		(Stats.Rejected == 1) &&
		(Stats.HandshakeErrors == 0),
		"TLS Listener close statistics mismatch");
	testRequire((xrtTlsStreamState(pClient) == XTLS_STREAM_OPEN) &&
		(xrtTlsStreamState(State.Stream) == XTLS_STREAM_OPEN),
		"TLS Listener close affected accepted streams");
	(void)xrtTlsStreamAbort(pClient);
	(void)xrtTlsStreamAbort(State.Stream);
	testTlsListenerWait(
		&State.Closed,
		3,
		"TLS Listener streams did not close"
	);
	(void)xrtNetStreamAbort(pStalled);
	xrtNetStreamDestroy(pStalled);
	xrtTlsStreamDestroy(pClient);
	xrtTlsStreamDestroy(State.Stream);
	xrtTlsListenerDestroy(pListener);
	xrtTlsVerifierRelease(pVerifier);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TLS Listener engine destroy failed");
	printf("[PASS] TLS Stream Listener\n");
	return 0;
}

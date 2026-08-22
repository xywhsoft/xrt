#include "../fixtures/tls_server.h"



#if !defined(TEST_TLS_STREAM_BACKEND)
	#define TEST_TLS_STREAM_BACKEND XNET_PORT_SELECT
#endif



/* 原始服务端故意不响应 ClientHello，用于验证独立握手 Timer。 */
typedef struct test_tls_stream_timeout {
	xnetstream* Server;
	xatomic32 Accepted;
	xatomic32 ServerOpen;
	xatomic32 ServerClose;
	xatomic32 ClientClose;
	xatomic32 ListenerClose;
	xatomic32 Result;
	xatomic32 Kind;
	xatomic32 Code;
} test_tls_stream_timeout;



/* 在测试截止时间前等待原子计数达到给定值。 */
static void testTlsStreamTimeoutWait(
	xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(5000000u);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}



/* 接管原始 TCP 服务端引用，但不处理收到的 ClientHello。 */
static bool testTlsStreamTimeoutAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	test_tls_stream_timeout* pTest =
		(test_tls_stream_timeout*)pData;

	(void)pListener;
	testRequire(xrtNetStreamSetData(pStream, pTest),
		"TLS timeout server data setup failed");
	pTest->Server = pStream;
	(void)xrtAtomic32FetchAdd(
		&pTest->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 记录原始服务端已经开放。 */
static void testTlsStreamTimeoutServerOpen(
	xnetstream* pStream,
	ptr pData
)
{
	test_tls_stream_timeout* pTest =
		(test_tls_stream_timeout*)pData;

	(void)pStream;
	(void)xrtAtomic32FetchAdd(
		&pTest->ServerOpen,
		1,
		XMEMORY_RELEASE
	);
}



/* 客户端超时异常关闭后，原始服务端结束自己的写方向。 */
static void testTlsStreamTimeoutServerEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	testRequire(xrtNetStreamClose(pStream),
		"TLS timeout server close request failed");
}



/* 记录原始服务端终态。 */
static void testTlsStreamTimeoutServerClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_tls_stream_timeout* pTest =
		(test_tls_stream_timeout*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pTest->ServerClose,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录超时测试 Listener 唯一关闭回调。 */
static void testTlsStreamTimeoutListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_tls_stream_timeout* pTest =
		(test_tls_stream_timeout*)pData;

	(void)pListener;
	(void)xrtAtomic32FetchAdd(
		&pTest->ListenerClose,
		1,
		XMEMORY_RELEASE
	);
}



/* 保存 TLS 客户端超时终态的稳定字段。 */
static void testTlsStreamTimeoutClientClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_tls_stream_timeout* pTest =
		(test_tls_stream_timeout*)pData;

	testRequire(xrtTlsStreamState(pStream) == XTLS_STREAM_FAILED,
		"TLS timeout did not publish FAILED");
	xrtAtomic32Store(&pTest->Result, (uint32)Result, XMEMORY_RELAXED);
	xrtAtomic32Store(
		&pTest->Kind,
		(uint32)(pError != NULL ? xrtErrorKind(pError) : XERR_NONE),
		XMEMORY_RELAXED
	);
	xrtAtomic32Store(
		&pTest->Code,
		(uint32)(pError != NULL ? xrtErrorCode(pError) : 0),
		XMEMORY_RELAXED
	);
	(void)xrtAtomic32FetchAdd(
		&pTest->ClientClose,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证握手超时拥有独立结果和 TLS 根因。 */
int main(void)
{
	test_tls_stream_timeout Test;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents ServerEvents;
	xtlsstreamevents ClientEvents;
	xtlsstreamconfig StreamConfig;
	xtlsclientconfig ClientConfig;
	xtlscontext* pContext;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xtlsstream* pClient;
	xnetaddr Address;

	memset(&Test, 0, sizeof(Test));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&ServerEvents, 0, sizeof(ServerEvents));
	memset(&ClientEvents, 0, sizeof(ClientEvents));
	ListenerEvents.Accept = testTlsStreamTimeoutAccept;
	ListenerEvents.Close = testTlsStreamTimeoutListenerClose;
	ServerEvents.Open = testTlsStreamTimeoutServerOpen;
	ServerEvents.End = testTlsStreamTimeoutServerEnd;
	ServerEvents.Close = testTlsStreamTimeoutServerClose;
	ClientEvents.Close = testTlsStreamTimeoutClientClose;
	pContext = testTlsServerContext();
	testRequire(pContext != NULL, "TLS timeout context creation failed");
	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.Context = pContext;
	ClientConfig.ServerName = XRT_STR_LITERAL("example.com");
	xrtTlsStreamConfigInit(&StreamConfig);
	StreamConfig.HandshakeTimeout = 50000u;
	StreamConfig.CloseTimeout = 50000u;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TLS_STREAM_BACKEND;
	EngineConfig.Workers = 2u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TLS timeout engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "TLS timeout loopback address setup failed");
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		&ServerEvents,
		&Test
	);
	testRequire((pListener != NULL) &&
		xrtNetListenerLocal(pListener, &Address),
		"TLS timeout listener creation failed");
	pClient = xrtTlsStreamConnect(
		pEngine,
		&Address,
		1u,
		NULL,
		&ClientConfig,
		&StreamConfig,
		&ClientEvents,
		&Test
	);
	testRequire(pClient != NULL, "TLS timeout client creation failed");
	testTlsStreamTimeoutWait(&Test.Accepted, 1u,
		"TLS timeout server was not accepted");
	testTlsStreamTimeoutWait(&Test.ServerOpen, 1u,
		"TLS timeout server Open callback missing");
	testTlsStreamTimeoutWait(&Test.ClientClose, 1u,
		"TLS handshake timeout Close callback missing");
	testTlsStreamTimeoutWait(&Test.ServerClose, 1u,
		"TLS timeout raw server Close callback missing");
	testRequire((xnetresult)xrtAtomic32Load(
		&Test.Result,
		XMEMORY_ACQUIRE
	) == XNET_RESULT_TIMEOUT && (xerrkind)xrtAtomic32Load(
		&Test.Kind,
		XMEMORY_ACQUIRE
	) == XERR_TIMEOUT && (xtlserror)xrtAtomic32Load(
		&Test.Code,
		XMEMORY_ACQUIRE
	) == XTLS_ERROR_HANDSHAKE,
		"TLS handshake timeout result mismatch");

	testRequire(xrtNetListenerClose(pListener),
		"TLS timeout listener close failed");
	testTlsStreamTimeoutWait(&Test.ListenerClose, 1u,
		"TLS timeout listener Close callback missing");
	xrtTlsStreamDestroy(pClient);
	xrtNetStreamDestroy(Test.Server);
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TLS timeout engine destroy failed");
	xrtTlsContextRelease(pContext);
	printf("[PASS] TLS stream handshake timeout\n");
	return 0;
}

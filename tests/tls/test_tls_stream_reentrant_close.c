/*
	确定性回归：Send 内同步重入 TransportClose 时不得提前释放运行时对象。

	在客户端 Open 回调中释放调用方引用（只保留运行时引用），置位底层
	WriteEnded 后调用 xrtTlsStreamSend()。SendBuffer -> DriveWrite ->
	TCP Fail -> TransportClose 在同一调用栈内稳定发生，修复前表现为
	Use-After-Free（SendMoved 读取已释放的 Session），修复后由
	ActiveDepth 延迟到最外层传输回调退出后释放。
*/
#include "../fixtures/tls_server.h"

#include "../../src/internal/xrt_tcp.h"

#if !defined(TEST_TLS_STREAM_REENTRANT_CLOSE)
	#define TEST_TLS_STREAM_REENTRANT_CLOSE
#endif



/* 只依赖原子事件结果，释放后的 Stream 指针不再被主线程触碰。 */
typedef struct test_reentrant_context {
	xtlsserverconfig ServerConfig;
	xtlsstreamconfig StreamConfig;
	xtlsstreamevents ServerEvents;
	xtlsstreamevents ClientEvents;
	xatomic32 Accepted;
	xatomic32 ClientOpen;
	xatomic32 ClientClose;
	xatomic32 ServerOpen;
	xatomic32 ServerClose;
	xatomic32 SendDone;
	xatomic32 SendResult;
	xatomic32 SendWritten;
} test_reentrant_context;



/* 在截止时间前等待原子计数达到给定值。 */
static void testReentrantWait(
	xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(10000000u);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}



/* 客户端 Open：释放调用方引用，制造只剩运行时引用的终局条件。 */
static void testReentrantClientOpen(xtlsstream* pStream, ptr pData)
{
	test_reentrant_context* pTest = (test_reentrant_context*)pData;
	xnetstream* pTransport;
	size_t iWritten = 0;
	xtlsresult Result;

	(void)xrtAtomic32FetchAdd(&pTest->ClientOpen, 1, XMEMORY_RELEASE);

	/* 只保留运行时引用：调用方引用在此释放。 */
	xrtTlsStreamDestroy(pStream);

	pTransport = xrtTlsStreamTransport(pStream);
	testRequire(pTransport != NULL,
		"reentrant close transport lookup failed");

	/* 置位写结束，让本次 Send 在同一栈内同步走完 TCP 关闭。 */
	xrtAtomic32Store(&pTransport->WriteEnded, 1, XMEMORY_RELEASE);

	Result = xrtTlsStreamSend(pStream, "X", 1u, &iWritten);

	/* 修复后此处仍然安全：释放被延迟到最外层回调退出。 */
	xrtAtomic32Store(&pTest->SendResult, (uint32)Result,
		XMEMORY_RELEASE);
	xrtAtomic32Store(&pTest->SendWritten, (uint32)iWritten,
		XMEMORY_RELEASE);
	xrtAtomic32Store(&pTest->SendDone, 1, XMEMORY_RELEASE);
}



/* 客户端终态：只记录事实，不再触碰 Stream。 */
static void testReentrantClientClose(
	xtlsstream* pStream, xnetresult iResult,
	const xerror* pError, ptr pData
)
{
	test_reentrant_context* pTest = (test_reentrant_context*)pData;

	(void)pStream;
	(void)iResult;
	(void)pError;
	(void)xrtAtomic32FetchAdd(&pTest->ClientClose, 1, XMEMORY_RELEASE);
}



/* 服务端消费对端数据，EOF 后按认证关闭收敛。 */
static void testReentrantServerRead(
	xtlsstream* pStream, const xnetbuf* pBuffer, ptr pData
)
{
	size_t iTotal = 0;

	(void)pData;
	while ( xrtTlsStreamAvailable(pStream) != 0 ) {
		const xnetbuf* pFront = xrtTlsStreamBuffer(pStream);
		xnetspan Spans[8];
		size_t iCount;
		size_t iTake = 0;

		if ( pFront == NULL ) {
			return;
		}
		iCount = xrtNetBufSpans(pFront, Spans, 8);
		for ( size_t i = 0; i < iCount; i++ ) {
			iTake += Spans[i].Size;
		}
		if ( ( iTake == 0 ) || !xrtTlsStreamConsume(pStream, iTake) ) {
			return;
		}
		iTotal += iTake;
	}
	(void)iTotal;
	(void)pBuffer;
}



static void testReentrantServerOpen(xtlsstream* pStream, ptr pData)
{
	test_reentrant_context* pTest = (test_reentrant_context*)pData;

	(void)pStream;
	(void)xrtAtomic32FetchAdd(&pTest->ServerOpen, 1, XMEMORY_RELEASE);
}



static void testReentrantServerClose(
	xtlsstream* pStream, xnetresult iResult,
	const xerror* pError, ptr pData
)
{
	test_reentrant_context* pTest = (test_reentrant_context*)pData;

	(void)pStream;
	(void)iResult;
	(void)pError;
	(void)xrtAtomic32FetchAdd(&pTest->ServerClose, 1, XMEMORY_RELEASE);
}



/* 在 TCP Accept 回调内接管服务端 TLS 组合对象。 */
static bool testReentrantAccept(
	xnetlistener* pListener, xnetstream* pTransport, ptr pData
)
{
	test_reentrant_context* pTest = (test_reentrant_context*)pData;
	xtlsstream* pStream = NULL;
	bool bAccepted;

	(void)pListener;
	bAccepted = xrtTlsStreamAccept(
		pTransport,
		&pTest->ServerConfig,
		&pTest->StreamConfig,
		&pTest->ServerEvents,
		pTest,
		&pStream
	);
	if ( bAccepted && ( pStream != NULL ) ) {
		xrtTlsStreamDestroy(pStream);
	}
	if ( bAccepted ) {
		(void)xrtAtomic32FetchAdd(
			&pTest->Accepted, 1, XMEMORY_RELEASE
		);
	}
	return bAccepted;
}



int main(void)
{
	static const xstrview Protocols[] = {
		XRT_STR_INIT("http/1.1")
	};
	test_reentrant_context Test;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetstreamconfig TransportConfig;
	xnetlistenerevents ListenerEvents;
	xtlsverifierconfig VerifierConfig;
	xtlsclientconfig ClientConfig;
	xtlscontext* pContext;
	xtlsidentity* pIdentity;
	xtlsverifier* pVerifier;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xnetaddr Address;
	xtlsstream* pClient;
	xnetenginestats Stats;

	memset(&Test, 0, sizeof(Test));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	ListenerEvents.Accept = testReentrantAccept;

	pContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	testRequire((pContext != NULL) && (pIdentity != NULL),
		"reentrant close fixture creation failed");
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(pVerifier != NULL,
		"reentrant close verifier creation failed");

	xrtTlsServerConfigInit(&Test.ServerConfig);
	Test.ServerConfig.Context = pContext;
	Test.ServerConfig.Identity = pIdentity;
	Test.ServerConfig.Protocols = Protocols;
	Test.ServerConfig.ProtocolCount = 1u;
	Test.ServerConfig.RequireProtocol = true;
	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.Context = pContext;
	ClientConfig.ServerName = XRT_STR_LITERAL("example.com");
	ClientConfig.Protocols = Protocols;
	ClientConfig.ProtocolCount = 1u;
	ClientConfig.Verifier = pVerifier;
	xrtTlsStreamConfigInit(&Test.StreamConfig);
	memset(&Test.ServerEvents, 0, sizeof(Test.ServerEvents));
	Test.ServerEvents.Open = testReentrantServerOpen;
	Test.ServerEvents.Read = testReentrantServerRead;
	Test.ServerEvents.Close = testReentrantServerClose;
	memset(&Test.ClientEvents, 0, sizeof(Test.ClientEvents));

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"reentrant close engine start failed");

	xrtNetListenConfigInit(&ListenConfig);
	xrtNetStreamConfigInit(&TransportConfig);
	ListenConfig.Stream = TransportConfig;
	ListenConfig.AcceptConcurrency = 4u;
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address, XNET_FAMILY_IPV4, 0
	), "reentrant close loopback address failed");
	pListener = xrtNetListen(
		pEngine, &ListenConfig, &ListenerEvents, NULL, &Test
	);
	testRequire((pListener != NULL) &&
		xrtNetListenerLocal(pListener, &Address) &&
		(Address.Port != 0),
		"reentrant close listener creation failed");

	/* 客户端事件表：Open 即触发确定性重入关闭。 */
	Test.ClientEvents.Open = testReentrantClientOpen;
	Test.ClientEvents.Close = testReentrantClientClose;
	pClient = xrtTlsStreamConnect(
		pEngine, &Address, 1u, &TransportConfig, &ClientConfig,
		&Test.StreamConfig, &Test.ClientEvents, &Test
	);
	testRequire(pClient != NULL,
		"reentrant close client creation failed");

	testReentrantWait(&Test.Accepted, 1u,
		"reentrant close server accept missing");
	testReentrantWait(&Test.ClientOpen, 1u,
		"reentrant close client Open missing");
	testReentrantWait(&Test.SendDone, 1u,
		"reentrant close in-callback Send missing");
	testReentrantWait(&Test.ClientClose, 1u,
		"reentrant close client terminal missing");

	/*
		Send 在同步重入关闭后必须安全返回：至少一字节已被会话受理，
		结果为 OK（部分写入后关闭）或 CLOSED（零字节被受理）。
	*/
	testRequire(((xrtAtomic32Load(
			&Test.SendResult, XMEMORY_ACQUIRE
		) == (uint32)XTLS_OK) &&
		(xrtAtomic32Load(&Test.SendWritten, XMEMORY_ACQUIRE) >= 1u)) ||
		((xrtAtomic32Load(&Test.SendResult, XMEMORY_ACQUIRE
		) == (uint32)XTLS_CLOSED) &&
		(xrtAtomic32Load(&Test.SendWritten, XMEMORY_ACQUIRE) == 0u)),
		"reentrant close Send did not converge safely");

	/* 服务端同样到达终态（截断或认证关闭都收敛）。 */
	testReentrantWait(&Test.ServerClose, 1u,
		"reentrant close server terminal missing");

	/* 门禁化收尾：监听关闭、对象归零、引擎销毁必须全部成功。 */
	testRequire(xrtNetListenerClose(pListener),
		"reentrant close listener close failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	{
		xdeadline Drain = xrtDeadlineAfter(10000000000u);

		for ( ;; ) {
			testRequire(xrtNetEngineStats(pEngine, &Stats),
				"reentrant close stats query failed");
			if ( ( Stats.LiveObjects == 0 ) ||
				xrtDeadlineExpired(Drain) ) {
				break;
			}
			xrtThreadYield();
		}
		testRequire(Stats.LiveObjects == 0,
			"reentrant close left live objects behind");
	}
	testRequire(xrtNetEngineDestroy(pEngine),
		"reentrant close engine destroy failed");

	/* 客户端调用方引用在回调内已释放；此处无需再次 Destroy。 */
	(void)pClient;
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	printf("reentrant close regression passed\n");
	return 0;
}

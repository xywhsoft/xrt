#include "../fixtures/tls_server.h"



#if !defined(TEST_TLS_STREAM_BACKEND)
	#define TEST_TLS_STREAM_BACKEND XNET_PORT_SELECT
#endif



/* 原始服务端使用公共会话层握手，回显后故意丢弃 close_notify。 */
typedef struct test_tls_stream_close_timeout {
	xtlsserverconfig ServerConfig;
	xtlssession* ServerSession;
	xnetstream* Server;
	xatomic32 Accepted;
	xatomic32 Echoed;
	xatomic32 ServerClose;
	xatomic32 ClientClose;
	xatomic32 ListenerClose;
	xatomic32 Result;
	xatomic32 Kind;
	xatomic32 Code;
	bool IgnoreInput;
} test_tls_stream_close_timeout;



/* 在测试截止时间前等待原子计数达到给定值。 */
static void testTlsStreamCloseWait(
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



/* 把会话当前全部密文复制提交给 TCP，并精确消费会话队列。 */
static void testTlsStreamCloseFlush(
	test_tls_stream_close_timeout* pTest
)
{
	xnetspan Spans[16];

	while ( xrtTlsSessionSendSize(pTest->ServerSession) != 0 ) {
		size_t iCount = xrtTlsSessionSendSpans(
			pTest->ServerSession,
			Spans,
			sizeof(Spans) / sizeof(Spans[0])
		);
		size_t iSent = 0;

		testRequire(iCount != 0,
			"TLS close timeout server send spans missing");
		for ( size_t i = 0; i < iCount; i++ ) {
			iSent += Spans[i].Size;
		}
		testRequire((xrtNetStreamSendVec(
			pTest->Server,
			Spans,
			iCount
		) == XNET_RESULT_OK) && xrtTlsSessionSendConsume(
			pTest->ServerSession,
			iSent
		), "TLS close timeout server flush failed");
	}
}



/* 接管原始 TCP 服务端，并在其 Worker 缓冲池上创建协议会话。 */
static bool testTlsStreamCloseAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	test_tls_stream_close_timeout* pTest =
		(test_tls_stream_close_timeout*)pData;
	xnetworker* pWorker = xrtNetStreamWorker(pStream);

	(void)pListener;
	testRequire(xrtNetStreamSetData(pStream, pTest),
		"TLS close timeout server data setup failed");
	pTest->ServerSession = xrtTlsServerCreate(
		&pTest->ServerConfig,
		xrtNetWorkerBufPool(pWorker)
	);
	testRequire(pTest->ServerSession != NULL,
		"TLS close timeout server session creation failed");
	pTest->Server = pStream;
	(void)xrtAtomic32FetchAdd(
		&pTest->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 驱动真实服务端握手和一次回显，此后丢弃全部输入密文。 */
static void testTlsStreamCloseServerRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	static const char Payload[] = "close-timeout";
	test_tls_stream_close_timeout* pTest =
		(test_tls_stream_close_timeout*)pData;

	(void)pStream;
	if ( pTest->IgnoreInput ) {
		xrtNetBufClear(pBuffer);
		return;
	}
	testRequire(xrtTlsSessionFeedBuffer(
		pTest->ServerSession,
		pBuffer
	) == XTLS_OK, "TLS close timeout server feed failed");
	for ( size_t i = 0; i < 64u; i++ ) {
		xtlsresult Result = xrtTlsServerDrive(pTest->ServerSession);
		size_t iAvailable;

		testRequire((Result == XTLS_OK) || (Result == XTLS_AGAIN),
			"TLS close timeout server drive failed");
		testTlsStreamCloseFlush(pTest);
		iAvailable = xrtTlsSessionPlainSize(pTest->ServerSession);
		if ( iAvailable != 0 ) {
			char Data[sizeof(Payload) - 1u];
			size_t iRead = 0;
			size_t iWritten = 0;

			testRequire((iAvailable == sizeof(Data)) &&
				(xrtTlsSessionRead(
					pTest->ServerSession,
					Data,
					sizeof(Data),
					&iRead
				) == XTLS_OK) && (iRead == sizeof(Data)) &&
				(memcmp(Data, Payload, sizeof(Data)) == 0),
				"TLS close timeout server plaintext mismatch");
			testRequire((xrtTlsSessionWrite(
				pTest->ServerSession,
				Data,
				sizeof(Data),
				&iWritten
			) == XTLS_OK) && (iWritten == sizeof(Data)),
				"TLS close timeout server echo encode failed");
			testTlsStreamCloseFlush(pTest);
			pTest->IgnoreInput = true;
			(void)xrtAtomic32FetchAdd(
				&pTest->Echoed,
				1,
				XMEMORY_RELEASE
			);
			return;
		}
		if ( Result == XTLS_AGAIN ) {
			return;
		}
	}
	testRequire(false, "TLS close timeout server drive budget exhausted");
}



/* 客户端开放后发送触发关闭超时的唯一应用消息。 */
static void testTlsStreamCloseClientOpen(
	xtlsstream* pStream,
	ptr pData
)
{
	static const char Payload[] = "close-timeout";
	size_t iWritten = 0;

	(void)pData;
	testRequire((xrtTlsStreamSend(
		pStream,
		Payload,
		sizeof(Payload) - 1u,
		&iWritten
	) == XTLS_OK) && (iWritten == (sizeof(Payload) - 1u)),
		"TLS close timeout client send failed");
}



/* 客户端收到回显后开始认证关闭。 */
static void testTlsStreamCloseClientRead(
	xtlsstream* pStream,
	const xnetbuf* pBuffer,
	ptr pData
)
{
	static const char Payload[] = "close-timeout";
	char Data[sizeof(Payload) - 1u];
	size_t iRead = 0;

	(void)pBuffer;
	(void)pData;
	testRequire((xrtTlsStreamRead(
		pStream,
		Data,
		sizeof(Data),
		&iRead
	) == XTLS_OK) && (iRead == sizeof(Data)) &&
		(memcmp(Data, Payload, sizeof(Data)) == 0),
		"TLS close timeout client echo mismatch");
	testRequire(xrtTlsStreamClose(pStream),
		"TLS close timeout client close request failed");
}



/* 保存组合客户端关闭超时的稳定结构化字段。 */
static void testTlsStreamCloseClientClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_tls_stream_close_timeout* pTest =
		(test_tls_stream_close_timeout*)pData;

	testRequire(xrtTlsStreamState(pStream) == XTLS_STREAM_FAILED,
		"TLS close timeout client did not fail");
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



/* 客户端异常关闭后结束原始服务端。 */
static void testTlsStreamCloseServerEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	testRequire(xrtNetStreamClose(pStream),
		"TLS close timeout raw server close failed");
}



/* 记录原始服务端终态。 */
static void testTlsStreamCloseServerClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_tls_stream_close_timeout* pTest =
		(test_tls_stream_close_timeout*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pTest->ServerClose,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 Listener 唯一关闭回调。 */
static void testTlsStreamCloseListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_tls_stream_close_timeout* pTest =
		(test_tls_stream_close_timeout*)pData;

	(void)pListener;
	(void)xrtAtomic32FetchAdd(
		&pTest->ListenerClose,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证成功握手后缺失对端 close_notify 会稳定映射为关闭超时。 */
int main(void)
{
	static const xstrview Protocols[] = {
		XRT_STR_INIT("http/1.1")
	};
	test_tls_stream_close_timeout Test;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamevents ServerEvents;
	xtlsstreamevents ClientEvents;
	xtlsstreamconfig StreamConfig;
	xtlsclientconfig ClientConfig;
	xtlsverifierconfig VerifierConfig;
	xtlscontext* pContext;
	xtlsidentity* pIdentity;
	xtlsverifier* pVerifier;
	xnetengine* pEngine;
	xnetlistener* pListener;
	xtlsstream* pClient;
	xnetaddr Address;

	memset(&Test, 0, sizeof(Test));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&ServerEvents, 0, sizeof(ServerEvents));
	memset(&ClientEvents, 0, sizeof(ClientEvents));
	ListenerEvents.Accept = testTlsStreamCloseAccept;
	ListenerEvents.Close = testTlsStreamCloseListenerClose;
	ServerEvents.Read = testTlsStreamCloseServerRead;
	ServerEvents.End = testTlsStreamCloseServerEnd;
	ServerEvents.Close = testTlsStreamCloseServerClose;
	ClientEvents.Open = testTlsStreamCloseClientOpen;
	ClientEvents.Read = testTlsStreamCloseClientRead;
	ClientEvents.Close = testTlsStreamCloseClientClose;
	pContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	testRequire((pContext != NULL) && (pIdentity != NULL),
		"TLS close timeout fixture creation failed");
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(pVerifier != NULL,
		"TLS close timeout verifier creation failed");
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
	xrtTlsStreamConfigInit(&StreamConfig);
	StreamConfig.CloseTimeout = 50000u;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TLS_STREAM_BACKEND;
	EngineConfig.Workers = 2u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TLS close timeout engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "TLS close timeout loopback setup failed");
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		&ListenerEvents,
		&ServerEvents,
		&Test
	);
	testRequire((pListener != NULL) &&
		xrtNetListenerLocal(pListener, &Address),
		"TLS close timeout listener creation failed");
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
	testRequire(pClient != NULL,
		"TLS close timeout client creation failed");
	testTlsStreamCloseWait(&Test.Accepted, 1u,
		"TLS close timeout raw server was not accepted");
	testTlsStreamCloseWait(&Test.Echoed, 1u,
		"TLS close timeout server echo missing");
	testTlsStreamCloseWait(&Test.ClientClose, 1u,
		"TLS close timeout client Close callback missing");
	testTlsStreamCloseWait(&Test.ServerClose, 1u,
		"TLS close timeout raw server Close callback missing");
	testRequire(((xnetresult)xrtAtomic32Load(
		&Test.Result,
		XMEMORY_ACQUIRE
	) == XNET_RESULT_TIMEOUT) && ((xerrkind)xrtAtomic32Load(
		&Test.Kind,
		XMEMORY_ACQUIRE
	) == XERR_TIMEOUT) && ((xtlserror)xrtAtomic32Load(
		&Test.Code,
		XMEMORY_ACQUIRE
	) == XTLS_ERROR_CLOSED),
		"TLS authenticated close timeout result mismatch");

	testRequire(xrtNetListenerClose(pListener),
		"TLS close timeout listener close failed");
	testTlsStreamCloseWait(&Test.ListenerClose, 1u,
		"TLS close timeout listener Close callback missing");
	xrtTlsStreamDestroy(pClient);
	xrtTlsSessionDestroy(Test.ServerSession);
	xrtNetStreamDestroy(Test.Server);
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetEngineDestroy(pEngine),
		"TLS close timeout engine destroy failed");
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	printf("[PASS] TLS stream authenticated close timeout\n");
	return 0;
}

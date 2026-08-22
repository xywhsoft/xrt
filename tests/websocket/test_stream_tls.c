#include "../fixtures/tls_server.h"

#include "../../src/internal/xrt_websocket.h"
#include "../../src/internal/xrt_websocket_stream.h"



#ifndef TEST_WS_CONNECTION_TLS_BACKEND
	#define TEST_WS_CONNECTION_TLS_BACKEND XNET_PORT_SELECT
#endif

#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE) && \
	!defined(TEST_WS_CONNECTION_TLS_FUTURE)
	#define TEST_WS_CONNECTION_TLS_EXPECT_DEFLATE
#endif

#define TEST_WS_CONNECTION_TLS_PAYLOAD (256u * 1024u)



typedef struct test_ws_connection_tls test_ws_connection_tls;



typedef struct test_ws_connection_tls_endpoint {
	test_ws_connection_tls* Test;
	xwsrole Role;
	xtlsstream* Handshake;
	xatomicptr Connection;
	xatomic32 Messages;
	xatomic32 Errors;
	xatomic32 Closed;
	size_t Message;
	size_t Offset;
	xwsstreamclose Close;
} test_ws_connection_tls_endpoint;



struct test_ws_connection_tls {
	xnetengine* Engine;
	xnetlistener* Listener;
	test_ws_connection_tls_endpoint Client;
	test_ws_connection_tls_endpoint Server;
	xtlsserverconfig ServerConfig;
	xtlsstreamconfig StreamConfig;
	xtlsstreamevents HandshakeEvents;
	xwsstreamevents ConnectionEvents;
	xatomic32 Accepted;
	xatomic32 HandshakeErrors;
	xatomic32 ListenerClosed;
	xatomic32 ListenerErrors;
	xatomic32 PauseSeen;
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_REF)
		xatomic32 RefReleased;
	#endif
	uint8 Payload[TEST_WS_CONNECTION_TLS_PAYLOAD];
};



/* 在测试截止时间内等待原子状态。 */
static void testWsConnectionTlsWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(
		UINT64_C(15000000)
	);

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



/* TLS 握手完成后把调用方 Stream 引用转移给共享 WebSocket 会话。 */
static void testWsConnectionTlsOpen(
	xtlsstream* pStream,
	ptr pData
)
{
	test_ws_connection_tls_endpoint* pEndpoint =
		(test_ws_connection_tls_endpoint*)pData;
	xwsstreamconfig Config;
	xwsstream* pConnection;

	xrtWsStreamConfigInit(&Config);
	Config.Role = pEndpoint->Role;
	Config.MessageLimit =
		TEST_WS_CONNECTION_TLS_PAYLOAD + 1024u;
	Config.FrameLimit =
		TEST_WS_CONNECTION_TLS_PAYLOAD + 1024u;
	Config.SendLimit =
		TEST_WS_CONNECTION_TLS_PAYLOAD * 2u;
	Config.ControlReserve = 512u;
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
		xrtWsDeflateInit(&Config.Deflate);
		Config.DeflateEnabled = true;
	#endif
	{
		xwsstreamconfig Invalid = Config;
		size_t iPlainSlot = 2u + XWS_CLOSE_PAYLOAD_MAX +
			(Config.Role == XWS_ROLE_CLIENT ? XWS_MASK_SIZE : 0u);

		Invalid.ControlReserve = iPlainSlot * 3u;
		xrtClearError();
		testRequire(
			(xrtWsStreamAttachTls(
				pStream,
				0,
				&Invalid,
				&pEndpoint->Test->ConnectionEvents,
				pEndpoint
			 ) == NULL) &&
			(xrtErrorCode(xrtGetError()) == XWS_STREAM_ERROR_CONFIG) &&
			(xrtTlsStreamState(pStream) == XTLS_STREAM_OPEN),
			"WebSocket TLS accepted a plaintext-only control reserve"
		);
		xrtClearError();
	}
	pConnection = xrtWsStreamAttachTls(
		pStream,
		0,
		&Config,
		&pEndpoint->Test->ConnectionEvents,
		pEndpoint
	);
	testRequire(
		pConnection != NULL,
		"WebSocket TLS attach failed"
	);
	xrtAtomicPtrStore(
		&pEndpoint->Connection,
		pConnection,
		XMEMORY_RELEASE
	);
}



/* 握手阶段 Close 只允许用于报告建连失败。 */
static void testWsConnectionTlsHandshakeClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_ws_connection_tls_endpoint* pEndpoint =
		(test_ws_connection_tls_endpoint*)pData;

	(void)pStream;
	if ( (Result != XNET_RESULT_OK) ||
		(pError != NULL) ) {
		(void)xrtAtomic32FetchAdd(
			&pEndpoint->Test->HandshakeErrors,
			1,
			XMEMORY_RELEASE
		);
	}
}



/* 服务端只接受两条 Binary 消息。 */
static void testWsConnectionTlsMessageBegin(
	xwsstream* pConnection,
	const xwsmessageinfo* pInfo,
	ptr pData
)
{
	test_ws_connection_tls_endpoint* pEndpoint =
		(test_ws_connection_tls_endpoint*)pData;

	(void)pConnection;
	testRequire(
		(pEndpoint->Role == XWS_ROLE_SERVER) &&
		(pInfo != NULL) &&
		(pInfo->Opcode == XWS_OPCODE_BINARY) &&
		#if defined(TEST_WS_CONNECTION_TLS_EXPECT_DEFLATE)
			((((pInfo->Flags & XWS_MESSAGE_COMPRESSED) != 0) &&
			  (pEndpoint->Message == 0u)) ||
			 (((pInfo->Flags & XWS_MESSAGE_COMPRESSED) == 0) &&
			  (pEndpoint->Message == 1u))) &&
		#endif
		(pEndpoint->Message < 2u),
		"WebSocket TLS message begin mismatch"
	);
	pEndpoint->Offset = 0;
}



/* 流式校验大消息和紧随其后的尾消息没有被 TLS 短写重排。 */
static void testWsConnectionTlsMessageData(
	xwsstream* pConnection,
	xbytesview Data,
	ptr pData
)
{
	static const uint8 Tail[] = "tail";
	test_ws_connection_tls_endpoint* pEndpoint =
		(test_ws_connection_tls_endpoint*)pData;

	(void)pConnection;
	if ( pEndpoint->Message == 0 ) {
		testRequire(
			Data.Size <=
				(TEST_WS_CONNECTION_TLS_PAYLOAD -
				 pEndpoint->Offset) &&
			(memcmp(
				Data.Data,
				pEndpoint->Test->Payload +
					pEndpoint->Offset,
				Data.Size
			 ) == 0),
			"WebSocket TLS large message overflow"
		);
	} else {
		testRequire(
			(Data.Size <=
			 ((sizeof(Tail) - 1u) -
			  pEndpoint->Offset)) &&
			(memcmp(
				Data.Data,
				Tail + pEndpoint->Offset,
				Data.Size
			 ) == 0),
			"WebSocket TLS tail message ordering mismatch"
		);
	}
	pEndpoint->Offset += Data.Size;
	if ( (pEndpoint->Role == XWS_ROLE_SERVER) &&
		(pEndpoint->Message == 0) &&
		!xrtAtomic32Load(
			&pEndpoint->Test->PauseSeen,
			XMEMORY_ACQUIRE
		) ) {
		xrtWsStreamPause(pConnection);
		xrtAtomic32Store(
			&pEndpoint->Test->PauseSeen,
			1,
			XMEMORY_RELEASE
		);
	}
}



/* 第二条消息完成后由服务端发起标准关闭握手。 */
static void testWsConnectionTlsMessageEnd(
	xwsstream* pConnection,
	ptr pData
)
{
	test_ws_connection_tls_endpoint* pEndpoint =
		(test_ws_connection_tls_endpoint*)pData;
	size_t iExpected = pEndpoint->Message == 0 ?
		TEST_WS_CONNECTION_TLS_PAYLOAD : 4u;

	testRequire(
		pEndpoint->Offset == iExpected,
		"WebSocket TLS message size mismatch"
	);
	pEndpoint->Message++;
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Messages,
		1,
		XMEMORY_RELEASE
	);
	if ( pEndpoint->Message == 2u ) {
		testRequire(
			xrtWsStreamClose(
				pConnection,
				XWS_CLOSE_NORMAL,
				XRT_STR_LITERAL("done")
			) == XNET_RESULT_OK,
			"WebSocket TLS server Close failed"
		);
	}
}



/* 正常 TLS 回环不允许发布 Connection 错误。 */
static void testWsConnectionTlsError(
	xwsstream* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_ws_connection_tls_endpoint* pEndpoint =
		(test_ws_connection_tls_endpoint*)pData;

	(void)pConnection;
	testRequire(
		pError != NULL,
		"WebSocket TLS error is null"
	);
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Errors,
		1,
		XMEMORY_RELEASE
	);
}



/* 保存认证传输上的标准关闭快照。 */
static void testWsConnectionTlsClose(
	xwsstream* pConnection,
	const xwsstreamclose* pClose,
	ptr pData
)
{
	test_ws_connection_tls_endpoint* pEndpoint =
		(test_ws_connection_tls_endpoint*)pData;

	testRequire(
		(pClose != NULL) &&
		(pClose->Transport == XNET_RESULT_OK) &&
		((pClose->Flags &
		  XWS_STREAM_CLOSE_CLEAN) != 0) &&
		(pClose->LocalCode == XWS_CLOSE_NORMAL) &&
		(pClose->RemoteCode == XWS_CLOSE_NORMAL) &&
		(xrtWsStreamState(pConnection) ==
		 XWS_STREAM_CLOSED),
		"WebSocket TLS Close snapshot mismatch"
	);
	pEndpoint->Close = *pClose;
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* TLS 服务端 helper 接管 Accepted TCP 引用。 */
static bool testWsConnectionTlsAccept(
	xnetlistener* pListener,
	xnetstream* pTransport,
	ptr pData
)
{
	test_ws_connection_tls* pTest =
		(test_ws_connection_tls*)pData;
	bool bAccepted;

	(void)pListener;
	bAccepted = xrtTlsStreamAccept(
		pTransport,
		&pTest->ServerConfig,
		&pTest->StreamConfig,
		&pTest->HandshakeEvents,
		&pTest->Server,
		&pTest->Server.Handshake
	);
	if ( bAccepted ) {
		(void)xrtAtomic32FetchAdd(
			&pTest->Accepted,
			1,
			XMEMORY_RELEASE
		);
	}
	return bAccepted;
}



/* Listener 错误与 TLS/WebSocket 错误分开记录。 */
static void testWsConnectionTlsListenerError(
	xnetlistener* pListener,
	const xerror* pError,
	ptr pData
)
{
	test_ws_connection_tls* pTest =
		(test_ws_connection_tls*)pData;

	(void)pListener;
	testRequire(
		pError != NULL,
		"WebSocket TLS listener error is null"
	);
	(void)xrtAtomic32FetchAdd(
		&pTest->ListenerErrors,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 Listener 唯一关闭事件。 */
static void testWsConnectionTlsListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_ws_connection_tls* pTest =
		(test_ws_connection_tls*)pData;

	(void)pListener;
	(void)xrtAtomic32FetchAdd(
		&pTest->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



#if !defined(TEST_WS_CONNECTION_TLS_FUTURE)
#if defined(XRT_FEATURE_WEBSOCKET_STREAM_REF) && \
	!defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
/* TLS 复制受理后立即释放来源引用。 */
static void testWsConnectionTlsRefRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	test_ws_connection_tls* pTest =
		(test_ws_connection_tls*)pContext;

	(void)iSize;
	(void)xrtAtomic32FetchAdd(
		&pTest->RefReleased,
		1,
		XMEMORY_RELEASE
	);
	xrtFree((ptr)pData);
}
#endif



/* TLS 短写余量必须按尚未生成的精确密文线路成本计入 Connection。 */
static void testWsConnectionTlsBudget(xwsstream* pConnection)
{
	__xrt_ws_output* pOutput = pConnection->OutputHead;
	size_t iOutput = 0;

	while ( pOutput != NULL ) {
		size_t iBound;

		testRequire(
			xrtTlsStreamSendBound(
				xrtWsStreamTls(pConnection),
				pOutput->Size - pOutput->Offset,
				&iBound
			),
			"WebSocket TLS output bound query failed"
		);
		testRequire(
			iBound <= (SIZE_MAX - iOutput),
			"WebSocket TLS output budget overflowed"
		);
		iOutput += iBound;
		pOutput = pOutput->Next;
	}
	testRequire(
		(xrtAtomic64Load(
			&pConnection->OutputBytes,
			XMEMORY_ACQUIRE
		 ) == (uint64)iOutput) &&
		(xrtWsStreamPending(pConnection) <=
		 pConnection->Config.SendLimit),
		"WebSocket TLS short-write budget did not match ciphertext"
	);
}



/* 在客户端 Worker 上连续提交大帧和尾帧，强制经过 TLS 明文短写队列。 */
static void testWsConnectionTlsSend(
	xnetworker* pWorker,
	ptr pData
)
{
	static const uint8 Tail[] = "tail";
	test_ws_connection_tls* pTest =
		(test_ws_connection_tls*)pData;
	xwsstream* pConnection = (xwsstream*)xrtAtomicPtrLoad(
		&pTest->Client.Connection,
		XMEMORY_ACQUIRE
	);

	(void)pWorker;
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
		xnetresult LargeResult = xrtWsStreamBinaryCompressed(
			pConnection,
			(xbytesview) {
				pTest->Payload,
				sizeof(pTest->Payload)
			}
		);
	#elif defined(XRT_FEATURE_WEBSOCKET_STREAM_REF)
		bytes pPayload = (bytes)xrtMalloc(
			sizeof(pTest->Payload)
		);
		xnetref Ref;
		xnetresult LargeResult;

		testRequire(
			pPayload != NULL,
			"WebSocket TLS reference allocation failed"
		);
		memcpy(
			pPayload,
			pTest->Payload,
			sizeof(pTest->Payload)
		);
		Ref = (xnetref) {
			pPayload,
			sizeof(pTest->Payload),
			testWsConnectionTlsRefRelease,
			pTest
		};
		LargeResult = xrtWsStreamBinaryRef(
			pConnection,
			&Ref
		);
		if ( LargeResult != XNET_RESULT_OK ) {
			xrtFree(pPayload);
		}
	#else
		xnetresult LargeResult = xrtWsStreamBinary(
			pConnection,
			(xbytesview) {
				pTest->Payload,
				sizeof(pTest->Payload)
			}
		);
	#endif
	testRequire(
		(pConnection != NULL) &&
		(xrtWsStreamTls(pConnection) != NULL) &&
		(LargeResult == XNET_RESULT_OK) &&
		#if defined(XRT_FEATURE_WEBSOCKET_STREAM_REF) && \
			!defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
			(xrtAtomic32Load(
				&pTest->RefReleased,
				XMEMORY_ACQUIRE
			 ) == 1) &&
		#endif
		(xrtWsStreamBinary(
			pConnection,
			(xbytesview) {
				Tail,
				sizeof(Tail) - 1u
			}
		 ) == XNET_RESULT_OK),
		"WebSocket TLS ordered send failed"
	);
	testWsConnectionTlsBudget(pConnection);
}
#endif



/* 为压缩 TLS 组合生成确定性的不可压缩消息，持续覆盖记录层短写。 */
static void testWsConnectionTlsPayload(
	uint8* pData,
	size_t iSize
)
{
	#if defined(TEST_WS_CONNECTION_TLS_EXPECT_DEFLATE)
		uint32 iState = UINT32_C(0x9e3779b9);

		for ( size_t i = 0; i < iSize; i++ ) {
			iState ^= iState << 13u;
			iState ^= iState >> 17u;
			iState ^= iState << 5u;
			pData[i] = (uint8)iState;
		}
	#else
		memset(pData, 0x5A, iSize);
	#endif
}



/* 验证 TLS 短写 FIFO、流式大消息和认证关闭。 */
int main(void)
{
	static const xstrview Protocols[] = {
		XRT_STR_INIT("http/1.1")
	};
	test_ws_connection_tls Test;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetstreamconfig TransportConfig;
	xnetlistenerevents ListenerEvents;
	xtlslimits Limits;
	xtlsclientconfig ClientConfig;
	xtlsverifierconfig VerifierConfig;
	xtlscontext* pContext;
	xtlsidentity* pIdentity;
	xtlsverifier* pVerifier;
	xnetaddr Address;
	xtlsstream* pClientTransport;
	xtlsstream* pServerTransport;
	xwsstream* pClient;
	xwsstream* pServer;
	xdeadline AttachDeadline;
	#if defined(TEST_WS_CONNECTION_TLS_FUTURE)
		xfuture* pLargeFuture;
		xfuture* pTailFuture;
		xfuture* pDrainFuture;
		xfuture* pCloseFuture;
	#endif

	memset(&Test, 0, sizeof(Test));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	testWsConnectionTlsPayload(
		Test.Payload,
		sizeof(Test.Payload)
	);
	Test.Client.Test = &Test;
	Test.Client.Role = XWS_ROLE_CLIENT;
	Test.Server.Test = &Test;
	Test.Server.Role = XWS_ROLE_SERVER;
	xrtAtomicPtrInit(&Test.Client.Connection, NULL);
	xrtAtomicPtrInit(&Test.Server.Connection, NULL);
	xrtAtomic32Init(&Test.Client.Messages, 0);
	xrtAtomic32Init(&Test.Server.Messages, 0);
	xrtAtomic32Init(&Test.Client.Errors, 0);
	xrtAtomic32Init(&Test.Server.Errors, 0);
	xrtAtomic32Init(&Test.Client.Closed, 0);
	xrtAtomic32Init(&Test.Server.Closed, 0);
	xrtAtomic32Init(&Test.Accepted, 0);
	xrtAtomic32Init(&Test.HandshakeErrors, 0);
	xrtAtomic32Init(&Test.ListenerClosed, 0);
	xrtAtomic32Init(&Test.ListenerErrors, 0);
	xrtAtomic32Init(&Test.PauseSeen, 0);
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_REF)
		xrtAtomic32Init(&Test.RefReleased, 0);
	#endif
	Test.HandshakeEvents.Open =
		testWsConnectionTlsOpen;
	Test.HandshakeEvents.Close =
		testWsConnectionTlsHandshakeClose;
	Test.ConnectionEvents.MessageBegin =
		testWsConnectionTlsMessageBegin;
	Test.ConnectionEvents.MessageData =
		testWsConnectionTlsMessageData;
	Test.ConnectionEvents.MessageEnd =
		testWsConnectionTlsMessageEnd;
	Test.ConnectionEvents.Error =
		testWsConnectionTlsError;
	Test.ConnectionEvents.Close =
		testWsConnectionTlsClose;
	ListenerEvents.Accept =
		testWsConnectionTlsAccept;
	ListenerEvents.Error =
		testWsConnectionTlsListenerError;
	ListenerEvents.Close =
		testWsConnectionTlsListenerClose;

	xrtTlsLimitsInit(&Limits);
	Limits.FeedLimit = XTLS_RECORD_HEADER_SIZE +
		XTLS12_RECORD_CIPHERTEXT_MAX;
	Limits.SendLimit = 32768u;
	Limits.PlainLimit = 32768u;
	pContext = testTlsServerContextWithLimits(&Limits);
	pIdentity = testTlsServerIdentity();
	testRequire(
		(pContext != NULL) &&
		(pIdentity != NULL),
		"WebSocket TLS fixtures failed"
	);
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(
		pVerifier != NULL,
		"WebSocket TLS verifier creation failed"
	);
	xrtTlsServerConfigInit(&Test.ServerConfig);
	Test.ServerConfig.Context = pContext;
	Test.ServerConfig.Identity = pIdentity;
	Test.ServerConfig.Protocols = Protocols;
	Test.ServerConfig.ProtocolCount = 1u;
	Test.ServerConfig.RequireProtocol = true;
	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.Context = pContext;
	ClientConfig.ServerName =
		XRT_STR_LITERAL("example.com");
	ClientConfig.Protocols = Protocols;
	ClientConfig.ProtocolCount = 1u;
	ClientConfig.Verifier = pVerifier;
	xrtTlsStreamConfigInit(&Test.StreamConfig);

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend =
		TEST_WS_CONNECTION_TLS_BACKEND;
	EngineConfig.Workers = 2;
	Test.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(Test.Engine != NULL) &&
		xrtNetEngineStart(Test.Engine),
		"WebSocket TLS engine start failed"
	);
	xrtNetListenConfigInit(&ListenConfig);
	xrtNetStreamConfigInit(&TransportConfig);
	TransportConfig.ReadSize = 4096;
	TransportConfig.ReadLimit = 262144;
	TransportConfig.WriteLimit = 524288;
	TransportConfig.WriteHighWater = 262144;
	TransportConfig.WriteLowWater = 65536;
	ListenConfig.Stream = TransportConfig;
	ListenConfig.AcceptConcurrency = 4;
	testRequire(
		xrtNetAddrLoopback(
			&ListenConfig.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"WebSocket TLS listener address failed"
	);
	Test.Listener = xrtNetListen(
		Test.Engine,
		&ListenConfig,
		&ListenerEvents,
		NULL,
		&Test
	);
	testRequire(
		(Test.Listener != NULL) &&
		xrtNetListenerLocal(Test.Listener, &Address),
		"WebSocket TLS listener start failed"
	);
	Test.Client.Handshake = xrtTlsStreamConnect(
		Test.Engine,
		&Address,
		1,
		&TransportConfig,
		&ClientConfig,
		&Test.StreamConfig,
		&Test.HandshakeEvents,
		&Test.Client
	);
	testRequire(
		Test.Client.Handshake != NULL,
		"WebSocket TLS client connect failed"
	);

	AttachDeadline = xrtDeadlineAfter(
		UINT64_C(15000000)
	);
	while ( ((pClient = (xwsstream*)xrtAtomicPtrLoad(
		&Test.Client.Connection,
		XMEMORY_ACQUIRE
	)) == NULL) || ((pServer = (xwsstream*)xrtAtomicPtrLoad(
		&Test.Server.Connection,
		XMEMORY_ACQUIRE
	)) == NULL) ) {
		testRequire(
			!xrtDeadlineExpired(AttachDeadline),
			"WebSocket TLS connections were not attached"
		);
		xrtThreadYield();
	}
	pClientTransport = xrtWsStreamTlsRef(pClient);
	pServerTransport = xrtWsStreamTlsRef(pServer);
	testRequire(
		(pClientTransport != NULL) &&
		(pServerTransport != NULL) &&
		(xrtWsStreamTls(pClient) == NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_STREAM_ERROR_STATE) &&
		(xrtWsStreamError(pClient) == NULL) &&
		(xrtWsStreamTcp(pClient) == NULL) &&
		(xrtWsStreamTcp(pServer) == NULL),
		"WebSocket TLS transport query mismatch"
	);
	xrtClearError();
	xrtTlsStreamDestroy(pClientTransport);
	xrtTlsStreamDestroy(pServerTransport);
	#if defined(TEST_WS_CONNECTION_TLS_FUTURE)
		pCloseFuture = xrtWsStreamWaitAsync(
			pClient,
			XWS_STREAM_WAIT_CLOSE
		);
		pLargeFuture = xrtWsStreamBinaryAsync(
			pClient,
			(xbytesview) {
				Test.Payload,
				sizeof(Test.Payload)
			}
		);
		pTailFuture = xrtWsStreamBinaryAsync(
			pClient,
			XRT_BYTES_LITERAL("tail")
		);
		testRequire(
			(pLargeFuture != NULL) &&
			(pTailFuture != NULL) &&
			(pCloseFuture != NULL) &&
			(xrtFutureWaitFor(
				pLargeFuture,
				UINT64_C(15000000)
			 ) == XWAIT_OK) &&
			(xrtFutureState(pLargeFuture) ==
			 XFUTURE_RESOLVED) &&
			(xrtFutureWaitFor(
				pTailFuture,
				UINT64_C(15000000)
			 ) == XWAIT_OK) &&
			(xrtFutureState(pTailFuture) ==
			 XFUTURE_RESOLVED),
			"WebSocket TLS cross-thread sends failed"
		);
		pDrainFuture = xrtWsStreamWaitAsync(
			pClient,
			XWS_STREAM_WAIT_DRAIN
		);
		testRequire(
			pDrainFuture != NULL,
			"WebSocket TLS drain Future creation failed"
		);
	#else
		testRequire(
			xrtNetEnginePost(
				Test.Engine,
				xrtNetWorkerIndex(
					xrtWsStreamWorker(pClient)
				),
				testWsConnectionTlsSend,
				&Test
			),
			"WebSocket TLS send task post failed"
		);
	#endif
	testWsConnectionTlsWait(
		&Test.PauseSeen,
		1,
		"WebSocket TLS server did not pause"
	);
	testRequire(
		xrtWsStreamPaused(pServer) &&
		(Test.Server.Offset > 0) &&
		(Test.Server.Offset <
		 TEST_WS_CONNECTION_TLS_PAYLOAD) &&
		(xrtAtomic32Load(
			&Test.Server.Messages,
			XMEMORY_ACQUIRE
		 ) == 0),
		"WebSocket TLS pause crossed the current message boundary"
	);
	testRequire(
		xrtWsStreamResume(pServer) &&
		!xrtWsStreamPaused(pServer),
		"WebSocket TLS cross-thread resume failed"
	);
	testWsConnectionTlsWait(
		&Test.Server.Messages,
		2,
		"WebSocket TLS ordered messages missing"
	);
	testWsConnectionTlsWait(
		&Test.Client.Closed,
		1,
		"WebSocket TLS client Close missing"
	);
	testWsConnectionTlsWait(
		&Test.Server.Closed,
		1,
		"WebSocket TLS server Close missing"
	);
	#if defined(TEST_WS_CONNECTION_TLS_FUTURE)
		testRequire(
			(xrtFutureWaitFor(
				pDrainFuture,
				UINT64_C(15000000)
			 ) == XWAIT_OK) &&
			(xrtFutureState(pDrainFuture) ==
			 XFUTURE_RESOLVED) &&
			(xrtFutureWaitFor(
				pCloseFuture,
				UINT64_C(15000000)
			 ) == XWAIT_OK) &&
			(xrtFutureState(pCloseFuture) ==
			 XFUTURE_RESOLVED),
			"WebSocket TLS drain or close Future failed"
		);
		xrtFutureDestroy(pLargeFuture);
		xrtFutureDestroy(pTailFuture);
		xrtFutureDestroy(pDrainFuture);
		xrtFutureDestroy(pCloseFuture);
	#endif
	testRequire(
		(xrtAtomic32Load(
			&Test.Client.Errors,
			XMEMORY_ACQUIRE
		 ) == 0) &&
		(xrtAtomic32Load(
			&Test.Server.Errors,
			XMEMORY_ACQUIRE
		 ) == 0) &&
		(xrtAtomic32Load(
			&Test.HandshakeErrors,
			XMEMORY_ACQUIRE
		 ) == 0) &&
		(xrtAtomic32Load(
			&Test.ListenerErrors,
			XMEMORY_ACQUIRE
		 ) == 0) &&
		((Test.Server.Close.Flags &
		  XWS_STREAM_CLOSE_REMOTE) == 0) &&
		((Test.Client.Close.Flags &
		  XWS_STREAM_CLOSE_REMOTE) != 0),
		"WebSocket TLS lifecycle mismatch"
	);
	testRequire(
		xrtNetListenerClose(Test.Listener),
		"WebSocket TLS listener close failed"
	);
	testWsConnectionTlsWait(
		&Test.ListenerClosed,
		1,
		"WebSocket TLS listener Close missing"
	);
	xrtWsStreamDestroy(pClient);
	xrtWsStreamDestroy(pServer);
	xrtNetListenerDestroy(Test.Listener);
	testRequire(
		xrtNetEngineDestroy(Test.Engine),
		"WebSocket TLS engine destroy failed"
	);
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	printf(
		"[PASS] WebSocket TLS short-write FIFO\n"
	);
	return 0;
}

#include "../fixtures/tls_server.h"



#define TEST_WS_UPGRADE_TLS_KEY "dGhlIHNhbXBsZSBub25jZQ=="



typedef struct test_ws_upgrade_tls test_ws_upgrade_tls;



typedef struct test_ws_upgrade_tls_endpoint {
	test_ws_upgrade_tls* Test;
	xtlsstream* Handshake;
	bool Client;
	bool Compressed;
	uint8 Data[32];
	size_t Size;
	xatomicptr Stream;
	xatomic32 Messages;
	xatomic32 Errors;
	xatomic32 Closed;
} test_ws_upgrade_tls_endpoint;



struct test_ws_upgrade_tls {
	xnetengine* Engine;
	xnetlistener* Listener;
	xtlsserverconfig TlsServer;
	xtlsstreamconfig TlsStream;
	xwsupgradeserverconfig UpgradeServer;
	xwsupgradeclientconfig UpgradeClient;
	xtlsstreamevents TlsEvents;
	xwsstreamevents WsEvents;
	test_ws_upgrade_tls_endpoint Server;
	test_ws_upgrade_tls_endpoint Client;
	char Request[1024];
	size_t RequestSize;
	uint8 Frame[64];
	size_t FrameSize;
	xatomic32 Accepted;
	xatomic32 TlsErrors;
	xatomic32 ListenerErrors;
	xatomic32 ListenerClosed;
};



/* 在测试截止时间内等待 WSS 链路状态。 */
static void testWsUpgradeTlsWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(15000000));

	while ( xrtAtomic32Load(
		pValue,
		XMEMORY_ACQUIRE
	) < iExpected ) {
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}



/* 写入一段必须被当前 TLS 队列完整受理的小消息。 */
static void testWsUpgradeTlsSend(
	xtlsstream* pStream,
	const void* pData,
	size_t iSize,
	cstr sMessage
)
{
	size_t iWritten = 0;

	testRequire(
		(xrtTlsStreamSend(
			pStream,
			pData,
			iSize,
			&iWritten
		) == XTLS_OK) && (iWritten == iSize),
		sMessage
	);
}



/* 构建带压缩 offer 的请求和掩码首帧。 */
static void testWsUpgradeTlsInputInit(test_ws_upgrade_tls* pTest)
{
	static const uint8 Mask[XWS_MASK_SIZE] = {
		UINT8_C(0x11), UINT8_C(0x22),
		UINT8_C(0x33), UINT8_C(0x44)
	};
	static const uint8 Payload[] = { 'h', 'e', 'l', 'l', 'o' };
	xhttpfield Fields[XWS_UPGRADE_REQUEST_FIELDS_MAX];
	xwsframe Frame;
	char Extensions[XWS_DEFLATE_MAX_SIZE + 1u];
	size_t iExtensions = 0;
	size_t iFields = 0;
	size_t iHead = 0;

	xrtWsUpgradeClientConfigInit(&pTest->UpgradeClient);
	pTest->UpgradeClient.Protocols = XRT_STR_LITERAL("chat");
	pTest->UpgradeClient.EnableDeflate = true;
	testRequire(
		xrtWsDeflateOfferWrite(
			&pTest->UpgradeClient.Deflate,
			Extensions,
			XWS_DEFLATE_MAX_SIZE,
			&iExtensions
		) && xrtWsUpgradeRequestFields(
			XRT_STR_LITERAL("example.com"),
			XRT_STR_LITERAL(TEST_WS_UPGRADE_TLS_KEY),
			pTest->UpgradeClient.Protocols,
			(xstrview) { Extensions, iExtensions },
			Fields,
			sizeof(Fields) / sizeof(Fields[0]),
			&iFields
		) && xrtHttp1RequestWrite(
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/socket"),
			XHTTP_VERSION_1_1,
			Fields,
			iFields,
			pTest->Request,
			sizeof(pTest->Request),
			&pTest->RequestSize
		),
		"WSS Upgrade request build failed"
	);
	xrtWsFrameInit(&Frame);
	Frame.Opcode = XWS_OPCODE_TEXT;
	Frame.Flags = XWS_FRAME_FIN | XWS_FRAME_MASKED;
	Frame.PayloadSize = sizeof(Payload);
	memcpy(Frame.Mask, Mask, sizeof(Mask));
	testRequire(
		xrtWsFrameWrite(
			&Frame,
			NULL,
			pTest->Frame,
			sizeof(pTest->Frame),
			&iHead
		) && (sizeof(Payload) <=
			(sizeof(pTest->Frame) - iHead)),
		"WSS Upgrade frame write failed"
	);
	memcpy(pTest->Frame + iHead, Payload, sizeof(Payload));
	testRequire(
		xrtWsMask(
			pTest->Frame + iHead,
			sizeof(Payload),
			Mask,
			0
		),
		"WSS Upgrade frame mask failed"
	);
	pTest->FrameSize = iHead + sizeof(Payload);
}



/* 记录消息压缩属性并重置同步回调累积。 */
static void testWsUpgradeTlsMessageBegin(
	xwsstream* pStream,
	const xwsmessageinfo* pInfo,
	ptr pData
)
{
	test_ws_upgrade_tls_endpoint* pEndpoint =
		(test_ws_upgrade_tls_endpoint*)pData;

	(void)pStream;
	testRequire(
		(pInfo != NULL) && (pInfo->Opcode == XWS_OPCODE_TEXT),
		"WSS Upgrade message type mismatch"
	);
	pEndpoint->Compressed =
		(pInfo->Flags & XWS_MESSAGE_COMPRESSED) != 0;
	pEndpoint->Size = 0;
}



/* 流式复制解码后的语义正文。 */
static void testWsUpgradeTlsMessageData(
	xwsstream* pStream,
	xbytesview Data,
	ptr pData
)
{
	test_ws_upgrade_tls_endpoint* pEndpoint =
		(test_ws_upgrade_tls_endpoint*)pData;

	(void)pStream;
	testRequire(
		Data.Size <= (sizeof(pEndpoint->Data) - pEndpoint->Size),
		"WSS Upgrade message overflow"
	);
	memcpy(pEndpoint->Data + pEndpoint->Size, Data.Data, Data.Size);
	pEndpoint->Size += Data.Size;
}



/* 服务端压缩响应，客户端验证解压结果并完成 Close。 */
static void testWsUpgradeTlsMessageEnd(xwsstream* pStream, ptr pData)
{
	test_ws_upgrade_tls_endpoint* pEndpoint =
		(test_ws_upgrade_tls_endpoint*)pData;

	if ( pEndpoint->Client ) {
		testRequire(
			pEndpoint->Compressed &&
			(pEndpoint->Size == 5u) &&
			(memcmp(pEndpoint->Data, "world", 5u) == 0),
			"WSS compressed response mismatch"
		);
		(void)xrtAtomic32FetchAdd(
			&pEndpoint->Messages,
			1,
			XMEMORY_RELEASE
		);
		testRequire(
			xrtWsStreamClose(
				pStream,
				XWS_CLOSE_NORMAL,
				(xstrview) { 0 }
			) == XNET_RESULT_OK,
			"WSS Close request failed"
		);
	} else {
		testRequire(
			!pEndpoint->Compressed &&
			(pEndpoint->Size == 5u) &&
			(memcmp(pEndpoint->Data, "hello", 5u) == 0),
			"WSS request suffix mismatch"
		);
		(void)xrtAtomic32FetchAdd(
			&pEndpoint->Messages,
			1,
			XMEMORY_RELEASE
		);
		testRequire(
			xrtWsStreamTextCompressed(
				pStream,
				XRT_STR_LITERAL("world")
			) == XNET_RESULT_OK,
			"WSS compressed response send failed"
		);
	}
}



/* 记录 WebSocket 协议或传输错误。 */
static void testWsUpgradeTlsWsError(
	xwsstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	test_ws_upgrade_tls_endpoint* pEndpoint =
		(test_ws_upgrade_tls_endpoint*)pData;

	(void)pStream;
	testRequire(pError != NULL, "WSS error omitted its cause");
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Errors,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录经过 WebSocket 与 TLS 双层关闭握手的终态。 */
static void testWsUpgradeTlsWsClose(
	xwsstream* pStream,
	const xwsstreamclose* pClose,
	ptr pData
)
{
	test_ws_upgrade_tls_endpoint* pEndpoint =
		(test_ws_upgrade_tls_endpoint*)pData;

	(void)pStream;
	testRequire(
		(pClose != NULL) &&
		(pClose->Transport == XNET_RESULT_OK) &&
		((pClose->Flags & XWS_STREAM_CLOSE_CLEAN) != 0),
		"WSS did not close cleanly"
	);
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 发送分成两个 TLS record 的 101 Header。 */
static void testWsUpgradeTlsResponse(
	xtlsstream* pTransport,
	const xwsupgrade* pUpgrade
)
{
	xhttpfield Fields[XWS_UPGRADE_RESPONSE_FIELDS_MAX];
	char Response[1024];
	size_t iFields = 0;
	size_t iSize = 0;
	size_t iSplit;

	testRequire(
		xrtWsUpgradeResponseFields(
			(xstrview) { pUpgrade->Accept, XWS_ACCEPT_SIZE },
			pUpgrade->Protocol,
			(xstrview) {
				pUpgrade->Extensions,
				pUpgrade->ExtensionSize
			},
			Fields,
			sizeof(Fields) / sizeof(Fields[0]),
			&iFields
		) && xrtHttp1ResponseWrite(
			XHTTP_VERSION_1_1,
			XHTTP_STATUS_SWITCHING_PROTOCOLS,
			XRT_STR_LITERAL("Switching Protocols"),
			Fields,
			iFields,
			Response,
			sizeof(Response),
			&iSize
		),
		"WSS Upgrade response build failed"
	);
	iSplit = 19u;
	testRequire(iSize > iSplit, "WSS response split fixture failed");
	testWsUpgradeTlsSend(
		pTransport,
		Response,
		iSplit,
		"WSS response prefix send failed"
	);
	testWsUpgradeTlsSend(
		pTransport,
		Response + iSplit,
		iSize - iSplit,
		"WSS response suffix send failed"
	);
}



/* 严格校验跨 record 请求并原子接管一字节残留帧头。 */
static void testWsUpgradeTlsServerRead(
	xtlsstream* pTransport,
	test_ws_upgrade_tls_endpoint* pEndpoint
)
{
	test_ws_upgrade_tls* pTest = pEndpoint->Test;
	xhttpfield Fields[16];
	xhttp1head Head;
	xwsupgrade Upgrade;
	xwsstreamconfig Config;
	xwsstream* pStream;
	xhttp1status Status;

	xrtHttp1HeadInit(
		&Head,
		Fields,
		sizeof(Fields) / sizeof(Fields[0])
	);
	Status = xrtHttp1RequestParseTls(
		pTransport,
		&Head,
		NULL,
		NULL
	);
	if ( Status == XHTTP1_MORE ) {
		return;
	}
	testRequire(
		(Status == XHTTP1_READY) &&
		xrtWsUpgradeRequestCheck(
			&Head,
			&pTest->UpgradeServer,
			&Upgrade
		) && Upgrade.DeflateEnabled,
		"WSS server Upgrade validation failed"
	);
	testWsUpgradeTlsResponse(pTransport, &Upgrade);
	testRequire(
		xrtWsUpgradeStreamConfig(
			&Config,
			XWS_ROLE_SERVER,
			&Upgrade
		),
		"WSS server Stream config failed"
	);
	pStream = xrtWsStreamAttachTls(
		pTransport,
		Head.Bytes,
		&Config,
		&pTest->WsEvents,
		pEndpoint
	);
	testRequire(pStream != NULL, "WSS server attach failed");
	xrtAtomicPtrStore(
		&pEndpoint->Stream,
		pStream,
		XMEMORY_RELEASE
	);
}



/* 校验跨 record 的 101 并接管后续压缩帧。 */
static void testWsUpgradeTlsClientRead(
	xtlsstream* pTransport,
	test_ws_upgrade_tls_endpoint* pEndpoint
)
{
	test_ws_upgrade_tls* pTest = pEndpoint->Test;
	xhttpfield Fields[16];
	xhttp1head Head;
	xwsupgrade Upgrade;
	xwsstreamconfig Config;
	xwsstream* pStream;
	xhttp1status Status;

	xrtHttp1HeadInit(
		&Head,
		Fields,
		sizeof(Fields) / sizeof(Fields[0])
	);
	Status = xrtHttp1ResponseParseTls(
		pTransport,
		&Head,
		NULL,
		NULL
	);
	if ( Status == XHTTP1_MORE ) {
		return;
	}
	testRequire(
		(Status == XHTTP1_READY) &&
		xrtWsUpgradeResponseCheck(
			&Head,
			XRT_STR_LITERAL(TEST_WS_UPGRADE_TLS_KEY),
			&pTest->UpgradeClient,
			&Upgrade
		) && Upgrade.DeflateEnabled &&
		xrtWsUpgradeStreamConfig(
			&Config,
			XWS_ROLE_CLIENT,
			&Upgrade
		),
		"WSS client Upgrade validation failed"
	);
	pStream = xrtWsStreamAttachTls(
		pTransport,
		Head.Bytes,
		&Config,
		&pTest->WsEvents,
		pEndpoint
	);
	testRequire(pStream != NULL, "WSS client attach failed");
	xrtAtomicPtrStore(
		&pEndpoint->Stream,
		pStream,
		XMEMORY_RELEASE
	);
}



/* 在 TLS Read 事件中按角色推进 HTTP Upgrade。 */
static void testWsUpgradeTlsRead(
	xtlsstream* pStream,
	const xnetbuf* pBuffer,
	ptr pData
)
{
	test_ws_upgrade_tls_endpoint* pEndpoint =
		(test_ws_upgrade_tls_endpoint*)pData;

	(void)pBuffer;
	if ( pEndpoint->Client ) {
		testWsUpgradeTlsClientRead(pStream, pEndpoint);
	} else {
		testWsUpgradeTlsServerRead(pStream, pEndpoint);
	}
}



/* 客户端把 HTTP、单字节帧头和剩余帧拆成三个 TLS record。 */
static void testWsUpgradeTlsOpen(xtlsstream* pStream, ptr pData)
{
	test_ws_upgrade_tls_endpoint* pEndpoint =
		(test_ws_upgrade_tls_endpoint*)pData;
	test_ws_upgrade_tls* pTest = pEndpoint->Test;
	size_t iSplit = 17u;

	if ( !pEndpoint->Client ) {
		return;
	}
	testRequire(
		(pTest->RequestSize > iSplit) && (pTest->FrameSize > 1u),
		"WSS request split fixture failed"
	);
	testWsUpgradeTlsSend(
		pStream,
		pTest->Request,
		iSplit,
		"WSS request prefix send failed"
	);
	testWsUpgradeTlsSend(
		pStream,
		pTest->Request + iSplit,
		pTest->RequestSize - iSplit,
		"WSS request Header suffix send failed"
	);
	testWsUpgradeTlsSend(
		pStream,
		pTest->Frame,
		1u,
		"WSS one-byte frame prefix send failed"
	);
	testWsUpgradeTlsSend(
		pStream,
		pTest->Frame + 1u,
		pTest->FrameSize - 1u,
		"WSS frame suffix send failed"
	);
}



/* Upgrade 前 TLS 失败保持独立统计。 */
static void testWsUpgradeTlsClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_ws_upgrade_tls_endpoint* pEndpoint =
		(test_ws_upgrade_tls_endpoint*)pData;

	(void)pStream;
	if ( (Result != XNET_RESULT_OK) || (pError != NULL) ) {
		(void)xrtAtomic32FetchAdd(
			&pEndpoint->Test->TlsErrors,
			1,
			XMEMORY_RELEASE
		);
	}
}



/* 接管 Accepted TCP 引用并启动服务端 TLS。 */
static bool testWsUpgradeTlsAccept(
	xnetlistener* pListener,
	xnetstream* pTransport,
	ptr pData
)
{
	test_ws_upgrade_tls* pTest = (test_ws_upgrade_tls*)pData;
	bool bAccepted;

	(void)pListener;
	bAccepted = xrtTlsStreamAccept(
		pTransport,
		&pTest->TlsServer,
		&pTest->TlsStream,
		&pTest->TlsEvents,
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



/* Listener 错误保持独立统计。 */
static void testWsUpgradeTlsListenerError(
	xnetlistener* pListener,
	const xerror* pError,
	ptr pData
)
{
	test_ws_upgrade_tls* pTest = (test_ws_upgrade_tls*)pData;

	(void)pListener;
	testRequire(pError != NULL, "WSS listener error omitted its cause");
	(void)xrtAtomic32FetchAdd(
		&pTest->ListenerErrors,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 Listener 终态。 */
static void testWsUpgradeTlsListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_ws_upgrade_tls* pTest = (test_ws_upgrade_tls*)pData;

	(void)pListener;
	(void)xrtAtomic32FetchAdd(
		&pTest->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证 TLS -> HTTP Upgrade -> 压缩 WebSocket 的完整核心链路。 */
int main(void)
{
	static const xstrview Protocols[] = {
		XRT_STR_INIT("http/1.1")
	};
	test_ws_upgrade_tls Test;
	xtlslimits Limits;
	xtlsclientconfig ClientConfig;
	xtlsverifierconfig VerifierConfig;
	xtlscontext* pContext;
	xtlsidentity* pIdentity;
	xtlsverifier* pVerifier;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetstreamconfig TransportConfig;
	xnetlistenerevents ListenerEvents;
	xnetaddr Address;
	xwsstream* pClient;
	xwsstream* pServer;

	memset(&Test, 0, sizeof(Test));
	Test.Server.Test = &Test;
	Test.Client.Test = &Test;
	Test.Client.Client = true;
	xrtAtomicPtrInit(&Test.Server.Stream, NULL);
	xrtAtomicPtrInit(&Test.Client.Stream, NULL);
	xrtWsUpgradeServerConfigInit(&Test.UpgradeServer);
	Test.UpgradeServer.Protocols = XRT_STR_LITERAL("chat");
	Test.UpgradeServer.EnableDeflate = true;
	testWsUpgradeTlsInputInit(&Test);
	Test.TlsEvents.Open = testWsUpgradeTlsOpen;
	Test.TlsEvents.Read = testWsUpgradeTlsRead;
	Test.TlsEvents.Close = testWsUpgradeTlsClose;
	Test.WsEvents.MessageBegin = testWsUpgradeTlsMessageBegin;
	Test.WsEvents.MessageData = testWsUpgradeTlsMessageData;
	Test.WsEvents.MessageEnd = testWsUpgradeTlsMessageEnd;
	Test.WsEvents.Error = testWsUpgradeTlsWsError;
	Test.WsEvents.Close = testWsUpgradeTlsWsClose;

	xrtTlsLimitsInit(&Limits);
	Limits.FeedLimit = XTLS_RECORD_HEADER_SIZE +
		XTLS12_RECORD_CIPHERTEXT_MAX;
	Limits.SendLimit = 32768u;
	Limits.PlainLimit = 32768u;
	pContext = testTlsServerContextWithLimits(&Limits);
	pIdentity = testTlsServerIdentity();
	testRequire(
		(pContext != NULL) && (pIdentity != NULL),
		"WSS TLS fixtures failed"
	);
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(pVerifier != NULL, "WSS verifier creation failed");
	xrtTlsServerConfigInit(&Test.TlsServer);
	Test.TlsServer.Context = pContext;
	Test.TlsServer.Identity = pIdentity;
	Test.TlsServer.Protocols = Protocols;
	Test.TlsServer.ProtocolCount = 1u;
	Test.TlsServer.RequireProtocol = true;
	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.Context = pContext;
	ClientConfig.ServerName = XRT_STR_LITERAL("example.com");
	ClientConfig.Protocols = Protocols;
	ClientConfig.ProtocolCount = 1u;
	ClientConfig.Verifier = pVerifier;
	xrtTlsStreamConfigInit(&Test.TlsStream);

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2u;
	Test.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(Test.Engine != NULL) && xrtNetEngineStart(Test.Engine),
		"WSS select engine start failed"
	);
	xrtNetListenConfigInit(&ListenConfig);
	xrtNetStreamConfigInit(&TransportConfig);
	TransportConfig.ReadSize = 3u;
	TransportConfig.ReadLimit = 65536u;
	TransportConfig.WriteLimit = 2u * 1024u * 1024u;
	TransportConfig.WriteHighWater = 1024u * 1024u;
	TransportConfig.WriteLowWater = 256u * 1024u;
	ListenConfig.Stream = TransportConfig;
	testRequire(
		xrtNetAddrLoopback(
			&ListenConfig.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"WSS listener address failed"
	);
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	ListenerEvents.Accept = testWsUpgradeTlsAccept;
	ListenerEvents.Error = testWsUpgradeTlsListenerError;
	ListenerEvents.Close = testWsUpgradeTlsListenerClose;
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
		"WSS listener start failed"
	);
	Test.Client.Handshake = xrtTlsStreamConnect(
		Test.Engine,
		&Address,
		1u,
		&TransportConfig,
		&ClientConfig,
		&Test.TlsStream,
		&Test.TlsEvents,
		&Test.Client
	);
	testRequire(
		Test.Client.Handshake != NULL,
		"WSS client connect failed"
	);

	testWsUpgradeTlsWait(
		&Test.Server.Messages,
		1u,
		"WSS server did not receive split request frame"
	);
	testWsUpgradeTlsWait(
		&Test.Client.Messages,
		1u,
		"WSS client did not receive compressed response"
	);
	testWsUpgradeTlsWait(
		&Test.Server.Closed,
		1u,
		"WSS server Close event missing"
	);
	testWsUpgradeTlsWait(
		&Test.Client.Closed,
		1u,
		"WSS client Close event missing"
	);
	pServer = (xwsstream*)xrtAtomicPtrLoad(
		&Test.Server.Stream,
		XMEMORY_ACQUIRE
	);
	pClient = (xwsstream*)xrtAtomicPtrLoad(
		&Test.Client.Stream,
		XMEMORY_ACQUIRE
	);
	testRequire(
		(pServer != NULL) && (pClient != NULL) &&
		(xrtAtomic32Load(&Test.Accepted, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Test.Server.Errors, XMEMORY_ACQUIRE) == 0) &&
		(xrtAtomic32Load(&Test.Client.Errors, XMEMORY_ACQUIRE) == 0) &&
		(xrtAtomic32Load(&Test.TlsErrors, XMEMORY_ACQUIRE) == 0) &&
		(xrtAtomic32Load(&Test.ListenerErrors, XMEMORY_ACQUIRE) == 0) &&
		(xrtWsStreamProtocol(pServer).Size == 4u) &&
		(xrtWsStreamProtocol(pClient).Size == 4u),
		"WSS Upgrade lifecycle mismatch"
	);
	testRequire(
		xrtNetListenerClose(Test.Listener),
		"WSS listener close failed"
	);
	testWsUpgradeTlsWait(
		&Test.ListenerClosed,
		1u,
		"WSS listener Close event missing"
	);
	xrtWsStreamDestroy(pClient);
	xrtWsStreamDestroy(pServer);
	xrtNetListenerDestroy(Test.Listener);
	testRequire(
		xrtNetEngineDestroy(Test.Engine),
		"WSS engine destroy failed"
	);
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);
	printf("[PASS] WebSocket TLS HTTP Upgrade handoff\n");
	return 0;
}

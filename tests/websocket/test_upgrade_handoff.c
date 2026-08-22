#include "../test.h"



#define TEST_WS_UPGRADE_KEY "dGhlIHNhbXBsZSBub25jZQ=="



typedef struct test_ws_upgrade_handoff test_ws_upgrade_handoff;



typedef struct test_ws_upgrade_endpoint {
	test_ws_upgrade_handoff* Test;
	bool Client;
	bool Compressed;
	uint8 Opcode;
	uint8 Data[32];
	size_t Size;
	xatomic32 Messages;
	xatomic32 Errors;
	xatomic32 Closed;
} test_ws_upgrade_endpoint;



struct test_ws_upgrade_handoff {
	xnetengine* Engine;
	xnetlistener* Listener;
	xwsupgradeserverconfig ServerConfig;
	xwsupgradeclientconfig ClientConfig;
	xnetstreamevents ServerRawEvents;
	xnetstreamevents ClientRawEvents;
	xwsstreamevents StreamEvents;
	test_ws_upgrade_endpoint Server;
	test_ws_upgrade_endpoint Client;
	char Request[1024];
	size_t RequestSize;
	uint8 Frame[64];
	size_t FrameSize;
	xatomicptr ServerStream;
	xatomicptr ClientStream;
	xatomic32 Accepted;
	xatomic32 RawErrors;
	xatomic32 ListenerErrors;
	xatomic32 ListenerClosed;
};



/* 在测试截止时间内等待完整链路状态。 */
static void testWsUpgradeWait(
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



/* 构建一个与 HTTP 请求同批发送的掩码 Text 帧。 */
static void testWsUpgradeFrameInit(test_ws_upgrade_handoff* pTest)
{
	static const uint8 Mask[XWS_MASK_SIZE] = {
		UINT8_C(0x11), UINT8_C(0x22),
		UINT8_C(0x33), UINT8_C(0x44)
	};
	static const uint8 Payload[] = { 'h', 'e', 'l', 'l', 'o' };
	xwsframe Frame;
	size_t iHead = 0;

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
		"WebSocket Upgrade handoff frame write failed"
	);
	memcpy(pTest->Frame + iHead, Payload, sizeof(Payload));
	testRequire(
		xrtWsMask(
			pTest->Frame + iHead,
			sizeof(Payload),
			Mask,
			0
		),
		"WebSocket Upgrade handoff frame mask failed"
	);
	pTest->FrameSize = iHead + sizeof(Payload);
}



/* 使用公共字段构造器生成带压缩 offer 的客户端请求。 */
static void testWsUpgradeRequestInit(test_ws_upgrade_handoff* pTest)
{
	xhttpfield Fields[XWS_UPGRADE_REQUEST_FIELDS_MAX];
	char Extensions[XWS_DEFLATE_MAX_SIZE + 1u];
	size_t iExtensions = 0;
	size_t iFields = 0;

	xrtWsUpgradeClientConfigInit(&pTest->ClientConfig);
	pTest->ClientConfig.Protocols = XRT_STR_LITERAL("chat");
	pTest->ClientConfig.EnableDeflate = true;
	testRequire(
		xrtWsDeflateOfferWrite(
			&pTest->ClientConfig.Deflate,
			Extensions,
			XWS_DEFLATE_MAX_SIZE,
			&iExtensions
		) && xrtWsUpgradeRequestFields(
			XRT_STR_LITERAL("127.0.0.1"),
			XRT_STR_LITERAL(TEST_WS_UPGRADE_KEY),
			pTest->ClientConfig.Protocols,
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
		"WebSocket Upgrade handoff request build failed"
	);
}



/* 每条消息直接流式写入测试端点，不建立连接级固定缓冲。 */
static void testWsUpgradeMessageBegin(
	xwsstream* pStream,
	const xwsmessageinfo* pInfo,
	ptr pData
)
{
	test_ws_upgrade_endpoint* pEndpoint =
		(test_ws_upgrade_endpoint*)pData;

	(void)pStream;
	testRequire(
		(pInfo != NULL) &&
		(pInfo->Opcode == XWS_OPCODE_TEXT),
		"WebSocket Upgrade handoff message type mismatch"
	);
	pEndpoint->Opcode = pInfo->Opcode;
	pEndpoint->Compressed =
		(pInfo->Flags & XWS_MESSAGE_COMPRESSED) != 0;
	pEndpoint->Size = 0;
}



/* 复制当前同步回调视图，验证跨块消息发布顺序。 */
static void testWsUpgradeMessageData(
	xwsstream* pStream,
	xbytesview Data,
	ptr pData
)
{
	test_ws_upgrade_endpoint* pEndpoint =
		(test_ws_upgrade_endpoint*)pData;

	(void)pStream;
	testRequire(
		Data.Size <= (sizeof(pEndpoint->Data) - pEndpoint->Size),
		"WebSocket Upgrade handoff message overflow"
	);
	memcpy(pEndpoint->Data + pEndpoint->Size, Data.Data, Data.Size);
	pEndpoint->Size += Data.Size;
}



/* 服务端收到同包余量后压缩回送，客户端收到解压正文后关闭。 */
static void testWsUpgradeMessageEnd(xwsstream* pStream, ptr pData)
{
	test_ws_upgrade_endpoint* pEndpoint =
		(test_ws_upgrade_endpoint*)pData;

	if ( pEndpoint->Client ) {
		testRequire(
			pEndpoint->Compressed &&
			(pEndpoint->Size == 5u) &&
			(memcmp(pEndpoint->Data, "world", 5u) == 0),
			"WebSocket compressed response handoff mismatch"
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
			"WebSocket Upgrade handoff close failed"
		);
	} else {
		testRequire(
			!pEndpoint->Compressed &&
			(pEndpoint->Size == 5u) &&
			(memcmp(pEndpoint->Data, "hello", 5u) == 0),
			"WebSocket request suffix handoff mismatch"
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
			"WebSocket compressed response send failed"
		);
	}
}



/* 记录 WebSocket 核心链路中的结构化错误。 */
static void testWsUpgradeStreamError(
	xwsstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	test_ws_upgrade_endpoint* pEndpoint =
		(test_ws_upgrade_endpoint*)pData;

	(void)pStream;
	testRequire(
		pError != NULL,
		"WebSocket Upgrade stream error omitted its cause"
	);
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Errors,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录双方完整 Close 握手终态。 */
static void testWsUpgradeStreamClose(
	xwsstream* pStream,
	const xwsstreamclose* pClose,
	ptr pData
)
{
	test_ws_upgrade_endpoint* pEndpoint =
		(test_ws_upgrade_endpoint*)pData;

	(void)pStream;
	testRequire(
		(pClose != NULL) &&
		((pClose->Flags & XWS_STREAM_CLOSE_CLEAN) != 0),
		"WebSocket Upgrade handoff did not close cleanly"
	);
	(void)xrtAtomic32FetchAdd(
		&pEndpoint->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 校验请求、发送 101，并原子跳过 Header 接管剩余帧。 */
static void testWsUpgradeServerRead(
	xnetstream* pTransport,
	xnetbuf* pBuffer,
	ptr pData
)
{
	test_ws_upgrade_handoff* pTest =
		(test_ws_upgrade_handoff*)pData;
	xhttpfield ParsedFields[16];
	xhttpfield ResponseFields[XWS_UPGRADE_RESPONSE_FIELDS_MAX];
	xhttp1head Head;
	xwsupgrade Upgrade;
	xwsstreamconfig Config;
	xwsstream* pStream;
	char Response[1024];
	size_t iFields = 0;
	size_t iResponse = 0;
	xhttp1status Status;

	xrtHttp1HeadInit(
		&Head,
		ParsedFields,
		sizeof(ParsedFields) / sizeof(ParsedFields[0])
	);
	Status = xrtHttp1RequestParseBuffer(
		pBuffer,
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
			&pTest->ServerConfig,
			&Upgrade
		) && Upgrade.DeflateEnabled &&
		xrtWsUpgradeResponseFields(
			(xstrview) { Upgrade.Accept, XWS_ACCEPT_SIZE },
			Upgrade.Protocol,
			(xstrview) {
				Upgrade.Extensions,
				Upgrade.ExtensionSize
			},
			ResponseFields,
			sizeof(ResponseFields) / sizeof(ResponseFields[0]),
			&iFields
		) && xrtHttp1ResponseWrite(
			XHTTP_VERSION_1_1,
			XHTTP_STATUS_SWITCHING_PROTOCOLS,
			XRT_STR_LITERAL("Switching Protocols"),
			ResponseFields,
			iFields,
			Response,
			sizeof(Response),
			&iResponse
		) && (xrtNetStreamSend(
			pTransport,
			Response,
			iResponse
		) == XNET_RESULT_OK) &&
		xrtWsUpgradeStreamConfig(
			&Config,
			XWS_ROLE_SERVER,
			&Upgrade
		),
		"WebSocket server Upgrade handoff failed"
	);
	pStream = xrtWsStreamAttach(
		pTransport,
		Head.Bytes,
		&Config,
		&pTest->StreamEvents,
		&pTest->Server
	);
	testRequire(
		pStream != NULL,
		"WebSocket server Stream attach failed"
	);
	xrtAtomicPtrStore(
		&pTest->ServerStream,
		pStream,
		XMEMORY_RELEASE
	);
}



/* 校验 101 与请求 Key 的绑定，并接管响应后的压缩帧余量。 */
static void testWsUpgradeClientRead(
	xnetstream* pTransport,
	xnetbuf* pBuffer,
	ptr pData
)
{
	test_ws_upgrade_handoff* pTest =
		(test_ws_upgrade_handoff*)pData;
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
	Status = xrtHttp1ResponseParseBuffer(
		pBuffer,
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
			XRT_STR_LITERAL(TEST_WS_UPGRADE_KEY),
			&pTest->ClientConfig,
			&Upgrade
		) && Upgrade.DeflateEnabled &&
		xrtWsUpgradeStreamConfig(
			&Config,
			XWS_ROLE_CLIENT,
			&Upgrade
		),
		"WebSocket client Upgrade handoff failed"
	);
	pStream = xrtWsStreamAttach(
		pTransport,
		Head.Bytes,
		&Config,
		&pTest->StreamEvents,
		&pTest->Client
	);
	testRequire(
		pStream != NULL,
		"WebSocket client Stream attach failed"
	);
	xrtAtomicPtrStore(
		&pTest->ClientStream,
		pStream,
		XMEMORY_RELEASE
	);
}



/* TCP 打开后把 HTTP Header 与首个 WebSocket 帧聚集为一次发送。 */
static void testWsUpgradeClientOpen(xnetstream* pStream, ptr pData)
{
	test_ws_upgrade_handoff* pTest =
		(test_ws_upgrade_handoff*)pData;
	xnetspan Spans[2];

	Spans[0] = (xnetspan) {
		(cbytes)pTest->Request,
		pTest->RequestSize
	};
	Spans[1] = (xnetspan) {
		pTest->Frame,
		pTest->FrameSize
	};
	testRequire(
		xrtNetStreamSendVec(
			pStream,
			Spans,
			sizeof(Spans) / sizeof(Spans[0])
		) == XNET_RESULT_OK,
		"WebSocket HTTP and frame vector send failed"
	);
}



/* Upgrade 前异常关闭只能计入原始传输错误。 */
static void testWsUpgradeRawClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_ws_upgrade_handoff* pTest =
		(test_ws_upgrade_handoff*)pData;

	(void)pStream;
	if ( (Result != XNET_RESULT_OK) || (pError != NULL) ) {
		(void)xrtAtomic32FetchAdd(
			&pTest->RawErrors,
			1,
			XMEMORY_RELEASE
		);
	}
}



/* Listener 把已接受引用交给原始 HTTP 回调。 */
static bool testWsUpgradeAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	test_ws_upgrade_handoff* pTest =
		(test_ws_upgrade_handoff*)pData;

	(void)pListener;
	testRequire(
		(xrtNetStreamState(pStream) == XNET_STREAM_OPEN) &&
		xrtNetStreamSetEvents(
			pStream,
			&pTest->ServerRawEvents,
			pTest
		),
		"WebSocket Upgrade accepted Stream setup failed"
	);
	(void)xrtAtomic32FetchAdd(
		&pTest->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* Listener 错误保持独立统计。 */
static void testWsUpgradeListenerError(
	xnetlistener* pListener,
	const xerror* pError,
	ptr pData
)
{
	test_ws_upgrade_handoff* pTest =
		(test_ws_upgrade_handoff*)pData;

	(void)pListener;
	testRequire(
		pError != NULL,
		"WebSocket Upgrade listener error omitted its cause"
	);
	(void)xrtAtomic32FetchAdd(
		&pTest->ListenerErrors,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 Listener 终态。 */
static void testWsUpgradeListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_ws_upgrade_handoff* pTest =
		(test_ws_upgrade_handoff*)pData;

	(void)pListener;
	(void)xrtAtomic32FetchAdd(
		&pTest->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证 TCP -> Upgrade -> 压缩 WebSocket 的完整高性能链路。 */
int main(void)
{
	test_ws_upgrade_handoff Test;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamconfig StreamConfig;
	xnetaddr Address;
	xnetstream* pClientTransport;
	xwsstream* pClient;
	xwsstream* pServer;

	memset(&Test, 0, sizeof(Test));
	Test.Server.Test = &Test;
	Test.Client.Test = &Test;
	Test.Client.Client = true;
	xrtAtomicPtrInit(&Test.ServerStream, NULL);
	xrtAtomicPtrInit(&Test.ClientStream, NULL);
	xrtWsUpgradeServerConfigInit(&Test.ServerConfig);
	Test.ServerConfig.Protocols = XRT_STR_LITERAL("chat");
	Test.ServerConfig.EnableDeflate = true;
	testWsUpgradeRequestInit(&Test);
	testWsUpgradeFrameInit(&Test);

	Test.StreamEvents.MessageBegin = testWsUpgradeMessageBegin;
	Test.StreamEvents.MessageData = testWsUpgradeMessageData;
	Test.StreamEvents.MessageEnd = testWsUpgradeMessageEnd;
	Test.StreamEvents.Error = testWsUpgradeStreamError;
	Test.StreamEvents.Close = testWsUpgradeStreamClose;
	Test.ServerRawEvents.Read = testWsUpgradeServerRead;
	Test.ServerRawEvents.Close = testWsUpgradeRawClose;
	Test.ClientRawEvents.Open = testWsUpgradeClientOpen;
	Test.ClientRawEvents.Read = testWsUpgradeClientRead;
	Test.ClientRawEvents.Close = testWsUpgradeRawClose;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2u;
	Test.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(Test.Engine != NULL) && xrtNetEngineStart(Test.Engine),
		"WebSocket Upgrade select engine start failed"
	);

	xrtNetListenConfigInit(&ListenConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ListenConfig.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"WebSocket Upgrade listener address failed"
	);
	ListenConfig.Stream.ReadSize = 3u;
	ListenConfig.Stream.ReadLimit = 2048u;
	ListenConfig.Stream.WriteLimit = 2u * 1024u * 1024u;
	ListenConfig.Stream.WriteHighWater = 1024u * 1024u;
	ListenConfig.Stream.WriteLowWater = 256u * 1024u;
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	ListenerEvents.Accept = testWsUpgradeAccept;
	ListenerEvents.Error = testWsUpgradeListenerError;
	ListenerEvents.Close = testWsUpgradeListenerClose;
	Test.Listener = xrtNetListen(
		Test.Engine,
		&ListenConfig,
		&ListenerEvents,
		&Test.ServerRawEvents,
		&Test
	);
	testRequire(
		(Test.Listener != NULL) &&
		xrtNetListenerLocal(Test.Listener, &Address),
		"WebSocket Upgrade listener start failed"
	);

	xrtNetStreamConfigInit(&StreamConfig);
	StreamConfig.ReadSize = 3u;
	StreamConfig.ReadLimit = 2048u;
	StreamConfig.WriteLimit = 2u * 1024u * 1024u;
	StreamConfig.WriteHighWater = 1024u * 1024u;
	StreamConfig.WriteLowWater = 256u * 1024u;
	pClientTransport = xrtNetStreamConnect(
		Test.Engine,
		&Address,
		1u,
		&StreamConfig,
		&Test.ClientRawEvents,
		&Test
	);
	testRequire(
		pClientTransport != NULL,
		"WebSocket Upgrade client connect failed"
	);

	testWsUpgradeWait(
		&Test.Server.Messages,
		1u,
		"WebSocket server did not receive Upgrade suffix"
	);
	testWsUpgradeWait(
		&Test.Client.Messages,
		1u,
		"WebSocket client did not receive compressed response"
	);
	testWsUpgradeWait(
		&Test.Server.Closed,
		1u,
		"WebSocket server Close event missing"
	);
	testWsUpgradeWait(
		&Test.Client.Closed,
		1u,
		"WebSocket client Close event missing"
	);
	pServer = (xwsstream*)xrtAtomicPtrLoad(
		&Test.ServerStream,
		XMEMORY_ACQUIRE
	);
	pClient = (xwsstream*)xrtAtomicPtrLoad(
		&Test.ClientStream,
		XMEMORY_ACQUIRE
	);
	testRequire(
		(pServer != NULL) && (pClient != NULL) &&
		(xrtAtomic32Load(&Test.Accepted, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Test.Server.Errors, XMEMORY_ACQUIRE) == 0) &&
		(xrtAtomic32Load(&Test.Client.Errors, XMEMORY_ACQUIRE) == 0) &&
		(xrtAtomic32Load(&Test.RawErrors, XMEMORY_ACQUIRE) == 0) &&
		(xrtAtomic32Load(&Test.ListenerErrors, XMEMORY_ACQUIRE) == 0) &&
		(xrtWsStreamProtocol(pServer).Size == 4u) &&
		(xrtWsStreamProtocol(pClient).Size == 4u),
		"WebSocket Upgrade handoff lifecycle mismatch"
	);
	testRequire(
		xrtNetListenerClose(Test.Listener),
		"WebSocket Upgrade listener close failed"
	);
	testWsUpgradeWait(
		&Test.ListenerClosed,
		1u,
		"WebSocket Upgrade listener Close event missing"
	);
	xrtWsStreamDestroy(pClient);
	xrtWsStreamDestroy(pServer);
	xrtNetListenerDestroy(Test.Listener);
	testRequire(
		xrtNetEngineDestroy(Test.Engine),
		"WebSocket Upgrade engine destroy failed"
	);
	printf("[PASS] WebSocket HTTP Upgrade handoff\n");
	return 0;
}

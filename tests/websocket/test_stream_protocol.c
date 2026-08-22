#include "../test.h"



#ifndef TEST_WS_CONNECTION_BACKEND
	#define TEST_WS_CONNECTION_BACKEND XNET_PORT_SELECT
#endif



typedef enum test_ws_protocol_kind {
	TEST_WS_PROTOCOL_UNMASKED = 0,
	TEST_WS_PROTOCOL_MASKED,
	TEST_WS_PROTOCOL_UTF8,
	TEST_WS_PROTOCOL_FRAME_LIMIT,
	TEST_WS_PROTOCOL_CONTINUATION,
	TEST_WS_PROTOCOL_MESSAGE_LIMIT,
	TEST_WS_PROTOCOL_FRAGMENT_UTF8
} test_ws_protocol_kind;



typedef struct test_ws_protocol_case {
	xwsrole Role;
	test_ws_protocol_kind Kind;
	uint16 CloseCode;
	xwsstreamerror ErrorCode;
} test_ws_protocol_case;



typedef struct test_ws_protocol {
	const test_ws_protocol_case* Case;
	xnetengine* Engine;
	xnetlistener* Listener;
	xatomicptr Connection;
	xatomicptr Raw;
	xatomic32 ErrorCount;
	xatomic32 ErrorCode;
	xatomic32 CloseSeen;
	xatomic32 ConnectionClosed;
	xatomic32 RawClosed;
	xatomic32 ListenerClosed;
	xatomic32 ListenerErrors;
	xwsstreamclose Close;
	bool RawReading;
	xwsstreamevents ConnectionEvents;
	xnetstreamevents RawEvents;
} test_ws_protocol;



/* 在测试截止时间内等待一个原子状态。 */
static void testWsProtocolWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(
		UINT64_C(10000000)
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



/* 写出小于 126 字节的原始帧，允许故意违反掩码方向。 */
static void testWsProtocolSendFrame(
	xnetstream* pStream,
	uint8 iOpcode,
	bool bFinal,
	bool bMasked,
	cbytes pPayload,
	size_t iSize
)
{
	static const uint8 Mask[XWS_MASK_SIZE] = {
		0x21, 0x43, 0x65, 0x87
	};
	uint8 Output[64];
	size_t iHead = 2;

	testRequire(
		(iSize <= 125u) &&
		(iSize <= (sizeof(Output) - 6u)),
		"WebSocket protocol test frame is too large"
	);
	Output[0] = (uint8)(
		iOpcode | (bFinal ? UINT8_C(0x80) : 0)
	);
	Output[1] = (uint8)(
		iSize | (bMasked ? UINT8_C(0x80) : 0)
	);
	if ( bMasked ) {
		memcpy(Output + iHead, Mask, sizeof(Mask));
		iHead += sizeof(Mask);
	}
	if ( iSize != 0 ) {
		memcpy(Output + iHead, pPayload, iSize);
		if ( bMasked ) {
			testRequire(
				xrtWsMask(
					Output + iHead,
					iSize,
					Mask,
					0
				),
				"WebSocket protocol test masking failed"
			);
		}
	}
	testRequire(
		xrtNetStreamSend(
			pStream,
			Output,
			iHead + iSize
		) == XNET_RESULT_OK,
		"WebSocket protocol test send failed"
	);
}



/* 按场景发送单帧或跨帧非法输入。 */
static void testWsProtocolSendInput(
	test_ws_protocol* pTest,
	xnetstream* pStream
)
{
	static const uint8 InvalidUtf8[] = {
		UINT8_C(0xC3), UINT8_C(0x28)
	};
	static const uint8 Oversized[] = {
		'0', '1', '2', '3', '4', '5', '6', '7', '8'
	};
	static const uint8 FirstSize[] = {
		'0', '1', '2', '3', '4', '5'
	};
	static const uint8 LastSize[] = {
		'6', '7', '8', '9'
	};
	static const uint8 FirstUtf8[] = {
		'A', UINT8_C(0xC3)
	};
	static const uint8 LastUtf8[] = {
		UINT8_C(0x28)
	};
	bool bMask = pTest->Case->Role == XWS_ROLE_SERVER;

	switch ( pTest->Case->Kind ) {
		case TEST_WS_PROTOCOL_UNMASKED:
			testWsProtocolSendFrame(
				pStream,
				(uint8)XWS_OPCODE_TEXT,
				true,
				false,
				(cbytes)"x",
				1
			);
			break;
		case TEST_WS_PROTOCOL_MASKED:
			testWsProtocolSendFrame(
				pStream,
				(uint8)XWS_OPCODE_TEXT,
				true,
				true,
				(cbytes)"x",
				1
			);
			break;
		case TEST_WS_PROTOCOL_UTF8:
			testWsProtocolSendFrame(
				pStream,
				(uint8)XWS_OPCODE_TEXT,
				true,
				bMask,
				InvalidUtf8,
				sizeof(InvalidUtf8)
			);
			break;
		case TEST_WS_PROTOCOL_FRAME_LIMIT:
			testWsProtocolSendFrame(
				pStream,
				(uint8)XWS_OPCODE_BINARY,
				true,
				bMask,
				Oversized,
				sizeof(Oversized)
			);
			break;
		case TEST_WS_PROTOCOL_CONTINUATION:
			testWsProtocolSendFrame(
				pStream,
				(uint8)XWS_OPCODE_CONTINUATION,
				true,
				bMask,
				(cbytes)"x",
				1
			);
			break;
		case TEST_WS_PROTOCOL_MESSAGE_LIMIT:
			testWsProtocolSendFrame(
				pStream,
				(uint8)XWS_OPCODE_BINARY,
				false,
				bMask,
				FirstSize,
				sizeof(FirstSize)
			);
			testWsProtocolSendFrame(
				pStream,
				(uint8)XWS_OPCODE_CONTINUATION,
				true,
				bMask,
				LastSize,
				sizeof(LastSize)
			);
			break;
		case TEST_WS_PROTOCOL_FRAGMENT_UTF8:
			testWsProtocolSendFrame(
				pStream,
				(uint8)XWS_OPCODE_TEXT,
				false,
				bMask,
				FirstUtf8,
				sizeof(FirstUtf8)
			);
			testWsProtocolSendFrame(
				pStream,
				(uint8)XWS_OPCODE_CONTINUATION,
				true,
				bMask,
				LastUtf8,
				sizeof(LastUtf8)
			);
			break;
		default:
			testRequire(
				false,
				"unknown WebSocket protocol test"
			);
			break;
	}
}



/* 记录统一会话发布的唯一协议错误。 */
static void testWsProtocolError(
	xwsstream* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_ws_protocol* pTest =
		(test_ws_protocol*)pData;

	(void)pConnection;
	testRequire(
		pError != NULL,
		"WebSocket protocol error is null"
	);
	xrtAtomic32Store(
		&pTest->ErrorCode,
		(uint32)xrtErrorCode(pError),
		XMEMORY_RELEASE
	);
	(void)xrtAtomic32FetchAdd(
		&pTest->ErrorCount,
		1,
		XMEMORY_RELEASE
	);
}



/* 保存协议失败后的关闭快照。 */
static void testWsProtocolClose(
	xwsstream* pConnection,
	const xwsstreamclose* pClose,
	ptr pData
)
{
	test_ws_protocol* pTest =
		(test_ws_protocol*)pData;

	testRequire(
		(pClose != NULL) &&
		(xrtWsStreamState(pConnection) ==
		 XWS_STREAM_CLOSED),
		"WebSocket protocol Close state mismatch"
	);
	pTest->Close = *pClose;
	(void)xrtAtomic32FetchAdd(
		&pTest->ConnectionClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 接管一个已经开放的端点并应用严格的小消息上限。 */
static xwsstream* testWsProtocolAttach(
	test_ws_protocol* pTest,
	xnetstream* pStream
)
{
	xwsstreamconfig Config;

	xrtWsStreamConfigInit(&Config);
	Config.Role = pTest->Case->Role;
	Config.MessageLimit = 8;
	Config.FrameLimit = 8;
	Config.SendLimit = 1024;
	Config.ControlReserve = 512;
	return xrtWsStreamAttach(
		pStream,
		0,
		&Config,
		&pTest->ConnectionEvents,
		pTest
	);
}



/* 解析会话发出的错误 Close，并验证线上状态码。 */
static void testWsProtocolRawRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	test_ws_protocol* pTest =
		(test_ws_protocol*)pData;
	xwsframeconfig Config;
	xwsframeerrorinfo FrameError;
	xwsframe Frame;
	xwsclose Close;
	uint8 Header[XWS_FRAME_HEAD_MAX];
	uint8 Payload[XWS_CLOSE_PAYLOAD_MAX];
	size_t iAvailable;
	size_t iHeader;
	size_t iTotal;
	xwsframestatus Status;

	if ( pTest->RawReading ) {
		return;
	}
	pTest->RawReading = true;
	iAvailable = xrtNetBufSize(pBuffer);
	iHeader = iAvailable < sizeof(Header) ?
		iAvailable : sizeof(Header);
	if ( (iHeader == 0) || (xrtNetBufPeek(
		pBuffer,
		0,
		Header,
		iHeader
	) != iHeader) ) {
		pTest->RawReading = false;
		return;
	}
	xrtWsFrameConfigInit(&Config);
	Config.Mask = pTest->Case->Role ==
		XWS_ROLE_CLIENT ?
			XWS_MASK_REQUIRED : XWS_MASK_FORBIDDEN;
	Config.MaxPayload = XWS_CLOSE_PAYLOAD_MAX;
	Status = xrtWsFrameParse(
		(xbytesview) { Header, iHeader },
		&Frame,
		&Config,
		&FrameError
	);
	if ( Status == XWS_FRAME_MORE ) {
		pTest->RawReading = false;
		return;
	}
	testRequire(
		(Status == XWS_FRAME_READY) &&
		(Frame.Opcode == (uint8)XWS_OPCODE_CLOSE) &&
		(Frame.PayloadSize <= sizeof(Payload)),
		"WebSocket protocol response is not a Close frame"
	);
	iTotal = Frame.HeadSize + (size_t)Frame.PayloadSize;
	if ( iAvailable < iTotal ) {
		pTest->RawReading = false;
		return;
	}
	testRequire(
		xrtNetBufPeek(
			pBuffer,
			Frame.HeadSize,
			Payload,
			(size_t)Frame.PayloadSize
		) == (size_t)Frame.PayloadSize,
		"WebSocket protocol Close payload peek failed"
	);
	if ( (Frame.Flags & XWS_FRAME_MASKED) != 0 ) {
		testRequire(
			xrtWsMask(
				Payload,
				(size_t)Frame.PayloadSize,
				Frame.Mask,
				0
			),
			"WebSocket protocol Close unmask failed"
		);
	}
	testRequire(
		xrtWsCloseParse(
			(xbytesview) {
				Payload,
				(size_t)Frame.PayloadSize
			},
			&Close
		) &&
		(Close.Code == pTest->Case->CloseCode),
		"WebSocket protocol Close code mismatch"
	);
	xrtAtomic32Store(
		&pTest->CloseSeen,
		1,
		XMEMORY_RELEASE
	);
	testRequire(
		xrtNetStreamConsume(pStream, iTotal) == iTotal,
		"WebSocket protocol response consume failed"
	);
	pTest->RawReading = false;
	testRequire(
		xrtNetStreamClose(pStream),
		"WebSocket raw peer close failed"
	);
}



/* 对端半关闭后完成本地正常关闭。 */
static void testWsProtocolRawEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	testRequire(
		xrtNetStreamClose(pStream),
		"WebSocket raw peer end close failed"
	);
}



/* 记录原始对端传输终态。 */
static void testWsProtocolRawClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_ws_protocol* pTest =
		(test_ws_protocol*)pData;

	(void)pStream;
	(void)pError;
	testRequire(
		Result == XNET_RESULT_OK,
		"WebSocket raw peer transport close failed"
	);
	(void)xrtAtomic32FetchAdd(
		&pTest->RawClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 服务端角色接管 Accepted Stream；客户端角色保留它作为原始对端。 */
static bool testWsProtocolAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	test_ws_protocol* pTest =
		(test_ws_protocol*)pData;

	(void)pListener;
	if ( pTest->Case->Role == XWS_ROLE_SERVER ) {
		xwsstream* pConnection =
			testWsProtocolAttach(pTest, pStream);

		testRequire(
			pConnection != NULL,
			"WebSocket protocol server attach failed"
		);
		xrtAtomicPtrStore(
			&pTest->Connection,
			pConnection,
			XMEMORY_RELEASE
		);
	} else {
		testRequire(
			xrtNetStreamSetEvents(
				pStream,
				&pTest->RawEvents,
				pTest
			),
			"WebSocket raw server event setup failed"
		);
		xrtAtomicPtrStore(
			&pTest->Raw,
			pStream,
			XMEMORY_RELEASE
		);
		testWsProtocolSendInput(pTest, pStream);
	}
	return true;
}



/* 客户端角色接管 Connect Stream；服务端角色把它作为原始对端。 */
static void testWsProtocolClientOpen(
	xnetstream* pStream,
	ptr pData
)
{
	test_ws_protocol* pTest =
		(test_ws_protocol*)pData;

	if ( pTest->Case->Role == XWS_ROLE_CLIENT ) {
		xwsstream* pConnection =
			testWsProtocolAttach(pTest, pStream);

		testRequire(
			pConnection != NULL,
			"WebSocket protocol client attach failed"
		);
		xrtAtomicPtrStore(
			&pTest->Connection,
			pConnection,
			XMEMORY_RELEASE
		);
	} else {
		xrtAtomicPtrStore(
			&pTest->Raw,
			pStream,
			XMEMORY_RELEASE
		);
		testWsProtocolSendInput(pTest, pStream);
	}
}



/* Listener 错误不能与会话协议错误混淆。 */
static void testWsProtocolListenerError(
	xnetlistener* pListener,
	const xerror* pError,
	ptr pData
)
{
	test_ws_protocol* pTest =
		(test_ws_protocol*)pData;

	(void)pListener;
	testRequire(
		pError != NULL,
		"WebSocket protocol listener error is null"
	);
	(void)xrtAtomic32FetchAdd(
		&pTest->ListenerErrors,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 Listener 唯一关闭事件。 */
static void testWsProtocolListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_ws_protocol* pTest =
		(test_ws_protocol*)pData;

	(void)pListener;
	(void)xrtAtomic32FetchAdd(
		&pTest->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 执行一个真实 TCP 协议失败场景。 */
static void testWsProtocolRun(
	const test_ws_protocol_case* pCase
)
{
	test_ws_protocol Test;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xnetstreamconfig StreamConfig;
	xnetstreamevents ClientEvents;
	xnetaddr Address;
	xnetstream* pConnected;
	xnetstream* pRaw;
	xwsstream* pConnection;
	xdeadline AttachDeadline;

	memset(&Test, 0, sizeof(Test));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	memset(&ClientEvents, 0, sizeof(ClientEvents));
	Test.Case = pCase;
	xrtAtomicPtrInit(&Test.Connection, NULL);
	xrtAtomicPtrInit(&Test.Raw, NULL);
	xrtAtomic32Init(&Test.ErrorCount, 0);
	xrtAtomic32Init(&Test.ErrorCode, 0);
	xrtAtomic32Init(&Test.CloseSeen, 0);
	xrtAtomic32Init(&Test.ConnectionClosed, 0);
	xrtAtomic32Init(&Test.RawClosed, 0);
	xrtAtomic32Init(&Test.ListenerClosed, 0);
	xrtAtomic32Init(&Test.ListenerErrors, 0);
	Test.ConnectionEvents.Error = testWsProtocolError;
	Test.ConnectionEvents.Close = testWsProtocolClose;
	Test.RawEvents.Read = testWsProtocolRawRead;
	Test.RawEvents.End = testWsProtocolRawEnd;
	Test.RawEvents.Close = testWsProtocolRawClose;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_WS_CONNECTION_BACKEND;
	EngineConfig.Workers = 2;
	Test.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(Test.Engine != NULL) &&
		xrtNetEngineStart(Test.Engine),
		"WebSocket protocol engine start failed"
	);
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ListenConfig.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"WebSocket protocol listener address failed"
	);
	ListenConfig.Stream.ReadSize = 1;
	ListenConfig.Stream.ReadLimit = 128;
	ListenConfig.Stream.WriteLimit = 1024;
	ListenConfig.Stream.WriteHighWater = 768;
	ListenConfig.Stream.WriteLowWater = 256;
	ListenerEvents.Accept = testWsProtocolAccept;
	ListenerEvents.Error = testWsProtocolListenerError;
	ListenerEvents.Close = testWsProtocolListenerClose;
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
		"WebSocket protocol listener start failed"
	);

	xrtNetStreamConfigInit(&StreamConfig);
	StreamConfig.ReadSize = 1;
	StreamConfig.ReadLimit = 128;
	StreamConfig.WriteLimit = 1024;
	StreamConfig.WriteHighWater = 768;
	StreamConfig.WriteLowWater = 256;
	ClientEvents.Open = testWsProtocolClientOpen;
	if ( pCase->Role == XWS_ROLE_SERVER ) {
		ClientEvents.Read = testWsProtocolRawRead;
		ClientEvents.End = testWsProtocolRawEnd;
		ClientEvents.Close = testWsProtocolRawClose;
	}
	pConnected = xrtNetStreamConnect(
		Test.Engine,
		&Address,
		1,
		&StreamConfig,
		&ClientEvents,
		&Test
	);
	testRequire(
		pConnected != NULL,
		"WebSocket protocol TCP connect failed"
	);

	AttachDeadline = xrtDeadlineAfter(
		UINT64_C(10000000)
	);
	while ( ((pConnection = (xwsstream*)
		xrtAtomicPtrLoad(
			&Test.Connection,
			XMEMORY_ACQUIRE
		)) == NULL) ||
		((pRaw = (xnetstream*)xrtAtomicPtrLoad(
			&Test.Raw,
			XMEMORY_ACQUIRE
		 )) == NULL) ) {
		testRequire(
			!xrtDeadlineExpired(AttachDeadline),
			"WebSocket protocol endpoints were not attached"
		);
		xrtThreadYield();
	}
	testWsProtocolWait(
		&Test.CloseSeen,
		1,
		"WebSocket protocol Close frame was not observed"
	);
	testWsProtocolWait(
		&Test.ConnectionClosed,
		1,
		"WebSocket protocol connection did not close"
	);
	testWsProtocolWait(
		&Test.RawClosed,
		1,
		"WebSocket protocol raw peer did not close"
	);
	testRequire(
		(xrtAtomic32Load(
			&Test.ErrorCount,
			XMEMORY_ACQUIRE
		 ) == 1) &&
		(xrtAtomic32Load(
			&Test.ErrorCode,
			XMEMORY_ACQUIRE
		 ) == (uint32)pCase->ErrorCode) &&
		(Test.Close.Transport == XNET_RESULT_OK) &&
		((Test.Close.Flags &
		  XWS_STREAM_CLOSE_SENT) != 0) &&
		((Test.Close.Flags &
		  XWS_STREAM_CLOSE_RECEIVED) == 0) &&
		((Test.Close.Flags &
		  XWS_STREAM_CLOSE_CLEAN) == 0) &&
		(Test.Close.LocalCode == pCase->CloseCode) &&
		(Test.Close.RemoteCode == 0) &&
		(xrtAtomic32Load(
			&Test.ListenerErrors,
			XMEMORY_ACQUIRE
		 ) == 0),
		"WebSocket protocol failure snapshot mismatch"
	);
	testRequire(
		xrtNetListenerClose(Test.Listener),
		"WebSocket protocol listener close failed"
	);
	testWsProtocolWait(
		&Test.ListenerClosed,
		1,
		"WebSocket protocol listener did not close"
	);
	xrtWsStreamDestroy(pConnection);
	xrtNetStreamDestroy(pRaw);
	xrtNetListenerDestroy(Test.Listener);
	testRequire(
		xrtNetEngineDestroy(Test.Engine),
		"WebSocket protocol engine destroy failed"
	);
}



/* 验证双向掩码、UTF-8、分片和大小限制的标准关闭码。 */
int main(void)
{
	static const test_ws_protocol_case Cases[] = {
		{
			XWS_ROLE_SERVER,
			TEST_WS_PROTOCOL_UNMASKED,
			XWS_CLOSE_PROTOCOL,
			XWS_STREAM_ERROR_FRAME
		},
		{
			XWS_ROLE_CLIENT,
			TEST_WS_PROTOCOL_MASKED,
			XWS_CLOSE_PROTOCOL,
			XWS_STREAM_ERROR_FRAME
		},
		{
			XWS_ROLE_SERVER,
			TEST_WS_PROTOCOL_UTF8,
			XWS_CLOSE_INVALID_DATA,
			XWS_STREAM_ERROR_MESSAGE
		},
		{
			XWS_ROLE_SERVER,
			TEST_WS_PROTOCOL_FRAME_LIMIT,
			XWS_CLOSE_TOO_BIG,
			XWS_STREAM_ERROR_FRAME
		},
		{
			XWS_ROLE_SERVER,
			TEST_WS_PROTOCOL_CONTINUATION,
			XWS_CLOSE_PROTOCOL,
			XWS_STREAM_ERROR_MESSAGE
		},
		{
			XWS_ROLE_SERVER,
			TEST_WS_PROTOCOL_MESSAGE_LIMIT,
			XWS_CLOSE_TOO_BIG,
			XWS_STREAM_ERROR_MESSAGE
		},
		{
			XWS_ROLE_SERVER,
			TEST_WS_PROTOCOL_FRAGMENT_UTF8,
			XWS_CLOSE_INVALID_DATA,
			XWS_STREAM_ERROR_MESSAGE
		}
	};

	for ( size_t i = 0;
		i < (sizeof(Cases) / sizeof(Cases[0]));
		i++
	) {
		testWsProtocolRun(&Cases[i]);
	}
	printf(
		"[PASS] WebSocket connection protocol failure matrix\n"
	);
	return 0;
}

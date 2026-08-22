#include "../test.h"



/* 运行时回环使用固定测试容量，不把协议行为和动态收集器混在一起。 */
typedef struct test_ws_runtime_output {
	uint8 Data[16384];
	size_t Size;
} test_ws_runtime_output;



/* 消息集成输出同时提交语义状态机并收集应用数据。 */
typedef struct test_ws_runtime_message {
	xwsmessagestate* Message;
	xwsmessageerrorinfo* Error;
	test_ws_runtime_output* Output;
} test_ws_runtime_message;



/* 收集一个运行时输出分块。 */
static bool testWsRuntimeOutput(
	xbytesview Data,
	ptr pData
)
{
	test_ws_runtime_output* pOutput =
		(test_ws_runtime_output*)pData;

	testRequire(
		Data.Size <=
			(sizeof(pOutput->Data) - pOutput->Size),
		"WebSocket runtime fixture overflowed"
	);
	memcpy(
		pOutput->Data + pOutput->Size,
		Data.Data,
		Data.Size
	);
	pOutput->Size += Data.Size;
	return true;
}



/* 把解压结果直接送入公开消息状态机，再交给应用收集器。 */
static bool testWsRuntimeMessageOutput(
	xbytesview Data,
	ptr pData
)
{
	test_ws_runtime_message* pMessage =
		(test_ws_runtime_message*)pData;

	return xrtWsMessagePayload(
		pMessage->Message,
		Data,
		pMessage->Error
	) && testWsRuntimeOutput(
		Data,
		pMessage->Output
	);
}



/* 构造一个不持有负载的帧描述。 */
static xwsframe testWsRuntimeFrame(
	uint8 iOpcode,
	bool bFinal,
	uint32 iRsv,
	uint64 iPayloadSize
)
{
	xwsframe Frame;

	xrtWsFrameInit(&Frame);
	Frame.Opcode = iOpcode;
	Frame.PayloadSize = iPayloadSize;
	Frame.Flags = iRsv;
	if ( bFinal ) {
		Frame.Flags |= XWS_FRAME_FIN;
	}
	return Frame;
}



/* 编码一条完整压缩消息。 */
static bool testWsRuntimeEncode(
	xwsdeflater* pDeflater,
	xbytesview Input,
	test_ws_runtime_output* pOutput
)
{
	size_t iOffset = 0;

	if ( !xrtWsDeflaterBegin(pDeflater, true) ) {
		return false;
	}
	while ( iOffset < Input.Size ) {
		size_t iSize = Input.Size - iOffset;

		if ( iSize > 113u ) {
			iSize = 113u;
		}
		if ( !xrtWsDeflaterWrite(
			pDeflater,
			(xbytesview){
				Input.Data + iOffset,
				iSize
			},
			testWsRuntimeOutput,
			pOutput
		) ) {
			return false;
		}
		iOffset += iSize;
	}
	return xrtWsDeflaterEnd(
		pDeflater,
		testWsRuntimeOutput,
		pOutput
	);
}



/* 解码一条完整压缩消息。 */
static bool testWsRuntimeDecode(
	xwsinflater* pInflater,
	xbytesview Input,
	test_ws_runtime_output* pOutput
)
{
	size_t iOffset = 0;

	if ( !xrtWsInflaterBegin(pInflater, true) ) {
		return false;
	}
	while ( iOffset < Input.Size ) {
		size_t iSize = Input.Size - iOffset;

		if ( iSize > 7u ) {
			iSize = 7u;
		}
		if ( !xrtWsInflaterWrite(
			pInflater,
			(xbytesview){
				Input.Data + iOffset,
				iSize
			},
			testWsRuntimeOutput,
			pOutput
		) ) {
			return false;
		}
		iOffset += iSize;
	}
	return xrtWsInflaterEnd(
		pInflater,
		testWsRuntimeOutput,
		pOutput
	);
}



/* 构造跨窗口才能明显复用的确定性输入。 */
static void testWsRuntimePayload(
	uint8* pData,
	size_t iSize
)
{
	uint32 iState = UINT32_C(0x31415926);
	size_t i;

	for ( i = 0; i < 2048u; i++ ) {
		iState = (iState * UINT32_C(1664525)) +
			UINT32_C(1013904223);
		pData[i] = (uint8)(iState >> 24u);
	}
	for ( i = 2048u; i < iSize; i++ ) {
		pData[i] = pData[(i - 2048u) & 2047u];
	}
}



/* 验证全部协商窗口都能流式回环，且解码器拒绝超出窗口的线路。 */
static void testWsRuntimeWindows(void)
{
	uint8 Plain[8192];
	test_ws_runtime_output Encoded;
	test_ws_runtime_output Decoded;
	xwsdeflaterconfig DeflaterConfig;
	xwsinflaterconfig InflaterConfig;
	size_t iBits;

	testWsRuntimePayload(Plain, sizeof(Plain));
	for ( iBits = XWS_DEFLATE_WINDOW_MIN;
		iBits <= XWS_DEFLATE_WINDOW_MAX;
		iBits++ ) {
		xwsdeflater* pDeflater;
		xwsinflater* pInflater;

		memset(&Encoded, 0, sizeof(Encoded));
		memset(&Decoded, 0, sizeof(Decoded));
		xrtWsDeflaterConfigInit(&DeflaterConfig);
		DeflaterConfig.WindowBits = (uint8)iBits;
		DeflaterConfig.NoContextTakeover = true;
		xrtWsInflaterConfigInit(&InflaterConfig);
		InflaterConfig.WindowBits = (uint8)iBits;
		InflaterConfig.NoContextTakeover = true;
		InflaterConfig.OutputLimit = sizeof(Plain);
		pDeflater = xrtWsDeflaterCreate(
			&DeflaterConfig
		);
		pInflater = xrtWsInflaterCreate(
			&InflaterConfig
		);
		testRequire(
			(pDeflater != NULL) &&
			(pInflater != NULL) &&
			testWsRuntimeEncode(
				pDeflater,
				(xbytesview){ Plain, sizeof(Plain) },
				&Encoded
			) &&
			testWsRuntimeDecode(
				pInflater,
				(xbytesview){
					Encoded.Data,
					Encoded.Size
				},
				&Decoded
			) &&
			(Decoded.Size == sizeof(Plain)) &&
			(memcmp(
				Decoded.Data,
				Plain,
				sizeof(Plain)
			 ) == 0),
			"permessage-deflate window roundtrip failed"
		);
		xrtWsInflaterDestroy(pInflater);
		xrtWsDeflaterDestroy(pDeflater);
	}

	memset(&Encoded, 0, sizeof(Encoded));
	xrtWsDeflaterConfigInit(&DeflaterConfig);
	DeflaterConfig.WindowBits =
		XWS_DEFLATE_WINDOW_MAX;
	{
		xwsdeflater* pDeflater =
			xrtWsDeflaterCreate(&DeflaterConfig);

		testRequire(
			(pDeflater != NULL) &&
			testWsRuntimeEncode(
				pDeflater,
				(xbytesview){ Plain, sizeof(Plain) },
				&Encoded
			),
			"wide WebSocket Deflater fixture failed"
		);
		xrtWsDeflaterDestroy(pDeflater);
	}

	xrtWsInflaterConfigInit(&InflaterConfig);
	InflaterConfig.WindowBits =
		XWS_DEFLATE_WINDOW_MIN;
	InflaterConfig.OutputLimit = sizeof(Plain);
	{
		xwsinflater* pInflater =
			xrtWsInflaterCreate(&InflaterConfig);

		memset(&Decoded, 0, sizeof(Decoded));
		testRequire(
			(pInflater != NULL) &&
			!testWsRuntimeDecode(
				pInflater,
				(xbytesview){
					Encoded.Data,
					Encoded.Size
				},
				&Decoded
			) &&
			(xrtErrorKind(xrtGetError()) ==
			 XERR_PROTOCOL) &&
			(xrtErrorCode(xrtGetError()) ==
			 XWS_DEFLATE_ERROR_DATA),
			"WebSocket Inflater accepted an oversized history reference"
		);
		xrtClearError();
		xrtWsInflaterDestroy(pInflater);
	}
}



/* 验证空消息仍形成可独立解码的合法压缩负载。 */
static void testWsRuntimeEmpty(void)
{
	test_ws_runtime_output Encoded;
	test_ws_runtime_output Decoded;
	xwsdeflaterconfig DeflaterConfig;
	xwsinflaterconfig InflaterConfig;
	xwsdeflater* pDeflater;
	xwsinflater* pInflater;

	memset(&Encoded, 0, sizeof(Encoded));
	memset(&Decoded, 0, sizeof(Decoded));
	xrtWsDeflaterConfigInit(&DeflaterConfig);
	DeflaterConfig.NoContextTakeover = true;
	xrtWsInflaterConfigInit(&InflaterConfig);
	InflaterConfig.NoContextTakeover = true;
	pDeflater = xrtWsDeflaterCreate(&DeflaterConfig);
	pInflater = xrtWsInflaterCreate(&InflaterConfig);
	testRequire(
		(pDeflater != NULL) &&
		(pInflater != NULL) &&
		testWsRuntimeEncode(
			pDeflater,
			(xbytesview){ NULL, 0u },
			&Encoded
		) &&
		(Encoded.Size != 0u) &&
		testWsRuntimeDecode(
			pInflater,
			(xbytesview){
				Encoded.Data,
				Encoded.Size
			},
			&Decoded
		) &&
		(Decoded.Size == 0u),
		"empty permessage-deflate roundtrip failed"
	);
	xrtWsInflaterDestroy(pInflater);
	xrtWsDeflaterDestroy(pDeflater);
}



/* 验证压缩分片、控制帧穿插、UTF-8 和消息边界共同工作。 */
static void testWsRuntimeMessage(void)
{
	static const char Plain[] =
		"fragmented permessage-deflate UTF-8: "
		"\xE4\xB8\xAD\xE6\x96\x87";
	test_ws_runtime_output Encoded;
	test_ws_runtime_output Decoded;
	test_ws_runtime_message Output;
	xwsmessageconfig MessageConfig;
	xwsmessagestate Message;
	xwsmessageerrorinfo Error;
	xwsmessageinfo Info;
	xwsdeflater* pDeflater;
	xwsinflater* pInflater;
	xwsframe Frame;
	size_t iSplit;

	memset(&Encoded, 0, sizeof(Encoded));
	memset(&Decoded, 0, sizeof(Decoded));
	pDeflater = xrtWsDeflaterCreate(NULL);
	pInflater = xrtWsInflaterCreate(NULL);
	testRequire(
		(pDeflater != NULL) &&
		(pInflater != NULL) &&
		testWsRuntimeEncode(
			pDeflater,
			XRT_BYTES_LITERAL(Plain),
			&Encoded
		) &&
		(Encoded.Size > 1u),
		"WebSocket message compression fixture failed"
	);

	xrtWsMessageConfigInit(&MessageConfig);
	MessageConfig.MaxSize = sizeof(Plain) - 1u;
	MessageConfig.FirstRsv = XWS_FRAME_RSV1;
	testRequire(
		xrtWsMessageInit(&Message, &MessageConfig),
		"WebSocket message integration state failed"
	);
	Output.Message = &Message;
	Output.Error = &Error;
	Output.Output = &Decoded;
	iSplit = Encoded.Size / 2u;

	Frame = testWsRuntimeFrame(
		(uint8)XWS_OPCODE_TEXT,
		false,
		XWS_FRAME_RSV1,
		iSplit
	);
	testRequire(
		xrtWsMessageFrameBegin(
			&Message,
			&Frame,
			&Info,
			&Error
		) &&
		((Info.Flags &
		  (XWS_MESSAGE_BEGIN |
		   XWS_MESSAGE_EXTENDED)) ==
		 (XWS_MESSAGE_BEGIN |
		  XWS_MESSAGE_EXTENDED)) &&
		xrtWsInflaterBegin(pInflater, true) &&
		xrtWsInflaterWrite(
			pInflater,
			(xbytesview){
				Encoded.Data,
				iSplit
			},
			testWsRuntimeMessageOutput,
			&Output
		) &&
		xrtWsMessageFrameEnd(&Message, &Error),
		"first compressed fragment failed"
	);

	Frame = testWsRuntimeFrame(
		(uint8)XWS_OPCODE_PING,
		true,
		0,
		1
	);
	testRequire(
		xrtWsMessageFrameBegin(
			&Message,
			&Frame,
			&Info,
			&Error
		) &&
		xrtWsMessagePayload(
			&Message,
			XRT_BYTES_LITERAL("p"),
			&Error
		) &&
		xrtWsMessageFrameEnd(&Message, &Error),
		"control frame interleave failed"
	);

	Frame = testWsRuntimeFrame(
		(uint8)XWS_OPCODE_CONTINUATION,
		true,
		0,
		Encoded.Size - iSplit
	);
	testRequire(
		xrtWsMessageFrameBegin(
			&Message,
			&Frame,
			&Info,
			&Error
		) &&
		((Info.Flags &
		  (XWS_MESSAGE_END |
		   XWS_MESSAGE_EXTENDED)) ==
		 (XWS_MESSAGE_END |
		  XWS_MESSAGE_EXTENDED)) &&
		xrtWsInflaterWrite(
			pInflater,
			(xbytesview){
				Encoded.Data + iSplit,
				Encoded.Size - iSplit
			},
			testWsRuntimeMessageOutput,
			&Output
		) &&
		xrtWsInflaterEnd(
			pInflater,
			testWsRuntimeMessageOutput,
			&Output
		) &&
		xrtWsMessageFrameEnd(&Message, &Error) &&
		(Decoded.Size == (sizeof(Plain) - 1u)) &&
		(memcmp(
			Decoded.Data,
			Plain,
			Decoded.Size
		 ) == 0),
		"final compressed fragment or message validation failed"
	);
	xrtWsInflaterDestroy(pInflater);
	xrtWsDeflaterDestroy(pDeflater);
}



/* 运行 permessage-deflate 运行时与消息层的组合门禁。 */
int main(void)
{
	testWsRuntimeWindows();
	testWsRuntimeEmpty();
	testWsRuntimeMessage();
	printf("[PASS] websocket_deflate_runtime\n");
	return 0;
}

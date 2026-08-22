#include "../test.h"



/* 构造测试矩阵使用的帧描述。 */
static xwsframe testMatrixFrame(
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
	Frame.Flags = iRsv | (bFinal ? XWS_FRAME_FIN : 0);
	return Frame;
}



/* 每个协议错误用新状态执行并核对建议的 Close 状态码。 */
static void testMessageSequenceErrors(void)
{
	xwsmessagestate State;
	xwsmessageerrorinfo Error;
	xwsmessageinfo Info;
	xwsframe Frame;

	testRequire(xrtWsMessageInit(&State, NULL), "matrix init failed");
	Frame = testMatrixFrame(
		(uint8)XWS_OPCODE_CONTINUATION,
		true,
		0,
		0
	);
	testRequire(
		!xrtWsMessageFrameBegin(&State, &Frame, &Info, &Error) &&
		(Error.Code == XWS_MESSAGE_ERROR_FRAGMENT) &&
		(Error.CloseCode == XWS_CLOSE_PROTOCOL) &&
		State.Failed,
		"continuation without message was accepted"
	);

	xrtWsMessageReset(&State);
	Frame = testMatrixFrame(
		(uint8)XWS_OPCODE_BINARY,
		false,
		0,
		0
	);
	testRequire(
		xrtWsMessageFrameBegin(&State, &Frame, &Info, &Error) &&
		xrtWsMessageFrameEnd(&State, &Error),
		"matrix fragmented message setup failed"
	);
	Frame = testMatrixFrame(
		(uint8)XWS_OPCODE_TEXT,
		true,
		0,
		0
	);
	testRequire(
		!xrtWsMessageFrameBegin(&State, &Frame, &Info, &Error) &&
		(Error.Code == XWS_MESSAGE_ERROR_FRAGMENT) &&
		(Error.CloseCode == XWS_CLOSE_PROTOCOL),
		"new data frame interrupted fragmented message"
	);

	xrtWsMessageReset(&State);
	Frame = testMatrixFrame(
		(uint8)XWS_OPCODE_PING,
		false,
		0,
		0
	);
	testRequire(
		!xrtWsMessageFrameBegin(&State, &Frame, &Info, &Error) &&
		(Error.CloseCode == XWS_CLOSE_PROTOCOL),
		"fragmented WebSocket Ping was accepted"
	);

	xrtWsMessageReset(&State);
	Frame = testMatrixFrame(
		(uint8)XWS_OPCODE_PONG,
		true,
		0,
		126
	);
	testRequire(
		!xrtWsMessageFrameBegin(&State, &Frame, &Info, &Error) &&
		(Error.CloseCode == XWS_CLOSE_PROTOCOL),
		"oversized WebSocket Pong was accepted"
	);

	xrtWsMessageReset(&State);
	Frame = testMatrixFrame(
		(uint8)XWS_OPCODE_CLOSE,
		true,
		0,
		1
	);
	testRequire(
		!xrtWsMessageFrameBegin(&State, &Frame, &Info, &Error) &&
		(Error.CloseCode == XWS_CLOSE_PROTOCOL),
		"one-byte WebSocket Close was accepted"
	);
}



/* 验证 RSV 位只能出现在配置允许的帧位置。 */
static void testMessageRsvMatrix(void)
{
	xwsmessageconfig Config;
	xwsmessagestate State;
	xwsmessageerrorinfo Error;
	xwsmessageinfo Info;
	xwsframe Frame;

	xrtWsMessageConfigInit(&Config);
	testRequire(xrtWsMessageInit(&State, &Config), "RSV matrix init failed");
	Frame = testMatrixFrame(
		(uint8)XWS_OPCODE_TEXT,
		true,
		XWS_FRAME_RSV1,
		0
	);
	testRequire(
		!xrtWsMessageFrameBegin(&State, &Frame, &Info, &Error) &&
		(Error.Code == XWS_MESSAGE_ERROR_RSV) &&
		(Error.CloseCode == XWS_CLOSE_PROTOCOL),
		"unnegotiated first-frame RSV1 was accepted"
	);

	Config.FirstRsv = XWS_FRAME_RSV1;
	testRequire(
		xrtWsMessageInit(&State, &Config) &&
		xrtWsMessageFrameBegin(&State, &Frame, &Info, &Error) &&
		((Info.Flags & XWS_MESSAGE_EXTENDED) != 0) &&
		xrtWsMessageFrameEnd(&State, &Error),
		"negotiated first-frame RSV1 was rejected"
	);

	Frame = testMatrixFrame(
		(uint8)XWS_OPCODE_TEXT,
		false,
		0,
		0
	);
	testRequire(
		xrtWsMessageInit(&State, &Config) &&
		xrtWsMessageFrameBegin(&State, &Frame, &Info, &Error) &&
		xrtWsMessageFrameEnd(&State, &Error),
		"RSV continuation setup failed"
	);
	Frame = testMatrixFrame(
		(uint8)XWS_OPCODE_CONTINUATION,
		true,
		XWS_FRAME_RSV2,
		0
	);
	testRequire(
		!xrtWsMessageFrameBegin(&State, &Frame, &Info, &Error) &&
		(Error.Code == XWS_MESSAGE_ERROR_RSV),
		"unnegotiated continuation RSV2 was accepted"
	);

	Config.ContinuationRsv = XWS_FRAME_RSV2;
	testRequire(
		xrtWsMessageInit(&State, &Config),
		"allowed continuation RSV matrix init failed"
	);
	Frame = testMatrixFrame(
		(uint8)XWS_OPCODE_BINARY,
		false,
		0,
		0
	);
	testRequire(
		xrtWsMessageFrameBegin(&State, &Frame, &Info, &Error) &&
		xrtWsMessageFrameEnd(&State, &Error),
		"allowed continuation setup failed"
	);
	Frame = testMatrixFrame(
		(uint8)XWS_OPCODE_CONTINUATION,
		true,
		XWS_FRAME_RSV2,
		1
	);
	testRequire(
		xrtWsMessageFrameBegin(&State, &Frame, &Info, &Error) &&
		((Info.Flags & XWS_MESSAGE_EXTENDED) != 0) &&
		xrtWsMessagePayload(
			&State,
			(xbytesview) { (const uint8*)"decoded", 7 },
			&Error
		) &&
		xrtWsMessageFrameEnd(&State, &Error),
		"allowed continuation RSV2 transform failed"
	);
}



/* 验证负载计数、累计上限和错误偏移。 */
static void testMessagePayloadMatrix(void)
{
	static const uint8 Byte[] = { 'x' };
	xwsmessageconfig Config;
	xwsmessagestate State;
	xwsmessageerrorinfo Error;
	xwsmessageinfo Info;
	xwsframe Frame;

	Frame = testMatrixFrame(
		(uint8)XWS_OPCODE_BINARY,
		true,
		0,
		2
	);
	testRequire(
		xrtWsMessageInit(&State, NULL) &&
		xrtWsMessageFrameBegin(&State, &Frame, &Info, &Error) &&
		xrtWsMessagePayload(
			&State,
			(xbytesview) { Byte, 1 },
			&Error
		) &&
		!xrtWsMessageFrameEnd(&State, &Error) &&
		(Error.Code == XWS_MESSAGE_ERROR_PAYLOAD) &&
		(Error.CloseCode == 0),
		"short semantic frame payload was not detected"
	);

	xrtWsMessageReset(&State);
	Frame.PayloadSize = 0;
	testRequire(
		xrtWsMessageFrameBegin(&State, &Frame, &Info, &Error) &&
		!xrtWsMessagePayload(
			&State,
			(xbytesview) { Byte, 1 },
			&Error
		) &&
		(Error.Code == XWS_MESSAGE_ERROR_PAYLOAD) &&
		(Error.CloseCode == 0),
		"semantic frame overfeed was not detected"
	);

	xrtWsMessageConfigInit(&Config);
	Config.MaxSize = 3;
	Frame = testMatrixFrame(
		(uint8)XWS_OPCODE_BINARY,
		false,
		0,
		2
	);
	testRequire(
		xrtWsMessageInit(&State, &Config) &&
		xrtWsMessageFrameBegin(&State, &Frame, &Info, &Error) &&
		xrtWsMessagePayload(
			&State,
			(xbytesview) { (const uint8*)"ab", 2 },
			&Error
		) &&
		xrtWsMessageFrameEnd(&State, &Error),
		"message limit matrix first fragment failed"
	);
	Frame = testMatrixFrame(
		(uint8)XWS_OPCODE_CONTINUATION,
		true,
		0,
		2
	);
	testRequire(
		!xrtWsMessageFrameBegin(&State, &Frame, &Info, &Error) &&
		(Error.Code == XWS_MESSAGE_ERROR_SIZE) &&
		(Error.CloseCode == XWS_CLOSE_TOO_BIG) &&
		(Error.Offset == 2u),
		"fragmented message cumulative limit was not enforced early"
	);
}



/* 验证 Close 状态码和原因错误分别映射到 1002 与 1007。 */
static void testMessageCloseMatrix(void)
{
	static const uint8 BadCode[] = { 0x03, 0xED };
	static const uint8 BadUtf8[] = { 0x03, 0xE8, 0xE4 };
	xwsmessagestate State;
	xwsmessageerrorinfo Error;
	xwsmessageinfo Info;
	xwsframe Frame;

	Frame = testMatrixFrame(
		(uint8)XWS_OPCODE_CLOSE,
		true,
		0,
		sizeof(BadCode)
	);
	testRequire(
		xrtWsMessageInit(&State, NULL) &&
		xrtWsMessageFrameBegin(&State, &Frame, &Info, &Error) &&
		!xrtWsMessagePayload(
			&State,
			(xbytesview) { BadCode, sizeof(BadCode) },
			&Error
		) &&
		(Error.Code == XWS_MESSAGE_ERROR_CLOSE) &&
		(Error.CloseCode == XWS_CLOSE_PROTOCOL),
		"forbidden Close code did not map to 1002"
	);

	xrtWsMessageReset(&State);
	Frame.PayloadSize = sizeof(BadUtf8);
	testRequire(
		xrtWsMessageFrameBegin(&State, &Frame, &Info, &Error) &&
		xrtWsMessagePayload(
			&State,
			(xbytesview) { BadUtf8, sizeof(BadUtf8) },
			&Error
		) &&
		!xrtWsMessageFrameEnd(&State, &Error) &&
		(Error.Code == XWS_MESSAGE_ERROR_UTF8) &&
		(Error.CloseCode == XWS_CLOSE_INVALID_DATA) &&
		(Error.Offset == 2u),
		"truncated Close reason did not map to 1007"
	);
}



/* 验证可选关闭文本校验和错误输出原子契约。 */
static void testMessagePolicyAndArguments(void)
{
	static const uint8 Invalid[] = { 0xC0, 0x80 };
	xwsmessageconfig Config;
	xwsmessagestate State;
	xwsmessageerrorinfo Error;
	xwsmessageinfo Before;
	xwsmessageinfo Info;
	xwsframe Frame;

	xrtWsMessageConfigInit(&Config);
	Config.ValidateText = false;
	Frame = testMatrixFrame(
		(uint8)XWS_OPCODE_TEXT,
		true,
		0,
		sizeof(Invalid)
	);
	testRequire(
		xrtWsMessageInit(&State, &Config) &&
		xrtWsMessageFrameBegin(&State, &Frame, &Info, &Error) &&
		xrtWsMessagePayload(
			&State,
			(xbytesview) { Invalid, sizeof(Invalid) },
			&Error
		) &&
		xrtWsMessageFrameEnd(&State, &Error),
		"disabled WebSocket text validation still rejected opaque data"
	);

	testRequire(
		xrtWsMessageInit(&State, NULL),
		"argument matrix state initialization failed"
	);
	Frame = testMatrixFrame(
		(uint8)XWS_OPCODE_CONTINUATION,
		true,
		0,
		0
	);
	memset(&Info, 0xA5, sizeof(Info));
	Before = Info;
	testRequire(
		!xrtWsMessageFrameBegin(
			&State,
			&Frame,
			&Info,
			&Error
		) &&
		(memcmp(&Info, &Before, sizeof(Info)) == 0) &&
		(Error.Code == XWS_MESSAGE_ERROR_FRAGMENT),
		"failed WebSocket frame begin published partial message info"
	);

	xrtWsMessageReset(&State);
	testRequire(
		!xrtWsMessageFrameBegin(
			&State,
			&Frame,
			NULL,
			&Error
		) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_MESSAGE_ERROR_ARGUMENT),
		"WebSocket message frame begin accepted null info output"
	);
	testRequire(
		!xrtWsMessagePayload(
			&State,
			(xbytesview) { NULL, 1 },
			&Error
		) &&
		(xrtErrorCode(xrtGetError()) ==
		 XWS_MESSAGE_ERROR_ARGUMENT),
		"WebSocket message payload accepted null nonempty input"
	);
}



/* 大量零负载分片和控制帧穿插不得改变状态或增长内存。 */
static void testMessageLongFragmentRun(void)
{
	xwsmessagestate State;
	xwsmessageinfo Info;
	xwsframe Frame;
	size_t i;

	testRequire(
		xrtWsMessageInit(&State, NULL),
		"long fragment state initialization failed"
	);
	Frame = testMatrixFrame(
		(uint8)XWS_OPCODE_BINARY,
		false,
		0,
		0
	);
	testRequire(
		xrtWsMessageFrameBegin(&State, &Frame, &Info, NULL) &&
		xrtWsMessageFrameEnd(&State, NULL),
		"long fragment first frame failed"
	);
	for ( i = 0; i < 10000u; i++ ) {
		Frame = testMatrixFrame(
			(uint8)XWS_OPCODE_PING,
			true,
			0,
			0
		);
		testRequire(
			xrtWsMessageFrameBegin(&State, &Frame, &Info, NULL) &&
			xrtWsMessageFrameEnd(&State, NULL) &&
			State.Fragmented,
			"long fragment interleaved control failed"
		);

		Frame = testMatrixFrame(
			(uint8)XWS_OPCODE_CONTINUATION,
			false,
			0,
			0
		);
		testRequire(
			xrtWsMessageFrameBegin(&State, &Frame, &Info, NULL) &&
			xrtWsMessageFrameEnd(&State, NULL) &&
			State.Fragmented,
			"long zero-size continuation failed"
		);
	}
	Frame = testMatrixFrame(
		(uint8)XWS_OPCODE_CONTINUATION,
		true,
		0,
		0
	);
	testRequire(
		xrtWsMessageFrameBegin(&State, &Frame, &Info, NULL) &&
		xrtWsMessageFrameEnd(&State, NULL) &&
		!State.Fragmented,
		"long fragment final continuation failed"
	);
}



/* 执行消息顺序、RSV、限额、Close 和长分片矩阵。 */
int main(void)
{
	testMessageSequenceErrors();
	testMessageRsvMatrix();
	testMessagePayloadMatrix();
	testMessageCloseMatrix();
	testMessagePolicyAndArguments();
	testMessageLongFragmentRun();
	printf("[PASS] websocket_message_matrix\n");
	return 0;
}

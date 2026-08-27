#include "../test.h"



/* 构造一个不持有负载的标准帧描述。 */
static xwsframe testMessageFrame(
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



/* 验证默认配置、初始化和配置错误。 */
static void testMessageInit(void)
{
	xwsmessageconfig Config;
	xwsmessagestate State;
	xwsmessagestate Before;

	memset(&Config, 0xA5, sizeof(Config));
	xrtWsMessageConfigInit(&Config);
	testRequire(
		(Config.MaxSize == SIZE_MAX) &&
		(Config.FirstRsv == 0) &&
		(Config.ContinuationRsv == 0) &&
		(Config.ControlRsv == 0) &&
		Config.ValidateText &&
		(sizeof(xwsmessagestate) <= 192u),
		"WebSocket message default configuration mismatch"
	);
	xrtWsMessageConfigInitSafe(&Config);
	testRequire(
		(Config.MaxSize == XWS_MESSAGE_SIZE_SAFE_DEFAULT) &&
		(Config.FirstRsv == 0) && Config.ValidateText,
		"WebSocket message safe configuration mismatch"
	);

	memset(&State, 0xA5, sizeof(State));
	testRequire(
		xrtWsMessageInit(&State, NULL) &&
		State.Initialized &&
		!State.Fragmented &&
		!State.FrameActive &&
		!State.Failed &&
		!State.Closed,
		"WebSocket message default initialization failed"
	);

	Config.FirstRsv = UINT16_C(0x8000);
	Before = State;
	testRequire(
		!xrtWsMessageInit(&State, &Config) &&
		(memcmp(&State, &Before, sizeof(State)) == 0) &&
		(xrtErrorCode(xrtGetError()) == XWS_MESSAGE_ERROR_CONFIG),
		"WebSocket message accepted invalid RSV configuration"
	);
}



/* 验证单帧文本可以跨任意网络分块完成增量 UTF-8 校验。 */
static void testMessageText(void)
{
	static const uint8 Text[] = { 'A', 0xE4, 0xB8, 0xAD };
	xwsmessagestate State;
	xwsmessageerrorinfo Error;
	xwsmessageinfo Info;
	xwsframe Frame = testMessageFrame(
		(uint8)XWS_OPCODE_TEXT,
		true,
		0,
		sizeof(Text)
	);

	testRequire(
		xrtWsMessageInit(&State, NULL),
		"WebSocket text state initialization failed"
	);
	testRequire(
		xrtWsMessageFrameBegin(
			&State,
			&Frame,
			&Info,
			&Error
		) &&
		(Info.Flags == (XWS_MESSAGE_BEGIN | XWS_MESSAGE_END)) &&
		(Info.Opcode == XWS_OPCODE_TEXT) &&
		(Info.FrameOpcode == XWS_OPCODE_TEXT) &&
		(Info.PayloadSize == sizeof(Text)) &&
		(Info.Offset == 0) &&
		(Error.Code == 0),
		"WebSocket text frame semantic metadata mismatch"
	);
	testRequire(
		xrtWsMessagePayload(
			&State,
			(xbytesview) { Text, 2 },
			&Error
		) &&
		xrtWsMessagePayload(
			&State,
			(xbytesview) { Text + 2u, sizeof(Text) - 2u },
			&Error
		) &&
		xrtWsMessageFrameEnd(&State, &Error) &&
		!State.Fragmented &&
		!State.FrameActive &&
		(State.Size == 0),
		"WebSocket split UTF-8 text message failed"
	);
}



/* 验证控制帧穿插不会破坏分片消息和跨帧 UTF-8 状态。 */
static void testMessageFragmentAndControl(void)
{
	static const uint8 First[] = { 'A', 0xE4, 0xB8 };
	static const uint8 Last[] = { 0xAD, 'B' };
	static const uint8 Ping[] = { 'h', 'i' };
	xwsmessagestate State;
	xwsmessageinfo Info;
	xwsframe Frame;

	testRequire(
		xrtWsMessageInit(&State, NULL),
		"WebSocket fragmented state initialization failed"
	);

	Frame = testMessageFrame(
		(uint8)XWS_OPCODE_TEXT,
		false,
		0,
		sizeof(First)
	);
	testRequire(
		xrtWsMessageFrameBegin(&State, &Frame, &Info, NULL) &&
		(Info.Flags == XWS_MESSAGE_BEGIN) &&
		xrtWsMessagePayload(
			&State,
			(xbytesview) { First, sizeof(First) },
			NULL
		) &&
		xrtWsMessageFrameEnd(&State, NULL) &&
		State.Fragmented &&
		(State.Size == sizeof(First)),
		"WebSocket first text fragment failed"
	);

	Frame = testMessageFrame(
		(uint8)XWS_OPCODE_PING,
		true,
		0,
		sizeof(Ping)
	);
	testRequire(
		xrtWsMessageFrameBegin(&State, &Frame, &Info, NULL) &&
		(Info.Flags == (
			XWS_MESSAGE_BEGIN |
			XWS_MESSAGE_END |
			XWS_MESSAGE_CONTROL
		)) &&
		(Info.Opcode == XWS_OPCODE_PING) &&
		xrtWsMessagePayload(
			&State,
			(xbytesview) { Ping, sizeof(Ping) },
			NULL
		) &&
		xrtWsMessageFrameEnd(&State, NULL) &&
		State.Fragmented &&
		(State.Size == sizeof(First)),
		"interleaved WebSocket Ping changed fragmented message state"
	);

	Frame = testMessageFrame(
		(uint8)XWS_OPCODE_CONTINUATION,
		true,
		0,
		sizeof(Last)
	);
	testRequire(
		xrtWsMessageFrameBegin(&State, &Frame, &Info, NULL) &&
		(Info.Flags == XWS_MESSAGE_END) &&
		(Info.Opcode == XWS_OPCODE_TEXT) &&
		(Info.FrameOpcode == XWS_OPCODE_CONTINUATION) &&
		(Info.Offset == sizeof(First)) &&
		xrtWsMessagePayload(
			&State,
			(xbytesview) { Last, sizeof(Last) },
			NULL
		) &&
		xrtWsMessageFrameEnd(&State, NULL) &&
		!State.Fragmented &&
		(State.Size == 0),
		"WebSocket final text continuation failed"
	);
}



/* 验证 Close 负载流式校验、关闭终态和显式复用。 */
static void testMessageClose(void)
{
	static const uint8 Payload[] = {
		0x03, 0xE8, 0xE4, 0xB8, 0xAD
	};
	xwsmessagestate State;
	xwsmessageerrorinfo Error;
	xwsmessageinfo Info;
	xwsframe Frame;

	testRequire(
		xrtWsMessageInit(&State, NULL),
		"WebSocket Close message state initialization failed"
	);
	Frame = testMessageFrame(
		(uint8)XWS_OPCODE_CLOSE,
		true,
		0,
		sizeof(Payload)
	);
	testRequire(
		xrtWsMessageFrameBegin(&State, &Frame, &Info, &Error) &&
		(Info.Flags == (
			XWS_MESSAGE_BEGIN |
			XWS_MESSAGE_END |
			XWS_MESSAGE_CONTROL
		)) &&
		xrtWsMessagePayload(
			&State,
			(xbytesview) { Payload, 1 },
			&Error
		) &&
		xrtWsMessagePayload(
			&State,
			(xbytesview) { Payload + 1u, 2 },
			&Error
		) &&
		xrtWsMessagePayload(
			&State,
			(xbytesview) { Payload + 3u, 2 },
			&Error
		) &&
		xrtWsMessageFrameEnd(&State, &Error) &&
		State.Closed,
		"streamed WebSocket Close payload failed"
	);

	Frame = testMessageFrame(
		(uint8)XWS_OPCODE_PING,
		true,
		0,
		0
	);
	testRequire(
		!xrtWsMessageFrameBegin(
			&State,
			&Frame,
			&Info,
			&Error
		) &&
		(Error.Code == XWS_MESSAGE_ERROR_STATE) &&
		(Error.CloseCode == 0),
		"WebSocket message state accepted a frame after Close"
	);

	xrtWsMessageReset(&State);
	testRequire(
		!State.Closed &&
		!State.Failed &&
		xrtWsMessageFrameBegin(
			&State,
			&Frame,
			&Info,
			&Error
		) &&
		xrtWsMessageFrameEnd(&State, &Error),
		"WebSocket message reset did not permit reuse"
	);
}



/* 验证消息上限作用于扩展解码后的字节，而不是压缩帧长度。 */
static void testMessageExtendedLimit(void)
{
	static const uint8 Decoded[] = { 'h', 'e', 'l', 'l', 'o' };
	xwsmessageconfig Config;
	xwsmessagestate State;
	xwsmessageerrorinfo Error;
	xwsmessageinfo Info;
	xwsframe Frame;

	xrtWsMessageConfigInit(&Config);
	Config.MaxSize = sizeof(Decoded);
	Config.FirstRsv = XWS_FRAME_RSV1;
	testRequire(
		xrtWsMessageInit(&State, &Config),
		"extended WebSocket message state initialization failed"
	);
	Frame = testMessageFrame(
		(uint8)XWS_OPCODE_TEXT,
		true,
		XWS_FRAME_RSV1,
		2
	);
	testRequire(
		xrtWsMessageFrameBegin(
			&State,
			&Frame,
			&Info,
			&Error
		) &&
		((Info.Flags & XWS_MESSAGE_EXTENDED) != 0) &&
		xrtWsMessagePayload(
			&State,
			(xbytesview) { Decoded, sizeof(Decoded) },
			&Error
		) &&
		xrtWsMessageFrameEnd(&State, &Error),
		"decoded WebSocket message at the limit failed"
	);

	Config.MaxSize = sizeof(Decoded) - 1u;
	testRequire(
		xrtWsMessageInit(&State, &Config) &&
		xrtWsMessageFrameBegin(
			&State,
			&Frame,
			&Info,
			&Error
		) &&
		!xrtWsMessagePayload(
			&State,
			(xbytesview) { Decoded, sizeof(Decoded) },
			&Error
		) &&
		(Error.Code == XWS_MESSAGE_ERROR_SIZE) &&
		(Error.CloseCode == XWS_CLOSE_TOO_BIG) &&
		State.Failed,
		"decoded WebSocket message limit was not enforced"
	);
}



/* 验证非法文本和截断标量映射到 1007。 */
static void testMessageInvalidUtf8(void)
{
	static const uint8 Invalid[] = { 0xC0, 0x80 };
	static const uint8 Truncated[] = { 0xE4 };
	xwsmessagestate State;
	xwsmessageerrorinfo Error;
	xwsmessageinfo Info;
	xwsframe Frame;

	testRequire(
		xrtWsMessageInit(&State, NULL),
		"invalid UTF-8 state initialization failed"
	);
	Frame = testMessageFrame(
		(uint8)XWS_OPCODE_TEXT,
		true,
		0,
		sizeof(Invalid)
	);
	testRequire(
		xrtWsMessageFrameBegin(&State, &Frame, &Info, &Error) &&
		!xrtWsMessagePayload(
			&State,
			(xbytesview) { Invalid, sizeof(Invalid) },
			&Error
		) &&
		(Error.Code == XWS_MESSAGE_ERROR_UTF8) &&
		(Error.CloseCode == XWS_CLOSE_INVALID_DATA) &&
		(Error.Offset == 0),
		"invalid WebSocket text did not map to 1007"
	);

	xrtWsMessageReset(&State);
	Frame.PayloadSize = sizeof(Truncated);
	testRequire(
		xrtWsMessageFrameBegin(&State, &Frame, &Info, &Error) &&
		xrtWsMessagePayload(
			&State,
			(xbytesview) { Truncated, sizeof(Truncated) },
			&Error
		) &&
		!xrtWsMessageFrameEnd(&State, &Error) &&
		(Error.Code == XWS_MESSAGE_ERROR_UTF8) &&
		(Error.CloseCode == XWS_CLOSE_INVALID_DATA) &&
		(Error.Offset == 0),
		"truncated WebSocket text did not fail at message end"
	);
}



/* 验证未对齐状态、事务提交、范围隔离和地址回绕契约。 */
static void testMessageMemoryContracts(void)
{
	static const uint8 Text[] = { 'o', 'k' };
	uint8 ConfigStorage[sizeof(xwsmessageconfig) + 2u];
	uint8 StateStorage[sizeof(xwsmessagestate) + 2u];
	uint8 FrameStorage[sizeof(xwsframe) + 2u];
	uint8 InfoStorage[sizeof(xwsmessageinfo) + 2u];
	uint8 ErrorStorage[sizeof(xwsmessageerrorinfo) + 2u];
	uint8 Before[sizeof(StateStorage)];
	xwsmessageconfig Config;
	xwsmessagestate State;
	xwsmessageerrorinfo Error;
	xwsmessageinfo Info;
	xwsframe Frame;
	xwsmessagestate* pState =
		(xwsmessagestate*)(void*)(StateStorage + 1u);
	xwsframe* pFrame =
		(xwsframe*)(void*)(FrameStorage + 1u);
	xwsmessageinfo* pInfo =
		(xwsmessageinfo*)(void*)(InfoStorage + 1u);
	xwsmessageerrorinfo* pError =
		(xwsmessageerrorinfo*)(void*)(ErrorStorage + 1u);

	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	memset(StateStorage, 0xA5, sizeof(StateStorage));
	memset(FrameStorage, 0xA5, sizeof(FrameStorage));
	memset(InfoStorage, 0xA5, sizeof(InfoStorage));
	memset(ErrorStorage, 0xA5, sizeof(ErrorStorage));
	xrtWsMessageConfigInit(
		(xwsmessageconfig*)(void*)(ConfigStorage + 1u)
	);
	testRequire(
		xrtWsMessageInit(
			pState,
			(const xwsmessageconfig*)(const void*)(ConfigStorage + 1u)
		),
		"WebSocket message init rejected unaligned state"
	);
	memcpy(&Config, ConfigStorage + 1u, sizeof(Config));
	memcpy(&State, StateStorage + 1u, sizeof(State));
	testRequire(
		(ConfigStorage[0] == UINT8_C(0xA5)) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == UINT8_C(0xA5)) &&
		(StateStorage[0] == UINT8_C(0xA5)) &&
		(StateStorage[sizeof(StateStorage) - 1u] == UINT8_C(0xA5)) &&
		(Config.MaxSize == SIZE_MAX) && Config.ValidateText &&
		State.Initialized && !State.Failed,
		"WebSocket message init corrupted unaligned storage"
	);

	Frame = testMessageFrame(
		(uint8)XWS_OPCODE_TEXT,
		true,
		0,
		sizeof(Text)
	);
	memcpy(FrameStorage + 1u, &Frame, sizeof(Frame));
	testRequire(
		xrtWsMessageFrameBegin(pState, pFrame, pInfo, pError) &&
		xrtWsMessagePayload(
			pState,
			(xbytesview){ Text, sizeof(Text) },
			pError
		) &&
		xrtWsMessageFrameEnd(pState, pError),
		"WebSocket message sequence rejected unaligned structures"
	);
	memcpy(&State, StateStorage + 1u, sizeof(State));
	memcpy(&Info, InfoStorage + 1u, sizeof(Info));
	memcpy(&Error, ErrorStorage + 1u, sizeof(Error));
	testRequire(
		!State.FrameActive && !State.Fragmented && !State.Failed &&
		((Info.Flags & (XWS_MESSAGE_BEGIN | XWS_MESSAGE_END)) ==
		 (XWS_MESSAGE_BEGIN | XWS_MESSAGE_END)) &&
		(Error.Code == 0) && (Error.Offset == XRT_NPOS) &&
		(FrameStorage[0] == UINT8_C(0xA5)) &&
		(FrameStorage[sizeof(FrameStorage) - 1u] == UINT8_C(0xA5)) &&
		(InfoStorage[0] == UINT8_C(0xA5)) &&
		(InfoStorage[sizeof(InfoStorage) - 1u] == UINT8_C(0xA5)) &&
		(ErrorStorage[0] == UINT8_C(0xA5)) &&
		(ErrorStorage[sizeof(ErrorStorage) - 1u] == UINT8_C(0xA5)),
		"WebSocket message sequence corrupted unaligned structures"
	);

	Frame = testMessageFrame(UINT8_C(3), true, 0, 0);
	memcpy(FrameStorage + 1u, &Frame, sizeof(Frame));
	memset(InfoStorage + 1u, 0xA5, sizeof(xwsmessageinfo));
	testRequire(
		!xrtWsMessageFrameBegin(pState, pFrame, pInfo, pError),
		"WebSocket message state accepted an invalid opcode"
	);
	memcpy(&State, StateStorage + 1u, sizeof(State));
	memcpy(&Error, ErrorStorage + 1u, sizeof(Error));
	testRequire(
		State.Failed &&
		(Error.Code == XWS_MESSAGE_ERROR_OPCODE) &&
		(Error.CloseCode == XWS_CLOSE_PROTOCOL) &&
		(InfoStorage[1] == UINT8_C(0xA5)),
		"WebSocket message protocol failure was not transactional"
	);
	xrtWsMessageReset(pState);
	memcpy(&State, StateStorage + 1u, sizeof(State));
	testRequire(
		State.Initialized && !State.Failed && !State.FrameActive,
		"WebSocket message reset did not recover unaligned state"
	);

	memcpy(Before, StateStorage, sizeof(StateStorage));
	testRequire(
		!xrtWsMessageFrameBegin(
			pState,
			(const xwsframe*)(const void*)pState,
			pInfo,
			pError
		) &&
		(memcmp(StateStorage, Before, sizeof(StateStorage)) == 0) &&
		!xrtWsMessagePayload(
			pState,
			(xbytesview){ (cbytes)(const void*)pState, 1u },
			pError
		) &&
		(memcmp(StateStorage, Before, sizeof(StateStorage)) == 0) &&
		!xrtWsMessageFrameEnd(
			pState,
			(xwsmessageerrorinfo*)(void*)pState
		) &&
		(memcmp(StateStorage, Before, sizeof(StateStorage)) == 0),
		"WebSocket message layer accepted overlapping ranges"
	);

	testRequire(
		!xrtWsMessageInit(
			(xwsmessagestate*)(uintptr_t)(UINTPTR_MAX - 1u),
			NULL
		) &&
		!xrtWsMessageInit(
			pState,
			(const xwsmessageconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
		) &&
		!xrtWsMessageFrameBegin(
			(xwsmessagestate*)(uintptr_t)(UINTPTR_MAX - 1u),
			pFrame,
			pInfo,
			pError
		) &&
		!xrtWsMessageFrameBegin(
			pState,
			(const xwsframe*)(uintptr_t)(UINTPTR_MAX - 1u),
			pInfo,
			pError
		) &&
		!xrtWsMessagePayload(
			pState,
			(xbytesview){
				(cbytes)(uintptr_t)(UINTPTR_MAX - 1u), 4u
			},
			pError
		) &&
		!xrtWsMessageFrameEnd(
			pState,
			(xwsmessageerrorinfo*)(uintptr_t)(UINTPTR_MAX - 1u)
		),
		"WebSocket message layer accepted wrapping ranges"
	);
	xrtWsMessageConfigInit(
		(xwsmessageconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	xrtWsMessageReset(
		(xwsmessagestate*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) == XWS_MESSAGE_ERROR_ARGUMENT),
		"WebSocket message wrapping range error mismatch"
	);
	xrtClearError();
}



/* 执行消息语义、分片、控制帧、Close、扩展和 UTF-8 测试。 */
int main(void)
{
	testMessageInit();
	testMessageText();
	testMessageFragmentAndControl();
	testMessageClose();
	testMessageExtendedLimit();
	testMessageInvalidUtf8();
	testMessageMemoryContracts();
	return 0;
}

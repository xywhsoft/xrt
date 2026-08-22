#include "../test.h"



/* 检查解析成功后的核心帧元数据。 */
static void testFrameRequire(
	const uint8* pHead,
	size_t iHeadSize,
	const xwsframeconfig* pConfig,
	uint8 iOpcode,
	uint32 iFlags,
	uint64 iPayloadSize
)
{
	xwsframeerrorinfo Error;
	xwsframe Frame;
	xbytesview Input;

	Input.Data = pHead;
	Input.Size = iHeadSize;
	testRequire(
		xrtWsFrameParse(Input, &Frame, pConfig, &Error) ==
			XWS_FRAME_READY,
		"WebSocket frame parse failed"
	);
	testRequire(
		(Frame.Opcode == iOpcode) &&
		(Frame.Flags == iFlags) &&
		(Frame.PayloadSize == iPayloadSize) &&
		(Frame.HeadSize == iHeadSize) &&
		(Error.Code == 0),
		"WebSocket frame metadata mismatch"
	);
}



/* 验证默认配置和空对象初始化不携带隐藏状态。 */
static void testFrameInit(void)
{
	xwsframeconfig Config;
	xwsframe Frame;

	memset(&Config, 0xA5, sizeof(Config));
	xrtWsFrameConfigInit(&Config);
	testRequire(
		(Config.MaxPayload == XWS_FRAME_PAYLOAD_MAX) &&
		(Config.AllowedOpcodes == XWS_OPCODES_STANDARD) &&
		(Config.AllowedRsv == 0) &&
		(Config.Mask == XWS_MASK_ANY),
		"WebSocket frame default configuration mismatch"
	);

	memset(&Frame, 0xA5, sizeof(Frame));
	xrtWsFrameInit(&Frame);
	testRequire(
		(Frame.Flags == 0) &&
		(Frame.Opcode == 0) &&
		(Frame.PayloadSize == 0) &&
		(Frame.HeadSize == 0),
		"WebSocket frame initialization mismatch"
	);

	xrtWsFrameConfigInit(NULL);
	xrtWsFrameInit(NULL);
}



/* 迁移旧编解码器的短帧、扩展长度和掩码向量。 */
static void testFrameLegacyVectors(void)
{
	static const uint8 MaskedText[] = {
		0x81, 0x85, 0x37, 0xFA, 0x21, 0x3D
	};
	static const uint8 Binary130[] = {
		0x82, 0x7E, 0x00, 0x82
	};
	static const uint8 Binary65536[] = {
		0x82, 0x7F, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x01, 0x00, 0x00
	};
	xwsframeconfig Config;
	xwsframe Frame;
	xbytesview Input;

	xrtWsFrameConfigInit(&Config);
	Config.Mask = XWS_MASK_REQUIRED;
	testFrameRequire(
		MaskedText, sizeof(MaskedText), &Config,
		(uint8)XWS_OPCODE_TEXT,
		(uint32)XWS_FRAME_FIN | (uint32)XWS_FRAME_MASKED,
		UINT64_C(5)
	);

	Input.Data = MaskedText;
	Input.Size = sizeof(MaskedText);
	testRequire(
		xrtWsFrameParse(Input, &Frame, &Config, NULL) ==
			XWS_FRAME_READY,
		"WebSocket masked text parse failed"
	);
	testRequire(
		(Frame.Mask[0] == 0x37) && (Frame.Mask[1] == 0xFA) &&
		(Frame.Mask[2] == 0x21) && (Frame.Mask[3] == 0x3D),
		"WebSocket mask key mismatch"
	);

	Config.Mask = XWS_MASK_ANY;
	testFrameRequire(
		Binary130, sizeof(Binary130), &Config,
		(uint8)XWS_OPCODE_BINARY, (uint32)XWS_FRAME_FIN,
		UINT64_C(130)
	);
	testFrameRequire(
		Binary65536, sizeof(Binary65536), &Config,
		(uint8)XWS_OPCODE_BINARY, (uint32)XWS_FRAME_FIN,
		UINT64_C(65536)
	);
}



/* 验证解析器只等待最多十四字节头部，不等待声明的负载。 */
static void testFrameHeadOnly(void)
{
	static const uint8 Head[] = {
		0x82, 0xFF,
		0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0x11, 0x22, 0x33, 0x44
	};
	xwsframe Frame;
	xbytesview Input;

	Input.Data = Head;
	Input.Size = sizeof(Head);
	testRequire(
		xrtWsFrameParse(Input, &Frame, NULL, NULL) ==
			XWS_FRAME_READY,
		"WebSocket maximum frame header did not become ready"
	);
	testRequire(
		(Frame.HeadSize == 14u) &&
		(Frame.PayloadSize == UINT64_C(0x7FFFFFFFFFFFFFFF)),
		"WebSocket maximum payload metadata mismatch"
	);
}



/* 验证默认拒绝扩展位和保留操作码，显式协商后允许它们。 */
static void testFrameExtensions(void)
{
	static const uint8 ExtensionFrame[] = { 0xC3, 0x00 };
	xwsframeconfig Config;
	xwsframeerrorinfo Error;
	xwsframe Frame;
	xbytesview Input;

	Input.Data = ExtensionFrame;
	Input.Size = sizeof(ExtensionFrame);
	testRequire(
		xrtWsFrameParse(Input, &Frame, NULL, &Error) ==
			XWS_FRAME_ERROR,
		"WebSocket accepted an unnegotiated extension"
	);
	testRequire(
		(Error.Code == XWS_FRAME_ERROR_OPCODE) ||
		(Error.Code == XWS_FRAME_ERROR_RSV),
		"WebSocket extension rejection code mismatch"
	);

	xrtWsFrameConfigInit(&Config);
	Config.AllowedOpcodes |= (uint16)(UINT16_C(1) << 3u);
	Config.AllowedRsv = (uint16)XWS_FRAME_RSV1;
	testFrameRequire(
		ExtensionFrame, sizeof(ExtensionFrame), &Config,
		UINT8_C(3),
		(uint32)XWS_FRAME_FIN | (uint32)XWS_FRAME_RSV1,
		0
	);
}



/* 验证旧实现已经压实的所有非法帧头。 */
static void testFrameRejectsMalformed(void)
{
	static const uint8 Rsv[] = { 0xC1, 0x00 };
	static const uint8 Opcode[] = { 0x83, 0x00 };
	static const uint8 FragmentedPing[] = { 0x09, 0x00 };
	static const uint8 LargePing[] = { 0x89, 0x7E, 0x00, 0x7E };
	static const uint8 Noncanonical16[] = { 0x82, 0x7E, 0x00, 0x7D };
	static const uint8 Noncanonical64[] = {
		0x82, 0x7F, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0xFF, 0xFF
	};
	static const uint8 HighBit64[] = {
		0x82, 0x7F, 0x80, 0x00, 0x00, 0x00,
		0x00, 0x01, 0x00, 0x00
	};
	static const uint8 CloseOne[] = { 0x88, 0x01 };
	static const struct {
		const uint8* Data;
		size_t Size;
		xwsframeerror Error;
	} Cases[] = {
		{ Rsv, sizeof(Rsv), XWS_FRAME_ERROR_RSV },
		{ Opcode, sizeof(Opcode), XWS_FRAME_ERROR_OPCODE },
		{ FragmentedPing, sizeof(FragmentedPing), XWS_FRAME_ERROR_CONTROL },
		{ LargePing, sizeof(LargePing), XWS_FRAME_ERROR_CONTROL },
		{ Noncanonical16, sizeof(Noncanonical16), XWS_FRAME_ERROR_LENGTH },
		{ Noncanonical64, sizeof(Noncanonical64), XWS_FRAME_ERROR_LENGTH },
		{ HighBit64, sizeof(HighBit64), XWS_FRAME_ERROR_LENGTH },
		{ CloseOne, sizeof(CloseOne), XWS_FRAME_ERROR_CLOSE }
	};

	for ( size_t i = 0; i < sizeof(Cases) / sizeof(Cases[0]); i++ ) {
		xwsframeerrorinfo Error;
		xwsframe Frame;
		xbytesview Input;

		memset(&Frame, 0xA5, sizeof(Frame));
		Input.Data = Cases[i].Data;
		Input.Size = Cases[i].Size;
		xrtClearError();
		testRequire(
			xrtWsFrameParse(Input, &Frame, NULL, &Error) ==
				XWS_FRAME_ERROR,
			"WebSocket malformed frame was accepted"
		);
		testRequire(
			(Error.Code == Cases[i].Error) &&
			(Frame.Flags == 0) && (Frame.PayloadSize == 0) &&
			(xrtErrorDomain(xrtGetError()) != NULL) &&
			(strcmp(
				xrtErrorDomain(xrtGetError()),
				"xrt.websocket.frame"
			) == 0),
			"WebSocket malformed frame error contract mismatch"
		);
	}
}



/* 验证最大负载、方向掩码和配置自身的约束。 */
static void testFramePolicy(void)
{
	static const uint8 Eleven[] = { 0x82, 0x0B };
	static const uint8 Unmasked[] = { 0x81, 0x00 };
	static const uint8 Masked[] = {
		0x81, 0x80, 0x00, 0x00, 0x00, 0x00
	};
	xwsframeconfig Config;
	xwsframeerrorinfo Error;
	xwsframe Frame;
	xbytesview Input;

	xrtWsFrameConfigInit(&Config);
	Config.MaxPayload = 10;
	Input.Data = Eleven;
	Input.Size = sizeof(Eleven);
	testRequire(
		(xrtWsFrameParse(Input, &Frame, &Config, &Error) ==
			XWS_FRAME_ERROR) &&
		(Error.Code == XWS_FRAME_ERROR_LENGTH),
		"WebSocket maximum payload policy was not enforced"
	);

	Config.MaxPayload = XWS_FRAME_PAYLOAD_MAX;
	Config.Mask = XWS_MASK_REQUIRED;
	Input.Data = Unmasked;
	Input.Size = sizeof(Unmasked);
	testRequire(
		(xrtWsFrameParse(Input, &Frame, &Config, &Error) ==
			XWS_FRAME_ERROR) &&
		(Error.Code == XWS_FRAME_ERROR_MASK),
		"WebSocket required mask policy was not enforced"
	);

	Config.Mask = XWS_MASK_FORBIDDEN;
	Input.Data = Masked;
	Input.Size = sizeof(Masked);
	testRequire(
		(xrtWsFrameParse(Input, &Frame, &Config, &Error) ==
			XWS_FRAME_ERROR) &&
		(Error.Code == XWS_FRAME_ERROR_MASK),
		"WebSocket forbidden mask policy was not enforced"
	);

	xrtWsFrameConfigInit(&Config);
	Config.AllowedOpcodes = 0;
	Input.Data = Unmasked;
	Input.Size = sizeof(Unmasked);
	testRequire(
		(xrtWsFrameParse(Input, &Frame, &Config, &Error) ==
			XWS_FRAME_ERROR) &&
		(Error.Code == XWS_FRAME_ERROR_CONFIG),
		"WebSocket accepted an empty opcode policy"
	);

	xrtWsFrameConfigInit(&Config);
	Config.AllowedRsv = UINT16_C(0x8000);
	testRequire(
		(xrtWsFrameParse(Input, &Frame, &Config, &Error) ==
			XWS_FRAME_ERROR) &&
		(Error.Code == XWS_FRAME_ERROR_CONFIG),
		"WebSocket accepted unknown RSV policy bits"
	);

	xrtWsFrameConfigInit(&Config);
	Config.MaxPayload = UINT64_MAX;
	testRequire(
		(xrtWsFrameParse(Input, &Frame, &Config, &Error) ==
			XWS_FRAME_ERROR) &&
		(Error.Code == XWS_FRAME_ERROR_CONFIG),
		"WebSocket accepted a payload policy above the wire limit"
	);

	xrtWsFrameConfigInit(&Config);
	Config.Mask = (xwsmaskpolicy)99;
	testRequire(
		(xrtWsFrameParse(Input, &Frame, &Config, &Error) ==
			XWS_FRAME_ERROR) &&
		(Error.Code == XWS_FRAME_ERROR_CONFIG),
		"WebSocket accepted an invalid mask policy"
	);
}



/* 验证封包器在全部长度分界上产生规范的最短帧头。 */
static void testFrameWriteBoundaries(void)
{
	static const uint64 Lengths[] = {
		UINT64_C(0), UINT64_C(1), UINT64_C(125),
		UINT64_C(126), UINT64_C(65535), UINT64_C(65536),
		UINT64_C(0x7FFFFFFFFFFFFFFF)
	};
	uint8 Head[XWS_FRAME_HEAD_MAX];

	for ( size_t i = 0; i < sizeof(Lengths) / sizeof(Lengths[0]); i++ ) {
		xwsframe Parsed;
		xwsframe Frame;
		xbytesview Input;
		size_t iExpected =
			(Lengths[i] <= UINT64_C(125)) ? 2u :
			((Lengths[i] <= UINT64_C(65535)) ? 4u : 10u);
		size_t iSize = SIZE_MAX;

		xrtWsFrameInit(&Frame);
		Frame.Flags = (uint32)XWS_FRAME_FIN;
		Frame.Opcode = (uint8)XWS_OPCODE_BINARY;
		Frame.PayloadSize = Lengths[i];
		testRequire(
			xrtWsFrameWrite(
				&Frame, NULL, NULL, 0, &iSize
			) && (iSize == iExpected),
			"WebSocket frame write query mismatch"
		);
		memset(Head, 0xA5, sizeof(Head));
		testRequire(
			xrtWsFrameWrite(
				&Frame, NULL, Head, sizeof(Head), &iSize
			) && (iSize == iExpected),
			"WebSocket frame write failed"
		);
		Input.Data = Head;
		Input.Size = iSize;
		testRequire(
			(xrtWsFrameParse(Input, &Parsed, NULL, NULL) ==
				XWS_FRAME_READY) &&
			(Parsed.PayloadSize == Lengths[i]) &&
			(Parsed.HeadSize == iExpected),
			"WebSocket frame write round trip mismatch"
		);
	}
}



/* 验证掩码帧封包、容量失败原子性和无效帧拒绝。 */
static void testFrameWriteContract(void)
{
	xwsframeconfig Config;
	xwsframe Frame;
	uint8 Head[XWS_FRAME_HEAD_MAX];
	uint8 Before[XWS_FRAME_HEAD_MAX];
	size_t iSize;

	xrtWsFrameInit(&Frame);
	Frame.Flags =
		(uint32)XWS_FRAME_FIN | (uint32)XWS_FRAME_MASKED;
	Frame.Opcode = (uint8)XWS_OPCODE_TEXT;
	Frame.PayloadSize = UINT64_C(126);
	Frame.Mask[0] = 1;
	Frame.Mask[1] = 2;
	Frame.Mask[2] = 3;
	Frame.Mask[3] = 4;
	testRequire(
		xrtWsFrameWrite(
			&Frame, NULL, Head, sizeof(Head), &iSize
		) && (iSize == 8u) &&
		(Head[0] == 0x81) && (Head[1] == 0xFE) &&
		(Head[2] == 0x00) && (Head[3] == 0x7E) &&
		(memcmp(Head + 4, Frame.Mask, 4) == 0),
		"WebSocket masked frame header mismatch"
	);

	memset(Head, 0xA5, sizeof(Head));
	memcpy(Before, Head, sizeof(Head));
	iSize = 0;
	testRequire(
		!xrtWsFrameWrite(
			&Frame, NULL, Head, 7, &iSize
		) && (iSize == 8u) &&
		(memcmp(Head, Before, sizeof(Head)) == 0) &&
		(xrtErrorCode(xrtGetError()) == XWS_FRAME_ERROR_OUTPUT),
		"WebSocket short output was not failure-atomic"
	);
	testRequire(
		!xrtWsFrameWrite(
			&Frame, NULL, NULL, 1, &iSize
		) &&
		(xrtErrorCode(xrtGetError()) == XWS_FRAME_ERROR_ARGUMENT),
		"WebSocket accepted null output with capacity"
	);

	xrtWsFrameConfigInit(&Config);
	Config.Mask = XWS_MASK_FORBIDDEN;
	testRequire(
		!xrtWsFrameWrite(
			&Frame, &Config, Head, sizeof(Head), &iSize
		) &&
		(xrtErrorCode(xrtGetError()) == XWS_FRAME_ERROR_MASK) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"WebSocket writer ignored mask policy"
	);

	Frame.Flags |= UINT32_C(0x80000000);
	testRequire(
		!xrtWsFrameWrite(
			&Frame, NULL, Head, sizeof(Head), &iSize
		) &&
		(xrtErrorCode(xrtGetError()) == XWS_FRAME_ERROR_ARGUMENT),
		"WebSocket writer accepted unknown frame flags"
	);
	Frame.Flags &= ~UINT32_C(0x80000000);

	Frame.PayloadSize = UINT64_C(0x8000000000000000);
	testRequire(
		!xrtWsFrameWrite(
			&Frame, NULL, Head, sizeof(Head), &iSize
		) &&
		(xrtErrorCode(xrtGetError()) == XWS_FRAME_ERROR_LENGTH) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"WebSocket writer accepted a 64-bit high payload bit"
	);

	Frame.Flags = (uint32)XWS_FRAME_FIN;
	Frame.Opcode = (uint8)XWS_OPCODE_CLOSE;
	Frame.PayloadSize = 1;
	testRequire(
		!xrtWsFrameWrite(
			&Frame, NULL, Head, sizeof(Head), &iSize
		) &&
		(xrtErrorCode(xrtGetError()) == XWS_FRAME_ERROR_CLOSE),
		"WebSocket writer accepted one-byte close payload"
	);

	Frame.Flags = 0;
	Frame.Opcode = (uint8)XWS_OPCODE_PING;
	Frame.PayloadSize = 0;
	testRequire(
		!xrtWsFrameWrite(
			&Frame, NULL, Head, sizeof(Head), &iSize
		) &&
		(xrtErrorCode(xrtGetError()) == XWS_FRAME_ERROR_CONTROL),
		"WebSocket writer accepted fragmented control frame"
	);
}



/* 验证参数错误和 MORE 状态不会发布残缺帧。 */
static void testFrameIncrementalContract(void)
{
	static const uint8 Head[] = {
		0x82, 0xFE, 0x00, 0x7E, 1, 2, 3, 4
	};
	xwsframeerrorinfo Error;
	xwsframe Frame;

	for ( size_t i = 0; i < sizeof(Head); i++ ) {
		xbytesview Input;

		memset(&Frame, 0xA5, sizeof(Frame));
		memset(&Error, 0xA5, sizeof(Error));
		Input.Data = Head;
		Input.Size = i;
		xrtClearError();
		testRequire(
			xrtWsFrameParse(Input, &Frame, NULL, &Error) ==
				XWS_FRAME_MORE,
			"WebSocket truncated frame head did not return MORE"
		);
		testRequire(
			(Frame.Flags == 0) && (Frame.PayloadSize == 0) &&
			(Error.Code == 0) && (xrtGetError() == NULL),
			"WebSocket MORE state published partial state or error"
		);
	}

	{
		xbytesview Invalid = { NULL, 1 };

		testRequire(
			(xrtWsFrameParse(
				Invalid, &Frame, NULL, &Error
			) == XWS_FRAME_ERROR) &&
			(Error.Code == XWS_FRAME_ERROR_ARGUMENT),
			"WebSocket accepted null nonempty input"
		);
	}

	{
		xbytesview Empty = { NULL, 0 };

		testRequire(
			(xrtWsFrameParse(
				Empty, NULL, NULL, &Error
			) == XWS_FRAME_ERROR) &&
			(Error.Code == XWS_FRAME_ERROR_ARGUMENT),
			"WebSocket accepted null frame output"
		);
	}
}



/* 验证未对齐结构、完整范围、输出隔离和掩码快照契约。 */
static void testFrameMemoryContracts(void)
{
	static const uint8 TextHead[] = { 0x81, 0x00 };
	uint8 ConfigStorage[sizeof(xwsframeconfig) + 2u];
	uint8 FrameStorage[sizeof(xwsframe) + 2u];
	uint8 ErrorStorage[sizeof(xwsframeerrorinfo) + 2u];
	uint8 SizeStorage[sizeof(size_t) + 2u];
	uint8 Output[XWS_FRAME_HEAD_MAX + 2u];
	uint8 Overlap[sizeof(xwsframe) + 2u];
	uint8 Before[sizeof(Overlap)];
	uint8 Masked[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	xwsframeconfig Config;
	xwsframe Frame;
	xwsframeerrorinfo Error;
	xbytesview Input;
	size_t iSize;

	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	memset(FrameStorage, 0xA5, sizeof(FrameStorage));
	xrtWsFrameConfigInit(
		(xwsframeconfig*)(void*)(ConfigStorage + 1u)
	);
	xrtWsFrameInit((xwsframe*)(void*)(FrameStorage + 1u));
	memcpy(&Config, ConfigStorage + 1u, sizeof(Config));
	memcpy(&Frame, FrameStorage + 1u, sizeof(Frame));
	testRequire(
		(ConfigStorage[0] == UINT8_C(0xA5)) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == UINT8_C(0xA5)) &&
		(FrameStorage[0] == UINT8_C(0xA5)) &&
		(FrameStorage[sizeof(FrameStorage) - 1u] == UINT8_C(0xA5)) &&
		(Config.MaxPayload == XWS_FRAME_PAYLOAD_MAX) &&
		(Frame.Flags == 0) && (Frame.PayloadSize == 0),
		"WebSocket frame init corrupted unaligned storage"
	);

	memset(ErrorStorage, 0xA5, sizeof(ErrorStorage));
	Input.Data = TextHead;
	Input.Size = sizeof(TextHead);
	testRequire(
		xrtWsFrameParse(
			Input,
			(xwsframe*)(void*)(FrameStorage + 1u),
			(const xwsframeconfig*)(const void*)(ConfigStorage + 1u),
			(xwsframeerrorinfo*)(void*)(ErrorStorage + 1u)
		) == XWS_FRAME_READY,
		"WebSocket frame parser rejected unaligned structures"
	);
	memcpy(&Frame, FrameStorage + 1u, sizeof(Frame));
	memcpy(&Error, ErrorStorage + 1u, sizeof(Error));
	testRequire(
		(Frame.Opcode == (uint8)XWS_OPCODE_TEXT) &&
		(Frame.HeadSize == sizeof(TextHead)) &&
		(Error.Code == 0) &&
		(ErrorStorage[0] == UINT8_C(0xA5)) &&
		(ErrorStorage[sizeof(ErrorStorage) - 1u] == UINT8_C(0xA5)),
		"WebSocket frame parser corrupted unaligned outputs"
	);

	memset(Output, 0xA5, sizeof(Output));
	memset(SizeStorage, 0xA5, sizeof(SizeStorage));
	testRequire(
		xrtWsFrameWrite(
			(const xwsframe*)(const void*)(FrameStorage + 1u),
			(const xwsframeconfig*)(const void*)(ConfigStorage + 1u),
			Output + 1u,
			XWS_FRAME_HEAD_MAX,
			(size_t*)(void*)(SizeStorage + 1u)
		),
		"WebSocket frame writer rejected unaligned structures"
	);
	memcpy(&iSize, SizeStorage + 1u, sizeof(iSize));
	testRequire(
		(iSize == sizeof(TextHead)) &&
		(memcmp(Output + 1u, TextHead, sizeof(TextHead)) == 0) &&
		(Output[0] == UINT8_C(0xA5)) &&
		(Output[sizeof(Output) - 1u] == UINT8_C(0xA5)) &&
		(SizeStorage[0] == UINT8_C(0xA5)) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] == UINT8_C(0xA5)),
		"WebSocket frame writer corrupted unaligned outputs"
	);

	memset(Overlap, 0xA5, sizeof(Overlap));
	memcpy(Overlap, TextHead, sizeof(TextHead));
	memcpy(Before, Overlap, sizeof(Overlap));
	Input.Data = Overlap;
	Input.Size = sizeof(TextHead);
	testRequire(
		(xrtWsFrameParse(
			Input,
			(xwsframe*)(void*)Overlap,
			NULL,
			&Error
		) == XWS_FRAME_ERROR) &&
		(Error.Code == XWS_FRAME_ERROR_ARGUMENT) &&
		(memcmp(Overlap, Before, sizeof(Overlap)) == 0),
		"WebSocket frame parser accepted overlapping output"
	);

	memset(Output, 0xA5, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	testRequire(
		!xrtWsFrameWrite(
			&Frame,
			NULL,
			Output,
			sizeof(Output),
			(size_t*)(void*)(Output + 1u)
		) &&
		(memcmp(Output, Before, sizeof(Output)) == 0),
		"WebSocket frame writer accepted overlapping outputs"
	);

	Input.Data = (cbytes)(uintptr_t)(UINTPTR_MAX - 1u);
	Input.Size = 4u;
	testRequire(
		(xrtWsFrameParse(
			Input, &Frame, NULL, &Error
		) == XWS_FRAME_ERROR) &&
		(Error.Code == XWS_FRAME_ERROR_ARGUMENT) &&
		!xrtWsFrameWrite(
			(const xwsframe*)(uintptr_t)(UINTPTR_MAX - 1u),
			NULL, Output, sizeof(Output), &iSize
		) &&
		!xrtWsFrameWrite(
			&Frame, NULL,
			(void*)(uintptr_t)(UINTPTR_MAX - 1u), 4u, &iSize
		) &&
		!xrtWsMask(
			(void*)(uintptr_t)(UINTPTR_MAX - 1u),
			4u, Masked, 0
		) &&
		!xrtWsMask(
			Masked, sizeof(Masked),
			(const uint8*)(uintptr_t)(UINTPTR_MAX - 1u), 0
		),
		"WebSocket frame layer accepted wrapping ranges"
	);

	testRequire(
		xrtWsMask(Masked, sizeof(Masked), Masked, 0) &&
		(Masked[0] == 0) && (Masked[1] == 0) &&
		(Masked[2] == 0) && (Masked[3] == 0) &&
		(Masked[4] == 4) && (Masked[5] == 4) &&
		(Masked[6] == 4) && (Masked[7] == 12),
		"WebSocket mask did not snapshot an overlapping key"
	);
	xrtClearError();
}



/* 执行帧头解析、封包、策略和错误契约测试。 */
int main(void)
{
	testFrameInit();
	testFrameLegacyVectors();
	testFrameHeadOnly();
	testFrameExtensions();
	testFrameRejectsMalformed();
	testFramePolicy();
	testFrameWriteBoundaries();
	testFrameWriteContract();
	testFrameIncrementalContract();
	testFrameMemoryContracts();
	return 0;
}

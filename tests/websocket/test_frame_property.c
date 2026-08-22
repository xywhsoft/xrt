#include "../test.h"



/* 生成可复现的测试数据，不依赖平台随机源。 */
static uint32 testFrameRandom(uint32* pState)
{
	*pState = (*pState * UINT32_C(1664525)) + UINT32_C(1013904223);
	return *pState;
}



/* 验证封包结果可在每个头部截断点安全增量解析。 */
static void testFrameSplitRoundTrip(void)
{
	static const uint64 Boundaries[] = {
		UINT64_C(0), UINT64_C(1), UINT64_C(2), UINT64_C(124),
		UINT64_C(125), UINT64_C(126), UINT64_C(127),
		UINT64_C(65534), UINT64_C(65535), UINT64_C(65536),
		UINT64_C(65537), UINT64_C(0x7FFFFFFFFFFFFFFF)
	};

	for ( size_t bMasked = 0; bMasked < 2u; bMasked++ ) {
		for ( size_t i = 0;
			i < sizeof(Boundaries) / sizeof(Boundaries[0]); i++ ) {
			xwsframe Parsed;
			xwsframe Frame;
			uint8 Head[XWS_FRAME_HEAD_MAX];
			size_t iHeadSize;

			xrtWsFrameInit(&Frame);
			Frame.Flags = (uint32)XWS_FRAME_FIN;
			if ( bMasked != 0 ) {
				Frame.Flags |= (uint32)XWS_FRAME_MASKED;
			}
			Frame.Opcode = (uint8)XWS_OPCODE_BINARY;
			Frame.PayloadSize = Boundaries[i];
			Frame.Mask[0] = 0x12;
			Frame.Mask[1] = 0x34;
			Frame.Mask[2] = 0x56;
			Frame.Mask[3] = 0x78;
			testRequire(
				xrtWsFrameWrite(
					&Frame, NULL, Head, sizeof(Head), &iHeadSize
				),
				"WebSocket property frame write failed"
			);

			for ( size_t j = 0; j < iHeadSize; j++ ) {
				xbytesview Input = { Head, j };

				testRequire(
					xrtWsFrameParse(
						Input, &Parsed, NULL, NULL
					) == XWS_FRAME_MORE,
					"WebSocket frame split did not return MORE"
				);
			}

			{
				xbytesview Input = { Head, iHeadSize };

				testRequire(
					(xrtWsFrameParse(
						Input, &Parsed, NULL, NULL
					) == XWS_FRAME_READY) &&
					(Parsed.Flags == Frame.Flags) &&
					(Parsed.Opcode == Frame.Opcode) &&
					(Parsed.PayloadSize == Frame.PayloadSize) &&
					(Parsed.HeadSize == iHeadSize) &&
					(!bMasked ||
					(memcmp(Parsed.Mask, Frame.Mask, 4) == 0)),
					"WebSocket split frame round trip mismatch"
				);
			}
		}
	}
}



/* 在密集短长度和伪随机长长度上验证规范封包往返。 */
static void testFrameLengthProperty(void)
{
	uint32 iState = UINT32_C(0x5A17C9E3);

	for ( size_t i = 0; i < 4096u; i++ ) {
		xwsframe Parsed;
		xwsframe Frame;
		xbytesview Input;
		uint8 Head[XWS_FRAME_HEAD_MAX];
		size_t iHeadSize;
		uint64 iLength;

		if ( i < 512u ) {
			iLength = (uint64)i;
		} else {
			iLength =
				((uint64)testFrameRandom(&iState) << 31u) ^
				(uint64)testFrameRandom(&iState);
			iLength &= UINT64_C(0x7FFFFFFFFFFFFFFF);
		}

		xrtWsFrameInit(&Frame);
		Frame.Flags = (uint32)XWS_FRAME_FIN;
		Frame.Opcode = (uint8)XWS_OPCODE_BINARY;
		Frame.PayloadSize = iLength;
		testRequire(
			xrtWsFrameWrite(
				&Frame, NULL, Head, sizeof(Head), &iHeadSize
			),
			"WebSocket length property write failed"
		);
		Input.Data = Head;
		Input.Size = iHeadSize;
		testRequire(
			(xrtWsFrameParse(Input, &Parsed, NULL, NULL) ==
				XWS_FRAME_READY) &&
			(Parsed.PayloadSize == iLength),
			"WebSocket length property round trip mismatch"
		);
	}
}



/* 验证所有标准操作码及允许扩展操作码的帧头往返。 */
static void testFrameOpcodeProperty(void)
{
	xwsframeconfig Config;

	xrtWsFrameConfigInit(&Config);
	Config.AllowedOpcodes = UINT16_MAX;
	Config.AllowedRsv =
		(uint16)XWS_FRAME_RSV1 |
		(uint16)XWS_FRAME_RSV2 |
		(uint16)XWS_FRAME_RSV3;

	for ( uint8 iOpcode = 0; iOpcode < 16u; iOpcode++ ) {
		for ( uint32 iRsv = 0; iRsv < 8u; iRsv++ ) {
			xwsframe Parsed;
			xwsframe Frame;
			xbytesview Input;
			uint8 Head[XWS_FRAME_HEAD_MAX];
			size_t iHeadSize;

			xrtWsFrameInit(&Frame);
			Frame.Flags = (uint32)XWS_FRAME_FIN;
			if ( (iRsv & 1u) != 0 ) {
				Frame.Flags |= (uint32)XWS_FRAME_RSV1;
			}
			if ( (iRsv & 2u) != 0 ) {
				Frame.Flags |= (uint32)XWS_FRAME_RSV2;
			}
			if ( (iRsv & 4u) != 0 ) {
				Frame.Flags |= (uint32)XWS_FRAME_RSV3;
			}
			Frame.Opcode = iOpcode;
			Frame.PayloadSize =
				((iOpcode & UINT8_C(0x08)) != 0) ? 0 : 4096;
			testRequire(
				xrtWsFrameWrite(
					&Frame, &Config, Head, sizeof(Head), &iHeadSize
				),
				"WebSocket opcode property write failed"
			);
			Input.Data = Head;
			Input.Size = iHeadSize;
			testRequire(
				(xrtWsFrameParse(
					Input, &Parsed, &Config, NULL
				) == XWS_FRAME_READY) &&
				(Parsed.Opcode == iOpcode) &&
				(Parsed.Flags == Frame.Flags),
				"WebSocket opcode property round trip mismatch"
			);
		}
	}
}



/* 使用逐字节参考实现验证优化掩码路径和任意起始偏移。 */
static void testFrameMaskReference(void)
{
	static const uint8 Mask[XWS_MASK_SIZE] = {
		0x37, 0xFA, 0x21, 0x3D
	};
	uint8 Actual[1024];
	uint8 Expected[1024];
	uint32 iState = UINT32_C(0x9E3779B9);

	for ( size_t iSize = 0; iSize <= sizeof(Actual); iSize++ ) {
		uint64 iOffset = (uint64)testFrameRandom(&iState);

		for ( size_t i = 0; i < iSize; i++ ) {
			Actual[i] = (uint8)testFrameRandom(&iState);
			Expected[i] = Actual[i] ^
				Mask[(size_t)((iOffset + (uint64)i) & UINT64_C(3))];
		}
		testRequire(
			xrtWsMask(Actual, iSize, Mask, iOffset) &&
			(memcmp(Actual, Expected, iSize) == 0),
			"WebSocket optimized mask differs from reference"
		);
		testRequire(
			xrtWsMask(Actual, iSize, Mask, iOffset),
			"WebSocket unmask failed"
		);
		for ( size_t i = 0; i < iSize; i++ ) {
			uint8 iOriginal = Expected[i] ^
				Mask[(size_t)((iOffset + (uint64)i) & UINT64_C(3))];

			testRequire(
				Actual[i] == iOriginal,
				"WebSocket mask was not symmetric"
			);
		}
	}
}



/* 验证任意分块处理与一次处理结果完全一致。 */
static void testFrameMaskChunkProperty(void)
{
	static const uint8 Mask[XWS_MASK_SIZE] = {
		0xDE, 0xAD, 0xBE, 0xEF
	};
	uint8 Whole[513];
	uint8 Chunked[513];

	for ( size_t i = 0; i < sizeof(Whole); i++ ) {
		Whole[i] = (uint8)((i * 37u) ^ (i >> 2u));
	}

	for ( size_t iSplit = 0; iSplit <= sizeof(Whole); iSplit++ ) {
		memcpy(Chunked, Whole, sizeof(Whole));
		testRequire(
			xrtWsMask(Chunked, iSplit, Mask, UINT64_C(7)) &&
			xrtWsMask(
				Chunked + iSplit,
				sizeof(Chunked) - iSplit,
				Mask,
				UINT64_C(7) + (uint64)iSplit
			),
			"WebSocket chunked mask call failed"
		);

		{
			uint8 Expected[513];

			memcpy(Expected, Whole, sizeof(Expected));
			testRequire(
				xrtWsMask(
					Expected, sizeof(Expected), Mask, UINT64_C(7)
				) &&
				(memcmp(Expected, Chunked, sizeof(Expected)) == 0),
				"WebSocket chunked mask differs from whole mask"
			);
		}
	}
}



/* 验证掩码参数边界。 */
static void testFrameMaskArguments(void)
{
	static const uint8 Mask[XWS_MASK_SIZE] = { 1, 2, 3, 4 };

	testRequire(
		xrtWsMask(NULL, 0, Mask, 0),
		"WebSocket rejected an empty payload"
	);
	testRequire(
		!xrtWsMask(NULL, 1, Mask, 0) &&
		(xrtErrorCode(xrtGetError()) == XWS_FRAME_ERROR_ARGUMENT),
		"WebSocket accepted null nonempty payload"
	);
	testRequire(
		!xrtWsMask(NULL, 0, NULL, 0) &&
		(xrtErrorCode(xrtGetError()) == XWS_FRAME_ERROR_ARGUMENT),
		"WebSocket accepted a null mask"
	);
}



/* 执行帧头和掩码的确定性性质测试。 */
int main(void)
{
	testFrameSplitRoundTrip();
	testFrameLengthProperty();
	testFrameOpcodeProperty();
	testFrameMaskReference();
	testFrameMaskChunkProperty();
	testFrameMaskArguments();
	return 0;
}

#include "../test.h"



/* 验证全部字节、查询模式、分配便捷层和大小写。 */
static void testHexRoundTrip(void)
{
	uint8 arrInput[256];
	char arrText[513];
	uint8 arrOutput[256];
	size_t iTextSize;
	size_t iOutputSize;
	str sAllocated;
	bytes pAllocated;

	for ( size_t i = 0; i < sizeof(arrInput); i++ ) {
		arrInput[i] = (uint8)i;
	}
	testRequire(xrtHexEncode(arrInput, sizeof(arrInput), NULL, 0,
		&iTextSize, 0) && (iTextSize == 512), "HEX encode query mismatch");
	testRequire(xrtHexEncode(arrInput, sizeof(arrInput), arrText,
		sizeof(arrText), &iTextSize, (uint32)XHEX_UPPER),
		"HEX uppercase encode failed");
	testRequire((arrText[0] == '0') && (arrText[1] == '0') &&
		(arrText[510] == 'F') && (arrText[511] == 'F') && (arrText[512] == 0),
		"HEX uppercase encode result mismatch");
	testRequire(xrtHexDecode((xstrview){ arrText, iTextSize }, arrOutput,
		sizeof(arrOutput), &iOutputSize, 0) &&
		(iOutputSize == sizeof(arrInput)) &&
		(memcmp(arrInput, arrOutput, sizeof(arrInput)) == 0),
		"HEX all-byte round trip mismatch");

	sAllocated = xrtHexEncodeNew(arrInput, 3, 0);
	testRequire((sAllocated != NULL) && (strcmp(sAllocated, "000102") == 0),
		"HEX allocated encode mismatch");
	xrtFree(sAllocated);
	pAllocated = xrtHexDecodeNew(XRT_STR_LITERAL("000102"),
		&iOutputSize, 0);
	testRequire((pAllocated != NULL) && (iOutputSize == 3) &&
		(memcmp(pAllocated, arrInput, 3) == 0) && (pAllocated[3] == 0),
		"HEX allocated decode mismatch");
	xrtFree(pAllocated);

	sAllocated = xrtHexEncodeNew(NULL, 0, 0);
	testRequire((sAllocated != NULL) && (sAllocated[0] == 0),
		"HEX empty encode ownership mismatch");
	xrtFree(sAllocated);
	pAllocated = xrtHexDecodeNew(XRT_STR_LITERAL(""), &iOutputSize, 0);
	testRequire((pAllocated != NULL) && (iOutputSize == 0) && (pAllocated[0] == 0),
		"HEX empty decode ownership mismatch");
	xrtFree(pAllocated);
}



/* 验证原地编解码和可选 ASCII 空白。 */
static void testHexInPlace(void)
{
	uint8 arrBuffer[64];
	size_t iSize;

	memcpy(arrBuffer, "abc", 3);
	testRequire(xrtHexEncode(arrBuffer, 3, (char*)arrBuffer,
		sizeof(arrBuffer), &iSize, 0) &&
		(iSize == 6) && (strcmp((char*)arrBuffer, "616263") == 0),
		"HEX in-place encode mismatch");
	testRequire(xrtHexDecode((xstrview){ (char*)arrBuffer, iSize }, arrBuffer,
		sizeof(arrBuffer), &iSize, 0) &&
		(iSize == 3) && (memcmp(arrBuffer, "abc", 3) == 0),
		"HEX in-place decode mismatch");
	testRequire(xrtHexDecode(XRT_STR_LITERAL("61 62\r\n63"), arrBuffer,
		sizeof(arrBuffer), &iSize, (uint32)XHEX_IGNORE_SPACE) &&
		(iSize == 3) && (memcmp(arrBuffer, "abc", 3) == 0),
		"HEX whitespace decode mismatch");
}



/* 验证格式、配置和容量失败保持输出原子。 */
static void testHexFailures(void)
{
	uint8 arrBuffer[32];
	uint8 arrBefore[32];
	size_t iSize = 99;

	memset(arrBuffer, 0xA5, sizeof(arrBuffer));
	memcpy(arrBefore, arrBuffer, sizeof(arrBuffer));
	testRequire(!xrtHexDecode(XRT_STR_LITERAL("0"), arrBuffer,
		sizeof(arrBuffer), &iSize, 0) && (iSize == 99) &&
		(memcmp(arrBuffer, arrBefore, sizeof(arrBuffer)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL) &&
		(xrtErrorCode(xrtGetError()) == XCODEC_ERROR_HEX_FORMAT),
		"HEX odd-length failure contract mismatch");
	xrtClearError();
	testRequire(!xrtHexDecode(XRT_STR_LITERAL("0g"), arrBuffer,
		sizeof(arrBuffer), &iSize, 0) && (iSize == 99) &&
		(memcmp(arrBuffer, arrBefore, sizeof(arrBuffer)) == 0),
		"HEX invalid digit failure was not atomic");
	xrtClearError();
	testRequire(!xrtHexDecode(XRT_STR_LITERAL("00 11"), arrBuffer,
		sizeof(arrBuffer), &iSize, 0),
		"HEX strict decoder accepted whitespace");
	xrtClearError();

	iSize = 99;
	testRequire(!xrtHexEncode("ab", 2, (char*)arrBuffer, 4, &iSize, 0) &&
		(iSize == 4) && (memcmp(arrBuffer, arrBefore, sizeof(arrBuffer)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HEX encode capacity failure contract mismatch");
	xrtClearError();
	iSize = 99;
	testRequire(!xrtHexDecode(XRT_STR_LITERAL("6162"), arrBuffer, 1,
		&iSize, 0) && (iSize == 2) &&
		(memcmp(arrBuffer, arrBefore, sizeof(arrBuffer)) == 0),
		"HEX decode capacity failure contract mismatch");
	xrtClearError();
	testRequire(!xrtHexEncode("a", 1, (char*)arrBuffer,
		sizeof(arrBuffer), &iSize, (uint32)XHEX_IGNORE_SPACE) &&
		(xrtErrorCode(xrtGetError()) == XCODEC_ERROR_HEX_CONFIG),
		"HEX encoder accepted decode-only flags");
	xrtClearError();
	testRequire(!xrtHexDecode(XRT_STR_LITERAL("00"), arrBuffer,
		sizeof(arrBuffer), &iSize, (uint32)XHEX_UPPER) &&
		(xrtErrorCode(xrtGetError()) == XCODEC_ERROR_HEX_CONFIG),
		"HEX decoder accepted encode-only flags");
	xrtClearError();
}



/* 穷举输入字节类别，并验证别名、部分重叠与超大长度边界。 */
static void testHexExhaustiveBoundaries(void)
{
	union {
		size_t Size;
		char Text[sizeof(size_t)];
	} Alias;
	uint8 arrBuffer[32];
	uint8 arrBefore[32];
	char arrPair[2] = { '0', 0 };
	size_t iSize;

	for ( size_t i = 0; i < 256u; i++ ) {
		bool bHex = ((i >= (size_t)'0') && (i <= (size_t)'9')) ||
			((i >= (size_t)'A') && (i <= (size_t)'F')) ||
			((i >= (size_t)'a') && (i <= (size_t)'f'));

		arrPair[1] = (char)i;
		iSize = SIZE_MAX;
		testRequire(xrtHexDecode((xstrview){ arrPair, sizeof(arrPair) },
			arrBuffer, sizeof(arrBuffer), &iSize, 0) == bHex,
			"HEX byte-class validation mismatch");
		if ( bHex ) {
			testRequire(iSize == 1u, "HEX valid pair length mismatch");
		} else {
			testRequire(iSize == SIZE_MAX,
				"HEX format failure changed the result length");
			xrtClearError();
		}
	}

	memset(arrBuffer, 0xA5, sizeof(arrBuffer));
	memcpy(arrBefore, arrBuffer, sizeof(arrBuffer));
	iSize = SIZE_MAX;
	testRequire(!xrtHexEncode(arrBuffer, 4, (char*)arrBuffer + 1,
		sizeof(arrBuffer) - 1u, &iSize, 0) && (iSize == SIZE_MAX) &&
		(memcmp(arrBuffer, arrBefore, sizeof(arrBuffer)) == 0),
		"HEX accepted partially overlapping encode buffers");
	xrtClearError();
	testRequire(!xrtHexDecode((xstrview){ (char*)arrBuffer, 4 },
		arrBuffer + 1, sizeof(arrBuffer) - 1u, &iSize, 0) &&
		(memcmp(arrBuffer, arrBefore, sizeof(arrBuffer)) == 0),
		"HEX accepted partially overlapping decode buffers");
	xrtClearError();
	testRequire(!xrtHexEncode(
		(const void*)(uintptr_t)1u, ((SIZE_MAX - 1u) / 2u) + 1u,
		NULL, 0, &iSize, 0
	) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HEX encoded-size overflow contract mismatch");
	xrtClearError();
	memset(&Alias, 0, sizeof(Alias));
	memcpy(Alias.Text, "00", 2);
	testRequire(xrtHexDecodeNew(
		(xstrview){ Alias.Text, 2 }, &Alias.Size, 0
	) == NULL, "HEX allocated decode accepted an aliased length output");
	xrtClearError();
}




/* 验证所有显式指针范围在扫描或写入前拒绝地址回绕。 */
static void testHexWrappingRanges(void)
{
	const void* pWrapping =
		(const void*)(uintptr_t)(UINTPTR_MAX - 1u);
	size_t* pWrappingSize =
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u);
	char Text[8];
	size_t iSize = 77u;

	testRequire(
		!xrtHexEncode(pWrapping, 2u, NULL, 0, &iSize, 0) &&
		(iSize == 77u) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HEX accepted a wrapping encode input"
	);
	testRequire(
		!xrtHexEncode("x", 1u, (char*)pWrapping, 2u, &iSize, 0) &&
		(iSize == 77u),
		"HEX accepted a wrapping encode output"
	);
	testRequire(
		!xrtHexEncode("x", 1u, Text, sizeof(Text), pWrappingSize, 0),
		"HEX accepted a wrapping encode size output"
	);
	testRequire(
		!xrtHexDecode(
			(xstrview){ (cstr)pWrapping, 2u }, NULL, 0, &iSize, 0
		) && (iSize == 77u),
		"HEX accepted a wrapping decode input"
	);
	testRequire(
		!xrtHexDecode(
			XRT_STR_LITERAL("00"), (ptr)pWrapping, 2u, &iSize, 0
		) && (iSize == 77u),
		"HEX accepted a wrapping decode output"
	);
	testRequire(
		!xrtHexDecode(
			XRT_STR_LITERAL("00"), Text, sizeof(Text), pWrappingSize, 0
		),
		"HEX accepted a wrapping decode size output"
	);
	testRequire(
		xrtHexDecodeNew(
			(xstrview){ (cstr)pWrapping, 2u }, &iSize, 0
		) == NULL,
		"HEX DecodeNew accepted a wrapping input"
	);
	testRequire(
		xrtHexDecodeNew(XRT_STR_LITERAL("00"), pWrappingSize, 0) == NULL,
		"HEX DecodeNew accepted a wrapping size output"
	);
}



/* 验证短缓冲区之后的独立长度字段不会被理论写入范围误判。 */
static void testHexAdjacentSize(void)
{
	typedef struct testhexoutput {
		uint8 Buffer[4];
		size_t Size;
	} testhexoutput;
	testhexoutput Output;
	uint8 Before[sizeof(Output.Buffer)];
	static const char Encoded[] = "000102030405060708090A0B";

	memset(&Output, 0xA5, sizeof(Output));
	memcpy(Before, Output.Buffer, sizeof(Before));
	Output.Size = 77u;
	testRequire(
		!xrtHexEncode(
			"0123456789ab", 12u, (char*)Output.Buffer,
			sizeof(Output.Buffer), &Output.Size, 0
		) && (Output.Size == 24u) &&
		(memcmp(Output.Buffer, Before, sizeof(Before)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HEX short encode rejected an adjacent size field"
	);

	memset(&Output, 0xA5, sizeof(Output));
	memcpy(Before, Output.Buffer, sizeof(Before));
	Output.Size = 77u;
	testRequire(
		!xrtHexDecode(
			XRT_STR_LITERAL(Encoded), Output.Buffer,
			sizeof(Output.Buffer), &Output.Size, 0
		) && (Output.Size == 12u) &&
		(memcmp(Output.Buffer, Before, sizeof(Before)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HEX short decode rejected an adjacent size field"
	);
}



/* 执行 HEX codec 全部契约测试。 */
int main(void)
{
	testHexRoundTrip();
	testHexInPlace();
	testHexFailures();
	testHexExhaustiveBoundaries();
	testHexWrappingRanges();
	testHexAdjacentSize();
	printf("[PASS] codec-hex\n");
	return 0;
}

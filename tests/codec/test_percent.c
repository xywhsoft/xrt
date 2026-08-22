#include "../test.h"



/* 验证基础向量、额外保留字符和 URI 与表单的语义边界。 */
static void testPercentVectors(void)
{
	static const char Reserved[] = ":/?#[]@!$&'()*+,;=";
	char Text[128];
	char Fragment[6];
	uint8 Data[128];
	size_t iSize;

	testRequire(xrtPercentEncode(
		"hello world/ok", 14, XRT_STR_LITERAL(""),
		Text, sizeof(Text), &iSize
	) && (iSize == 18) && (strcmp(Text, "hello%20world%2Fok") == 0),
		"percent base encode vector mismatch");
	testRequire(xrtPercentEncode(
		"hello world/ok", 14, XRT_STR_LITERAL("/"),
		Text, sizeof(Text), &iSize
	) && (strcmp(Text, "hello%20world/ok") == 0),
		"percent extra-safe vector mismatch");
	testRequire(xrtPercentEncode(
		Reserved, sizeof(Reserved) - 1u, XRT_STR_LITERAL(":/?#[]@!$&'()*+,;="),
		Text, sizeof(Text), &iSize
	) && (iSize == sizeof(Reserved) - 1u) &&
		(memcmp(Text, Reserved, iSize + 1u) == 0),
		"percent reserved character preservation mismatch");
	testRequire(xrtPercentDecode(
		XRT_STR_LITERAL("hello%20world%2fok"), Data, sizeof(Data), &iSize
	) && (iSize == 14) && (memcmp(Data, "hello world/ok", iSize) == 0),
		"percent mixed-case decode vector mismatch");
	testRequire(xrtPercentDecode(
		XRT_STR_LITERAL("a+b"), Data, sizeof(Data), &iSize
	) && (iSize == 3) && (memcmp(Data, "a+b", iSize) == 0),
		"generic percent decoder changed plus into space");
	testRequire(xrtPercentDecode(
		XRT_STR_LITERAL("%00A"), Data, sizeof(Data), &iSize
	) && (iSize == 2) && (Data[0] == 0) && (Data[1] == (uint8)'A'),
		"percent embedded-zero decode mismatch");
	testRequire(xrtPercentEncode(
		NULL, 0, XRT_STR_LITERAL(""), Text, sizeof(Text), &iSize
	) && (iSize == 0) && (Text[0] == '\0'),
		"percent empty encode mismatch");
	memset(Fragment, 0x5A, sizeof(Fragment));
	testRequire(xrtPercentWrite(
		"a b", 3, XRT_STR_LITERAL(""),
		Fragment, 5, &iSize
	) && (iSize == 5) &&
		(memcmp(Fragment, "a%20b", 5) == 0) &&
		(Fragment[5] == (char)0x5A),
		"percent raw fragment write mismatch");
	testRequire(xrtPercentDecode(
		(xstrview){ NULL, 0 }, NULL, 0, &iSize
	) && (iSize == 0), "percent empty decode query mismatch");
}



/* 验证全部字节都能按规范编码并无损解码。 */
static void testPercentAllBytes(void)
{
	uint8 Input[256];
	char Encoded[769];
	uint8 Decoded[256];
	size_t iEncoded;
	size_t iDecoded;
	size_t i;

	for ( i = 0; i < sizeof(Input); i++ ) {
		Input[i] = (uint8)i;
	}
	testRequire(xrtPercentEncode(
		Input, sizeof(Input), XRT_STR_LITERAL(""),
		Encoded, sizeof(Encoded), &iEncoded
	) && (iEncoded == 636), "percent all-byte encoded size mismatch");
	testRequire(xrtPercentDecode(
		(xstrview){ Encoded, iEncoded }, Decoded, sizeof(Decoded), &iDecoded
	) && (iDecoded == sizeof(Input)) &&
		(memcmp(Decoded, Input, sizeof(Input)) == 0),
		"percent all-byte round trip mismatch");
}



/* 验证同址原地扩张与收缩路径。 */
static void testPercentInPlace(void)
{
	uint8 Buffer[64];
	size_t iSize;

	memcpy(Buffer, "a b/c", 5);
	testRequire(xrtPercentEncode(
		Buffer, 5, XRT_STR_LITERAL("/"),
		(char*)Buffer, sizeof(Buffer), &iSize
	) && (strcmp((char*)Buffer, "a%20b/c") == 0),
		"percent in-place encode failed");
	testRequire(xrtPercentDecode(
		(xstrview){ (cstr)Buffer, iSize }, Buffer, sizeof(Buffer), &iSize
	) && (iSize == 5) && (memcmp(Buffer, "a b/c", 5) == 0),
		"percent in-place decode failed");
}



/* 验证格式、配置、容量和重叠失败都保持输出原子性。 */
static void testPercentFailures(void)
{
	static const xstrview Invalid[] = {
		XRT_STR_INIT("%"), XRT_STR_INIT("%0"), XRT_STR_INIT("%GG"),
		XRT_STR_INIT("ok%2"), XRT_STR_INIT("ok%2X")
	};
	static const char InvalidExtra[] = { '\0', (char)0xFF };
	uint8 Buffer[32];
	uint8 Before[32];
	size_t iSize;
	size_t i;

	memset(Buffer, 0xA5, sizeof(Buffer));
	memcpy(Before, Buffer, sizeof(Buffer));
	for ( i = 0; i < sizeof(Invalid) / sizeof(Invalid[0]); i++ ) {
		iSize = 77;
		xrtClearError();
		testRequire(!xrtPercentDecode(
			Invalid[i], Buffer, sizeof(Buffer), &iSize
		) && (iSize == 77) && (memcmp(Buffer, Before, sizeof(Buffer)) == 0) &&
			(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL) &&
			(xrtErrorCode(xrtGetError()) == XCODEC_ERROR_PERCENT_FORMAT),
			"percent malformed input contract mismatch");
	}

	iSize = 77;
	testRequire(!xrtPercentEncode(
		"a b", 3, XRT_STR_LITERAL(""), (char*)Buffer, 5, &iSize
	) && (iSize == 5) && (memcmp(Buffer, Before, sizeof(Buffer)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"percent short encode was not atomic");
	iSize = 77;
	testRequire(!xrtPercentDecode(
		XRT_STR_LITERAL("a%20b"), Buffer, 2, &iSize
	) && (iSize == 3) && (memcmp(Buffer, Before, sizeof(Buffer)) == 0),
		"percent short decode was not atomic");

	iSize = 77;
	testRequire(!xrtPercentEncode(
		Buffer, 4, XRT_STR_LITERAL(""),
		(char*)Buffer + 1, sizeof(Buffer) - 1u, &iSize
	) && (iSize == 77) && (memcmp(Buffer, Before, sizeof(Buffer)) == 0),
		"percent accepted partially overlapping encode");
	memcpy(Buffer, "a%20b", 5);
	memcpy(Before, Buffer, sizeof(Buffer));
	iSize = 77;
	testRequire(!xrtPercentDecode(
		(xstrview){ (cstr)Buffer, 5 }, Buffer + 1, sizeof(Buffer) - 1u, &iSize
	) && (iSize == 77) && (memcmp(Buffer, Before, sizeof(Buffer)) == 0),
		"percent accepted partially overlapping decode");

	iSize = 77;
	testRequire(!xrtPercentEncode(
		"x", 1, XRT_STR_LITERAL("%"),
		(char*)Buffer, sizeof(Buffer), &iSize
	) && (iSize == 77) &&
		(xrtErrorCode(xrtGetError()) == XCODEC_ERROR_PERCENT_CONFIG),
		"percent accepted percent as extra-safe");
	testRequire(!xrtPercentEncode(
		"x", 1, XRT_STR_LITERAL("\\"),
		(char*)Buffer, sizeof(Buffer), &iSize
	), "percent accepted a non-URI extra-safe character");
	testRequire(!xrtPercentEncode(
		"x", 1, (xstrview){ InvalidExtra, 1 },
		(char*)Buffer, sizeof(Buffer), &iSize
	), "percent accepted NUL as an extra-safe character");
	testRequire(!xrtPercentEncode(
		"x", 1, (xstrview){ InvalidExtra + 1, 1 },
		(char*)Buffer, sizeof(Buffer), &iSize
	), "percent accepted non-ASCII as an extra-safe character");
	testRequire(xrtPercentEncode(
		"AZaz09-._~", 11, XRT_STR_LITERAL("AA~~"),
		(char*)Buffer, sizeof(Buffer), &iSize
	), "percent rejected redundant unreserved extra-safe characters");

	testRequire(!xrtPercentEncode(
		NULL, 1, XRT_STR_LITERAL(""), NULL, 0, &iSize
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"percent accepted a null non-empty input");
	testRequire(!xrtPercentDecode(
		(xstrview){ NULL, 1 }, NULL, 0, &iSize
	), "percent accepted a null non-empty text view");
	testRequire(!xrtPercentEncode(
		"x", 1, (xstrview){ NULL, 1 }, NULL, 0, &iSize
	), "percent accepted a null non-empty extra-safe view");
}



/* 验证地址回绕在任何输入读取或输出写入前被拒绝。 */
static void testPercentWrappingRanges(void)
{
	const void* pWrapping =
		(const void*)(uintptr_t)(UINTPTR_MAX - 1u);
	size_t* pWrappingSize =
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u);
	char Text[8];
	size_t iSize = 77u;

	testRequire(
		!xrtPercentEncode(
			pWrapping,
			2u,
			XRT_STR_LITERAL(""),
			NULL,
			0,
			&iSize
		) && (iSize == 77u) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"percent accepted a wrapping encode input"
	);
	testRequire(
		!xrtPercentEncode(
			"x",
			1u,
			(xstrview){ (cstr)pWrapping, 2u },
			NULL,
			0,
			&iSize
		) && (iSize == 77u),
		"percent accepted a wrapping extra-safe range"
	);
	testRequire(
		!xrtPercentEncode(
			"x",
			1u,
			XRT_STR_LITERAL(""),
			(char*)pWrapping,
			2u,
			&iSize
		) && (iSize == 77u),
		"percent accepted a wrapping encode output"
	);
	testRequire(
		!xrtPercentEncode(
			"x",
			1u,
			XRT_STR_LITERAL(""),
			Text,
			sizeof(Text),
			pWrappingSize
		),
		"percent accepted a wrapping encode size output"
	);
	testRequire(
		!xrtPercentDecode(
			(xstrview){ (cstr)pWrapping, 2u },
			NULL,
			0,
			&iSize
		) && (iSize == 77u),
		"percent accepted a wrapping decode input"
	);
	testRequire(
		!xrtPercentDecode(
			XRT_STR_LITERAL("x"),
			(ptr)pWrapping,
			2u,
			&iSize
		) && (iSize == 77u),
		"percent accepted a wrapping decode output"
	);
	testRequire(
		!xrtPercentDecode(
			XRT_STR_LITERAL("x"),
			Text,
			sizeof(Text),
			pWrappingSize
		),
		"percent accepted a wrapping decode size output"
	);
	testRequire(
		xrtPercentEncodeNew(
			"x",
			1u,
			XRT_STR_LITERAL(""),
			pWrappingSize
		) == NULL,
		"percent EncodeNew accepted a wrapping size output"
	);
	testRequire(
		xrtPercentDecodeNew(
			XRT_STR_LITERAL("x"),
			pWrappingSize
		) == NULL,
		"percent DecodeNew accepted a wrapping size output"
	);
}



/* 验证短缓冲后的独立长度字段不会被理论输出范围误判为重叠。 */
static void testPercentAdjacentSize(void)
{
	typedef struct testpercentoutput {
		uint8 Buffer[4];
		size_t Size;
	} testpercentoutput;
	testpercentoutput Output;
	uint8 Before[sizeof(Output.Buffer)];
	static const char Encoded[] =
		"%00%01%02%03%04%05%06%07%08%09%0A%0B";

	memset(&Output, 0xA5, sizeof(Output));
	memcpy(Before, Output.Buffer, sizeof(Before));
	Output.Size = 77u;
	testRequire(
		!xrtPercentEncode(
			"            ",
			12u,
			XRT_STR_LITERAL(""),
			(char*)Output.Buffer,
			sizeof(Output.Buffer),
			&Output.Size
		) && (Output.Size == 36u) &&
		(memcmp(Output.Buffer, Before, sizeof(Before)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"percent short encode rejected an adjacent size field"
	);

	memset(&Output, 0xA5, sizeof(Output));
	memcpy(Before, Output.Buffer, sizeof(Before));
	Output.Size = 77u;
	testRequire(
		!xrtPercentDecode(
			XRT_STR_LITERAL(Encoded),
			Output.Buffer,
			sizeof(Output.Buffer),
			&Output.Size
		) && (Output.Size == 12u) &&
		(memcmp(Output.Buffer, Before, sizeof(Before)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"percent short decode rejected an adjacent size field"
	);
}



/* 验证流式 percent 解码不分配、不污染错误并支持表单加号规则。 */
static void testPercentNext(void)
{
	static const char sText[] = "a%20+b";
	static const uint8 arrUri[] = { 'a', ' ', '+', 'b' };
	static const uint8 arrForm[] = { 'a', ' ', ' ', 'b' };
	size_t iOffset;
	uint8 iValue;
	xerror* pMarker;
	const xerror* pPrevious;

	for ( int iMode = 0; iMode < 2; iMode++ ) {
		const uint8* pExpected = (iMode == 0) ? arrUri : arrForm;

		iOffset = 0;
		for ( size_t i = 0; i < sizeof(arrUri); i++ ) {
			testRequire(
				xrtPercentNext(
					XRT_STR_LITERAL(sText),
					iMode != 0,
					&iOffset,
					&iValue
				) == XPERCENT_NEXT_BYTE,
				"percent Next did not publish a byte"
			);
			testRequire(iValue == pExpected[i],
				"percent Next published the wrong byte");
		}
		testRequire(
			xrtPercentNext(
				XRT_STR_LITERAL(sText),
				iMode != 0,
				&iOffset,
				&iValue
			) == XPERCENT_NEXT_END,
			"percent Next did not publish end"
		);
	}

	pMarker = xrtErrorCreate(
		XERR_STATE,
		"test.codec.percent",
		1,
		"marker"
	);
	testRequire(pMarker != NULL, "percent Next marker allocation failed");
	xrtSetError(pMarker);
	xrtErrorFree(pMarker);
	pPrevious = xrtGetError();
	iOffset = 0;
	iValue = 0xA5u;
	testRequire(
		xrtPercentNext(
			XRT_STR_LITERAL("%G0"),
			false,
			&iOffset,
			&iValue
		) == XPERCENT_NEXT_ERROR,
		"percent Next accepted an invalid escape"
	);
	testRequire((iOffset == 0) && (iValue == 0xA5u) &&
		(xrtGetError() == pPrevious),
		"percent Next changed outputs or the previous error");
	xrtClearError();
}



/* 验证可复用字符位图和已测量快速路径的完整契约。 */
static void testPercentMapPath(void)
{
	static const char InvalidSafe[] = { '\x01' };
	xpercentmap Map;
	xpercentmap Before;
	char Output[16];
	uint8 Decoded[8];
	size_t iSize;

	testRequire(
		xrtPercentMapInit(
			&Map,
			XRT_STR_LITERAL("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
				"abcdefghijklmnopqrstuvwxyz0123456789*-._"),
			false
		),
		"percent custom map initialization failed"
	);
	testRequire(
		xrtPercentMeasure(
			"A *~", 4u, &Map, true, &iSize
		) && (iSize == 6u),
		"percent custom map measurement mismatch"
	);
	memset(Output, 0x5A, sizeof(Output));
	testRequire(
		xrtPercentWriteMeasured(
			"A *~", 4u, &Map, true, Output
		) == 6u &&
		(memcmp(Output, "A+*%7E", 6u) == 0) &&
		(Output[6] == (char)0x5A),
		"percent measured fragment write mismatch"
	);

	memcpy(Output, "A ~", 3u);
	testRequire(
		xrtPercentMeasure(
			Output, 3u, &Map, true, &iSize
		) && (iSize == 5u),
		"percent in-place custom map measurement failed"
	);
	xrtPercentEncodeMeasured(
		Output, 3u, &Map, true, Output, iSize, true
	);
	testRequire(
		strcmp(Output, "A+%7E") == 0,
		"percent measured in-place encode mismatch"
	);
	testRequire(
		xrtPercentDecodeMeasure(
			XRT_STR_LITERAL("A+%7E"), true, &iSize
		) && (iSize == 3u) &&
		(xrtPercentDecodeMeasured(
			XRT_STR_LITERAL("A+%7E"), true, Decoded
		) == 3u) &&
		(memcmp(Decoded, "A ~", 3u) == 0),
		"percent measured form decode mismatch"
	);

	memcpy(&Before, &Map, sizeof(Map));
	testRequire(
		!xrtPercentMapInit(
			&Map,
			(xstrview){ InvalidSafe, sizeof(InvalidSafe) },
			false
		) && (memcmp(&Map, &Before, sizeof(Map)) == 0) &&
		(xrtErrorCode(xrtGetError()) == XCODEC_ERROR_PERCENT_CONFIG),
		"percent invalid custom map was not atomic"
	);
}



/* 执行 percent codec 的向量、全字节、原地和失败契约测试。 */
int main(void)
{
	testPercentVectors();
	testPercentAllBytes();
	testPercentInPlace();
	testPercentFailures();
	testPercentWrappingRanges();
	testPercentAdjacentSize();
	testPercentNext();
	testPercentMapPath();
	printf("[PASS] codec_percent\n");
	return 0;
}

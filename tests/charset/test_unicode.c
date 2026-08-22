#include "../test.h"



/* 校验旧版覆盖过的 UTF-8 合法边界和现代 RFC 3629 上限。 */
static void testValidUtf8(void)
{
	static const unsigned char arrValid[][4] = {
		{ 0x00u, 0, 0, 0 },
		{ 0x7Fu, 0, 0, 0 },
		{ 0xC2u, 0x80u, 0, 0 },
		{ 0xDFu, 0xBFu, 0, 0 },
		{ 0xE0u, 0xA0u, 0x80u, 0 },
		{ 0xEFu, 0xBFu, 0xBFu, 0 },
		{ 0xF0u, 0x90u, 0x80u, 0x80u },
		{ 0xF4u, 0x8Fu, 0xBFu, 0xBFu }
	};
	static const size_t arrSize[] = { 1, 1, 2, 2, 3, 3, 4, 4 };

	for ( size_t i = 0; i < (sizeof(arrSize) / sizeof(arrSize[0])); i++ ) {
		xstrview Text = { (cstr)arrValid[i], arrSize[i] };
		uint32 iScalar = 0;
		size_t iRead = 0;

		testRequire(xrtUtf8Valid(Text, NULL), "valid UTF-8 boundary rejected");
		testRequire(xrtUtf8Decode(Text, &iScalar, &iRead) == XUTF_OK,
			"valid UTF-8 boundary did not decode");
		testRequire(iRead == arrSize[i], "UTF-8 decoder consumed wrong length");
	}
}



/* 拒绝过长形式、代理项、越界标量、错误续字节和截断序列。 */
static void testInvalidUtf8(void)
{
	static const unsigned char arrInvalid[][4] = {
		{ 0x80u, 0, 0, 0 },
		{ 0xC0u, 0x80u, 0, 0 },
		{ 0xC1u, 0xBFu, 0, 0 },
		{ 0xE0u, 0x80u, 0x80u, 0 },
		{ 0xEDu, 0xA0u, 0x80u, 0 },
		{ 0xF0u, 0x80u, 0x80u, 0x80u },
		{ 0xF4u, 0x90u, 0x80u, 0x80u },
		{ 0xF5u, 0x80u, 0x80u, 0x80u },
		{ 0xE1u, 0x80u, 0, 0 },
		{ 0xE1u, 0x41u, 0x80u, 0 }
	};
	static const size_t arrSize[] = { 1, 2, 2, 3, 3, 4, 4, 4, 2, 3 };

	for ( size_t i = 0; i < (sizeof(arrSize) / sizeof(arrSize[0])); i++ ) {
		size_t iError = XRT_NPOS;
		xstrview Text = { (cstr)arrInvalid[i], arrSize[i] };

		testRequire(!xrtUtf8Valid(Text, &iError), "invalid UTF-8 accepted");
		testRequire(iError == 0, "invalid UTF-8 error offset is wrong");
	}
}



/* 校验 Unicode 标量、UTF-16 代理项和标量编码原语。 */
static void testScalarPrimitives(void)
{
	uint16 arrPair[] = { 0xD83Du, 0xDE00u };
	uint16 arrInvalid[] = { 0xD800u, 0x0041u };
	uint32 arrUtf32[] = { 0, 0x10FFFFu };
	uint32 iScalar = 0;
	size_t iRead = 0;
	char arrUtf8[4];
	uint16 arrUtf16[2];

	testRequire(xrtUnicodeScalar(0), "U+0000 is a scalar");
	testRequire(xrtUnicodeScalar(0x10FFFFu), "U+10FFFF is a scalar");
	testRequire(!xrtUnicodeScalar(0xD800u), "surrogate is not a scalar");
	testRequire(!xrtUnicodeScalar(0x110000u), "out-of-range value is not a scalar");
	testRequire(xrtUtf16Decode(xrtUtf16View(arrPair, 2), &iScalar, &iRead) == XUTF_OK,
		"valid surrogate pair rejected");
	testRequire((iScalar == 0x1F600u) && (iRead == 2),
		"surrogate pair decoded incorrectly");
	testRequire(!xrtUtf16Valid(xrtUtf16View(arrInvalid, 2), NULL),
		"invalid surrogate pair accepted");
	testRequire(xrtUtf32Valid(xrtUtf32View(arrUtf32, 2), NULL),
		"valid UTF-32 rejected");
	testRequire(xrtUtf8Encode(0x1F600u, arrUtf8) == 4,
		"supplementary scalar UTF-8 encoding failed");
	testRequire(xrtUtf16Encode(0x1F600u, arrUtf16) == 2,
		"supplementary scalar UTF-16 encoding failed");
	testRequire((arrUtf16[0] == 0xD83Du) && (arrUtf16[1] == 0xDE00u),
		"supplementary scalar UTF-16 encoding is wrong");
}



/* 码元长度、标量计数和嵌入零必须保持明确区别。 */
static void testLengthsAndCounts(void)
{
	static const char arrUtf8[] = { 'A', 0, (char)0xE4, (char)0xBD, (char)0xA0 };
	static const uint16 arrUtf16[] = { 'A', 0, 0x4F60u, 0 };
	static const uint32 arrUtf32[] = { 'A', 0, 0x4F60u, 0 };
	uint16* pUtf16;
	uint32* pUtf32;

	testRequire(xrtUtf8Count((xstrview){ arrUtf8, sizeof(arrUtf8) }) == 3,
		"UTF-8 embedded NUL count failed");
	testRequire(xrtUtf16Count(xrtUtf16View(arrUtf16, 3)) == 3,
		"UTF-16 embedded NUL count failed");
	testRequire(xrtUtf16Len(arrUtf16) == 1,
		"zero-terminated UTF-16 length failed");
	testRequire(xrtUtf32Len(arrUtf32) == 1,
		"zero-terminated UTF-32 length failed");

	pUtf16 = xrtUtf16DupView(xrtUtf16View(arrUtf16, 3));
	pUtf32 = xrtUtf32DupView(xrtUtf32View(arrUtf32, 3));
	testRequire((pUtf16 != NULL) && (memcmp(pUtf16, arrUtf16,
		sizeof(uint16) * 3u) == 0) && (pUtf16[3] == 0),
		"UTF-16 view duplicate did not preserve embedded NUL");
	testRequire((pUtf32 != NULL) && (memcmp(pUtf32, arrUtf32,
		sizeof(uint32) * 3u) == 0) && (pUtf32[3] == 0),
		"UTF-32 view duplicate did not preserve embedded NUL");
	xrtFree(pUtf16);
	xrtFree(pUtf32);

	pUtf16 = xrtUtf16Dup(arrUtf16);
	pUtf32 = xrtUtf32Dup(arrUtf32);
	testRequire((pUtf16 != NULL) && (pUtf16[0] == (uint16)'A') &&
		(pUtf16[1] == 0), "UTF-16 terminated duplicate mismatch");
	testRequire((pUtf32 != NULL) && (pUtf32[0] == (uint32)'A') &&
		(pUtf32[1] == 0), "UTF-32 terminated duplicate mismatch");
	xrtFree(pUtf16);
	xrtFree(pUtf32);

	xrtClearError();
	testRequire(xrtUtf16DupView(xrtUtf16View(NULL, 1)) == NULL,
		"invalid UTF-16 view duplicate must fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"invalid UTF-16 duplicate error mismatch");
	xrtClearError();
	testRequire(xrtUtf32DupView(xrtUtf32View(arrUtf32, SIZE_MAX)) == NULL,
		"overflowing UTF-32 view duplicate must fail");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) == XUTF_ERROR_OVERFLOW),
		"overflowing UTF-32 duplicate error mismatch");
	xrtClearError();
}



/* 标量索引和字节偏移必须严格互逆，并保留嵌入零。 */
static void testUtf8Indexing(void)
{
	static const char arrText[] = {
		'A',
		(char)0xE4, (char)0xBD, (char)0xA0,
		(char)0xF0, (char)0x9F, (char)0x98, (char)0x80,
		0
	};
	static const char arrInvalid[] = { 'A', (char)0xE0, (char)0x80, (char)0x80 };
	xstrview Text = { arrText, sizeof(arrText) };
	xstrview Slice;
	uint32 iScalar = 0;

	testRequire(xrtUtf8Offset(Text, 0) == 0, "UTF-8 zero index offset mismatch");
	testRequire(xrtUtf8Offset(Text, 1) == 1, "UTF-8 ASCII offset mismatch");
	testRequire(xrtUtf8Offset(Text, 2) == 4, "UTF-8 BMP offset mismatch");
	testRequire(xrtUtf8Offset(Text, 3) == 8, "UTF-8 supplementary offset mismatch");
	testRequire(xrtUtf8Offset(Text, 4) == 9, "UTF-8 embedded NUL end offset mismatch");
	testRequire(xrtUtf8Index(Text, 8) == 3, "UTF-8 byte-to-index mismatch");
	testRequire(xrtUtf8At(Text, 1, &iScalar) && (iScalar == 0x4F60u),
		"UTF-8 scalar lookup mismatch");
	testRequire(xrtUtf8At(Text, 3, &iScalar) && (iScalar == 0),
		"UTF-8 embedded NUL lookup mismatch");
	testRequire(xrtUtf8Slice(Text, 1, 2, &Slice) &&
		(Slice.Size == 7) && (memcmp(Slice.Data, arrText + 1, 7) == 0),
		"UTF-8 scalar slice mismatch");
	testRequire(xrtUtf8Slice(Text, 20, XRT_NPOS, &Slice) && (Slice.Size == 0),
		"UTF-8 clamped slice mismatch");

	xrtClearError();
	testRequire(xrtUtf8Index(Text, 2) == XRT_NPOS,
		"UTF-8 interior byte offset must fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"UTF-8 interior byte error mismatch");
	xrtClearError();
	testRequire(xrtUtf8Offset((xstrview){ arrInvalid, sizeof(arrInvalid) }, 2) == XRT_NPOS,
		"invalid UTF-8 index traversal must fail");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(xrtErrorCode(xrtGetError()) == XUTF_ERROR_INVALID),
		"invalid UTF-8 index traversal error mismatch");
	xrtClearError();
}



/* 穷举全部 Unicode 标量，验证两种编码器和解码器互为逆运算。 */
static void testEveryScalar(void)
{
	for ( uint32 iScalar = 0; iScalar <= 0x10FFFFu; iScalar++ ) {
		char arrUtf8[4];
		uint16 arrUtf16[2];
		uint32 iDecoded = 0;
		size_t iUtf8Size;
		size_t iUtf16Size;
		size_t iRead = 0;

		if ( !xrtUnicodeScalar(iScalar) ) {
			continue;
		}
		iUtf8Size = xrtUtf8Encode(iScalar, arrUtf8);
		iUtf16Size = xrtUtf16Encode(iScalar, arrUtf16);
		testRequire(xrtUtf8Decode((xstrview){ arrUtf8, iUtf8Size },
			&iDecoded, &iRead) == XUTF_OK, "encoded scalar did not decode from UTF-8");
		testRequire((iDecoded == iScalar) && (iRead == iUtf8Size),
			"UTF-8 scalar round trip changed value");
		testRequire(xrtUtf16Decode(xrtUtf16View(arrUtf16, iUtf16Size),
			&iDecoded, &iRead) == XUTF_OK, "encoded scalar did not decode from UTF-16");
		testRequire((iDecoded == iScalar) && (iRead == iUtf16Size),
			"UTF-16 scalar round trip changed value");
	}
}



/* 验证所有输出参数和流状态都不会覆盖借用输入或彼此重叠。 */
static void testOutputAliasing(void)
{
	union {
		uint64 Align;
		unsigned char Bytes[64];
	} Memory;
	unsigned char arrBefore[sizeof(Memory.Bytes)];
	xutf8state State;
	xutf8state BeforeState;
	size_t iRead = 77;
	size_t iShared = SIZE_MAX;

	memset(&Memory, 0, sizeof(Memory));
	memcpy(Memory.Bytes, "abc", 3);
	memcpy(arrBefore, Memory.Bytes, sizeof(arrBefore));
	xrtClearError();
	testRequire(xrtUtf8Decode(
		(xstrview){ (cstr)Memory.Bytes, 3 },
		(uint32*)(void*)Memory.Bytes, &iRead
	) == XUTF_INVALID, "UTF-8 decode accepted an output inside its input");
	testRequire((iRead == 77) &&
		(memcmp(Memory.Bytes, arrBefore, sizeof(arrBefore)) == 0),
		"UTF-8 decode alias failure modified outputs");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"UTF-8 decode alias error mismatch");

	xrtClearError();
	testRequire(xrtUtf8Decode(
		XRT_STR_LITERAL("A"), (uint32*)(void*)&iShared, &iShared
	) == XUTF_INVALID, "UTF-8 decode accepted overlapping outputs");
	testRequire(iShared == SIZE_MAX,
		"UTF-8 decode overlapping outputs were not failure atomic");

	xrtClearError();
	testRequire(!xrtUtf8Valid(
		(xstrview){ (cstr)Memory.Bytes, 3 }, (size_t*)(void*)Memory.Bytes
	), "UTF-8 validation accepted an error output inside its input");
	testRequire(memcmp(Memory.Bytes, arrBefore, sizeof(arrBefore)) == 0,
		"UTF-8 validation alias failure modified input");

	xrtClearError();
	testRequire(!xrtUtf8At(
		(xstrview){ (cstr)Memory.Bytes, 3 }, 0,
		(uint32*)(void*)Memory.Bytes
	), "UTF-8 scalar lookup accepted an output inside its input");
	testRequire(!xrtUtf8Slice(
		(xstrview){ (cstr)Memory.Bytes, 3 }, 0, 1,
		(xstrview*)(void*)Memory.Bytes
	), "UTF-8 slice accepted an output inside its input");
	testRequire(memcmp(Memory.Bytes, arrBefore, sizeof(arrBefore)) == 0,
		"UTF-8 view alias failure modified input");

	xrtUtf8StateInit(&State);
	BeforeState = State;
	xrtClearError();
	testRequire(xrtUtf8StateFeed(
		&State, (xstrview){ (cstr)&State, sizeof(State) }, false
	) == XUTF_INVALID, "UTF-8 stream accepted state as input storage");
	testRequire(memcmp(&State, &BeforeState, sizeof(State)) == 0,
		"UTF-8 stream alias failure modified state");
}



/* 执行 Unicode 标量和严格校验测试。 */
int main(void)
{
	testValidUtf8();
	testInvalidUtf8();
	testScalarPrimitives();
	testLengthsAndCounts();
	testUtf8Indexing();
	testEveryScalar();
	testOutputAliasing();
	return 0;
}

#include "../test.h"



/* 验证 RFC 8187 标准格式、UTF-8 默认值和百分号闭环。 */
static void testHttpExtValueRoundtrip(void)
{
	static const uint8 Utf8Name[] = {
		0xE4, 0xB8, 0xAD, 0xE6, 0x96, 0x87, '.', 't', 'x', 't'
	};
	static const char Expected[] =
		"UTF-8'zh-CN'%E4%B8%AD%E6%96%87.txt";
	xhttpextvalue Value;
	uint8 Decoded[32];
	char Text[64];
	size_t iSize;

	testRequire(xrtHttpExtValueWrite(
		XRT_STR_LITERAL(""), XRT_STR_LITERAL("zh-CN"),
		(xbytesview){ Utf8Name, sizeof(Utf8Name) },
		Text, sizeof(Text), &iSize
	) && (iSize == (sizeof(Expected) - 1u)) &&
		(memcmp(Text, Expected, iSize) == 0),
		"HTTP ext-value write mismatch");
	testRequire(xrtHttpExtValueParse(
		(xstrview){ Text, iSize }, &Value
	) && xrtHttpTokenEqual(
		Value.Charset, XRT_STR_LITERAL("UTF-8")
	) && xrtHttpTokenEqual(
		Value.Language, XRT_STR_LITERAL("zh-CN")
	) && xrtHttpExtValueRead(
		&Value, Decoded, sizeof(Decoded), &iSize
	) && (iSize == sizeof(Utf8Name)) &&
		(memcmp(Decoded, Utf8Name, sizeof(Utf8Name)) == 0),
		"HTTP ext-value roundtrip mismatch");
	testRequire(xrtHttpExtValueParse(
		XRT_STR_LITERAL("X{Y}'x-private'value"), &Value
	) && (Value.Charset.Size == 4u) &&
		(memcmp(Value.Charset.Data, "X{Y}", 4u) == 0) &&
		(Value.Language.Size == 9u),
		"HTTP ext-value rejected RFC mime-charset or language syntax");
	testRequire(xrtHttpExtValueParse(
		XRT_STR_LITERAL("x%y''value"), &Value
	) && (Value.Charset.Size == 3u),
		"HTTP ext-value rejected percent in mime-charset");
}



/* 验证显式单字节字符集的所有字节值和多种长度都能无损往返。 */
static void testHttpExtValueBoundaries(void)
{
	uint8 Data[257];
	uint8 Decoded[257];
	char Encoded[(sizeof(Data) * 3u) + 16u];
	xhttpextvalue Value;
	size_t iEncoded;
	size_t iDecoded;

	for ( size_t i = 0; i < sizeof(Data); i++ ) {
		Data[i] = (uint8)i;
	}
	for ( size_t i = 0; i <= sizeof(Data); i++ ) {
		testRequire(xrtHttpExtValueWrite(
			XRT_STR_LITERAL("ISO-8859-1"), (xstrview){ NULL, 0 },
			(xbytesview){ Data, i }, Encoded, sizeof(Encoded), &iEncoded
		) && xrtHttpExtValueParse(
			(xstrview){ Encoded, iEncoded }, &Value
		) && xrtHttpExtValueRead(
			&Value, Decoded, sizeof(Decoded), &iDecoded
		) && (iDecoded == i) && (memcmp(Data, Decoded, i) == 0),
			"HTTP ext-value boundary roundtrip mismatch");
	}
}



/* 验证严格语法、容量查询、未对齐描述符和失败原子性。 */
static void testHttpExtValueFailures(void)
{
	static const xstrview Invalid[] = {
		XRT_STR_INIT(""),
		XRT_STR_INIT("'en'value"),
		XRT_STR_INIT("UTF-8'en"),
		XRT_STR_INIT("UTF-8'-en'value"),
		XRT_STR_INIT("UTF-8'en-'value"),
		XRT_STR_INIT("UTF-8'en'%"),
		XRT_STR_INIT("UTF-8'en'%0"),
		XRT_STR_INIT("UTF-8'en'%GG"),
		XRT_STR_INIT("UTF-8'en'a*b"),
		XRT_STR_INIT("UTF.8''value"),
		XRT_STR_INIT("UTF*8''value"),
		XRT_STR_INIT("UTF|8''value"),
		XRT_STR_INIT("UTF-8'1-en'value"),
		XRT_STR_INIT("UTF-8'abcdefghi'value"),
		XRT_STR_INIT("UTF-8'en-abcdefghi'value")
	};
	union {
		uint8 Bytes[sizeof(xhttpextvalue) + 1u];
		uintptr_t Align;
	} Storage;
	xhttpextvalue Value;
	xhttpextvalue Before;
	xstrview Wrapped = {
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
	};
	uint8 Byte = (uint8)'a';
	char Output[16];
	size_t iSize;

	for ( size_t i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		testRequire(!xrtHttpExtValueParse(Invalid[i], &Value) &&
			(xrtErrorKind(xrtGetError()) == XERR_VALUE),
			"HTTP ext-value accepted malformed syntax");
		xrtClearError();
	}
	testRequire(xrtHttpExtValueParse(
		XRT_STR_LITERAL("UTF-8''a%20b"),
		(xhttpextvalue*)(void*)(Storage.Bytes + 1u)
	) && xrtHttpExtValueRead(
		(const xhttpextvalue*)(const void*)(Storage.Bytes + 1u),
		NULL, 0, &iSize
	) && (iSize == 3u),
		"HTTP ext-value rejected unaligned descriptor");

	iSize = 99u;
	memset(Output, 0x5A, sizeof(Output));
	testRequire(!xrtHttpExtValueRead(
		(const xhttpextvalue*)(const void*)(Storage.Bytes + 1u),
		Output, 2u, &iSize
	) && (iSize == 3u) && (Output[0] == (char)0x5A) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTTP ext-value short output was not atomic");
	xrtClearError();

	iSize = 99u;
	memset(&Value, 0xA5, sizeof(Value));
	Before = Value;
	testRequire(!xrtHttpExtValueParse(Wrapped, &Value) &&
		(memcmp(&Value, &Before, sizeof(Value)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP ext-value parser accepted a wrapped input range");
	xrtClearError();
	testRequire(!xrtHttpExtValueWrite(
		XRT_STR_LITERAL("UTF-8"), (xstrview){ NULL, 0 },
		(xbytesview){ &Byte, 1u },
		(void*)(uintptr_t)(UINTPTR_MAX - 1u), 4u, &iSize
	) && (iSize == 99u) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP ext-value writer accepted a wrapped output capacity");
	xrtClearError();
	testRequire(!xrtHttpExtValueWrite(
		XRT_STR_LITERAL("UTF.8"), (xstrview){ NULL, 0 },
		(xbytesview){ &Byte, 1u }, Output, sizeof(Output), &iSize
	) && (iSize == 99u) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP ext-value writer accepted an invalid mime-charset");
	xrtClearError();
}



/* 执行扩展值线路、边界和失败契约测试。 */
int main(void)
{
	testHttpExtValueRoundtrip();
	testHttpExtValueBoundaries();
	testHttpExtValueFailures();
	return 0;
}


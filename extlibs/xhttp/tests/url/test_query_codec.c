#include "../test.h"



/* 验证常用 Query 构建会按 RFC 3986 编码键和值并保留值存在状态。 */
static void testQueryCodecVectors(void)
{
	static const uint8 Zero[] = { 0 };
	static const xquerypair Pairs[] = {
		{ XQUERY_HAS_VALUE, XRT_STR_INIT("a b"), XRT_STR_INIT("~&=") },
		{ 0, XRT_STR_INIT("flag"), { NULL, 0 } },
		{ XQUERY_HAS_VALUE, { (cstr)Zero, 1 }, XRT_STR_INIT("+") },
		{ XQUERY_HAS_VALUE, XRT_STR_INIT(""), XRT_STR_INIT("empty") }
	};
	static const char Expected[] =
		"a%20b=~%26%3D&flag&%00=%2B&=empty";
	char Text[128];
	str sBuilt;
	size_t iSize;

	memset(Text, 0x5A, sizeof(Text));
	testRequire(xrtQueryWrite(
		Pairs, sizeof(Pairs) / sizeof(Pairs[0]), NULL, 0, &iSize
	) && (iSize == sizeof(Expected) - 1u),
		"query codec size query mismatch");
	testRequire(xrtQueryWrite(
		Pairs, sizeof(Pairs) / sizeof(Pairs[0]), Text, iSize, &iSize
	) && (memcmp(Text, Expected, iSize) == 0) &&
		((uint8)Text[iSize] == UINT8_C(0x5A)),
		"query codec vector or no-terminator contract mismatch");
	sBuilt = xrtQueryBuild(
		Pairs, sizeof(Pairs) / sizeof(Pairs[0]), &iSize
	);
	testRequire((sBuilt != NULL) && (strcmp(sBuilt, Expected) == 0),
		"query codec allocated build mismatch");
	xrtFree(sBuilt);

	sBuilt = xrtQueryBuild(NULL, 0, &iSize);
	testRequire((sBuilt != NULL) && (iSize == 0) && (sBuilt[0] == '\0'),
		"query codec empty build mismatch");
	xrtFree(sBuilt);
}



/* 验证全部字节经过 Query 编码、结构扫描和 percent 解码后无损返回。 */
static void testQueryCodecAllBytes(void)
{
	uint8 Input[256];
	char Text[1600];
	uint8 Key[256];
	uint8 Value[256];
	xquerypair Source;
	xquerypair Parsed;
	size_t iOffset = 0;
	size_t iText;
	size_t iKey;
	size_t iValue;
	size_t i;

	for ( i = 0; i < sizeof(Input); i++ ) {
		Input[i] = (uint8)i;
	}
	Source.Flags = XQUERY_HAS_VALUE;
	Source.Key = (xstrview){ (cstr)Input, sizeof(Input) };
	Source.Value = Source.Key;
	testRequire(xrtQueryWrite(
		&Source, 1, Text, sizeof(Text), &iText
	), "query codec all-byte write failed");
	testRequire(xrtQueryNext(
		(xstrview){ Text, iText }, &iOffset, &Parsed
	) == XQUERY_NEXT_ITEM, "query codec all-byte item missing");
	testRequire(xrtPercentDecode(
		Parsed.Key, Key, sizeof(Key), &iKey
	) && xrtPercentDecode(
		Parsed.Value, Value, sizeof(Value), &iValue
	) && (iKey == sizeof(Input)) && (iValue == sizeof(Input)) &&
		(memcmp(Key, Input, sizeof(Input)) == 0) &&
		(memcmp(Value, Input, sizeof(Input)) == 0),
		"query codec all-byte round trip mismatch");
}



/* 验证容量、状态、别名和空值边界在失败时保持原子。 */
static void testQueryCodecFailures(void)
{
	xquerypair Pair = {
		XQUERY_HAS_VALUE, XRT_STR_LITERAL("a b"), XRT_STR_LITERAL("1&2")
	};
	char Buffer[32];
	char Before[32];
	size_t iSize;

	memset(Buffer, 0xA5, sizeof(Buffer));
	memcpy(Before, Buffer, sizeof(Buffer));
	iSize = 77;
	testRequire(!xrtQueryWrite(
		&Pair, 1, Buffer, 3, &iSize
	) && (iSize == 11) && (memcmp(Buffer, Before, sizeof(Buffer)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"query codec short write was not atomic");

	Pair.Flags = 0;
	Pair.Key = XRT_STR_LITERAL("");
	Pair.Value = (xstrview){ NULL, 0 };
	iSize = 77;
	testRequire(!xrtQueryWrite(
		&Pair, 1, Buffer, sizeof(Buffer), &iSize
	) && (iSize == 77) && (xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"query codec accepted an unrepresentable empty missing-value item");

	memcpy(Buffer, "a b", 3);
	Pair.Flags = XQUERY_HAS_VALUE;
	Pair.Key = (xstrview){ Buffer, 3 };
	Pair.Value = XRT_STR_LITERAL("1");
	iSize = 77;
	testRequire(!xrtQueryWrite(
		&Pair, 1, Buffer, sizeof(Buffer), &iSize
	) && (iSize == 77) && (memcmp(Buffer, "a b", 3) == 0),
		"query codec accepted overlapping input and output");
	testRequire(!xrtQueryWrite(
		&Pair, 1, NULL, 1, &iSize
	), "query codec accepted capacity without output");
}



/* 执行 Query codec 的向量、全字节和失败契约测试。 */
int main(void)
{
	testQueryCodecVectors();
	testQueryCodecAllBytes();
	testQueryCodecFailures();
	printf("[PASS] query_codec\n");
	return 0;
}

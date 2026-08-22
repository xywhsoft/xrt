#include "../test.h"



/* 按字节验证借用查询项。 */
static void testQueryPair(
	const xquerypair* pPair,
	cstr sKey,
	cstr sValue,
	bool bHasValue
)
{
	size_t iKey = strlen(sKey);
	size_t iValue = sValue != NULL ? strlen(sValue) : 0;

	testRequire(pPair->Key.Size == iKey,
		"query key size mismatch");
	testRequire((iKey == 0) ||
		(memcmp(pPair->Key.Data, sKey, iKey) == 0),
		"query key bytes mismatch");
	testRequire(((pPair->Flags & XQUERY_HAS_VALUE) != 0) == bHasValue,
		"query value presence mismatch");
	if ( bHasValue ) {
		testRequire(pPair->Value.Size == iValue,
			"query value size mismatch");
		testRequire((iValue == 0) ||
			(memcmp(pPair->Value.Data, sValue, iValue) == 0),
			"query value bytes mismatch");
	} else {
		testRequire((pPair->Value.Data == NULL) &&
			(pPair->Value.Size == 0),
			"missing query value retained stale view");
	}
}



/* 验证前导问号、空段、空 key/value、缺失值和重复 key。 */
static void testQueryIteration(void)
{
	xstrview Query = XRT_STR_LITERAL("?a=1&&b&=zero&c=&a=2&x=1=2&");
	xquerypair Pair;
	size_t iOffset = 0;
	size_t iCount;

	testRequire(xrtQueryNext(Query, &iOffset, &Pair) == XQUERY_NEXT_ITEM,
		"query first item missing");
	testQueryPair(&Pair, "a", "1", true);
	testRequire(xrtQueryNext(Query, &iOffset, &Pair) == XQUERY_NEXT_ITEM,
		"query missing-value item missing");
	testQueryPair(&Pair, "b", NULL, false);
	testRequire(xrtQueryNext(Query, &iOffset, &Pair) == XQUERY_NEXT_ITEM,
		"query empty-key item missing");
	testQueryPair(&Pair, "", "zero", true);
	testRequire(xrtQueryNext(Query, &iOffset, &Pair) == XQUERY_NEXT_ITEM,
		"query empty-value item missing");
	testQueryPair(&Pair, "c", "", true);
	testRequire(xrtQueryNext(Query, &iOffset, &Pair) == XQUERY_NEXT_ITEM,
		"query duplicate item missing");
	testQueryPair(&Pair, "a", "2", true);
	testRequire(xrtQueryNext(Query, &iOffset, &Pair) == XQUERY_NEXT_ITEM,
		"query equals-in-value item missing");
	testQueryPair(&Pair, "x", "1=2", true);
	testRequire(xrtQueryNext(Query, &iOffset, &Pair) == XQUERY_NEXT_END,
		"query iterator did not finish");
	testRequire(xrtQueryCount(Query, &iCount) && (iCount == 6),
		"query count mismatch");

	iOffset = 0;
	testRequire(xrtQueryNext(
		XRT_STR_LITERAL("?&&"), &iOffset, &Pair
	) == XQUERY_NEXT_END, "query did not skip empty segments");
	iOffset = 0;
	testRequire(xrtQueryNext(
		(xstrview){ NULL, 0 }, &iOffset, &Pair
	) == XQUERY_NEXT_END, "empty query did not finish cleanly");
}



/* 验证重复 key 可以通过共享偏移连续查找。 */
static void testQueryFind(void)
{
	xstrview Query = XRT_STR_LITERAL("a=1&b=2&a=3");
	xquerypair Pair;
	size_t iOffset = 0;

	testRequire(xrtQueryFind(
		Query, XRT_STR_LITERAL("a"), &iOffset, &Pair
	) == XQUERY_NEXT_ITEM, "query first duplicate not found");
	testQueryPair(&Pair, "a", "1", true);
	testRequire(xrtQueryFind(
		Query, XRT_STR_LITERAL("a"), &iOffset, &Pair
	) == XQUERY_NEXT_ITEM, "query second duplicate not found");
	testQueryPair(&Pair, "a", "3", true);
	testRequire(xrtQueryFind(
		Query, XRT_STR_LITERAL("a"), &iOffset, &Pair
	) == XQUERY_NEXT_END, "query duplicate search did not end");

	iOffset = 0;
	testRequire(xrtQueryFind(
		Query, XRT_STR_LITERAL("missing"), &iOffset, &Pair
	) == XQUERY_NEXT_END, "query missing key was reported as found");
}



/* 验证服务端可以显式限制 pair 数和单项原始长度。 */
static void testQueryLimits(void)
{
	xquerylimits Limits = { 3, 5, 5 };
	size_t iCount = 77;

	testRequire(xrtQueryValidate(
		XRT_STR_LITERAL("a=1&name=value&empty="), &Limits, &iCount
	) && (iCount == 3), "query explicit limits rejected valid input");
	iCount = 77;
	Limits.MaxPairs = 2;
	testRequire(!xrtQueryValidate(
		XRT_STR_LITERAL("a=1&b=2&c=3"), &Limits, &iCount
	) && (iCount == 77) && (xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"query pair limit was not enforced atomically");
	Limits.MaxPairs = 0;
	Limits.MaxKey = 3;
	testRequire(!xrtQueryValidate(
		XRT_STR_LITERAL("name=1"), &Limits, &iCount
	), "query key limit was not enforced");
	Limits.MaxKey = 0;
	Limits.MaxValue = 2;
	testRequire(!xrtQueryValidate(
		XRT_STR_LITERAL("a=123"), &Limits, &iCount
	), "query value limit was not enforced");
	testRequire(xrtQueryValidate(
		XRT_STR_LITERAL("a=123&b=456"), NULL, NULL
	), "query null limits introduced a hidden bound");
}



/* 验证原始查询写出不隐式编码并保留值存在位。 */
static void testQueryWrite(void)
{
	static const xquerypair Pairs[] = {
		{ XQUERY_HAS_VALUE, XRT_STR_INIT("a"), XRT_STR_INIT("1") },
		{ 0, XRT_STR_INIT("b"), { NULL, 0 } },
		{ XQUERY_HAS_VALUE, XRT_STR_INIT(""), XRT_STR_INIT("zero") },
		{ XQUERY_HAS_VALUE, XRT_STR_INIT("c"), XRT_STR_INIT("") },
		{ XQUERY_HAS_VALUE, XRT_STR_INIT("raw%20key"), XRT_STR_INIT("1=2") }
	};
	char Text[128];
	str sBuilt;
	size_t iSize;

	memset(Text, 0x5A, sizeof(Text));
	testRequire(xrtQueryRawWrite(
		Pairs, sizeof(Pairs) / sizeof(Pairs[0]),
		NULL, 0, &iSize
	) && (iSize == 28), "query write size mismatch");
	testRequire(xrtQueryRawWrite(
		Pairs, sizeof(Pairs) / sizeof(Pairs[0]),
		Text, iSize, &iSize
	) && (memcmp(Text, "a=1&b&=zero&c=&raw%20key=1=2", iSize) == 0) &&
		((uint8)Text[iSize] == UINT8_C(0x5A)),
		"query write bytes or no-terminator contract mismatch");
	sBuilt = xrtQueryRawBuild(
		Pairs, sizeof(Pairs) / sizeof(Pairs[0]), &iSize
	);
	testRequire((sBuilt != NULL) &&
		(strcmp(sBuilt, "a=1&b&=zero&c=&raw%20key=1=2") == 0),
		"query allocated build mismatch");
	xrtFree(sBuilt);

	sBuilt = xrtQueryRawBuild(NULL, 0, &iSize);
	testRequire((sBuilt != NULL) && (iSize == 0) && (sBuilt[0] == '\0'),
		"empty query build mismatch");
	xrtFree(sBuilt);
}



/* 验证参数、容量、分隔符、状态和重叠失败保持原子性。 */
static void testQueryFailures(void)
{
	xquerypair Pair = {
		XQUERY_HAS_VALUE, XRT_STR_LITERAL("a"), XRT_STR_LITERAL("1")
	};
	xstrview Wrapped = {
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
	};
	char Buffer[32];
	char Before[32];
	size_t iSize;
	size_t iOffset;

	memset(Buffer, 0xA5, sizeof(Buffer));
	memcpy(Before, Buffer, sizeof(Buffer));
	iSize = 77;
	testRequire(!xrtQueryRawWrite(
		&Pair, 1, Buffer, 2, &iSize
	) && (iSize == 3) && (memcmp(Buffer, Before, sizeof(Buffer)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"query short write was not atomic");

	Pair.Key = XRT_STR_LITERAL("a&b");
	iSize = 77;
	testRequire(!xrtQueryRawWrite(
		&Pair, 1, Buffer, sizeof(Buffer), &iSize
	) && (iSize == 77) && (memcmp(Buffer, Before, sizeof(Buffer)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"query accepted an injected key delimiter");
	Pair.Key = XRT_STR_LITERAL("a=b");
	testRequire(!xrtQueryRawWrite(
		&Pair, 1, Buffer, sizeof(Buffer), &iSize
	), "query accepted an injected key equals delimiter");
	Pair.Key = XRT_STR_LITERAL("a");
	Pair.Value = XRT_STR_LITERAL("1&b=2");
	testRequire(!xrtQueryRawWrite(
		&Pair, 1, Buffer, sizeof(Buffer), &iSize
	), "query accepted an injected value delimiter");

	Pair.Flags = 0;
	Pair.Key = XRT_STR_LITERAL("");
	Pair.Value = (xstrview){ NULL, 0 };
	testRequire(!xrtQueryRawWrite(
		&Pair, 1, Buffer, sizeof(Buffer), &iSize
	), "query accepted an unrepresentable empty missing-value pair");
	Pair.Key = XRT_STR_LITERAL("a");
	Pair.Value = XRT_STR_LITERAL("");
	testRequire(!xrtQueryRawWrite(
		&Pair, 1, Buffer, sizeof(Buffer), &iSize
	), "query accepted a value view without the presence flag");
	Pair.Flags = UINT32_C(0x80000000);
	Pair.Value = (xstrview){ NULL, 0 };
	testRequire(!xrtQueryRawWrite(
		&Pair, 1, Buffer, sizeof(Buffer), &iSize
	), "query accepted unknown pair flags");

	iOffset = 4;
	testRequire(xrtQueryNext(
		XRT_STR_LITERAL("a=1"), &iOffset, &Pair
	) == XQUERY_NEXT_ERROR && (iOffset == 4),
		"query accepted an out-of-range offset");
	iOffset = 0;
	testRequire(xrtQueryNext(
		(xstrview){ NULL, 1 }, &iOffset, &Pair
	) == XQUERY_NEXT_ERROR, "query accepted a null non-empty view");
	iOffset = 0;
	testRequire(xrtQueryNext(
		Wrapped, &iOffset, &Pair
	) == XQUERY_NEXT_ERROR && (iOffset == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"query accepted a wrapped input range");

	memcpy(Buffer, "a=1", 3);
	Pair.Flags = XQUERY_HAS_VALUE;
	Pair.Key = (xstrview){ Buffer, 1 };
	Pair.Value = (xstrview){ Buffer + 2, 1 };
	iSize = 77;
	testRequire(!xrtQueryRawWrite(
		&Pair, 1, Buffer, sizeof(Buffer), &iSize
	) && (iSize == 77), "query accepted overlapping output and input views");
}



/* 执行 Query 原始结构的迭代、查找、写出和边界测试。 */
int main(void)
{
	testQueryIteration();
	testQueryFind();
	testQueryLimits();
	testQueryWrite();
	testQueryFailures();
	printf("[PASS] query\n");
	return 0;
}

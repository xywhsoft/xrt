#include "../test.h"



/* 按字节验证一个借用 Cookie pair。 */
static void testCookiePair(
	const xcookiepair* pPair,
	cstr sName,
	cstr sValue
)
{
	size_t iName = strlen(sName);
	size_t iValue = strlen(sValue);

	testRequire((pPair->Name.Size == iName) &&
		(memcmp(pPair->Name.Data, sName, iName) == 0),
		"cookie name mismatch");
	testRequire((pPair->Value.Size == iValue) &&
		(memcmp(pPair->Value.Data, sValue, iValue) == 0),
		"cookie value mismatch");
}



/* 验证 OWS、引号值、空值、重复名称和结束状态。 */
static void testCookieIteration(void)
{
	xstrview Text = XRT_STR_LITERAL(
		" a = 1 ; theme=\"dark\"; empty=; dup=1; dup=2"
	);
	xcookiepair Pair;
	size_t iOffset = 0;
	size_t iCount = 0;

	testRequire(xrtCookieNext(Text, &iOffset, &Pair) ==
		XCOOKIE_NEXT_ITEM, "cookie first pair missing");
	testCookiePair(&Pair, "a", "1");
	testRequire(xrtCookieNext(Text, &iOffset, &Pair) ==
		XCOOKIE_NEXT_ITEM, "cookie quoted pair missing");
	testCookiePair(&Pair, "theme", "\"dark\"");
	testRequire(xrtCookieNext(Text, &iOffset, &Pair) ==
		XCOOKIE_NEXT_ITEM, "cookie empty pair missing");
	testCookiePair(&Pair, "empty", "");
	testRequire(xrtCookieNext(Text, &iOffset, &Pair) ==
		XCOOKIE_NEXT_ITEM, "cookie first duplicate missing");
	testCookiePair(&Pair, "dup", "1");
	testRequire(xrtCookieNext(Text, &iOffset, &Pair) ==
		XCOOKIE_NEXT_ITEM, "cookie second duplicate missing");
	testCookiePair(&Pair, "dup", "2");
	testRequire(xrtCookieNext(Text, &iOffset, &Pair) ==
		XCOOKIE_NEXT_END, "cookie iterator did not end");
	testRequire(xrtCookieValidate(Text, NULL, &iCount) &&
		(iCount == 5), "cookie count mismatch");
	testRequire(xrtCookieValidate(
		(xstrview){ NULL, 0 }, NULL, &iCount
	) && (iCount == 0), "empty cookie field failed");
}



/* 验证查找完整预检并支持重复名称迭代。 */
static void testCookieFind(void)
{
	xstrview Text = XRT_STR_LITERAL("a=1; dup=first; dup=second");
	xcookiepair Pair;
	size_t iOffset = 0;

	testRequire(xrtCookieFind(
		Text, XRT_STR_LITERAL("dup"), &iOffset, &Pair
	) == XCOOKIE_NEXT_ITEM, "cookie first duplicate not found");
	testCookiePair(&Pair, "dup", "first");
	testRequire(xrtCookieFind(
		Text, XRT_STR_LITERAL("dup"), &iOffset, &Pair
	) == XCOOKIE_NEXT_ITEM, "cookie second duplicate not found");
	testCookiePair(&Pair, "dup", "second");
	testRequire(xrtCookieFind(
		Text, XRT_STR_LITERAL("dup"), &iOffset, &Pair
	) == XCOOKIE_NEXT_END, "cookie duplicate lookup did not end");

	iOffset = 0;
	testRequire(xrtCookieFind(
		XRT_STR_LITERAL("a=1; wanted=ok;"),
		XRT_STR_LITERAL("wanted"), &iOffset, &Pair
	) == XCOOKIE_NEXT_ERROR && (iOffset == 0),
		"cookie lookup ignored malformed trailing syntax");
}



/* 验证显式限额和批量借用解析的原子容量契约。 */
static void testCookieLimitsAndParse(void)
{
	xstrview Text = XRT_STR_LITERAL("a=1; name=value; c=3");
	xcookielimits Limits = { 3, 4, 5, 64 };
	xcookiepair Pairs[3];
	xcookiepair Before[3];
	size_t iCount = 77;

	memset(Pairs, 0xA5, sizeof(Pairs));
	memcpy(Before, Pairs, sizeof(Pairs));
	testRequire(xrtCookieValidate(Text, &Limits, &iCount) &&
		(iCount == 3), "cookie explicit limits rejected valid input");
	Limits.MaxPairs = 2;
	iCount = 77;
	testRequire(!xrtCookieValidate(Text, &Limits, &iCount) &&
		(iCount == 77) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"cookie pair limit was not atomic");
	Limits.MaxPairs = 0;
	Limits.MaxName = 3;
	testRequire(!xrtCookieValidate(Text, &Limits, NULL),
		"cookie name limit was not enforced");
	Limits.MaxName = 0;
	Limits.MaxValue = 4;
	testRequire(!xrtCookieValidate(Text, &Limits, NULL),
		"cookie value limit was not enforced");
	Limits.MaxValue = 0;
	Limits.MaxBytes = 8;
	testRequire(!xrtCookieValidate(Text, &Limits, NULL),
		"cookie byte limit was not enforced");

	iCount = 77;
	testRequire(xrtCookieParse(
		Text, NULL, 0, &iCount, NULL
	) && (iCount == 3),
		"cookie null array count query failed");
	iCount = 77;
	testRequire(!xrtCookieParse(
		Text, Pairs, 2, &iCount, NULL
	) && (iCount == 3) &&
		(memcmp(Pairs, Before, sizeof(Pairs)) == 0),
		"cookie short pair array was not atomic");
	testRequire(xrtCookieParse(
		Text, Pairs, 3, &iCount, NULL
	) && (iCount == 3), "cookie pair array parse failed");
	testCookiePair(&Pairs[1], "name", "value");
}



/* 验证规范写出、精确计长、分配构建和空列表。 */
static void testCookieWrite(void)
{
	static const xcookiepair Pairs[] = {
		{ XRT_STR_INIT("sid"), XRT_STR_INIT("abc123") },
		{ XRT_STR_INIT("theme"), XRT_STR_INIT("\"dark\"") },
		{ XRT_STR_INIT("empty"), XRT_STR_INIT("") }
	};
	char Text[64];
	str sBuilt;
	size_t iSize;

	memset(Text, 0x5A, sizeof(Text));
	testRequire(xrtCookieWrite(
		Pairs, 3, NULL, 0, &iSize
	) && (iSize == 32), "cookie write size mismatch");
	testRequire(xrtCookieWrite(
		Pairs, 3, Text, iSize, &iSize
	) && (memcmp(
		Text, "sid=abc123; theme=\"dark\"; empty=", iSize
	) == 0) && ((uint8)Text[iSize] == UINT8_C(0x5A)),
		"cookie write bytes or no-terminator contract mismatch");
	sBuilt = xrtCookieBuild(Pairs, 3, &iSize);
	testRequire((sBuilt != NULL) &&
		(strcmp(sBuilt, "sid=abc123; theme=\"dark\"; empty=") == 0),
		"cookie allocated build mismatch");
	xrtFree(sBuilt);

	sBuilt = xrtCookieBuild(NULL, 0, &iSize);
	testRequire((sBuilt != NULL) && (iSize == 0) &&
		(sBuilt[0] == '\0'), "empty cookie build mismatch");
	xrtFree(sBuilt);
}



/* 验证语法、容量、别名和输出提交失败均不会留下半成品。 */
static void testCookieFailures(void)
{
	xcookiepair Pair = {
		XRT_STR_LITERAL("a"), XRT_STR_LITERAL("1")
	};
	char Buffer[32];
	char Before[32];
	size_t iOffset;
	size_t iSize;

	memset(Buffer, 0xA5, sizeof(Buffer));
	memcpy(Before, Buffer, sizeof(Buffer));
	iSize = 77;
	testRequire(!xrtCookieWrite(
		&Pair, 1, Buffer, 2, &iSize
	) && (iSize == 3) &&
		(memcmp(Buffer, Before, sizeof(Buffer)) == 0),
		"cookie short write was not atomic");
	Pair.Name = XRT_STR_LITERAL("bad name");
	iSize = 77;
	testRequire(!xrtCookieWrite(
		&Pair, 1, Buffer, sizeof(Buffer), &iSize
	) && (iSize == 77) &&
		(memcmp(Buffer, Before, sizeof(Buffer)) == 0),
		"cookie writer accepted an invalid name");
	Pair.Name = XRT_STR_LITERAL("a");
	Pair.Value = XRT_STR_LITERAL("bad;value");
	testRequire(!xrtCookieWrite(
		&Pair, 1, Buffer, sizeof(Buffer), &iSize
	), "cookie writer accepted an injected delimiter");

	memcpy(Buffer, "a=1", 3);
	Pair.Name = (xstrview){ Buffer, 1 };
	Pair.Value = (xstrview){ Buffer + 2, 1 };
	testRequire(!xrtCookieWrite(
		&Pair, 1, Buffer, sizeof(Buffer), &iSize
	), "cookie writer accepted overlapping input and output");

	iOffset = 0;
	memset(&Pair, 0xA5, sizeof(Pair));
	testRequire(xrtCookieNext(
		XRT_STR_LITERAL("a=1;"), &iOffset, &Pair
	) == XCOOKIE_NEXT_ERROR && (iOffset == 0),
		"cookie iterator accepted a trailing delimiter");
	iOffset = 0;
	testRequire(xrtCookieNext(
		XRT_STR_LITERAL("missing"), &iOffset, &Pair
	) == XCOOKIE_NEXT_ERROR && (iOffset == 0),
		"cookie iterator accepted a missing equals sign");
	iOffset = 4;
	testRequire(xrtCookieNext(
		XRT_STR_LITERAL("a=1"), &iOffset, &Pair
	) == XCOOKIE_NEXT_ERROR && (iOffset == 4),
		"cookie iterator accepted an out-of-range offset");

	iOffset = 0;
	testRequire(xrtCookieFind(
		(xstrview){ NULL, 1 }, XRT_STR_LITERAL("a"),
		&iOffset, &Pair
	) == XCOOKIE_NEXT_ERROR &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"cookie find did not reject an invalid borrowed view");
	testRequire(!xrtCookieParse(
		(xstrview){ NULL, 1 }, &Pair, 1, &iSize, NULL
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"cookie parse did not reject an invalid borrowed view");
}



/* 验证全部借用描述符支持未对齐存储，并拒绝回绕范围。 */
static void testCookieUnaligned(void)
{
	uint8 OffsetStorage[sizeof(size_t) + 2u];
	uint8 PairStorage[(sizeof(xcookiepair) * 2u) + 2u];
	uint8 CountStorage[sizeof(size_t) + 2u];
	uint8 LimitsStorage[sizeof(xcookielimits) + 2u];
	uint8 SizeStorage[sizeof(size_t) + 2u];
	size_t* pOffset = (size_t*)(void*)(OffsetStorage + 1u);
	xcookiepair* pPairs =
		(xcookiepair*)(void*)(PairStorage + 1u);
	size_t* pCount = (size_t*)(void*)(CountStorage + 1u);
	xcookielimits* pLimits =
		(xcookielimits*)(void*)(LimitsStorage + 1u);
	size_t* pSize = (size_t*)(void*)(SizeStorage + 1u);
	xcookielimits Limits = { 2, 5, 5, 32 };
	xcookiepair Pair;
	char Output[32];
	size_t iValue = 0;

	memset(OffsetStorage, 0xA5, sizeof(OffsetStorage));
	memset(PairStorage, 0xA5, sizeof(PairStorage));
	memset(CountStorage, 0xA5, sizeof(CountStorage));
	memset(LimitsStorage, 0xA5, sizeof(LimitsStorage));
	memset(SizeStorage, 0xA5, sizeof(SizeStorage));
	memcpy(pOffset, &iValue, sizeof(iValue));
	memcpy(pLimits, &Limits, sizeof(Limits));
	testRequire(
		(xrtCookieNext(
			XRT_STR_LITERAL("a=1; b=2"),
			pOffset,
			pPairs
		 ) == XCOOKIE_NEXT_ITEM),
		"cookie rejected unaligned iterator outputs"
	);
	memcpy(&Pair, pPairs, sizeof(Pair));
	testCookiePair(&Pair, "a", "1");
	testRequire(xrtCookieValidate(
		XRT_STR_LITERAL("a=1; b=2"),
		pLimits,
		pCount
	), "cookie rejected unaligned limits or count");
	memcpy(&iValue, pCount, sizeof(iValue));
	testRequire(iValue == 2,
		"cookie unaligned count mismatch");
	testRequire(xrtCookieParse(
		XRT_STR_LITERAL("a=1; b=2"),
		pPairs,
		2,
		pCount,
		pLimits
	), "cookie rejected unaligned pair array");
	memcpy(
		&Pair,
		PairStorage + 1u + sizeof(Pair),
		sizeof(Pair)
	);
	testCookiePair(&Pair, "b", "2");
	testRequire(xrtCookieWrite(
		pPairs,
		2,
		Output,
		sizeof(Output),
		pSize
	), "cookie rejected unaligned writer descriptors");
	memcpy(&iValue, pSize, sizeof(iValue));
	testRequire(
		(iValue == strlen("a=1; b=2")) &&
		(memcmp(Output, "a=1; b=2", iValue) == 0) &&
		(OffsetStorage[0] == UINT8_C(0xA5)) &&
		(OffsetStorage[sizeof(OffsetStorage) - 1u] ==
		 UINT8_C(0xA5)) &&
		(PairStorage[0] == UINT8_C(0xA5)) &&
		(PairStorage[sizeof(PairStorage) - 1u] ==
		 UINT8_C(0xA5)),
		"cookie unaligned output or guard mismatch"
	);
	testRequire(!xrtCookieValidate(
		(xstrview){
			(cstr)(uintptr_t)(UINTPTR_MAX - 1u),
			4
		},
		NULL,
		NULL
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"cookie accepted a wrapping text range");
}



/* 执行 Cookie 请求字段的解析、查找、写出和边界测试。 */
int main(void)
{
	testCookieIteration();
	testCookieFind();
	testCookieLimitsAndParse();
	testCookieWrite();
	testCookieFailures();
	testCookieUnaligned();
	printf("[PASS] cookie\n");
	return 0;
}

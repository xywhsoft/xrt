#include "../test.h"



/* 要求一个借用视图与预期文本完全相同。 */
static bool testQueryParamsTextEqual(
	xstrview Text,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		((iSize == 0) || (memcmp(Text.Data, sExpected, iSize) == 0));
}



/* 验证解析、重复项、缺省值与构建的基本契约。 */
static void testQueryParamsVectors(void)
{
	static const char Input[] =
		"?q=hello+world&q=%2B&flag&empty=&=value&&"
		"tilde=~&star=*%2A&nul=%00";
	static const char Expected[] =
		"q=hello+world&q=%2B&flag&empty=&=value&"
		"tilde=%7E&star=**&nul=%00";
	xqueryparams* pParams;
	xquerypair Pair;
	str sBuilt;
	size_t iIndex = 0;
	size_t iError = SIZE_MAX;
	size_t iSize;

	pParams = xrtQueryParamsParse(
		(xstrview){ Input, sizeof(Input) - 1u },
		NULL,
		&iError
	);
	testRequire((pParams != NULL) &&
		(iError == (sizeof(Input) - 1u)) &&
		(xrtQueryParamsCount(pParams) == 8u) &&
		(xrtQueryParamsCountName(
			pParams, XRT_STR_LITERAL("q")
		) == 2u), "query params parse count mismatch");
	testRequire(xrtQueryParamsFind(
		pParams, XRT_STR_LITERAL("q"), &iIndex, &Pair
	) == XQUERY_NEXT_ITEM &&
		testQueryParamsTextEqual(Pair.Value, "hello world"),
		"query params first repeated value mismatch");
	testRequire(xrtQueryParamsFind(
		pParams, XRT_STR_LITERAL("q"), &iIndex, &Pair
	) == XQUERY_NEXT_ITEM &&
		testQueryParamsTextEqual(Pair.Value, "+"),
		"query params second repeated value mismatch");
	testRequire(xrtQueryParamsFind(
		pParams, XRT_STR_LITERAL("q"), &iIndex, &Pair
	) == XQUERY_NEXT_END,
		"query params repeated traversal did not end");
	testRequire(xrtQueryParamsAt(pParams, 2u, &Pair) &&
		((Pair.Flags & XQUERY_HAS_VALUE) == 0) &&
		testQueryParamsTextEqual(Pair.Key, "flag"),
		"query params missing-value state mismatch");
	testRequire(xrtQueryParamsAt(pParams, 3u, &Pair) &&
		((Pair.Flags & XQUERY_HAS_VALUE) != 0) &&
		(Pair.Value.Size == 0),
		"query params empty-value state mismatch");
	testRequire(xrtQueryParamsGet(
		pParams, XRT_STR_LITERAL("nul"), &Pair
	) && ((Pair.Flags & XQUERY_HAS_VALUE) != 0) &&
		(Pair.Value.Size == 1u) && (Pair.Value.Data[0] == '\0'),
		"query params embedded NUL mismatch");

	sBuilt = xrtQueryParamsBuild(pParams, &iSize);
	testRequire((sBuilt != NULL) &&
		(iSize == sizeof(Expected) - 1u) &&
		(memcmp(sBuilt, Expected, sizeof(Expected)) == 0),
		"query params build vector mismatch");
	xrtFree(sBuilt);
	xrtQueryParamsDestroy(pParams);
}



/* 验证别名输入、Set、删除、稳定排序、Clone 和 Compact。 */
static void testQueryParamsMutations(void)
{
	xqueryparams* pParams = xrtQueryParamsCreate(NULL);
	xqueryparams* pClone;
	xquerypair Pair;
	size_t iIndex;

	testRequire(pParams != NULL, "query params mutation create failed");
	testRequire(xrtQueryParamsAppend(
		pParams, XRT_STR_LITERAL("q"), XRT_STR_LITERAL("one")
	) && xrtQueryParamsAppend(
		pParams, XRT_STR_LITERAL("q"), XRT_STR_LITERAL("two")
	) && xrtQueryParamsAppend(
		pParams, XRT_STR_LITERAL("b"), XRT_STR_LITERAL("last")
	) && xrtQueryParamsAppend(
		pParams, XRT_STR_LITERAL("a"), XRT_STR_LITERAL("first")
	) && xrtQueryParamsAppend(
		pParams, XRT_STR_LITERAL("a"), XRT_STR_LITERAL("second")
	) && xrtQueryParamsAppend(
		pParams, XRT_STR_LITERAL("B"), XRT_STR_LITERAL("upper")
	), "query params mutation fixture failed");

	testRequire(xrtQueryParamsAt(pParams, 1u, &Pair) &&
		xrtQueryParamsAppendPair(pParams, Pair) &&
		(xrtQueryParamsCountName(
			pParams, XRT_STR_LITERAL("q")
		) == 3u), "query params self-alias append failed");
	testRequire(xrtQueryParamsAt(pParams, 0u, &Pair) &&
		xrtQueryParamsSetPair(pParams, Pair) &&
		(xrtQueryParamsCountName(
			pParams, XRT_STR_LITERAL("q")
		) == 1u), "query params self-alias Set failed");
	testRequire(xrtQueryParamsSet(
		pParams, XRT_STR_LITERAL("q"), XRT_STR_LITERAL("final")
	) && xrtQueryParamsHas(
		pParams, XRT_STR_LITERAL("q")
	), "query params common Set failed");

	testRequire(xrtQueryParamsSort(pParams),
		"query params stable sort failed");
	testRequire(xrtQueryParamsAt(pParams, 0u, &Pair) &&
		testQueryParamsTextEqual(Pair.Key, "B"),
		"query params byte sort order mismatch");
	iIndex = 0;
	testRequire(xrtQueryParamsFind(
		pParams, XRT_STR_LITERAL("a"), &iIndex, &Pair
	) == XQUERY_NEXT_ITEM &&
		testQueryParamsTextEqual(Pair.Value, "first"),
		"query params stable sort first duplicate mismatch");
	testRequire(xrtQueryParamsFind(
		pParams, XRT_STR_LITERAL("a"), &iIndex, &Pair
	) == XQUERY_NEXT_ITEM &&
		testQueryParamsTextEqual(Pair.Value, "second"),
		"query params stable sort second duplicate mismatch");

	testRequire((xrtQueryParamsRemove(
		pParams, XRT_STR_LITERAL("a")
	) == 2u) && !xrtQueryParamsHas(
		pParams, XRT_STR_LITERAL("a")
	) && xrtQueryParamsCompact(pParams),
		"query params remove or compact failed");
	pClone = xrtQueryParamsClone(pParams);
	testRequire((pClone != NULL) && xrtQueryParamsAppend(
		pClone, XRT_STR_LITERAL("clone"), XRT_STR_LITERAL("yes")
	) && !xrtQueryParamsHas(
		pParams, XRT_STR_LITERAL("clone")
	), "query params clone independence mismatch");
	memset(&Pair, 0xA5, sizeof(Pair));
	testRequire(!xrtQueryParamsGet(
		pParams, XRT_STR_LITERAL("missing"), &Pair
	) && (Pair.Key.Data == NULL) && (Pair.Key.Size == 0) &&
		(Pair.Value.Data == NULL) && (Pair.Value.Size == 0),
		"query params missing Get did not clear output");
	xrtQueryParamsDestroy(pClone);
	xrtQueryParamsDestroy(pParams);
}



/* 验证严格与浏览器兼容的宽松 percent 策略。 */
static void testQueryParamsPercentModes(void)
{
	xqueryparamsconfig Config;
	xqueryparams* pParams;
	xquerypair Pair;
	str sBuilt;
	size_t iError = SIZE_MAX;
	size_t iSize;

	pParams = xrtQueryParamsParse(
		XRT_STR_LITERAL("ok=1&bad=%GG"), NULL, &iError
	);
	testRequire((pParams == NULL) && (iError == 9u) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"query params strict percent error mismatch");
	xrtClearError();

	xrtQueryParamsConfigInit(&Config);
	Config.Flags = XQUERY_PARAMS_LENIENT_PERCENT;
	pParams = xrtQueryParamsParse(
		XRT_STR_LITERAL("bad=%GG+ok"), &Config, &iError
	);
	testRequire((pParams != NULL) && (iError == 10u) &&
		xrtQueryParamsAt(pParams, 0, &Pair) &&
		testQueryParamsTextEqual(Pair.Value, "%GG ok"),
		"query params lenient percent decode mismatch");
	sBuilt = xrtQueryParamsBuild(pParams, &iSize);
	testRequire((sBuilt != NULL) &&
		(strcmp(sBuilt, "bad=%25GG+ok") == 0),
		"query params lenient percent build mismatch");
	xrtFree(sBuilt);
	xrtQueryParamsDestroy(pParams);
}



/* 验证解析和写出失败不会暴露部分状态或部分输出。 */
static void testQueryParamsFailureAtomicity(void)
{
	xqueryparamsconfig Config;
	xqueryparams* pParams;
	xquerypair Invalid = {
		0, XRT_STR_INIT(""), { NULL, 0 }
	};
	xquerypair Pair;
	xstrview Wrapped = {
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
	};
	char Output[32];
	char Before[32];
	size_t iError = SIZE_MAX;
	size_t iIndex;
	size_t iSize = SIZE_MAX;

	xrtQueryParamsConfigInit(&Config);
	Config.InitialPairs = 0;
	Config.InitialBytes = 0;
	Config.MaxPairs = 2;
	Config.MaxName = 4;
	Config.MaxValue = 4;
	Config.MaxBytes = 8;
	pParams = xrtQueryParamsCreate(&Config);
	testRequire((pParams != NULL) && xrtQueryParamsAppend(
		pParams, XRT_STR_LITERAL("a"), XRT_STR_LITERAL("1")
	), "query params tight fixture failed");
	testRequire(!xrtQueryParamsParseAppend(
		pParams, XRT_STR_LITERAL("b=2&bad=%GG"), &iError
	) && (iError == 8u) &&
		(xrtQueryParamsCount(pParams) == 1u) &&
		!xrtQueryParamsHas(pParams, XRT_STR_LITERAL("b")),
		"query params parse append exposed partial state");
	xrtClearError();
	testRequire(!xrtQueryParamsAppendPair(pParams, Invalid) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"query params accepted unrepresentable empty bare item");
	xrtClearError();
	testRequire(!xrtQueryParamsAppend(
		pParams, XRT_STR_LITERAL("long-name"), XRT_STR_LITERAL("x")
	) && (xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"query params name limit mismatch");
	xrtClearError();

	iError = SIZE_MAX;
	testRequire(xrtQueryParamsParse(
		Wrapped, NULL, &iError
	) == NULL && (iError == SIZE_MAX) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"query params accepted a wrapped input range");
	xrtClearError();
	iIndex = xrtQueryParamsCount(pParams) + 1u;
	memset(&Pair, 0xA5, sizeof(Pair));
	testRequire(xrtQueryParamsFind(
		pParams, XRT_STR_LITERAL("a"), &iIndex, &Pair
	) == XQUERY_NEXT_ERROR &&
		(iIndex == (xrtQueryParamsCount(pParams) + 1u)) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"query params accepted an out-of-range find cursor");
	xrtClearError();

	memset(Output, 0xA5, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	testRequire(!xrtQueryParamsWrite(
		pParams, Output, 1u, &iSize
	) && (iSize == 3u) &&
		(memcmp(Output, Before, sizeof(Output)) == 0),
		"query params short write was not atomic");
	xrtQueryParamsDestroy(pParams);
}



/* 执行拥有型 QueryParams 的完整契约测试。 */
int main(void)
{
	testQueryParamsVectors();
	testQueryParamsMutations();
	testQueryParamsPercentModes();
	testQueryParamsFailureAtomicity();
	printf("[PASS] query_params\n");
	return 0;
}

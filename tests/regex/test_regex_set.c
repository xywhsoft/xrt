#include "../test.h"



/* 要求当前错误属于指定基础错误种类。 */
static void testRegexSetErrorKind(xerrkind Kind, cstr sMessage)
{
	const xerror* pError = xrtGetError();

	testRequire(pError != NULL, sMessage);
	testRequire(xrtErrorKind(pError) == Kind, sMessage);
}



/* 验证集合保留编译对象并按升序返回全部命中索引。 */
static void testRegexSetCreate(void)
{
	xregexconfig Config;
	xregex* arrRegex[3];
	xregexset* pSet;
	xregexsetmatcher* pMatcher;

	xrtRegexConfigInit(&Config);
	Config.Flags = XREGEX_IGNORE_CASE;
	arrRegex[0] = xrtRegexCompileConfig(XRT_STR_LITERAL("cat"), &Config);
	arrRegex[1] = xrtRegexCompile(XRT_STR_LITERAL("dog"));
	arrRegex[2] = xrtRegexCompile(XRT_STR_LITERAL("a.+z"));
	testRequire(
		(arrRegex[0] != NULL) && (arrRegex[1] != NULL) && (arrRegex[2] != NULL),
		"regex set source compile failed"
	);
	pSet = xrtRegexSetCreate(arrRegex, 3u);
	testRequire(pSet != NULL, "regex set create failed");
	for ( size_t i = 0; i < 3u; i++ ) {
		xrtRegexRelease(arrRegex[i]);
	}
	testRequire(xrtRegexSetCount(pSet) == 3u, "regex set count mismatch");
	testRequire(
		xrtRegexFlags(xrtRegexSetRegex(pSet, 0)) == XREGEX_IGNORE_CASE,
		"regex set did not retain per-pattern flags"
	);
	pMatcher = xrtRegexSetMatcherCreate(pSet);
	xrtRegexSetRelease(pSet);
	testRequire(pMatcher != NULL, "regex set matcher create failed");
	testRequire(
		xrtRegexSetMatcherMatch(
			pMatcher,
			XRT_STR_LITERAL("CAT dog abcz"),
			0
		) == XREGEX_MATCH,
		"regex set multi-match failed"
	);
	testRequire(xrtRegexSetMatcherCount(pMatcher) == 3u, "regex set match count mismatch");
	for ( size_t i = 0; i < 3u; i++ ) {
		testRequire(xrtRegexSetMatcherIndex(pMatcher, i) == i, "regex set index order mismatch");
		testRequire(xrtRegexSetMatcherMatched(pMatcher, i), "regex set membership mismatch");
	}
	testRequire(xrtRegexSetMatcherFirst(pMatcher) == 0, "regex set first index mismatch");
	xrtRegexSetMatcherFree(pMatcher);
}



/* 验证批量编译、非零起点、未命中与一次性入口。 */
static void testRegexSetMatch(void)
{
	const xstrview arrPattern[] = {
		{ "cat", 3u },
		{ "dog", 3u },
		{ "bird", 4u }
	};
	xregexset* pSet = xrtRegexSetCompile(arrPattern, 3u);
	xregexsetmatcher* pMatcher;

	testRequire(pSet != NULL, "regex set batch compile failed");
	pMatcher = xrtRegexSetMatcherCreate(pSet);
	testRequire(pMatcher != NULL, "regex set batch matcher create failed");
	testRequire(
		xrtRegexSetMatcherMatch(pMatcher, XRT_STR_LITERAL("cat dog"), 4u) == XREGEX_MATCH,
		"regex set nonzero start failed"
	);
	testRequire(
		(xrtRegexSetMatcherCount(pMatcher) == 1u) &&
		(xrtRegexSetMatcherFirst(pMatcher) == 1u) &&
		!xrtRegexSetMatcherMatched(pMatcher, 0) &&
		xrtRegexSetMatcherMatched(pMatcher, 1),
		"regex set nonzero start result mismatch"
	);
	testRequire(
		xrtRegexSetMatcherMatch(pMatcher, XRT_STR_LITERAL("fish"), 0) == XREGEX_NONE,
		"regex set unexpected match"
	);
	testRequire(
		(xrtRegexSetMatcherCount(pMatcher) == 0) &&
		(xrtRegexSetMatcherFirst(pMatcher) == XRT_NPOS),
		"regex set empty result mismatch"
	);
	testRequire(
		xrtRegexSetTest(pSet, XRT_STR_LITERAL("a bird")) == XREGEX_MATCH,
		"regex set one-shot test failed"
	);
	xrtRegexSetMatcherFree(pMatcher);
	xrtRegexSetRelease(pSet);
}



/* 验证跨多个位图字的集合命中、最高位移位和结果清理。 */
static void testRegexSetBitmapWords(void)
{
	xregex* arrRegex[65];
	xregex* pRegex = xrtRegexCompile(XRT_STR_LITERAL("a"));
	xregexset* pSet;
	xregexsetmatcher* pMatcher;

	testRequire(pRegex != NULL, "regex bitmap fixture compile failed");
	for ( size_t i = 0; i < 65u; i++ ) {
		arrRegex[i] = pRegex;
	}
	pSet = xrtRegexSetCreate(arrRegex, 65u);
	xrtRegexRelease(pRegex);
	testRequire(pSet != NULL, "large regex set create failed");
	pMatcher = xrtRegexSetMatcherCreate(pSet);
	xrtRegexSetRelease(pSet);
	testRequire(pMatcher != NULL, "large regex set matcher create failed");
	testRequire(
		xrtRegexSetMatcherMatch(pMatcher, XRT_STR_LITERAL("a"), 0) == XREGEX_MATCH,
		"large regex set match failed"
	);
	testRequire(
		xrtRegexSetMatcherCount(pMatcher) == 65u,
		"large regex set match count mismatch"
	);
	for ( size_t i = 0; i < 65u; i++ ) {
		testRequire(
			xrtRegexSetMatcherIndex(pMatcher, i) == i,
			"large regex set index mismatch"
		);
	}
	testRequire(
		xrtRegexSetMatcherMatch(pMatcher, XRT_STR_LITERAL("b"), 0) == XREGEX_NONE,
		"large regex set retained stale bitmap bits"
	);
	testRequire(
		xrtRegexSetMatcherCount(pMatcher) == 0,
		"large regex set stale result count mismatch"
	);
	xrtRegexSetMatcherFree(pMatcher);
}



/* 验证空集合仍是可执行对象且永远返回未命中。 */
static void testRegexSetEmpty(void)
{
	xregexset* pSet = xrtRegexSetCompile(NULL, 0);
	xregexsetmatcher* pMatcher;

	testRequire(pSet != NULL, "empty regex set compile failed");
	testRequire(xrtRegexSetCount(pSet) == 0, "empty regex set count mismatch");
	pMatcher = xrtRegexSetMatcherCreate(pSet);
	testRequire(pMatcher != NULL, "empty regex set matcher create failed");
	testRequire(
		xrtRegexSetMatcherMatch(pMatcher, XRT_STR_LITERAL("anything"), 8u) == XREGEX_NONE,
		"empty regex set returned a match"
	);
	testRequire(xrtRegexSetMatcherCount(pMatcher) == 0, "empty regex set result mismatch");
	xrtRegexSetMatcherFree(pMatcher);
	xrtRegexSetRelease(pSet);
}



/* 验证批量编译错误保留失败模式索引和原始原因。 */
static void testRegexSetCompileError(void)
{
	const xstrview arrPattern[] = {
		{ "valid", 5u },
		{ "[", 1u }
	};
	xregexset* pSet;
	size_t iIndex = XRT_NPOS;

	xrtClearError();
	pSet = xrtRegexSetCompile(arrPattern, 2u);
	testRequire(pSet == NULL, "invalid regex set pattern was accepted");
	testRequire(
		xrtRegexSetErrorIndex(xrtGetError(), &iIndex) && (iIndex == 1u),
		"regex set compile error index mismatch"
	);
	testRequire(
		xrtErrorCause(xrtGetError()) != NULL,
		"regex set compile error lost its original cause"
	);
}



/* 验证参数、状态、范围和尺寸错误不会混用。 */
static void testRegexSetErrors(void)
{
	const xstrview arrPattern[] = { { "a", 1u } };
	xregexconfig Config;
	xregexset* pSet;
	xregexsetmatcher* pMatcher;
	xregex* arrRegex[1] = { NULL };

	xrtRegexConfigInit(&Config);
	Config.Reserved[0] = 1u;
	xrtClearError();
	pSet = xrtRegexSetCompileConfig(NULL, 0, &Config);
	testRequire(pSet == NULL, "empty regex set ignored invalid config");
	testRegexSetErrorKind(XERR_ARGUMENT, "empty regex set config error mismatch");

	xrtClearError();
	pSet = xrtRegexSetCompileConfig(NULL, 0, NULL);
	testRequire(pSet == NULL, "empty regex set accepted null config");
	testRegexSetErrorKind(XERR_ARGUMENT, "empty regex set null config error mismatch");

	xrtClearError();
	pSet = xrtRegexSetCreate(NULL, 1u);
	testRequire(pSet == NULL, "regex set accepted null pattern array");
	testRegexSetErrorKind(XERR_ARGUMENT, "regex set null array error mismatch");

	xrtClearError();
	pSet = xrtRegexSetCreate(arrRegex, SIZE_MAX);
	testRequire(pSet == NULL, "regex set accepted overflowing pattern count");
	testRegexSetErrorKind(XERR_RANGE, "regex set size overflow error mismatch");

	pSet = xrtRegexSetCompile(arrPattern, 1u);
	testRequire(pSet != NULL, "regex set error fixture compile failed");
	pMatcher = xrtRegexSetMatcherCreate(pSet);
	testRequire(pMatcher != NULL, "regex set error fixture matcher failed");

	xrtClearError();
	testRequire(xrtRegexSetMatcherCount(NULL) == 0, "null matcher returned a count");
	testRegexSetErrorKind(XERR_ARGUMENT, "null matcher error mismatch");

	xrtClearError();
	testRequire(xrtRegexSetMatcherFirst(pMatcher) == XRT_NPOS, "fresh matcher returned a result");
	testRegexSetErrorKind(XERR_STATE, "fresh matcher state error mismatch");

	testRequire(
		xrtRegexSetMatcherMatch(pMatcher, XRT_STR_LITERAL("a"), 0) == XREGEX_MATCH,
		"regex set error fixture match failed"
	);
	xrtClearError();
	testRequire(
		xrtRegexSetMatcherIndex(pMatcher, 1u) == XRT_NPOS,
		"regex set accepted an invalid result index"
	);
	testRegexSetErrorKind(XERR_RANGE, "regex set result range error mismatch");

	xrtClearError();
	testRequire(
		xrtRegexSetMatcherMatch(pMatcher, XRT_STR_LITERAL("a"), 2u) == XREGEX_ERROR,
		"regex set accepted an invalid start"
	);
	testRegexSetErrorKind(XERR_ARGUMENT, "regex set start error mismatch");
	xrtRegexSetMatcherFree(pMatcher);
	xrtRegexSetRelease(pSet);
}



/* 运行正则集合层全部契约测试。 */
int main(void)
{
	testRegexSetCreate();
	testRegexSetMatch();
	testRegexSetBitmapWords();
	testRegexSetEmpty();
	testRegexSetCompileError();
	testRegexSetErrors();
	printf("[PASS] regex set\n");
	return 0;
}

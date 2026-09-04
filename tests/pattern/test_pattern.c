#include "../test.h"



static bool testViewEqual(xstrview View, cstr sText)
{
	size_t iSize = strlen(sText);

	return (View.Size == iSize) &&
		((iSize == 0) || (memcmp(View.Data, sText, iSize) == 0));
}



/* 验证默认分隔符、严格空字段、顺序捕获与零分配单条入口。 */
static void testPatternExtract(void)
{
	xstrview arrCapture[3];
	size_t iCount = 0;

	testRequire(
		xrtPatternExtract(
			XRT_STR_LITERAL("/users/{id}"),
			XRT_STR_LITERAL("/users/42"),
			arrCapture,
			3u,
			&iCount
		) == XPATTERN_MATCH,
		"single pattern extraction failed"
	);
	testRequire(
		(iCount == 1u) && testViewEqual(arrCapture[0], "42"),
		"single pattern capture mismatch"
	);
	testRequire(
		xrtPatternExtract(
			XRT_STR_LITERAL("/users/{id}"),
			XRT_STR_LITERAL("/users//42"),
			arrCapture,
			3u,
			&iCount
		) == XPATTERN_NONE,
		"capture accepted an empty field"
	);
	testRequire(
		xrtPatternExtract(
			XRT_STR_LITERAL("/literal/{{name}}"),
			XRT_STR_LITERAL("/literal/{name}"),
			NULL,
			0,
			&iCount
		) == XPATTERN_MATCH,
		"escaped literal braces failed"
	);
	testRequire(iCount == 0, "literal brace pattern reported a capture");
}



/* 混合字段允许一个非空捕获，并保持转义、前缀与后缀的精确语义。 */
static void testPatternAffixExtract(void)
{
	xstrview Capture;
	size_t iCount = 0;

	testRequire(
		xrtPatternExtract(
			XRT_STR_LITERAL("/file/prefix{id}suffix"),
			XRT_STR_LITERAL("/file/prefix42suffix"),
			&Capture,
			1u,
			&iCount
		) == XPATTERN_MATCH && (iCount == 1u) &&
		testViewEqual(Capture, "42"),
		"prefix/suffix extraction failed"
	);
	testRequire(
		xrtPatternExtract(
			XRT_STR_LITERAL("/{id}140"),
			XRT_STR_LITERAL("/x140"),
			&Capture,
			1u,
			&iCount
		) == XPATTERN_MATCH && testViewEqual(Capture, "x"),
		"suffix-only extraction failed"
	);
	testRequire(
		xrtPatternExtract(
			XRT_STR_LITERAL("/{id}140"),
			XRT_STR_LITERAL("/140"),
			&Capture,
			1u,
			&iCount
		) == XPATTERN_NONE,
		"affix capture accepted an empty value"
	);
	testRequire(
		xrtPatternExtract(
			XRT_STR_LITERAL("/{{pre{id}}}"),
			XRT_STR_LITERAL("/{pre42}"),
			&Capture,
			1u,
			&iCount
		) == XPATTERN_MATCH && testViewEqual(Capture, "42"),
		"escaped affix braces failed"
	);
}



/* 验证任意单分隔符与小分隔符集合保持精确字节语义。 */
static void testPatternSeparators(void)
{
	xpatternconfig Config;
	xpattern* pPattern;
	xpatternmatch Match;
	xstrview arrCapture[3];
	size_t iCount;

	xrtPatternConfigInit(&Config);
	Config.Separators = XRT_STR_LITERAL(".");
	testRequire(
		xrtPatternExtractConfig(
			XRT_STR_LITERAL("{service}.{zone}.{tld}"),
			XRT_STR_LITERAL("api.example.com"),
			&Config,
			arrCapture,
			3u,
			&iCount
		) == XPATTERN_MATCH,
		"dot-separated extraction failed"
	);
	testRequire(
		(iCount == 3u) &&
		testViewEqual(arrCapture[0], "api") &&
		testViewEqual(arrCapture[1], "example") &&
		testViewEqual(arrCapture[2], "com"),
		"dot-separated captures mismatch"
	);
	pPattern = xrtPatternCompileConfig(
		XRT_STR_LITERAL("svc-{id}.example"),
		&Config
	);
	testRequire(pPattern != NULL, "dot-separated affix compile failed");
	testRequire(
		xrtPatternMatch(
			pPattern,
			XRT_STR_LITERAL("svc-api.example"),
			arrCapture,
			3u,
			&Match
		) == XPATTERN_MATCH && testViewEqual(arrCapture[0], "api"),
		"dot-separated affix match failed"
	);
	xrtPatternRelease(pPattern);

	Config.Separators = XRT_STR_LITERAL("/.");
	testRequire(
		xrtPatternExtractConfig(
			XRT_STR_LITERAL("/package/{name}.{ext}"),
			XRT_STR_LITERAL("/package/xrt.zip"),
			&Config,
			arrCapture,
			3u,
			&iCount
		) == XPATTERN_MATCH,
		"multiple-separator extraction failed"
	);
	testRequire(
		(iCount == 2u) && testViewEqual(arrCapture[0], "xrt") &&
		testViewEqual(arrCapture[1], "zip"),
		"multiple-separator captures mismatch"
	);
	testRequire(
		xrtPatternExtractConfig(
			XRT_STR_LITERAL("/package/{name}.{ext}"),
			XRT_STR_LITERAL("/package/xrt/tar.zip"),
			&Config,
			arrCapture,
			3u,
			&iCount
		) == XPATTERN_NONE,
		"separator bytes were treated as interchangeable"
	);
}



/* 验证尾捕获可以为空并且可以跨越全部普通分隔符。 */
static void testPatternTail(void)
{
	xpattern* pPattern = xrtPatternCompile(XRT_STR_LITERAL("/static/{*path}"));
	xpatternmatch Match;
	xstrview Capture;

	testRequire(pPattern != NULL, "tail pattern compile failed");
	testRequire(
		xrtPatternMatch(
			pPattern,
			XRT_STR_LITERAL("/static/css/app.css"),
			&Capture,
			1u,
			&Match
		) == XPATTERN_MATCH,
		"tail pattern match failed"
	);
	testRequire(
		(Match.CaptureCount == 1u) && testViewEqual(Capture, "css/app.css"),
		"tail capture mismatch"
	);
	testRequire(
		xrtPatternMatch(
			pPattern,
			XRT_STR_LITERAL("/static/"),
			&Capture,
			1u,
			&Match
		) == XPATTERN_MATCH,
		"empty tail capture failed"
	);
	testRequire(Capture.Size == 0, "empty tail capture was not empty");
	xrtPatternRelease(pPattern);
}



/* 验证确定化保留字面量失败后的参数候选，并选择最具体模式。 */
static void testPatternDeterminize(void)
{
	xpatternspec arrSpec[3];
	xpattern* pPattern;
	xpatternmatch Match;
	xstrview arrCapture[2];

	memset(arrSpec, 0, sizeof(arrSpec));
	arrSpec[0].Pattern = XRT_STR_LITERAL("/a/b/c");
	arrSpec[0].Value = (ptr)(uintptr_t)1u;
	arrSpec[1].Pattern = XRT_STR_LITERAL("/a/{x}/d");
	arrSpec[1].Value = (ptr)(uintptr_t)2u;
	arrSpec[2].Pattern = XRT_STR_LITERAL("/a/{x}/{y}");
	arrSpec[2].Value = (ptr)(uintptr_t)3u;
	pPattern = xrtPatternCompileMany(arrSpec, 3u);
	testRequire(pPattern != NULL, "determinized pattern compile failed");
	testRequire(
		xrtPatternMatch(
			pPattern,
			XRT_STR_LITERAL("/a/b/d"),
			arrCapture,
			2u,
			&Match
		) == XPATTERN_MATCH,
		"literal-to-parameter fallback match failed"
	);
	testRequire(
		(Match.Value == (ptr)(uintptr_t)2u) &&
		(Match.CaptureCount == 1u) && testViewEqual(arrCapture[0], "b"),
		"literal-to-parameter fallback selected the wrong pattern"
	);
	testRequire(
		xrtPatternLookup(
			pPattern,
			XRT_STR_LITERAL("/a/b/c"),
			&Match
		) == XPATTERN_MATCH,
		"specific literal lookup failed"
	);
	testRequire(
		Match.Value == (ptr)(uintptr_t)1u,
		"specific literal pattern did not outrank captures"
	);
	xrtPatternRelease(pPattern);
}



/* 局部字节 DFA 必须合并 literal/affix/capture 候选并保留后续回退。 */
static void testPatternAffixDeterminize(void)
{
	xpatternspec arrSpec[8];
	xpattern* pPattern;
	xpatternmatch Match;
	xstrview Capture;

	memset(arrSpec, 0, sizeof(arrSpec));
	arrSpec[0].Pattern = XRT_STR_LITERAL("/asset/exact.json/end");
	arrSpec[0].Value = (ptr)(uintptr_t)1u;
	arrSpec[1].Pattern = XRT_STR_LITERAL("/asset/file-{id}.json/end");
	arrSpec[1].Value = (ptr)(uintptr_t)2u;
	arrSpec[2].Pattern = XRT_STR_LITERAL("/asset/{name}/end");
	arrSpec[2].Value = (ptr)(uintptr_t)3u;
	arrSpec[3].Pattern = XRT_STR_LITERAL("/a/pre{x}s/c");
	arrSpec[3].Value = (ptr)(uintptr_t)4u;
	arrSpec[4].Pattern = XRT_STR_LITERAL("/a/{y}/d");
	arrSpec[4].Value = (ptr)(uintptr_t)5u;
	arrSpec[5].Pattern = XRT_STR_LITERAL("/b/foo/c");
	arrSpec[5].Value = (ptr)(uintptr_t)6u;
	arrSpec[6].Pattern = XRT_STR_LITERAL("/b/f{z}o/d");
	arrSpec[6].Value = (ptr)(uintptr_t)7u;
	arrSpec[7].Pattern = XRT_STR_LITERAL("/b/{any}/d");
	arrSpec[7].Value = (ptr)(uintptr_t)8u;
	pPattern = xrtPatternCompileMany(arrSpec, 8u);
	testRequire(pPattern != NULL, "affix matcher compile failed");
	testRequire(
		xrtPatternLookup(
			pPattern,
			XRT_STR_LITERAL("/asset/exact.json/end"),
			&Match
		) == XPATTERN_MATCH && Match.Value == (ptr)(uintptr_t)1u,
		"literal did not outrank an overlapping affix"
	);
	testRequire(
		xrtPatternMatch(
			pPattern,
			XRT_STR_LITERAL("/asset/file-42.json/end"),
			&Capture,
			1u,
			&Match
		) == XPATTERN_MATCH && Match.Value == (ptr)(uintptr_t)2u &&
		testViewEqual(Capture, "42"),
		"affix did not outrank a whole-field capture"
	);
	testRequire(
		xrtPatternMatch(
			pPattern,
			XRT_STR_LITERAL("/asset/readme.txt/end"),
			&Capture,
			1u,
			&Match
		) == XPATTERN_MATCH && Match.Value == (ptr)(uintptr_t)3u &&
		testViewEqual(Capture, "readme.txt"),
		"whole-field fallback after affix miss failed"
	);
	testRequire(
		xrtPatternMatch(
			pPattern,
			XRT_STR_LITERAL("/a/preVs/d"),
			&Capture,
			1u,
			&Match
		) == XPATTERN_MATCH && Match.Value == (ptr)(uintptr_t)5u &&
		testViewEqual(Capture, "preVs"),
		"capture fallback after a later affix branch miss failed"
	);
	testRequire(
		xrtPatternMatch(
			pPattern,
			XRT_STR_LITERAL("/b/foo/d"),
			&Capture,
			1u,
			&Match
		) == XPATTERN_MATCH && Match.Value == (ptr)(uintptr_t)7u &&
		testViewEqual(Capture, "o"),
		"literal transition did not retain its overlapping affix candidate"
	);
	xrtPatternRelease(pPattern);
}



/* 重叠后缀由固定字节数量决胜，不依赖注册数量或运行期线性扫描。 */
static void testPatternAffixSpecificity(void)
{
	xpatternspec arrSpec[3];
	xpattern* pPattern;
	xpatternmatch Match;
	xstrview Capture;

	memset(arrSpec, 0, sizeof(arrSpec));
	arrSpec[0].Pattern = XRT_STR_LITERAL("/x/{short}a");
	arrSpec[0].Value = (ptr)(uintptr_t)1u;
	arrSpec[1].Pattern = XRT_STR_LITERAL("/x/{long}aa");
	arrSpec[1].Value = (ptr)(uintptr_t)2u;
	arrSpec[2].Pattern = XRT_STR_LITERAL("/x/z{middle}aa");
	arrSpec[2].Value = (ptr)(uintptr_t)3u;
	pPattern = xrtPatternCompileMany(arrSpec, 3u);
	testRequire(pPattern != NULL, "overlapping affix compile failed");
	testRequire(
		xrtPatternMatch(
			pPattern,
			XRT_STR_LITERAL("/x/zvaa"),
			&Capture,
			1u,
			&Match
		) == XPATTERN_MATCH && Match.Value == (ptr)(uintptr_t)3u &&
		testViewEqual(Capture, "v"),
		"most-specific overlapping affix did not win"
	);
	xrtPatternRelease(pPattern);
}



/* 覆盖高扇出开放寻址分派，并确认模式数量不进入查询循环。 */
static void testPatternHighFanout(void)
{
	char arrText[32][48];
	xpatternspec arrSpec[32];
	xpattern* pPattern;
	xpatternmatch Match;
	xstrview Capture;

	memset(arrSpec, 0, sizeof(arrSpec));
	for ( size_t i = 0; i < 32u; i++ ) {
		int iWritten = snprintf(
			arrText[i],
			sizeof(arrText[i]),
			"/api/resource%zu/{id}",
			i
		);

		testRequire(iWritten > 0, "high-fanout fixture format failed");
		arrSpec[i].Pattern = (xstrview){ arrText[i], (size_t)iWritten };
		arrSpec[i].Value = (ptr)(uintptr_t)(i + 1u);
	}
	pPattern = xrtPatternCompileMany(arrSpec, 32u);
	testRequire(pPattern != NULL, "high-fanout matcher compile failed");
	testRequire(
		xrtPatternMatch(
			pPattern,
			XRT_STR_LITERAL("/api/resource31/9001"),
			&Capture,
			1u,
			&Match
		) == XPATTERN_MATCH,
		"high-fanout hash dispatch failed"
	);
	testRequire(
		(Match.Value == (ptr)(uintptr_t)32u) &&
		testViewEqual(Capture, "9001"),
		"high-fanout result mismatch"
	);
	xrtPatternRelease(pPattern);
}



/* 大量同前缀混合段必须共享局部 trie，而不是复制永久活跃通配状态。 */
static void testPatternAffixScale(void)
{
	char arrText[1000][56];
	xpatternspec arrSpec[1000];
	xpatternconfig Config;
	xpattern* pPattern;
	xpatternmatch Match;
	xstrview Capture;

	memset(arrSpec, 0, sizeof(arrSpec));
	for ( size_t i = 0; i < 1000u; i++ ) {
		int iWritten = snprintf(
			arrText[i],
			sizeof(arrText[i]),
			"/affix/prefix-{id}-suffix%zu",
			i
		);

		testRequire(iWritten > 0, "affix scale fixture format failed");
		arrSpec[i].Pattern = (xstrview){ arrText[i], (size_t)iWritten };
		arrSpec[i].Value = (ptr)(uintptr_t)(i + 1u);
	}
	xrtPatternConfigInit(&Config);
	Config.MaxCompiledBytes = 1024u * 1024u;
	pPattern = xrtPatternCompileManyConfig(arrSpec, 1000u, &Config);
	testRequire(pPattern != NULL, "shared affix trie scale compile failed");
	testRequire(
		xrtPatternMatch(
			pPattern,
			XRT_STR_LITERAL("/affix/prefix-value-suffix999"),
			&Capture,
			1u,
			&Match
		) == XPATTERN_MATCH && Match.Value == (ptr)(uintptr_t)1000u &&
		testViewEqual(Capture, "value"),
		"shared affix trie scale match failed"
	);
	xrtPatternRelease(pPattern);
}



/* 验证名称只作为冷元数据访问，值仍按顺序输出。 */
static void testPatternMetadata(void)
{
	xpattern* pPattern = xrtPatternCompile(
		XRT_STR_LITERAL("/repo/{owner}/{name}")
	);
	xpatternmatch Match;
	xstrview arrCapture[2];
	xstrview Name;

	testRequire(pPattern != NULL, "metadata pattern compile failed");
	testRequire(xrtPatternCount(pPattern) == 1u, "pattern count mismatch");
	testRequire(
		xrtPatternCaptureCount(pPattern, 0) == 2u &&
		xrtPatternMaxCaptureCount(pPattern) == 2u,
		"capture metadata count mismatch"
	);
	testRequire(
		xrtPatternCaptureName(pPattern, 0, 1u, &Name) &&
		testViewEqual(Name, "name"),
		"capture name lookup failed"
	);
	testRequire(
		xrtPatternCaptureIndex(
			pPattern,
			0,
			XRT_STR_LITERAL("owner")
		) == 0,
		"named capture index mismatch"
	);
	testRequire(
		xrtPatternMatch(
			pPattern,
			XRT_STR_LITERAL("/repo/openai/xrt"),
			arrCapture,
			2u,
			&Match
		) == XPATTERN_MATCH,
		"metadata fixture match failed"
	);
	testRequire(
		testViewEqual(arrCapture[0], "openai") &&
		testViewEqual(arrCapture[1], "xrt"),
		"ordered captures mismatch"
	);
	testRequire(xrtPatternCompiledBytes(pPattern) != 0, "compiled size is zero");
	xrtPatternRelease(pPattern);
}



/* 超过栈内字段边界缓存后必须正确退回冷回放路径。 */
static void testPatternFieldReplayFallback(void)
{
	xpattern* pPattern = xrtPatternCompile(XRT_STR_LITERAL(
		"/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/{id}"
	));
	xpatternmatch Match;
	xstrview Capture;

	testRequire(pPattern != NULL, "long-field fallback compile failed");
	testRequire(
		xrtPatternMatch(
			pPattern,
			XRT_STR_LITERAL(
				"/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/value"
			),
			&Capture,
			1u,
			&Match
		) == XPATTERN_MATCH && testViewEqual(Capture, "value"),
		"long-field capture replay fallback failed"
	);
	xrtPatternRelease(pPattern);
}



/* 验证无效语法、重复名称、容量与结构冲突都有稳定错误。 */
static void testPatternErrors(void)
{
	const xstrview arrInvalid[] = {
		XRT_STR_INIT("/{}"),
		XRT_STR_INIT("/{9name}"),
		XRT_STR_INIT("/{name}/{name}"),
		XRT_STR_INIT("/{a}-{b}"),
		XRT_STR_INIT("/{*rest}/tail"),
		XRT_STR_INIT("/prefix{*rest}"),
		XRT_STR_INIT("/literal/{brace")
	};
	xstrview Capture;
	size_t iCount;
	size_t iOffset;
	xpatternspec arrConflict[2];
	xpattern* pPattern;

	for ( size_t i = 0; i < sizeof(arrInvalid) / sizeof(arrInvalid[0]); i++ ) {
		xrtClearError();
		pPattern = xrtPatternCompile(arrInvalid[i]);
		testRequire(pPattern == NULL, "invalid pattern was accepted");
		testRequire(
			xrtGetError() != NULL &&
			xrtErrorCode(xrtGetError()) == XPATTERN_ERROR_PATTERN &&
			xrtPatternErrorOffset(xrtGetError(), &iOffset),
			"invalid pattern did not expose an offset"
		);
	}

	xrtClearError();
	testRequire(
		xrtPatternExtract(
			XRT_STR_LITERAL("/{a}/{b}"),
			XRT_STR_LITERAL("/x/y"),
			&Capture,
			1u,
			&iCount
		) == XPATTERN_ERROR,
		"insufficient capture capacity was accepted"
	);
	testRequire(
		(iCount == 2u) &&
		(xrtErrorCode(xrtGetError()) == XPATTERN_ERROR_CAPACITY),
		"capture capacity error metadata mismatch"
	);

	memset(arrConflict, 0, sizeof(arrConflict));
	arrConflict[0].Pattern = XRT_STR_LITERAL("/x/{a}");
	arrConflict[1].Pattern = XRT_STR_LITERAL("/x/{b}");
	xrtClearError();
	pPattern = xrtPatternCompileMany(arrConflict, 2u);
	testRequire(pPattern == NULL, "same-priority pattern conflict was accepted");
	testRequire(
		xrtErrorCode(xrtGetError()) == XPATTERN_ERROR_CONFLICT,
		"pattern conflict error code mismatch"
	);
	arrConflict[1].Priority = 1;
	arrConflict[1].Value = (ptr)(uintptr_t)2u;
	pPattern = xrtPatternCompileMany(arrConflict, 2u);
	testRequire(pPattern != NULL, "priority did not resolve structural conflict");
	{
		xpatternmatch Match;

		testRequire(
			xrtPatternLookup(pPattern, XRT_STR_LITERAL("/x/value"), &Match) ==
				XPATTERN_MATCH && Match.Value == (ptr)(uintptr_t)2u,
			"higher-priority duplicate did not win"
		);
	}
	xrtPatternRelease(pPattern);

	arrConflict[0].Pattern = XRT_STR_LITERAL("/x/pre{a}suf");
	arrConflict[0].Priority = 0;
	arrConflict[1].Pattern = XRT_STR_LITERAL("/x/pre{b}suf");
	arrConflict[1].Priority = 0;
	xrtClearError();
	pPattern = xrtPatternCompileMany(arrConflict, 2u);
	testRequire(
		pPattern == NULL &&
		xrtErrorCode(xrtGetError()) == XPATTERN_ERROR_CONFLICT,
		"indistinguishable affix patterns did not conflict"
	);
}



/* 空集合仍然是可执行、不可变对象。 */
static void testPatternEmpty(void)
{
	xpattern* pPattern = xrtPatternCompileMany(NULL, 0);

	testRequire(pPattern != NULL, "empty pattern set compile failed");
	testRequire(xrtPatternCount(pPattern) == 0, "empty pattern count mismatch");
	testRequire(
		xrtPatternTest(pPattern, XRT_STR_LITERAL("anything")) == XPATTERN_NONE,
		"empty matcher unexpectedly matched"
	);
	xrtPatternRelease(pPattern);
}



/* 资源预算必须在分配或状态膨胀前给出可识别失败。 */
static void testPatternLimits(void)
{
	xpatternconfig Config;
	xpatternspec arrSpec[2];
	xpattern* pPattern;
	char arrLong[901];

	memset(arrSpec, 0, sizeof(arrSpec));
	arrSpec[0].Pattern = XRT_STR_LITERAL("/a");
	arrSpec[1].Pattern = XRT_STR_LITERAL("/b");
	xrtPatternConfigInit(&Config);
	Config.MaxPatterns = 1u;
	pPattern = xrtPatternCompileManyConfig(arrSpec, 2u, &Config);
	testRequire(
		pPattern == NULL &&
		xrtErrorCode(xrtGetError()) == XPATTERN_ERROR_LIMIT,
		"pattern count budget was not enforced"
	);

	xrtPatternConfigInit(&Config);
	Config.MaxCaptures = 1u;
	pPattern = xrtPatternCompileConfig(
		XRT_STR_LITERAL("/{a}/{b}"),
		&Config
	);
	testRequire(
		pPattern == NULL &&
		xrtErrorCode(xrtGetError()) == XPATTERN_ERROR_LIMIT,
		"capture budget was not enforced"
	);

	xrtPatternConfigInit(&Config);
	Config.MaxStates = 1u;
	pPattern = xrtPatternCompileConfig(XRT_STR_LITERAL("/a"), &Config);
	testRequire(
		pPattern == NULL &&
		xrtErrorCode(xrtGetError()) == XPATTERN_ERROR_LIMIT,
		"state budget was not enforced"
	);

	xrtPatternConfigInit(&Config);
	Config.MaxStates = 2u;
	pPattern = xrtPatternCompileConfig(
		XRT_STR_LITERAL("prefix{id}suffix"),
		&Config
	);
	testRequire(
		pPattern == NULL &&
		xrtErrorCode(xrtGetError()) == XPATTERN_ERROR_LIMIT,
		"affix local state budget was not enforced"
	);

	memset(arrLong, 'a', sizeof(arrLong) - 1u);
	arrLong[sizeof(arrLong) - 1u] = 0;
	xrtPatternConfigInit(&Config);
	Config.MaxCompiledBytes = 1024u;
	pPattern = xrtPatternCompileConfig(
		(xstrview){ arrLong, sizeof(arrLong) - 1u },
		&Config
	);
	testRequire(
		pPattern == NULL &&
		xrtErrorCode(xrtGetError()) == XPATTERN_ERROR_LIMIT,
		"compiled byte budget was not enforced"
	);
}



int main(void)
{
	testPatternExtract();
	testPatternAffixExtract();
	testPatternSeparators();
	testPatternTail();
	testPatternDeterminize();
	testPatternAffixDeterminize();
	testPatternAffixSpecificity();
	testPatternHighFanout();
	testPatternAffixScale();
	testPatternMetadata();
	testPatternFieldReplayFallback();
	testPatternErrors();
	testPatternEmpty();
	testPatternLimits();
	printf("[PASS] pattern\n");
	return 0;
}

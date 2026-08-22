#include "../test.h"



/* 验证搜索、非零起点和指定位置语义。 */
static void testRegexFind(void)
{
	xregex* pRegex = xrtRegexCompile(XRT_STR_LITERAL("(?<value>a|ab)"));
	xregexmatcher* pMatcher;
	xregexcapture Capture;

	testRequire(pRegex != NULL, "regex find pattern compile failed");
	pMatcher = xrtRegexMatcherCreate(pRegex);
	testRequire(pMatcher != NULL, "regex matcher create failed");
	testRequire(
		xrtRegexMatcherFind(pMatcher, XRT_STR_LITERAL("a ab"), 2u) == XREGEX_MATCH,
		"regex find from nonzero position failed"
	);
	testRequire(
		xrtRegexMatcherCaptureNamed(pMatcher, XRT_STR_LITERAL("value"), &Capture) &&
		Capture.Matched &&
		(Capture.Span.Begin == 2u) &&
		(Capture.Span.End == 3u) &&
		(Capture.Text.Size == 1u) &&
		(Capture.Text.Data[0] == 'a'),
		"regex nonzero capture position mismatch"
	);
	testRequire(
		xrtRegexMatcherAt(pMatcher, XRT_STR_LITERAL("xxa"), 0u) == XREGEX_NONE,
		"regex at accepted a later match"
	);
	testRequire(
		xrtRegexMatcherAt(pMatcher, XRT_STR_LITERAL("xxa"), 2u) == XREGEX_MATCH,
		"regex at rejected an exact-position match"
	);
	xrtRegexMatcherFree(pMatcher);
	xrtRegexRelease(pRegex);
}



/* 验证 full match 不会被较短的高优先分支截断。 */
static void testRegexFull(void)
{
	xregex* pRegex = xrtRegexCompile(XRT_STR_LITERAL("(a|ab)"));
	xregexmatcher* pMatcher;
	xregexcapture Capture;

	testRequire(pRegex != NULL, "regex full pattern compile failed");
	pMatcher = xrtRegexMatcherCreate(pRegex);
	testRequire(pMatcher != NULL, "regex full matcher create failed");
	testRequire(
		xrtRegexMatcherFull(pMatcher, XRT_STR_LITERAL("ab")) == XREGEX_MATCH,
		"regex full match did not try the complete alternative"
	);
	testRequire(
		xrtRegexMatcherCapture(pMatcher, 0, &Capture) &&
		(Capture.Span.Begin == 0) &&
		(Capture.Span.End == 2u),
		"regex full match span mismatch"
	);
	testRequire(
		xrtRegexMatcherFull(pMatcher, XRT_STR_LITERAL("aba")) == XREGEX_NONE,
		"regex full match accepted trailing input"
	);
	xrtRegexMatcherFree(pMatcher);
	xrtRegexRelease(pRegex);
}



/* 验证未参与捕获与空捕获保持可区分。 */
static void testRegexOptionalCapture(void)
{
	xregex* pRegex = xrtRegexCompile(XRT_STR_LITERAL("(a)?b()"));
	xregexmatcher* pMatcher;
	xregexcapture Missing;
	xregexcapture Empty;

	testRequire(pRegex != NULL, "optional capture pattern compile failed");
	pMatcher = xrtRegexMatcherCreate(pRegex);
	testRequire(pMatcher != NULL, "optional capture matcher create failed");
	testRequire(
		xrtRegexMatcherFull(pMatcher, XRT_STR_LITERAL("b")) == XREGEX_MATCH,
		"optional capture full match failed"
	);
	testRequire(
		xrtRegexMatcherCapture(pMatcher, 1, &Missing) &&
		!Missing.Matched &&
		(Missing.Text.Data == NULL) &&
		(Missing.Text.Size == 0),
		"unmatched capture state mismatch"
	);
	testRequire(
		xrtRegexMatcherCapture(pMatcher, 2, &Empty) &&
		Empty.Matched &&
		(Empty.Span.Begin == Empty.Span.End) &&
		(Empty.Text.Size == 0),
		"empty capture state mismatch"
	);
	xrtRegexMatcherFree(pMatcher);
	xrtRegexRelease(pRegex);
}



/* 验证空匹配按 UTF-8 标量推进且不会在末尾循环。 */
static void testRegexNext(void)
{
	static const char arrText[] = { 'A', (char)0xC3, (char)0xA9 };
	xregex* pRegex = xrtRegexCompile(XRT_STR_LITERAL("(?:)"));
	xregexmatcher* pMatcher;
	xregexcapture Capture;

	testRequire(pRegex != NULL, "empty regex compile failed");
	pMatcher = xrtRegexMatcherCreate(pRegex);
	testRequire(pMatcher != NULL, "empty regex matcher create failed");
	testRequire(
		xrtRegexMatcherFind(pMatcher, (xstrview){ arrText, sizeof(arrText) }, 0) == XREGEX_MATCH,
		"initial empty regex match failed"
	);
	testRequire(xrtRegexMatcherCapture(pMatcher, 0, &Capture) && (Capture.Span.Begin == 0), "initial empty span mismatch");
	testRequire(xrtRegexMatcherNext(pMatcher) == XREGEX_MATCH, "second empty regex match failed");
	testRequire(xrtRegexMatcherCapture(pMatcher, 0, &Capture) && (Capture.Span.Begin == 1u), "ASCII empty-match advance mismatch");
	testRequire(xrtRegexMatcherNext(pMatcher) == XREGEX_MATCH, "terminal empty regex match failed");
	testRequire(xrtRegexMatcherCapture(pMatcher, 0, &Capture) && (Capture.Span.Begin == 3u), "UTF-8 empty-match advance mismatch");
	testRequire(xrtRegexMatcherNext(pMatcher) == XREGEX_NONE, "terminal empty regex match repeated");
	xrtRegexMatcherFree(pMatcher);
	xrtRegexRelease(pRegex);
}



/* 验证明示长度输入可包含零字节。 */
static void testRegexEmbeddedZero(void)
{
	const char arrText[] = { 'a', 0, 'b' };
	xregex* pRegex = xrtRegexCompile(XRT_STR_LITERAL("\\C"));
	xregexmatcher* pMatcher;
	xregexcapture Capture;

	testRequire(pRegex != NULL, "byte regex compile failed");
	pMatcher = xrtRegexMatcherCreate(pRegex);
	testRequire(pMatcher != NULL, "byte regex matcher create failed");
	testRequire(
		xrtRegexMatcherFind(pMatcher, (xstrview){ arrText, sizeof(arrText) }, 1u) == XREGEX_MATCH,
		"embedded-zero regex match failed"
	);
	testRequire(
		xrtRegexMatcherCapture(pMatcher, 0, &Capture) &&
		(Capture.Span.Begin == 1u) &&
		(Capture.Span.End == 2u) &&
		(Capture.Text.Size == 1u) &&
		(Capture.Text.Data[0] == 0),
		"embedded-zero regex capture mismatch"
	);
	xrtRegexMatcherFree(pMatcher);
	xrtRegexRelease(pRegex);
}



/* 验证无需持久 matcher 的常见便捷入口。 */
static void testRegexConvenience(void)
{
	testRequire(
		xrtRegexMatch(XRT_STR_LITERAL("\\d+"), XRT_STR_LITERAL("id=42")) == XREGEX_MATCH,
		"one-shot regex search failed"
	);
	testRequire(
		xrtRegexFullMatch(XRT_STR_LITERAL("\\d+"), XRT_STR_LITERAL("42")) == XREGEX_MATCH,
		"one-shot regex full match failed"
	);
	testRequire(
		xrtRegexFullMatch(XRT_STR_LITERAL("\\d+"), XRT_STR_LITERAL("id=42")) == XREGEX_NONE,
		"one-shot regex full match accepted extra input"
	);
}



/* 验证忽略大小写后收敛到同一范围的分支不会重复加入 DFA 状态。 */
static void testRegexCaseFoldAlternative(void)
{
	xregexconfig Config;
	xregex* pRegex;
	xregexmatcher* pMatcher;
	xregexcapture Capture;

	xrtRegexConfigInit(&Config);
	Config.Flags = XREGEX_IGNORE_CASE;
	pRegex = xrtRegexCompileConfig(XRT_STR_LITERAL("abc|ABC"), &Config);
	testRequire(pRegex != NULL, "case-fold alternative compile failed");
	testRequire(
		xrtRegexTest(pRegex, XRT_STR_LITERAL("ABC")) == XREGEX_MATCH,
		"case-fold alternative DFA match failed"
	);
	pMatcher = xrtRegexMatcherCreate(pRegex);
	testRequire(pMatcher != NULL, "case-fold alternative matcher create failed");
	testRequire(
		xrtRegexMatcherFull(pMatcher, XRT_STR_LITERAL("abc")) == XREGEX_MATCH,
		"case-fold alternative full match failed"
	);
	testRequire(
		xrtRegexMatcherCapture(pMatcher, 0u, &Capture) &&
		Capture.Matched &&
		(Capture.Span.Begin == 0u) &&
		(Capture.Span.End == 3u),
		"case-fold alternative capture mismatch"
	);
	xrtRegexMatcherFree(pMatcher);
	xrtRegexRelease(pRegex);
}



/* 运行 matcher 层全部契约测试。 */
int main(void)
{
	testRegexFind();
	testRegexFull();
	testRegexOptionalCapture();
	testRegexNext();
	testRegexEmbeddedZero();
	testRegexConvenience();
	testRegexCaseFoldAlternative();
	printf("[PASS] regex match\n");
	return 0;
}

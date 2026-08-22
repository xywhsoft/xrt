#include "../test.h"



/* 要求当前错误是稳定的替换模板错误。 */
static void testRegexReplacementError(size_t iExpectOffset, cstr sMessage)
{
	const xerror* pError = xrtGetError();
	size_t iOffset = XRT_NPOS;

	testRequire(pError != NULL, sMessage);
	testRequire(xrtErrorCode(pError) == XREGEX_ERROR_REPLACEMENT, sMessage);
	testRequire(xrtRegexErrorOffset(pError, &iOffset), sMessage);
	testRequire(iOffset == iExpectOffset, sMessage);
}



/* 验证数字捕获、命名捕获和美元转义。 */
static void testRegexReplaceTemplate(void)
{
	xregex* pRegex = xrtRegexCompile(
		XRT_STR_LITERAL("(?<key>[a-z])=(?<value>\\d)")
	);
	str sResult;

	testRequire(pRegex != NULL, "regex replacement pattern compile failed");
	sResult = xrtRegexReplace(
		pRegex,
		XRT_STR_LITERAL("a=1 b=2"),
		XRT_STR_LITERAL("${key}:$2=$$0")
	);
	testRequire(sResult != NULL, "regex template replacement failed");
	testRequire(
		strcmp(sResult, "a:1=$0 b:2=$0") == 0,
		"regex template replacement output mismatch"
	);
	xrtFree(sResult);
	xrtRegexRelease(pRegex);
}



/* 验证未参与捕获按空文本替换且整体捕获仍可引用。 */
static void testRegexReplaceOptional(void)
{
	xregex* pRegex = xrtRegexCompile(XRT_STR_LITERAL("(a)?b"));
	str sResult;

	testRequire(pRegex != NULL, "optional replacement pattern compile failed");
	sResult = xrtRegexReplace(
		pRegex,
		XRT_STR_LITERAL("b ab"),
		XRT_STR_LITERAL("<$1|$0>")
	);
	testRequire(sResult != NULL, "optional capture replacement failed");
	testRequire(
		strcmp(sResult, "<|b> <a|ab>") == 0,
		"optional capture replacement output mismatch"
	);
	xrtFree(sResult);
	xrtRegexRelease(pRegex);
}



/* 验证次数上限、首次替换和零上限语义。 */
static void testRegexReplaceLimit(void)
{
	xregex* pRegex = xrtRegexCompile(XRT_STR_LITERAL("\\d+"));
	xstrbuf Output;
	str sFirst;
	size_t iCount = 99u;

	testRequire(pRegex != NULL, "limited replacement pattern compile failed");
	xrtStrBufInit(&Output);
	testRequire(
		xrtRegexReplaceTo(
			pRegex,
			XRT_STR_LITERAL("1 2 3"),
			XRT_STR_LITERAL("x"),
			2u,
			&Output,
			&iCount
		),
		"limited regex replacement failed"
	);
	testRequire(
		(iCount == 2u) && (strcmp(Output.Data, "x x 3") == 0),
		"limited regex replacement mismatch"
	);
	xrtStrBufClear(&Output);
	testRequire(
		xrtRegexReplaceTo(
			pRegex,
			XRT_STR_LITERAL("1 2"),
			XRT_STR_LITERAL("x"),
			0,
			&Output,
			&iCount
		),
		"zero-limit regex replacement failed"
	);
	testRequire(
		(iCount == 0) && (strcmp(Output.Data, "1 2") == 0),
		"zero-limit regex replacement mismatch"
	);
	sFirst = xrtRegexReplaceFirst(
		pRegex,
		XRT_STR_LITERAL("1 2"),
		XRT_STR_LITERAL("x")
	);
	testRequire(
		(sFirst != NULL) && (strcmp(sFirst, "x 2") == 0),
		"first regex replacement mismatch"
	);
	xrtFree(sFirst);
	xrtStrBufFree(&Output);
	xrtRegexRelease(pRegex);
}



/* 验证空匹配按 UTF-8 标量推进并保留全部输入字节。 */
static void testRegexReplaceEmpty(void)
{
	static const char arrText[] = { 'A', (char)0xC3, (char)0xA9 };
	static const char arrExpect[] = {
		'_', 'A', '_', (char)0xC3, (char)0xA9, '_'
	};
	xregex* pRegex = xrtRegexCompile(XRT_STR_LITERAL("(?:)"));
	xstrbuf Output;

	testRequire(pRegex != NULL, "empty replacement pattern compile failed");
	xrtStrBufInit(&Output);
	testRequire(
		xrtRegexReplaceTo(
			pRegex,
			(xstrview){ arrText, sizeof(arrText) },
			XRT_STR_LITERAL("_"),
			SIZE_MAX,
			&Output,
			NULL
		),
		"empty regex replacement failed"
	);
	testRequire(
		(Output.Size == sizeof(arrExpect)) &&
		(memcmp(Output.Data, arrExpect, sizeof(arrExpect)) == 0),
		"empty regex replacement UTF-8 advance mismatch"
	);
	xrtStrBufFree(&Output);
	xrtRegexRelease(pRegex);
}



/* 验证明示长度替换可穿过嵌入零字节。 */
static void testRegexReplaceEmbeddedZero(void)
{
	static const char arrText[] = { 'a', 0, 'b' };
	static const char arrExpect[] = {
		'<', 'a', '>', '<', 0, '>', '<', 'b', '>'
	};
	xregex* pRegex = xrtRegexCompile(XRT_STR_LITERAL("\\C"));
	xstrbuf Output;

	testRequire(pRegex != NULL, "byte replacement pattern compile failed");
	xrtStrBufInit(&Output);
	testRequire(
		xrtRegexReplaceTo(
			pRegex,
			(xstrview){ arrText, sizeof(arrText) },
			XRT_STR_LITERAL("<$0>"),
			SIZE_MAX,
			&Output,
			NULL
		),
		"embedded-zero regex replacement failed"
	);
	testRequire(
		(Output.Size == sizeof(arrExpect)) &&
		(memcmp(Output.Data, arrExpect, sizeof(arrExpect)) == 0),
		"embedded-zero regex replacement mismatch"
	);
	xrtStrBufFree(&Output);
	xrtRegexRelease(pRegex);
}



/* 验证输入和模板都可借用输出当前内容且增长后仍稳定。 */
static void testRegexReplaceAlias(void)
{
	xregex* pRegex = xrtRegexCompile(XRT_STR_LITERAL("1"));
	xstrbuf Output;
	xstrview Text;
	xstrview Replacement;

	testRequire(pRegex != NULL, "alias replacement pattern compile failed");
	xrtStrBufInit(&Output);
	testRequire(
		xrtStrBufAppendRepeat(&Output, XRT_STR_LITERAL("1"), 80u),
		"alias replacement fixture append failed"
	);
	Text = xrtStrBufView(&Output);
	Replacement = (xstrview){ Output.Data, 1u };
	testRequire(
		xrtRegexReplaceTo(
			pRegex,
			Text,
			Replacement,
			SIZE_MAX,
			&Output,
			NULL
		),
		"self-aliasing regex replacement failed"
	);
	testRequire(Output.Size == 160u, "self-aliasing regex replacement size mismatch");
	for ( size_t i = 0; i < Output.Size; i++ ) {
		testRequire(Output.Data[i] == '1', "self-aliasing regex replacement data mismatch");
	}
	xrtStrBufFree(&Output);
	xrtRegexRelease(pRegex);
}



/* 回调把第一个捕获按 ASCII 大写追加到输出。 */
static bool testRegexReplaceUpper(
	const xregexmatcher* pMatcher,
	xstrbuf* pOutput,
	ptr pUserData
)
{
	xregexcapture Capture;

	(void)pUserData;
	if ( !xrtRegexMatcherCapture(pMatcher, 1u, &Capture) ) {
		return false;
	}
	for ( size_t i = 0; i < Capture.Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Capture.Text.Data[i];

		if ( (iByte >= (unsigned char)'a') && (iByte <= (unsigned char)'z') ) {
			iByte = (unsigned char)(iByte - ((unsigned char)'a' - (unsigned char)'A'));
		}
		if ( !xrtStrBufAppendByte(pOutput, (char)iByte) ) {
			return false;
		}
	}
	return true;
}



/* 回调在追加部分数据后主动失败，用于验证事务回滚。 */
static bool testRegexReplaceFail(
	const xregexmatcher* pMatcher,
	xstrbuf* pOutput,
	ptr pUserData
)
{
	(void)pMatcher;
	(void)pUserData;
	(void)xrtStrBufAppend(pOutput, XRT_STR_LITERAL("partial"));
	return false;
}



/* 验证回调替换和失败时的完整回滚。 */
static void testRegexReplaceCallback(void)
{
	xregex* pRegex = xrtRegexCompile(XRT_STR_LITERAL("([a-z]+)"));
	xstrbuf Output;
	size_t iCount = 0;

	testRequire(pRegex != NULL, "callback replacement pattern compile failed");
	xrtStrBufInit(&Output);
	testRequire(xrtStrBufAppend(&Output, XRT_STR_LITERAL("prefix:")), "callback prefix append failed");
	testRequire(
		xrtRegexReplaceFuncTo(
			pRegex,
			XRT_STR_LITERAL("ab cd"),
			SIZE_MAX,
			testRegexReplaceUpper,
			NULL,
			&Output,
			&iCount
		),
		"callback regex replacement failed"
	);
	testRequire(
		(iCount == 2u) && (strcmp(Output.Data, "prefix:AB CD") == 0),
		"callback regex replacement mismatch"
	);
	xrtStrBufClear(&Output);
	testRequire(xrtStrBufAppend(&Output, XRT_STR_LITERAL("stable")), "rollback prefix append failed");
	xrtClearError();
	testRequire(
		!xrtRegexReplaceFuncTo(
			pRegex,
			XRT_STR_LITERAL("ab"),
			SIZE_MAX,
			testRegexReplaceFail,
			NULL,
			&Output,
			NULL
		),
		"failing regex callback was accepted"
	);
	testRequire(strcmp(Output.Data, "stable") == 0, "regex callback failure did not roll back");
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XREGEX_ERROR_CALLBACK),
		"regex callback failure error mismatch"
	);
	xrtStrBufFree(&Output);
	xrtRegexRelease(pRegex);
}



/* 验证无效模板在执行前失败、定位准确且不修改输出或计数。 */
static void testRegexReplaceErrors(void)
{
	xregex* pRegex = xrtRegexCompile(XRT_STR_LITERAL("(a)"));
	xstrbuf Output;
	size_t iCount = 77u;

	testRequire(pRegex != NULL, "replacement error pattern compile failed");
	xrtStrBufInit(&Output);
	testRequire(xrtStrBufAppend(&Output, XRT_STR_LITERAL("stable")), "replacement error prefix failed");
	xrtClearError();
	testRequire(
		!xrtRegexReplaceTo(
			pRegex,
			XRT_STR_LITERAL("none"),
			XRT_STR_LITERAL("x$9"),
			SIZE_MAX,
			&Output,
			&iCount
		),
		"out-of-range replacement capture was accepted"
	);
	testRegexReplacementError(1u, "replacement capture range error mismatch");
	testRequire(
		(strcmp(Output.Data, "stable") == 0) && (iCount == 77u),
		"invalid replacement changed output or count"
	);
	xrtClearError();
	testRequire(
		!xrtRegexReplaceTo(
			pRegex,
			XRT_STR_LITERAL("a"),
			XRT_STR_LITERAL("${missing}"),
			SIZE_MAX,
			&Output,
			NULL
		),
		"missing named replacement capture was accepted"
	);
	testRegexReplacementError(0, "replacement capture name error mismatch");
	xrtClearError();
	testRequire(
		!xrtRegexReplaceTo(
			pRegex,
			XRT_STR_LITERAL("a"),
			XRT_STR_LITERAL("$"),
			SIZE_MAX,
			&Output,
			NULL
		),
		"incomplete replacement token was accepted"
	);
	testRegexReplacementError(0, "replacement incomplete token error mismatch");
	xrtStrBufFree(&Output);
	xrtRegexRelease(pRegex);
}



/* 运行正则替换层全部契约测试。 */
int main(void)
{
	testRegexReplaceTemplate();
	testRegexReplaceOptional();
	testRegexReplaceLimit();
	testRegexReplaceEmpty();
	testRegexReplaceEmbeddedZero();
	testRegexReplaceAlias();
	testRegexReplaceCallback();
	testRegexReplaceErrors();
	printf("[PASS] regex replace\n");
	return 0;
}

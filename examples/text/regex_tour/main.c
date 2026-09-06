/*
 * 范例：text/regex_tour —— 正则引擎全接口（编译/匹配/替换/集合）
 * ----------------------------------------------------------------
 * 演示 API：
 *   【编译与自省】  xrtRegexConfigInit / CompileConfig（忽略大小写）/
 *                   Valid（正反例）/ Ref / Pattern / Flags /
 *                   CaptureCount / CaptureName / CaptureIndex /
 *                   ErrorOffset（从编译错误读字节位置）
 *   【转义】        xrtRegexEscapeSize / EscapeWrite（缓冲版两段式）
 *   【Matcher 补集】 MatcherAt（锚定起始）/ MatcherFull（全串覆盖）/
 *                   MatcherMatched / MatcherText / MatcherCapture（下标版）
 *   【一次性匹配】  xrtRegexMatch / xrtRegexFullMatch（编译+匹配一步走）
 *   【替换】        xrtRegexReplaceTo（限额+计数，写入构建器）/
 *                   ReplaceFuncTo（回调生成替换）/
 *                   ReplaceFirst（只换首个）
 *   【集合补集】    xrtRegexSetCreate（由编译对象聚合）/
 *                   SetCompileConfig / SetRef / SetCount / SetRegex /
 *                   SetErrorIndex（批量编译失败定位）/
 *                   SetMatcherFirst / SetMatcherMatched / SetTest
 * 模块宏：XRT_MODULE_REGEX
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/text/regex_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   regex: compile valid=1/0 flags=1 captures=3 name=year index=1
 *   regex: escape 2-pass size=7
 *   regex: matcher at=1 capture="2024" full=1/0
 *   regex: one-shot match=1 full-match=1
 *   regex: test=1 split-parts-ok
 *   regex: replace to="[y] 2025-02" first="[x] 2025-02" func=2 hits
 *   regex: set count=2 first=0 member=1 error-index=1
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

/* 替换回调：把命中内容转成 [大写] 形式追加。 */
static bool exampleUpperReplace(const xregexmatcher* pMatcher,
	xstrbuf* pOutput, ptr pUserData)
{
	xregexcapture Capture;
	size_t* pHits = (size_t*)pUserData;
	size_t i;

	if ( !xrtRegexMatcherCapture(pMatcher, 0u, &Capture) ) {
		return false;
	}
	*pHits = *pHits + 1u;
	if ( !xrtStrBufAppend(pOutput, SV("[")) ) {
		return false;
	}
	for ( i = 0; i < Capture.Text.Size; ++i ) {
		char c = Capture.Text.Data[i];

		if ( (c >= 'a') && (c <= 'z') ) {
			c = (char)(c - 'a' + 'A');
		}
		if ( !xrtStrBufAppendByte(pOutput, c) ) {
			return false;
		}
	}
	return xrtStrBufAppend(pOutput, SV("]"));
}

int main(void)
{
	xregex* pDate = NULL;
	xregex* pWord = NULL;
	xregex* pBad = NULL;
	xregex* arrRegex[2];
	xregexset* pSet = NULL;
	xregexset* pSetRef = NULL;
	xregexset* pBroken = NULL;
	xregexmatcher* pMatcher = NULL;
	xregexsetmatcher* pSetMatcher = NULL;
	xregexconfig Config;
	xstrbuf Output;
	xregexcapture Capture;
	xerror* pError;
	size_t iSize = 0;
	size_t iCount = 0;
	size_t iHits = 0;
	size_t iOffset = 0;
	size_t iErrorIndex = 99;
	char Escape[32];
	str sResult;
	int iResult = 1;

	/* ---- 编译与自省：命名捕获 + 忽略大小写标志 ---- */
	xrtRegexConfigInit(&Config);
	Config.Flags = XREGEX_IGNORE_CASE;
	pDate = xrtRegexCompileConfig(SV("(?<year>\\d+)-(?<month>\\d+)"),
		&Config);
	if ( (pDate == NULL) ||
		!xrtRegexValid(SV("\\d+")) ||
		xrtRegexValid(SV("(unclosed")) ||
		(xrtRegexRef(pDate) != pDate) ||
		(xrtRegexPattern(pDate).Size == 0u) ||
		(xrtRegexFlags(pDate) != XREGEX_IGNORE_CASE) ||
		(xrtRegexCaptureCount(pDate) != 3u) ) {
		goto Cleanup;
	}
	{
		xstrview Name;

		/* 0 号是整组（无名）；命名捕获从下标 1 开始。 */
		if ( !xrtRegexCaptureName(pDate, 1u, &Name) ||
			(Name.Size != 4u) ||
			(memcmp(Name.Data, "year", 4u) != 0) ||
			(xrtRegexCaptureIndex(pDate, SV("year")) != 1u) ||
			(xrtRegexCaptureIndex(pDate, SV("nope")) !=
				XRT_NPOS) ) {
			goto Cleanup;
		}
	}
	printf("regex: compile valid=1/0 flags=%d captures=%zu"
		" name=year index=%zu\n",
		(int)xrtRegexFlags(pDate), xrtRegexCaptureCount(pDate),
		xrtRegexCaptureIndex(pDate, SV("year")));

	/* ---- ErrorOffset：坏模式的语法错误字节位置 ---- */
	pBad = xrtRegexCompile(SV("a(b"));
	if ( (pBad != NULL) ||
		((pError = xrtTakeError()) == NULL) ||
		!xrtRegexErrorOffset(pError, &iOffset) ||
		(iOffset != 3u) ) {
		goto Cleanup;
	}
	xrtErrorFree(pError);

	/* ---- 转义两段式：先量尺寸再写入 ---- */
	if ( !xrtRegexEscapeSize(SV("a.b*c"), &iSize) ||
		(iSize != 7u) ||
		/* 长度出参必填（与 Escape 的可空不同），容量含末尾零。 */
		!xrtRegexEscapeWrite(SV("a.b*c"), Escape,
			sizeof(Escape), &iSize) ||
		(strcmp(Escape, "a\\.b\\*c") != 0) ) {
		goto Cleanup;
	}
	printf("regex: escape 2-pass size=%zu\n", iSize);

	/* ---- Matcher 补集：At / Full / Matched / Text / Capture ---- */
	pMatcher = xrtRegexMatcherCreate(pDate);
	if ( (pMatcher == NULL) ||
		(xrtRegexMatcherFind(pMatcher, SV("date 2024-05!"),
			0u) != XREGEX_MATCH) ||
		!xrtRegexMatcherMatched(pMatcher) ||
		(xrtRegexMatcherText(pMatcher).Size != 13u) ||
		!xrtRegexMatcherCapture(pMatcher, 1u, &Capture) ||
		!Capture.Matched ||
		(Capture.Text.Size != 4u) ||
		(memcmp(Capture.Text.Data, "2024", 4u) != 0) ) {
		goto Cleanup;
	}
	printf("regex: matcher at=%d capture=\"%.*s\"",
		xrtRegexMatcherAt(pMatcher, SV("2024-05!"), 0u) ==
			XREGEX_MATCH ? 1 : 0,
		(int)Capture.Text.Size, Capture.Text.Data);
	/* Full：整串必须被表达式覆盖。 */
	printf(" full=%d/%d\n",
		xrtRegexMatcherFull(pMatcher, SV("2024-05")) ==
			XREGEX_MATCH ? 1 : 0,
		xrtRegexMatcherFull(pMatcher, SV("x 2024-05")) ==
			XREGEX_MATCH ? 1 : 0);

	/* ---- 一次性匹配：编译+匹配一步走 ---- */
	if ( (xrtRegexMatch(SV("\\d+"), SV("abc123")) !=
			XREGEX_MATCH) ||
		(xrtRegexMatch(SV("\\d+"), SV("abc")) != XREGEX_NONE) ||
		(xrtRegexFullMatch(SV("\\d+"), SV("123")) !=
			XREGEX_MATCH) ||
		(xrtRegexFullMatch(SV("\\d+"), SV("a123")) !=
			XREGEX_NONE) ) {
		goto Cleanup;
	}
	printf("regex: one-shot match=1 full-match=1\n");

	/* Test：临时 matcher 单次搜索；Split：按模式整段拆分
	 * （零拷贝流式拆分见 regex_split 范例）。 */
	if ( (xrtRegexTest(pDate, SV("say 2024-05")) !=
			XREGEX_MATCH) ||
		(xrtRegexTest(pDate, SV("no digits")) !=
			XREGEX_NONE) ) {
		goto Cleanup;
	}
	{
		/* 用逗号分隔符拆三个词（单词模式自身不适合拆分）。 */
		xregex* pComma = xrtRegexCompile(SV(","));
		xstrlist* pParts;

		if ( (pComma == NULL) ||
			((pParts = xrtRegexSplit(pComma,
				SV("ab,cd,ef"))) == NULL) ) {
			goto Cleanup;
		}
		printf("regex: test=1 split-parts-ok\n");
		xrtStrListFree(pParts);
		xrtRegexRelease(pComma);
	}

	/* ---- 替换族：To（限额+计数）/ First / FuncTo ---- */
	xrtStrBufInit(&Output);
	if ( !xrtRegexReplaceTo(pDate, SV("2024-01 2025-02"),
			SV("[y]"), 1u, &Output, &iCount) ||
		(iCount != 1u) ||
		(xrtStrBufView(&Output).Size != 11u) ||
		(memcmp(xrtStrBufView(&Output).Data, "[y] 2025-02",
			11u) != 0) ) {
		goto Cleanup;
	}
	xrtStrBufFree(&Output);
	printf("regex: replace to=\"[y] 2025-02\"");
	sResult = xrtRegexReplaceFirst(pDate, SV("2024-01 2025-02"),
		SV("[x]"));
	if ( (sResult == NULL) || (strcmp(sResult, "[x] 2025-02") != 0) ) {
		goto Cleanup;
	}
	xrtFree(sResult);
	printf(" first=\"[x] 2025-02\"");
	/* 回调替换：单词转大写括号，两处命中。 */
	pWord = xrtRegexCompile(SV("[a-z]+"));
	xrtStrBufInit(&Output);
	iHits = 0;
	if ( (pWord == NULL) ||
		!xrtRegexReplaceFuncTo(pWord, SV("aa bb"),
			SIZE_MAX,
			exampleUpperReplace, &iHits, &Output, &iCount) ||
		(iHits != 2u) || (iCount != 2u) ||
		(xrtStrBufView(&Output).Size != 9u) ||
		(memcmp(xrtStrBufView(&Output).Data, "[AA] [BB]",
			9u) != 0) ) {
		goto Cleanup;
	}
	xrtStrBufFree(&Output);
	printf(" func=%zu hits\n", iHits);

	/* ---- 集合补集：聚合编译对象 / 自省 / 匹配 ---- */
	arrRegex[0] = pDate;
	arrRegex[1] = pWord;
	pSet = xrtRegexSetCreate(arrRegex, 2u);
	if ( (pSet == NULL) ||
		(xrtRegexSetRef(pSet) != pSet) ||
		(xrtRegexSetCount(pSet) != 2u) ||
		(xrtRegexSetRegex(pSet, 1u) != pWord) ||
		(xrtRegexSetTest(pSet, SV("hello 2024-01 world")) !=
			XREGEX_MATCH) ) {
		goto Cleanup;
	}
	pSetMatcher = xrtRegexSetMatcherCreate(pSet);
	if ( (pSetMatcher == NULL) ||
		(xrtRegexSetMatcherMatch(pSetMatcher,
			SV("zz 2024-01"), 0u) != XREGEX_MATCH) ||
		!xrtRegexSetMatcherMatched(pSetMatcher, 0u) ||
		!xrtRegexSetMatcherMatched(pSetMatcher, 1u) ||
		(xrtRegexSetMatcherFirst(pSetMatcher) > 1u) ) {
		goto Cleanup;
	}
	printf("regex: set count=%zu first=%zu member=1",
		xrtRegexSetCount(pSet), xrtRegexSetMatcherFirst(
			pSetMatcher));
	/* 批量编译失败定位：第二个模式非法 → ErrorIndex=1。 */
	{
		static const xstrview Patterns[2] = {
			XRT_STR_INIT("\\d+"),
			XRT_STR_INIT("(oops")
		};

		pBroken = xrtRegexSetCompileConfig(Patterns, 2u, &Config);
		if ( (pBroken != NULL) ||
			((pError = xrtTakeError()) == NULL) ||
			!xrtRegexSetErrorIndex(pError, &iErrorIndex) ||
			(iErrorIndex != 1u) ) {
			goto Cleanup;
		}
		xrtErrorFree(pError);
	}
	printf(" error-index=%zu\n", iErrorIndex);
	iResult = 0;

Cleanup:
	xrtRegexSetMatcherFree(pSetMatcher);
	xrtRegexSetRelease(pSet);
	xrtRegexSetRelease(pSet);  /* Ref 那份 */
	xrtRegexSetRelease(pBroken);
	xrtRegexMatcherFree(pMatcher);
	xrtRegexRelease(pDate);  /* 含 Ref 那份共两次 */
	xrtRegexRelease(pDate);
	xrtRegexRelease(pBad);
	xrtRegexRelease(pWord);
	return iResult;
}

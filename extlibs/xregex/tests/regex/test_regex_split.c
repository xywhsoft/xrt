#include "../test.h"



/* 要求拆分部分具有指定文本、捕获索引和参与状态。 */
static void testRegexSplitPart(
	const xregexsplitpart* pPart,
	xstrview Text,
	size_t iCapture,
	bool bMatched,
	cstr sMessage
)
{
	testRequire(pPart->Capture == iCapture, sMessage);
	testRequire(pPart->Matched == bMatched, sMessage);
	testRequire(pPart->Text.Size == Text.Size, sMessage);
	testRequire(
		(Text.Size == 0) || (memcmp(pPart->Text.Data, Text.Data, Text.Size) == 0),
		sMessage
	);
}



/* 验证默认便捷入口保留首尾空字段并返回独立副本。 */
static void testRegexSplitList(void)
{
	xregex* pRegex = xrtRegexCompile(XRT_STR_LITERAL(","));
	xstrlist* pList;
	const char* arrExpect[] = { "", "a", "", "b", "" };

	testRequire(pRegex != NULL, "regex split list pattern compile failed");
	pList = xrtRegexSplit(pRegex, XRT_STR_LITERAL(",a,,b,"));
	xrtRegexRelease(pRegex);
	testRequire(pList != NULL, "regex split list failed");
	testRequire(pList->Count == 5u, "regex split list count mismatch");
	for ( size_t i = 0; i < pList->Count; i++ ) {
		testRequire(
			(pList->Items[i].Size == strlen(arrExpect[i])) &&
			(strcmp(pList->Items[i].Data, arrExpect[i]) == 0),
			"regex split list item mismatch"
		);
	}
	xrtStrListFree(pList);
}



/* 验证捕获按组号紧随字段输出，并保留未参与状态。 */
static void testRegexSplitCaptures(void)
{
	xregex* pRegex = xrtRegexCompile(XRT_STR_LITERAL("([,;])|(:)"));
	xregexsplitconfig Config;
	xregexsplitter* pSplitter;
	xregexsplitpart Part;
	xregexresult Result;

	testRequire(pRegex != NULL, "regex split capture pattern compile failed");
	xrtRegexSplitConfigInit(&Config);
	Config.Flags = XREGEX_SPLIT_CAPTURES;
	pSplitter = xrtRegexSplitterCreate(
		pRegex,
		XRT_STR_LITERAL("a,b:c"),
		&Config
	);
	xrtRegexRelease(pRegex);
	testRequire(pSplitter != NULL, "regex capture splitter create failed");

	Result = xrtRegexSplitterNext(pSplitter, &Part);
	testRequire(Result == XREGEX_MATCH, "regex split first field missing");
	testRegexSplitPart(&Part, XRT_STR_LITERAL("a"), XRT_NPOS, true, "regex split first field mismatch");
	Result = xrtRegexSplitterNext(pSplitter, &Part);
	testRequire(Result == XREGEX_MATCH, "regex split first capture missing");
	testRegexSplitPart(&Part, XRT_STR_LITERAL(","), 1u, true, "regex split first capture mismatch");
	Result = xrtRegexSplitterNext(pSplitter, &Part);
	testRequire(Result == XREGEX_MATCH, "regex split unmatched capture missing");
	testRequire(
		(Part.Capture == 2u) && !Part.Matched &&
		(Part.Text.Data == NULL) && (Part.Text.Size == 0),
		"regex split unmatched capture mismatch"
	);
	Result = xrtRegexSplitterNext(pSplitter, &Part);
	testRequire(Result == XREGEX_MATCH, "regex split middle field missing");
	testRegexSplitPart(&Part, XRT_STR_LITERAL("b"), XRT_NPOS, true, "regex split middle field mismatch");
	Result = xrtRegexSplitterNext(pSplitter, &Part);
	testRequire(Result == XREGEX_MATCH, "regex split second unmatched capture missing");
	testRequire((Part.Capture == 1u) && !Part.Matched, "regex split second unmatched capture mismatch");
	Result = xrtRegexSplitterNext(pSplitter, &Part);
	testRequire(Result == XREGEX_MATCH, "regex split second capture missing");
	testRegexSplitPart(&Part, XRT_STR_LITERAL(":"), 2u, true, "regex split second capture mismatch");
	Result = xrtRegexSplitterNext(pSplitter, &Part);
	testRequire(Result == XREGEX_MATCH, "regex split tail field missing");
	testRegexSplitPart(&Part, XRT_STR_LITERAL("c"), XRT_NPOS, true, "regex split tail field mismatch");
	testRequire(
		xrtRegexSplitterNext(pSplitter, &Part) == XREGEX_NONE,
		"regex splitter did not finish"
	);
	xrtRegexSplitterFree(pSplitter);
}



/* 验证分隔上限和空字段过滤语义。 */
static void testRegexSplitConfig(void)
{
	xregex* pRegex = xrtRegexCompile(XRT_STR_LITERAL(","));
	xregexsplitconfig Config;
	xregexsplitter* pSplitter;
	xregexsplitpart Part;

	testRequire(pRegex != NULL, "configured split pattern compile failed");
	xrtRegexSplitConfigInit(&Config);
	Config.Limit = 1u;
	pSplitter = xrtRegexSplitterCreate(pRegex, XRT_STR_LITERAL("a,b,c"), &Config);
	testRequire(pSplitter != NULL, "limited splitter create failed");
	testRequire(xrtRegexSplitterNext(pSplitter, &Part) == XREGEX_MATCH, "limited split first field missing");
	testRegexSplitPart(&Part, XRT_STR_LITERAL("a"), XRT_NPOS, true, "limited split first field mismatch");
	testRequire(xrtRegexSplitterNext(pSplitter, &Part) == XREGEX_MATCH, "limited split tail missing");
	testRegexSplitPart(&Part, XRT_STR_LITERAL("b,c"), XRT_NPOS, true, "limited split tail mismatch");
	testRequire(xrtRegexSplitterNext(pSplitter, &Part) == XREGEX_NONE, "limited splitter did not finish");
	xrtRegexSplitterFree(pSplitter);

	xrtRegexSplitConfigInit(&Config);
	Config.Limit = 0;
	pSplitter = xrtRegexSplitterCreate(pRegex, XRT_STR_LITERAL("a,b"), &Config);
	testRequire(pSplitter != NULL, "zero-limit splitter create failed");
	testRequire(xrtRegexSplitterNext(pSplitter, &Part) == XREGEX_MATCH, "zero-limit split field missing");
	testRegexSplitPart(&Part, XRT_STR_LITERAL("a,b"), XRT_NPOS, true, "zero-limit split mismatch");
	testRequire(xrtRegexSplitterNext(pSplitter, &Part) == XREGEX_NONE, "zero-limit splitter did not finish");
	xrtRegexSplitterFree(pSplitter);

	xrtRegexSplitConfigInit(&Config);
	Config.Flags = XREGEX_SPLIT_SKIP_EMPTY;
	pSplitter = xrtRegexSplitterCreate(pRegex, XRT_STR_LITERAL(",a,,"), &Config);
	xrtRegexRelease(pRegex);
	testRequire(pSplitter != NULL, "skip-empty splitter create failed");
	testRequire(xrtRegexSplitterNext(pSplitter, &Part) == XREGEX_MATCH, "skip-empty split field missing");
	testRegexSplitPart(&Part, XRT_STR_LITERAL("a"), XRT_NPOS, true, "skip-empty split mismatch");
	testRequire(xrtRegexSplitterNext(pSplitter, &Part) == XREGEX_NONE, "skip-empty splitter retained empty fields");
	xrtRegexSplitterFree(pSplitter);
}



/* 验证空分隔模式按 UTF-8 标量推进且只产生一个尾字段。 */
static void testRegexSplitEmpty(void)
{
	static const char arrText[] = { 'A', (char)0xC3, (char)0xA9 };
	xregex* pRegex = xrtRegexCompile(XRT_STR_LITERAL("(?:)"));
	xstrlist* pList;

	testRequire(pRegex != NULL, "empty split pattern compile failed");
	pList = xrtRegexSplit(pRegex, (xstrview){ arrText, sizeof(arrText) });
	xrtRegexRelease(pRegex);
	testRequire(pList != NULL, "empty regex split failed");
	testRequire(pList->Count == 4u, "empty regex split count mismatch");
	testRequire(pList->Items[0].Size == 0, "empty regex split leading field mismatch");
	testRequire(
		(pList->Items[1].Size == 1u) && (pList->Items[1].Data[0] == 'A'),
		"empty regex split ASCII field mismatch"
	);
	testRequire(
		(pList->Items[2].Size == 2u) &&
		(memcmp(pList->Items[2].Data, arrText + 1, 2u) == 0),
		"empty regex split UTF-8 field mismatch"
	);
	testRequire(pList->Items[3].Size == 0, "empty regex split tail field mismatch");
	xrtStrListFree(pList);
}



/* 验证明示长度拆分可识别嵌入零字节。 */
static void testRegexSplitEmbeddedZero(void)
{
	static const char arrText[] = { 'a', 0, 'b' };
	xregex* pRegex = xrtRegexCompile(XRT_STR_LITERAL("\\x00"));
	xstrlist* pList;

	testRequire(pRegex != NULL, "zero-byte split pattern compile failed");
	pList = xrtRegexSplit(pRegex, (xstrview){ arrText, sizeof(arrText) });
	xrtRegexRelease(pRegex);
	testRequire(pList != NULL, "embedded-zero regex split failed");
	testRequire(
		(pList->Count == 2u) &&
		(pList->Items[0].Size == 1u) && (pList->Items[0].Data[0] == 'a') &&
		(pList->Items[1].Size == 1u) && (pList->Items[1].Data[0] == 'b'),
		"embedded-zero regex split mismatch"
	);
	xrtStrListFree(pList);
}



/* 验证配置与参数错误使用稳定种类和代码。 */
static void testRegexSplitErrors(void)
{
	xregex* pRegex = xrtRegexCompile(XRT_STR_LITERAL(","));
	xregexsplitconfig Config;
	xregexsplitter* pSplitter;

	testRequire(pRegex != NULL, "split error pattern compile failed");
	xrtRegexSplitConfigInit(&Config);
	Config.Reserved[0] = 1u;
	xrtClearError();
	pSplitter = xrtRegexSplitterCreate(pRegex, XRT_STR_LITERAL("a,b"), &Config);
	testRequire(pSplitter == NULL, "splitter accepted reserved config data");
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) == XREGEX_ERROR_CONFIG),
		"splitter config error mismatch"
	);
	xrtClearError();
	testRequire(
		xrtRegexSplitterNext(NULL, NULL) == XREGEX_ERROR,
		"splitter accepted null arguments"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "splitter argument error mismatch");
	xrtRegexRelease(pRegex);
}



/* 运行正则拆分层全部契约测试。 */
int main(void)
{
	testRegexSplitList();
	testRegexSplitCaptures();
	testRegexSplitConfig();
	testRegexSplitEmpty();
	testRegexSplitEmbeddedZero();
	testRegexSplitErrors();
	printf("[PASS] regex split\n");
	return 0;
}

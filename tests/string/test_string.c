#include "../test.h"



/* 判断视图与明确长度字节串完全相等。 */
static bool testViewEqual(xstrview View, cstr sText, size_t iSize)
{
	return (View.Size == iSize) && ((iSize == 0) || (memcmp(View.Data, sText, iSize) == 0));
}



/* 验证等长变换的调用方缓冲区、原地路径和失败原子性。 */
static void testStringTransforms(void)
{
	char arrOutput[32];
	char arrBefore[32];
	char arrText[32];

	testRequire(xrtStrReverseBytesTo(XRT_STR_LITERAL("abc"), arrOutput,
		sizeof(arrOutput)) && (strcmp(arrOutput, "cba") == 0),
		"byte reverse target mismatch");
	memcpy(arrText, "ab\0cd", 5);
	testRequire(xrtStrReverseBytesTo((xstrview){ arrText, 5 }, arrText,
		sizeof(arrText)) && (memcmp(arrText, "dc\0ba", 5) == 0) &&
		(arrText[5] == 0), "byte reverse in-place mismatch");

	testRequire(xrtStrLowerTo(XRT_STR_LITERAL("AbC你"), arrOutput,
		sizeof(arrOutput)) && (strcmp(arrOutput, "abc你") == 0),
		"ASCII lower target mismatch");
	memcpy(arrText, "aBcD", 5);
	testRequire(xrtStrUpperTo((xstrview){ arrText, 4 }, arrText,
		sizeof(arrText)) && (strcmp(arrText, "ABCD") == 0),
		"ASCII upper in-place mismatch");

	memset(arrOutput, 0xA5, sizeof(arrOutput));
	memcpy(arrBefore, arrOutput, sizeof(arrOutput));
	testRequire(!xrtStrLowerTo(XRT_STR_LITERAL("abcd"), arrOutput, 4) &&
		(memcmp(arrOutput, arrBefore, sizeof(arrOutput)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"short transform target changed output");
	xrtClearError();
	memcpy(arrText, "abcdef", 7);
	testRequire(!xrtStrUpperTo((xstrview){ arrText, 6 }, arrText + 1,
		sizeof(arrText) - 1u) && (strcmp(arrText, "abcdef") == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"partial transform overlap was accepted");
	xrtClearError();
}



/* 验证字节集合过滤的查询、分配、二进制和原地契约。 */
static void testStringFilter(void)
{
	static const char arrBinary[] = { 'a', 0, 'b', '1', 'a', 0 };
	static const char arrSet[] = { 'a', 0 };
	char arrOutput[32];
	char arrBefore[32];
	char arrText[32];
	size_t iSize = 99;
	str sResult;

	testRequire(xrtStrFilterTo((xstrview){ arrBinary, sizeof(arrBinary) },
		(xstrview){ arrSet, sizeof(arrSet) }, NULL, 0, &iSize) &&
		(iSize == 2), "byte filter query mismatch");
	testRequire(xrtStrFilterTo((xstrview){ arrBinary, sizeof(arrBinary) },
		(xstrview){ arrSet, sizeof(arrSet) }, arrOutput,
		sizeof(arrOutput), &iSize) && (iSize == 2) &&
		(memcmp(arrOutput, "b1", 2) == 0) && (arrOutput[2] == 0),
		"binary byte filter mismatch");

	memcpy(arrText, arrBinary, sizeof(arrBinary));
	testRequire(xrtStrFilterTo((xstrview){ arrText, sizeof(arrBinary) },
		(xstrview){ arrSet, sizeof(arrSet) }, arrText,
		sizeof(arrText), &iSize) && (iSize == 2) &&
		(memcmp(arrText, "b1", 3) == 0), "byte filter in-place mismatch");

	sResult = xrtStrFilter(XRT_STR_LITERAL("a1b2c3"),
		XRT_STR_LITERAL("123"));
	testRequire((sResult != NULL) && (strcmp(sResult, "abc") == 0),
		"allocated byte filter mismatch");
	xrtFree(sResult);
	sResult = xrtStrFilter(XRT_STR_LITERAL("aaa"), XRT_STR_LITERAL("a"));
	testRequire((sResult != NULL) && (sResult[0] == 0),
		"all-byte filter ownership mismatch");
	xrtFree(sResult);

	memset(arrOutput, 0xA5, sizeof(arrOutput));
	memcpy(arrBefore, arrOutput, sizeof(arrOutput));
	iSize = 99;
	testRequire(!xrtStrFilterTo(XRT_STR_LITERAL("a1b2"),
		XRT_STR_LITERAL("12"), arrOutput, 2, &iSize) &&
		(iSize == 2) && (memcmp(arrOutput, arrBefore, sizeof(arrOutput)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"short byte filter target changed output");
	xrtClearError();
}



/* 验证视图、查找、变换和所有权边界。 */
int main(void)
{
	static const char sBinary[] = { 'a', 0, 'b', 'a', 0, 'b' };
	static const char sUtf8[] = "A你Z";
	xstrview arrJoin[3];
	xstrview After;
	xstrview Before;
	xstrview Rest;
	xstrview View;
	str sResult;

	arrJoin[0] = xrtStrView("one");
	arrJoin[1] = xrtStrViewN(sBinary, 3);
	arrJoin[2] = xrtStrView("three");
	testRequire(xrtStrEmpty(xrtStrView(NULL)), "null text must be empty");
	testRequire(xrtStrEqual(xrtStrViewN(sBinary, 3), xrtStrViewN(sBinary + 3, 3)),
		"embedded null equality mismatch");
	testRequire(xrtStrCompare(XRT_STR_LITERAL("abc"), XRT_STR_LITERAL("abd")) < 0,
		"lexical comparison mismatch");
	testRequire(xrtStrCaseEqual(XRT_STR_LITERAL("AbC"), XRT_STR_LITERAL("aBc")),
		"ASCII case equality mismatch");
	testRequire(!xrtStrCaseEqual(xrtStrView(sUtf8), XRT_STR_LITERAL("a你x")),
		"ASCII case comparison changed non-ASCII bytes");

	testRequire(xrtStrFind(XRT_STR_LITERAL("0123456789-0123456789"), XRT_STR_LITERAL("3456789-0"), 0) == 3,
		"long pattern search mismatch");
	testRequire(xrtStrFind(XRT_STR_LITERAL("abcabc"), XRT_STR_LITERAL("abc"), 1) == 3,
		"search start mismatch");
	testRequire(xrtStrFind(XRT_STR_LITERAL("abc"), XRT_STR_LITERAL(""), 2) == 2,
		"empty pattern search mismatch");
	testRequire(xrtStrFind(XRT_STR_LITERAL("abc"), XRT_STR_LITERAL("a"), 4) == XRT_NPOS,
		"out-of-range search mismatch");
	testRequire(xrtStrCaseFind(XRT_STR_LITERAL("xx-AbCd-yy"), XRT_STR_LITERAL("aBcD"), 0) == 3,
		"case search mismatch");
	testRequire(xrtStrCaseFind(
		XRT_STR_LITERAL("prefix-0123456789-AbCdEfGhIj-tail"),
		XRT_STR_LITERAL("0123456789-aBcDeFgHiJ"), 0) == 7,
		"long case search mismatch");
	testRequire(xrtStrFindByte(xrtStrViewN(sBinary, sizeof(sBinary)), 0, 0) == 1,
		"binary byte search mismatch");
	testRequire(xrtStrFindAny(XRT_STR_LITERAL("path:name"), XRT_STR_LITERAL("\\/:*?"), 0) == 4,
		"byte-set search mismatch");
	testRequire(xrtStrFindAny(XRT_STR_LITERAL("plain"), XRT_STR_LITERAL(""), 0) == XRT_NPOS,
		"empty byte-set search mismatch");
	testRequire(xrtStrRFind(XRT_STR_LITERAL("ab-ab-ab"), XRT_STR_LITERAL("ab")) == 6,
		"reverse search mismatch");
	testRequire(xrtStrCaseRFind(XRT_STR_LITERAL("ab-AB-aB"), XRT_STR_LITERAL("Ab")) == 6,
		"case reverse search mismatch");
	testRequire(xrtStrCount(XRT_STR_LITERAL("aaaaa"), XRT_STR_LITERAL("aa")) == 2,
		"non-overlapping count mismatch");
	testRequire(xrtStrCaseCount(XRT_STR_LITERAL("aAaaA"), XRT_STR_LITERAL("AA")) == 2,
		"case non-overlapping count mismatch");
	testRequire(xrtStrContains(XRT_STR_LITERAL("hello"), XRT_STR_LITERAL("ell")),
		"contains mismatch");
	testRequire(xrtStrCaseContains(XRT_STR_LITERAL("hello"), XRT_STR_LITERAL("ELL")),
		"case contains mismatch");
	testRequire(xrtStrContainsAny(XRT_STR_LITERAL("path:name"), XRT_STR_LITERAL("\\/:*?")),
		"contains-any mismatch");
	testRequire(xrtStrStarts(XRT_STR_LITERAL("hello"), XRT_STR_LITERAL("he")),
		"starts mismatch");
	testRequire(xrtStrCaseStarts(XRT_STR_LITERAL("Hello"), XRT_STR_LITERAL("hE")),
		"case starts mismatch");
	testRequire(xrtStrEnds(XRT_STR_LITERAL("hello"), XRT_STR_LITERAL("lo")),
		"ends mismatch");
	testRequire(xrtStrCaseEnds(XRT_STR_LITERAL("Hello"), XRT_STR_LITERAL("LO")),
		"case ends mismatch");
	testRequire(xrtStrCaseEnds(xrtStrView(NULL), xrtStrView(NULL)),
		"empty case suffix mismatch");

	testRequire(xrtStrCut(XRT_STR_LITERAL("name=value=tail"), XRT_STR_LITERAL("="),
		&Before, &After), "cut did not find separator");
	testRequire(testViewEqual(Before, "name", 4) &&
		testViewEqual(After, "value=tail", 10), "cut result mismatch");
	testRequire(xrtStrRCut(XRT_STR_LITERAL("name=value=tail"), XRT_STR_LITERAL("="),
		&Before, &After), "reverse cut did not find separator");
	testRequire(testViewEqual(Before, "name=value", 10) &&
		testViewEqual(After, "tail", 4), "reverse cut result mismatch");
	testRequire(!xrtStrCut(XRT_STR_LITERAL("plain"), XRT_STR_LITERAL("="),
		&Before, &After) && testViewEqual(Before, "plain", 5) && (After.Size == 0),
		"cut miss contract mismatch");
	testRequire(xrtStrCutPrefix(XRT_STR_LITERAL("prefix-value"), XRT_STR_LITERAL("prefix-"),
		&Rest) && testViewEqual(Rest, "value", 5), "cut-prefix mismatch");
	testRequire(xrtStrCutSuffix(XRT_STR_LITERAL("value.txt"), XRT_STR_LITERAL(".txt"),
		&Rest) && testViewEqual(Rest, "value", 5), "cut-suffix mismatch");
	testRequire(!xrtStrCutPrefix(XRT_STR_LITERAL("value"), XRT_STR_LITERAL("x"),
		&Rest) && testViewEqual(Rest, "value", 5), "cut-prefix miss contract mismatch");

	View = xrtStrSlice(XRT_STR_LITERAL("abcdef"), 2, 3);
	testRequire(testViewEqual(View, "cde", 3), "slice mismatch");
	View = xrtStrSlice(XRT_STR_LITERAL("abcdef"), 20, XRT_NPOS);
	testRequire(View.Size == 0, "clamped slice mismatch");
	View = xrtStrTrim(XRT_STR_LITERAL(" \t\r\n text \v\f"));
	testRequire(testViewEqual(View, "text", 4), "ASCII trim mismatch");
	View = xrtStrTrimSet(XRT_STR_LITERAL("123abc321"), XRT_STR_LITERAL("123"));
	testRequire(testViewEqual(View, "abc", 3), "set trim mismatch");
	sResult = xrtStrDupView(View);
	testRequire((sResult != NULL) && (strcmp(sResult, "abc") == 0), "view duplicate mismatch");
	xrtFree(sResult);
	View = xrtStrTrimSet(XRT_STR_LITERAL("111"), XRT_STR_LITERAL("1"));
	testRequire(View.Size == 0, "all-content trim mismatch");
	testRequire(xrtStrBlank(XRT_STR_LITERAL(" \t\r\n\v\f")), "blank detection mismatch");
	testRequire(!xrtStrBlank(XRT_STR_LITERAL(" x ")), "non-blank detection mismatch");

	sResult = xrtStrDup(NULL);
	testRequire((sResult != NULL) && (sResult[0] == 0), "empty duplicate ownership mismatch");
	xrtFree(sResult);
	sResult = xrtStrConcat(XRT_STR_LITERAL("left"), XRT_STR_LITERAL("right"));
	testRequire((sResult != NULL) && (strcmp(sResult, "leftright") == 0), "concat mismatch");
	xrtFree(sResult);
	sResult = xrtStrJoin(XRT_STR_LITERAL("|"), arrJoin, 3);
	testRequire((sResult != NULL) && (memcmp(sResult, "one|a\0b|three", 13) == 0) && (sResult[13] == 0),
		"binary join mismatch");
	xrtFree(sResult);
	sResult = xrtStrRepeat(XRT_STR_LITERAL("ab"), 3);
	testRequire((sResult != NULL) && (strcmp(sResult, "ababab") == 0), "repeat mismatch");
	xrtFree(sResult);
	sResult = xrtStrRepeat(XRT_STR_LITERAL("ab"), 0);
	testRequire((sResult != NULL) && (sResult[0] == 0), "zero repeat ownership mismatch");
	xrtFree(sResult);
	sResult = xrtStrReplace(XRT_STR_LITERAL("a--b--c"), XRT_STR_LITERAL("--"), XRT_STR_LITERAL("/"));
	testRequire((sResult != NULL) && (strcmp(sResult, "a/b/c") == 0), "replace mismatch");
	xrtFree(sResult);
	sResult = xrtStrInsert(XRT_STR_LITERAL("abcd"), 2, XRT_STR_LITERAL("XY"));
	testRequire((sResult != NULL) && (strcmp(sResult, "abXYcd") == 0), "insert mismatch");
	xrtFree(sResult);
	sResult = xrtStrRemove(XRT_STR_LITERAL("abXYcd"), 2, 2);
	testRequire((sResult != NULL) && (strcmp(sResult, "abcd") == 0), "remove mismatch");
	xrtFree(sResult);
	sResult = xrtStrReverseBytes(xrtStrViewN(sBinary, sizeof(sBinary)));
	testRequire((sResult != NULL) && (memcmp(sResult, "b\0ab\0a", sizeof(sBinary)) == 0),
		"binary reverse mismatch");
	xrtFree(sResult);
	sResult = xrtStrLower(xrtStrView(sUtf8));
	testRequire((sResult != NULL) && (strcmp(sResult, "a你z") == 0), "ASCII lower mismatch");
	xrtFree(sResult);
	sResult = xrtStrUpper(XRT_STR_LITERAL("a你z"));
	testRequire((sResult != NULL) && (strcmp(sResult, "A你Z") == 0), "ASCII upper mismatch");
	xrtFree(sResult);
	testStringTransforms();
	testStringFilter();
	sResult = xrtStrPadLeft(XRT_STR_LITERAL("ab"), 6, XRT_STR_LITERAL("xy"));
	testRequire((sResult != NULL) && (strcmp(sResult, "xyxyab") == 0), "left pad mismatch");
	xrtFree(sResult);
	sResult = xrtStrPadRight(XRT_STR_LITERAL("ab"), 6, XRT_STR_LITERAL("xy"));
	testRequire((sResult != NULL) && (strcmp(sResult, "abxyxy") == 0), "right pad mismatch");
	xrtFree(sResult);
	sResult = xrtStrPadCenter(XRT_STR_LITERAL("ab"), 5, XRT_STR_LITERAL("xy"));
	testRequire((sResult != NULL) && (strcmp(sResult, "xabxy") == 0), "center pad mismatch");
	xrtFree(sResult);

	xrtClearError();
	testRequire(xrtStrDupN(NULL, 1) == NULL, "invalid duplicate must fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "invalid view error mismatch");
	xrtClearError();
	testRequire(xrtStrRepeat(XRT_STR_LITERAL("ab"), SIZE_MAX) == NULL, "repeat overflow must fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE, "repeat overflow error mismatch");
	xrtClearError();
	printf("[PASS] string\n");
	return 0;
}

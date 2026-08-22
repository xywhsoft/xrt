#include "../test.h"



/* 判断拆分结果中的指定视图与字节串相等。 */
static bool testItemEqual(const xstrlist* pList, size_t iIndex, cstr sText, size_t iSize)
{
	if ( (pList == NULL) || (iIndex >= pList->Count) ) {
		return false;
	}
	return (pList->Items[iIndex].Size == iSize) &&
		((iSize == 0) || (memcmp(pList->Items[iIndex].Data, sText, iSize) == 0)) &&
		(pList->Items[iIndex].Data[iSize] == 0);
}



/* 验证零分配迭代器和单块便捷结果的全部边界。 */
int main(void)
{
	static const char sBinary[] = { 'a', 0, 'b', '|', 'c', 0, 'd' };
	xstrsplit tSplit;
	xstrlines tLines;
	xstrfields tFields;
	xstrview Item;
	xstrlist* pList;
	size_t iCount;
	size_t iOffset;

	/* 低级列表构建接口必须保留单块布局、容量检查和零结尾约定。 */
	pList = xrtStrListAlloc(2, 5);
	testRequire((pList != NULL) && (pList->Count == 2) && (pList->DataSize == 5),
		"string list allocation mismatch");
	iOffset = 0;
	testRequire(xrtStrListWrite(pList, 0, XRT_STR_LITERAL("ab"), &iOffset) &&
		(iOffset == 3), "first string list write failed");
	testRequire(xrtStrListWrite(pList, 1, XRT_STR_LITERAL("c"), &iOffset) &&
		(iOffset == 5), "second string list write failed");
	testRequire(testItemEqual(pList, 0, "ab", 2) && testItemEqual(pList, 1, "c", 1),
		"string list write content mismatch");
	testRequire((pList->Items[2].Data == NULL) && (pList->Items[2].Size == 0),
		"string list sentinel mismatch");
	xrtClearError();
	testRequire(!xrtStrListWrite(pList, 1, XRT_STR_LITERAL("x"), &iOffset),
		"full string list data region must reject another write");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"string list capacity error mismatch");
	xrtClearError();
	xrtStrListFree(pList);

	/* 未初始化和损坏的公开状态必须失败，且不能泄漏上一次输出。 */
	memset(&tSplit, 0, sizeof(tSplit));
	Item = XRT_STR_LITERAL("unchanged");
	xrtClearError();
	testRequire(!xrtStrSplitNext(&tSplit, &Item), "uninitialized split iterator must fail");
	testRequire((Item.Data == NULL) && (Item.Size == 0),
		"failed split iterator did not clear output");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE,
		"uninitialized split iterator error mismatch");
	xrtClearError();

	testRequire(xrtStrSplitInit(&tSplit, XRT_STR_LITERAL("a--b----"), XRT_STR_LITERAL("--")),
		"split iterator init failed");
	iCount = 0;
	while ( xrtStrSplitNext(&tSplit, &Item) ) {
		static const char* arrExpected[] = { "a", "b", "", "" };

		testRequire(iCount < 4, "split iterator returned too many items");
		testRequire((Item.Size == strlen(arrExpected[iCount])) &&
			(memcmp(Item.Data, arrExpected[iCount], Item.Size) == 0), "split iterator item mismatch");
		iCount++;
	}
	testRequire(iCount == 4, "split iterator count mismatch");

	testRequire(xrtStrSplitInit(&tSplit, XRT_STR_LITERAL("abc"), XRT_STR_LITERAL("")),
		"empty separator init failed");
	testRequire(xrtStrSplitNext(&tSplit, &Item) && (Item.Size == 3),
		"empty separator must return whole text");
	testRequire(!xrtStrSplitNext(&tSplit, &Item), "empty separator returned extra item");

	tSplit.Position = tSplit.Text.Size + 1u;
	Item = XRT_STR_LITERAL("unchanged");
	xrtClearError();
	testRequire(!xrtStrSplitNext(&tSplit, &Item), "damaged split iterator must fail");
	testRequire((Item.Data == NULL) && (Item.Size == 0),
		"damaged split iterator did not clear output");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE,
		"damaged split iterator error mismatch");
	xrtClearError();

	pList = xrtStrSplit(XRT_STR_LITERAL("abc"), XRT_STR_LITERAL("separator-is-longer"));
	testRequire((pList != NULL) && (pList->Count == 1) && testItemEqual(pList, 0, "abc", 3),
		"long separator boundary mismatch");
	xrtStrListFree(pList);
	pList = xrtStrSplit(xrtStrViewN(sBinary, sizeof(sBinary)), XRT_STR_LITERAL("|"));
	testRequire((pList != NULL) && (pList->Count == 2), "binary split count mismatch");
	testRequire(testItemEqual(pList, 0, sBinary, 3), "binary split first item mismatch");
	testRequire(testItemEqual(pList, 1, sBinary + 4, 3), "binary split second item mismatch");
	xrtStrListFree(pList);
	pList = xrtStrSplit(XRT_STR_LITERAL(""), XRT_STR_LITERAL(","));
	testRequire((pList != NULL) && (pList->Count == 1) && testItemEqual(pList, 0, "", 0),
		"empty text split mismatch");
	xrtStrListFree(pList);

	testRequire(xrtStrLinesInit(&tLines, XRT_STR_LITERAL("a\r\nb\rc\n\n")),
		"line iterator init failed");
	iCount = 0;
	while ( xrtStrLinesNext(&tLines, &Item) ) {
		static const char* arrExpected[] = { "a", "b", "c", "" };

		testRequire(iCount < 4, "line iterator returned too many items");
		testRequire((Item.Size == strlen(arrExpected[iCount])) &&
			(memcmp(Item.Data, arrExpected[iCount], Item.Size) == 0), "line iterator item mismatch");
		iCount++;
	}
	testRequire(iCount == 4, "line iterator count mismatch");

	tLines.Position = tLines.Text.Size + 1u;
	Item = XRT_STR_LITERAL("unchanged");
	xrtClearError();
	testRequire(!xrtStrLinesNext(&tLines, &Item), "damaged line iterator must fail");
	testRequire((Item.Data == NULL) && (Item.Size == 0),
		"damaged line iterator did not clear output");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE,
		"damaged line iterator error mismatch");
	xrtClearError();

	pList = xrtStrSplitLines(XRT_STR_LITERAL("a\r\nb\rc\n\n"));
	testRequire((pList != NULL) && (pList->Count == 4), "line list count mismatch");
	testRequire(testItemEqual(pList, 0, "a", 1) && testItemEqual(pList, 1, "b", 1) &&
		testItemEqual(pList, 2, "c", 1) && testItemEqual(pList, 3, "", 0),
		"line list item mismatch");
	xrtStrListFree(pList);
	pList = xrtStrSplitLines(XRT_STR_LITERAL(""));
	testRequire((pList != NULL) && (pList->Count == 0), "empty line list mismatch");
	xrtStrListFree(pList);

	testRequire(xrtStrFieldsInit(&tFields, XRT_STR_LITERAL(" \talpha\r\n beta  gamma\f")),
		"field iterator init failed");
	iCount = 0;
	while ( xrtStrFieldsNext(&tFields, &Item) ) {
		static const char* arrExpected[] = { "alpha", "beta", "gamma" };

		testRequire(iCount < 3, "field iterator returned too many items");
		testRequire((Item.Size == strlen(arrExpected[iCount])) &&
			(memcmp(Item.Data, arrExpected[iCount], Item.Size) == 0),
			"field iterator item mismatch");
		iCount++;
	}
	testRequire(iCount == 3, "field iterator count mismatch");

	tFields.Position = tFields.Text.Size + 1u;
	Item = XRT_STR_LITERAL("unchanged");
	xrtClearError();
	testRequire(!xrtStrFieldsNext(&tFields, &Item), "damaged field iterator must fail");
	testRequire((Item.Data == NULL) && (Item.Size == 0),
		"damaged field iterator did not clear output");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE,
		"damaged field iterator error mismatch");
	xrtClearError();

	pList = xrtStrFields(XRT_STR_LITERAL(" \talpha\r\n beta  gamma\f"));
	testRequire((pList != NULL) && (pList->Count == 3), "field list count mismatch");
	testRequire(testItemEqual(pList, 0, "alpha", 5) &&
		testItemEqual(pList, 1, "beta", 4) &&
		testItemEqual(pList, 2, "gamma", 5), "field list item mismatch");
	xrtStrListFree(pList);
	pList = xrtStrFields(XRT_STR_LITERAL(" \t\r\n\v\f"));
	testRequire((pList != NULL) && (pList->Count == 0), "blank field list mismatch");
	xrtStrListFree(pList);
	printf("[PASS] string-split\n");
	return 0;
}

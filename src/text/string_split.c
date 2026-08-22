#include "../internal/xrt_string.h"



#if defined(XRT_FEATURE_STRING_SPLIT)

#define XRT_STR_SPLIT_STATE 0x53504C54u
#define XRT_STR_LINES_STATE 0x4C494E45u
#define XRT_STR_FIELDS_STATE 0x4649454Cu

/* 检查通用拆分迭代器的公开状态是否仍然自洽。 */
static bool __xrtStrSplitValid(const xstrsplit* pSplit)
{
	if ( pSplit == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pSplit->State != XRT_STR_SPLIT_STATE) ||
		 ((pSplit->Text.Data == NULL) && (pSplit->Text.Size != 0)) ||
		 ((pSplit->Separator.Data == NULL) && (pSplit->Separator.Size != 0)) ||
		 (pSplit->Position > pSplit->Text.Size) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 检查行迭代器的公开状态是否仍然自洽。 */
static bool __xrtStrLinesValid(const xstrlines* pLines)
{
	if ( pLines == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pLines->State != XRT_STR_LINES_STATE) ||
		 ((pLines->Text.Data == NULL) && (pLines->Text.Size != 0)) ||
		 (pLines->Position > pLines->Text.Size) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 检查字段迭代器的公开状态是否仍然自洽。 */
static bool __xrtStrFieldsValid(const xstrfields* pFields)
{
	if ( pFields == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pFields->State != XRT_STR_FIELDS_STATE) ||
		 ((pFields->Text.Data == NULL) && (pFields->Text.Size != 0)) ||
		 (pFields->Position > pFields->Text.Size) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 为指定片段数量和数据量分配单块拆分结果。 */
XRT_API xstrlist* xrtStrListAlloc(size_t iCount, size_t iDataSize)
{
	size_t iItemCount;
	size_t iItemBytes;
	size_t iTotal;
	xstrlist* pList;

	if ( iCount == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iItemCount = iCount + 1u;
	if ( iItemCount > (SIZE_MAX / sizeof(xstrview)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iItemBytes = iItemCount * sizeof(xstrview);
	if ( (iItemBytes > (SIZE_MAX - sizeof(xstrlist))) ||
		 (iDataSize > (SIZE_MAX - sizeof(xstrlist) - iItemBytes)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iTotal = sizeof(xstrlist) + iItemBytes + iDataSize;
	pList = (xstrlist*)xrtMalloc(iTotal);
	if ( pList == NULL ) {
		return NULL;
	}
	pList->Count = iCount;
	pList->Items = (xstrview*)(pList + 1);
	pList->DataSize = iDataSize;
	memset(pList->Items, 0, iItemBytes);
	return pList;
}



/* 把借用片段复制到结果的连续零结尾数据区。 */
XRT_API bool xrtStrListWrite(
	xstrlist* pList,
	size_t iIndex,
	xstrview Item,
	size_t* pOffset
)
{
	size_t iItemCount;
	size_t iItemBytes;
	size_t iNeed;
	str sWrite;

	if ( (pList == NULL) || (pOffset == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtStrViewValid(Item) ) {
		return false;
	}
	if ( iIndex >= pList->Count ) {
		__xrtErrorSetRange();
		return false;
	}
	if ( pList->Count == SIZE_MAX ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	iItemCount = pList->Count + 1u;
	if ( iItemCount > (SIZE_MAX / sizeof(xstrview)) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	iItemBytes = iItemCount * sizeof(xstrview);
	if ( pList->Items != (xstrview*)(pList + 1) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( (Item.Size == SIZE_MAX) || (*pOffset > pList->DataSize) ) {
		__xrtErrorSetRange();
		return false;
	}
	iNeed = Item.Size + 1u;
	if ( iNeed > (pList->DataSize - *pOffset) ) {
		__xrtErrorSetRange();
		return false;
	}
	sWrite = (str)pList->Items + iItemBytes + *pOffset;
	pList->Items[iIndex].Data = sWrite;
	pList->Items[iIndex].Size = Item.Size;
	if ( Item.Size != 0 ) {
		memmove(sWrite, Item.Data, Item.Size);
	}
	sWrite[Item.Size] = 0;
	*pOffset += iNeed;
	return true;
}



/* 初始化不分配内存的字符串拆分迭代器。 */
XRT_API bool xrtStrSplitInit(xstrsplit* pSplit, xstrview Text, xstrview Separator)
{
	if ( pSplit == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pSplit, 0, sizeof(xstrsplit));
	if ( !__xrtStrViewValid(Text) || !__xrtStrViewValid(Separator) ) {
		return false;
	}
	pSplit->Text = Text;
	pSplit->Separator = Separator;
	pSplit->State = XRT_STR_SPLIT_STATE;
	return true;
}



/* 返回下一个借用片段，结束时返回 false。 */
XRT_API bool xrtStrSplitNext(xstrsplit* pSplit, xstrview* pItem)
{
	size_t iFound;

	if ( pItem == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pItem->Data = NULL;
	pItem->Size = 0;
	if ( !__xrtStrSplitValid(pSplit) ) {
		return false;
	}
	if ( pSplit->Done ) {
		return false;
	}
	if ( pSplit->Separator.Size == 0 ) {
		*pItem = pSplit->Text;
		pSplit->Done = true;
		return true;
	}
	iFound = xrtStrFind(pSplit->Text, pSplit->Separator, pSplit->Position);
	if ( iFound == XRT_NPOS ) {
		*pItem = xrtStrSlice(pSplit->Text, pSplit->Position, XRT_NPOS);
		pSplit->Done = true;
		return true;
	}
	*pItem = xrtStrSlice(pSplit->Text, pSplit->Position, iFound - pSplit->Position);
	pSplit->Position = iFound + pSplit->Separator.Size;
	return true;
}



/* 初始化不分配内存的行迭代器。 */
XRT_API bool xrtStrLinesInit(xstrlines* pLines, xstrview Text)
{
	if ( pLines == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pLines, 0, sizeof(xstrlines));
	if ( !__xrtStrViewValid(Text) ) {
		return false;
	}
	pLines->Text = Text;
	pLines->State = XRT_STR_LINES_STATE;
	pLines->Done = Text.Size == 0;
	return true;
}



/* 返回下一行借用视图，结束时返回 false。 */
XRT_API bool xrtStrLinesNext(xstrlines* pLines, xstrview* pLine)
{
	size_t iStart;
	size_t iPosition;

	if ( pLine == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pLine->Data = NULL;
	pLine->Size = 0;
	if ( !__xrtStrLinesValid(pLines) ) {
		return false;
	}
	if ( pLines->Done ) {
		return false;
	}
	iStart = pLines->Position;
	iPosition = iStart;
	while ( (iPosition < pLines->Text.Size) &&
		(pLines->Text.Data[iPosition] != '\r') && (pLines->Text.Data[iPosition] != '\n') ) {
		iPosition++;
	}
	*pLine = xrtStrSlice(pLines->Text, iStart, iPosition - iStart);
	if ( iPosition == pLines->Text.Size ) {
		pLines->Done = true;
		return true;
	}
	if ( (pLines->Text.Data[iPosition] == '\r') && ((iPosition + 1u) < pLines->Text.Size) &&
		 (pLines->Text.Data[iPosition + 1u] == '\n') ) {
		iPosition += 2u;
	} else {
		iPosition++;
	}
	pLines->Position = iPosition;
	pLines->Done = iPosition == pLines->Text.Size;
	return true;
}



/* 初始化按连续 ASCII 空白拆分的零分配字段迭代器。 */
XRT_API bool xrtStrFieldsInit(xstrfields* pFields, xstrview Text)
{
	if ( pFields == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pFields, 0, sizeof(xstrfields));
	if ( !__xrtStrViewValid(Text) ) {
		return false;
	}
	pFields->Text = Text;
	pFields->State = XRT_STR_FIELDS_STATE;
	pFields->Done = Text.Size == 0;
	return true;
}



/* 返回下一个非空借用字段，结束时返回 false。 */
XRT_API bool xrtStrFieldsNext(xstrfields* pFields, xstrview* pField)
{
	size_t iStart;
	size_t iPosition;

	if ( pField == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pField->Data = NULL;
	pField->Size = 0;
	if ( !__xrtStrFieldsValid(pFields) ) {
		return false;
	}
	if ( pFields->Done ) {
		return false;
	}
	iPosition = pFields->Position;
	while ( (iPosition < pFields->Text.Size) &&
		__xrtStrAsciiSpace((unsigned char)pFields->Text.Data[iPosition]) ) {
		iPosition++;
	}
	if ( iPosition == pFields->Text.Size ) {
		pFields->Position = iPosition;
		pFields->Done = true;
		return false;
	}
	iStart = iPosition;
	while ( (iPosition < pFields->Text.Size) &&
		!__xrtStrAsciiSpace((unsigned char)pFields->Text.Data[iPosition]) ) {
		iPosition++;
	}
	*pField = xrtStrSlice(pFields->Text, iStart, iPosition - iStart);
	pFields->Position = iPosition;
	pFields->Done = iPosition == pFields->Text.Size;
	return true;
}



/* 一次性拆分字符串并返回独立的零结尾片段。 */
XRT_API xstrlist* xrtStrSplit(xstrview Text, xstrview Separator)
{
	xstrsplit tSplit;
	xstrview Item;
	xstrlist* pList;
	size_t iCount = 0;
	size_t iDataSize = 0;
	size_t iIndex = 0;
	size_t iOffset = 0;

	if ( !xrtStrSplitInit(&tSplit, Text, Separator) ) {
		return NULL;
	}
	while ( xrtStrSplitNext(&tSplit, &Item) ) {
		if ( (iCount == SIZE_MAX) || (Item.Size == SIZE_MAX) ||
			 ((Item.Size + 1u) > (SIZE_MAX - iDataSize)) ) {
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
		iCount++;
		iDataSize += Item.Size + 1u;
	}
	pList = xrtStrListAlloc(iCount, iDataSize);
	if ( pList == NULL ) {
		return NULL;
	}
	(void)xrtStrSplitInit(&tSplit, Text, Separator);
	while ( xrtStrSplitNext(&tSplit, &Item) ) {
		if ( !xrtStrListWrite(pList, iIndex++, Item, &iOffset) ) {
			xrtStrListFree(pList);
			return NULL;
		}
	}
	return pList;
}



/* 一次性按行拆分字符串。 */
XRT_API xstrlist* xrtStrSplitLines(xstrview Text)
{
	xstrlines tLines;
	xstrview Line;
	xstrlist* pList;
	size_t iCount = 0;
	size_t iDataSize = 0;
	size_t iIndex = 0;
	size_t iOffset = 0;

	if ( !xrtStrLinesInit(&tLines, Text) ) {
		return NULL;
	}
	while ( xrtStrLinesNext(&tLines, &Line) ) {
		if ( (iCount == SIZE_MAX) || (Line.Size == SIZE_MAX) ||
			 ((Line.Size + 1u) > (SIZE_MAX - iDataSize)) ) {
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
		iCount++;
		iDataSize += Line.Size + 1u;
	}
	pList = xrtStrListAlloc(iCount, iDataSize);
	if ( pList == NULL ) {
		return NULL;
	}
	(void)xrtStrLinesInit(&tLines, Text);
	while ( xrtStrLinesNext(&tLines, &Line) ) {
		if ( !xrtStrListWrite(pList, iIndex++, Line, &iOffset) ) {
			xrtStrListFree(pList);
			return NULL;
		}
	}
	return pList;
}



/* 一次性按连续 ASCII 空白拆分字符串。 */
XRT_API xstrlist* xrtStrFields(xstrview Text)
{
	xstrfields tFields;
	xstrview Field;
	xstrlist* pList;
	size_t iCount = 0;
	size_t iDataSize = 0;
	size_t iIndex = 0;
	size_t iOffset = 0;

	if ( !xrtStrFieldsInit(&tFields, Text) ) {
		return NULL;
	}
	while ( xrtStrFieldsNext(&tFields, &Field) ) {
		if ( (iCount == SIZE_MAX) || (Field.Size == SIZE_MAX) ||
			 ((Field.Size + 1u) > (SIZE_MAX - iDataSize)) ) {
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
		iCount++;
		iDataSize += Field.Size + 1u;
	}
	pList = xrtStrListAlloc(iCount, iDataSize);
	if ( pList == NULL ) {
		return NULL;
	}
	(void)xrtStrFieldsInit(&tFields, Text);
	while ( xrtStrFieldsNext(&tFields, &Field) ) {
		if ( !xrtStrListWrite(pList, iIndex++, Field, &iOffset) ) {
			xrtStrListFree(pList);
			return NULL;
		}
	}
	return pList;
}



/* 释放便捷拆分结果。 */
XRT_API void xrtStrListFree(xstrlist* pList)
{
	xrtFree(pList);
}

#undef XRT_STR_SPLIT_STATE
#undef XRT_STR_LINES_STATE
#undef XRT_STR_FIELDS_STATE

#endif

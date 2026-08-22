#include "../internal/xrt_http.h"

#include <xrt/http_expect.h>



#if defined(XRT_FEATURE_HTTP_EXPECT)

/* 跳过一个完整 quoted-string 并拒绝非法转义或未闭合输入。 */
static bool __xrtHttpExpectQuoted(
	xstrview Text,
	size_t* pOffset
)
{
	size_t i = *pOffset;

	if ( (i >= Text.Size) || (Text.Data[i] != '"') ) {
		return false;
	}
	i++;
	while ( i < Text.Size ) {
		unsigned char iByte = (unsigned char)Text.Data[i++];

		if ( iByte == (unsigned char)'"' ) {
			*pOffset = i;
			return true;
		}
		if ( iByte == (unsigned char)'\\' ) {
			if ( (i >= Text.Size) ||
				!__xrtHttpQuotedPairByte(
					(unsigned char)Text.Data[i]
				) ) {
				return false;
			}
			i++;
		} else if ( !__xrtHttpQuotedTextByte(iByte) ) {
			return false;
		}
	}
	return false;
}



/* 读取 expectation 或参数的 token/quoted-string 值。 */
static bool __xrtHttpExpectValue(
	xstrview Text,
	size_t* pOffset,
	xstrview* pValue,
	bool* pQuoted
)
{
	size_t iStart = *pOffset;
	size_t i = iStart;

	*pQuoted = false;
	if ( (i < Text.Size) && (Text.Data[i] == '"') ) {
		if ( !__xrtHttpExpectQuoted(Text, &i) ) {
			return false;
		}
		*pQuoted = true;
	} else {
		while ( (i < Text.Size) &&
			__xrtHttpTokenByte(
				(unsigned char)Text.Data[i]
			) ) {
			i++;
		}
		if ( i == iStart ) {
			return false;
		}
	}
	pValue->Data = Text.Data + iStart;
	pValue->Size = i - iStart;
	*pOffset = i;
	return true;
}



/* 严格解析一个已经与列表分隔符分开的 expectation。 */
static bool __xrtHttpExpectationParseValue(
	xstrview Text,
	xhttpexpectation* pExpectation
)
{
	xhttpexpectation Expectation;
	xstrview Ignored;
	bool bQuoted;
	size_t iParameters = XRT_NPOS;
	size_t iStart;
	size_t i = 0;

	memset(&Expectation, 0, sizeof(Expectation));
	Text = xrtHttpOwsTrim(Text);
	Expectation.Element = Text;
	iStart = i;
	while ( (i < Text.Size) &&
		__xrtHttpTokenByte((unsigned char)Text.Data[i]) ) {
		i++;
	}
	if ( i == iStart ) {
		return false;
	}
	Expectation.Name = (xstrview){
		Text.Data + iStart, i - iStart
	};
	if ( i == Text.Size ) {
		memcpy(
			pExpectation, &Expectation, sizeof(Expectation)
		);
		return true;
	}
	if ( Text.Data[i] != '=' ) {
		return false;
	}
	i++;
	if ( !__xrtHttpExpectValue(
		Text, &i, &Expectation.Value, &bQuoted
	) ) {
		return false;
	}
	Expectation.Flags |= (uint32)XHTTP_EXPECT_HAS_VALUE;
	if ( bQuoted ) {
		Expectation.Flags |=
			(uint32)XHTTP_EXPECT_VALUE_QUOTED;
	}
	while ( true ) {
		while ( (i < Text.Size) &&
			((Text.Data[i] == ' ') ||
			 (Text.Data[i] == '\t')) ) {
			i++;
		}
		if ( i == Text.Size ) {
			break;
		}
		if ( Text.Data[i] != ';' ) {
			return false;
		}
		if ( iParameters == XRT_NPOS ) {
			iParameters = i;
		}
		Expectation.Flags |=
			(uint32)XHTTP_EXPECT_HAS_PARAMETERS;
		i++;
		while ( (i < Text.Size) &&
			((Text.Data[i] == ' ') ||
			 (Text.Data[i] == '\t')) ) {
			i++;
		}
		if ( (i == Text.Size) || (Text.Data[i] == ';') ) {
			continue;
		}
		iStart = i;
		while ( (i < Text.Size) &&
			__xrtHttpTokenByte(
				(unsigned char)Text.Data[i]
			) ) {
			i++;
		}
		if ( (i == iStart) || (i == Text.Size) ||
			(Text.Data[i] != '=') ) {
			return false;
		}
		i++;
		if ( !__xrtHttpExpectValue(
			Text, &i, &Ignored, &bQuoted
		) ) {
			return false;
		}
	}
	if ( iParameters != XRT_NPOS ) {
		Expectation.Parameters = (xstrview){
			Text.Data + iParameters,
			Text.Size - iParameters
		};
	}
	memcpy(pExpectation, &Expectation, sizeof(Expectation));
	return true;
}



/* 从列表位置读取下一项，不修改公开错误状态。 */
static xhttpnext __xrtHttpExpectItemNext(
	xstrview Value,
	size_t iOffset,
	size_t* pNext,
	xhttpexpectation* pExpectation
)
{
	xhttpexpectation Expectation;
	xstrview Element;
	xhttpnext Next;

	memset(&Expectation, 0, sizeof(Expectation));
	Next = __xrtHttpQuotedListNext(
		Value, iOffset, pNext, &Element
	);
	if ( Next != XHTTP_NEXT_ITEM ) {
		memcpy(
			pExpectation, &Expectation,
			sizeof(Expectation)
		);
		return Next;
	}
	if ( !__xrtHttpExpectationParseValue(
		Element, &Expectation
	) ) {
		return XHTTP_NEXT_ERROR;
	}
	memcpy(
		pExpectation, &Expectation, sizeof(Expectation)
	);
	return XHTTP_NEXT_ITEM;
}



/* 完整验证一个字段值，并可同时统计 expectation 数量。 */
static bool __xrtHttpExpectMeasure(
	xstrview Value,
	size_t* pCount
)
{
	xhttpexpectation Expectation;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iNext;
	size_t iCount = 0;

	for ( ;; ) {
		Next = __xrtHttpExpectItemNext(
			Value, iOffset, &iNext, &Expectation
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			*pCount = iCount;
			return true;
		}
		if ( iCount == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iCount++;
		iOffset = iNext;
	}
}



/* 初始化单字段游标。 */
XRT_API void xrtHttpExpectCursorInit(
	xhttpexpectcursor* pCursor
)
{
	xhttpexpectcursor Cursor;

	if ( !__xrtRangeValid(pCursor, sizeof(Cursor)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Cursor, 0, sizeof(Cursor));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
}



/* 初始化重复字段游标。 */
XRT_API void xrtHttpExpectFieldCursorInit(
	xhttpexpectfieldcursor* pCursor
)
{
	xhttpexpectfieldcursor Cursor;

	if ( !__xrtRangeValid(pCursor, sizeof(Cursor)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Cursor, 0, sizeof(Cursor));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
}



/* 严格解析一个 expectation 元素。 */
XRT_API bool xrtHttpExpectationParse(
	xstrview Element,
	xhttpexpectation* pExpectation
)
{
	xhttpexpectation Expectation;

	memset(&Expectation, 0, sizeof(Expectation));
	if ( !__xrtHttpViewValid(Element) ||
		!__xrtRangeValid(
			pExpectation, sizeof(Expectation)
		) || __xrtRangesOverlap(
			Element.Data, Element.Size,
			pExpectation, sizeof(Expectation)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(
		pExpectation, &Expectation, sizeof(Expectation)
	);
	if ( !__xrtHttpExpectationParseValue(
		Element, &Expectation
	) ) {
		__xrtErrorSetValue();
		return false;
	}
	memcpy(
		pExpectation, &Expectation, sizeof(Expectation)
	);
	return true;
}



/* 完整验证一个 Expect 字段值。 */
XRT_API bool xrtHttpExpectValid(xstrview Value)
{
	size_t iCount;

	if ( !__xrtHttpViewValid(Value) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpExpectMeasure(Value, &iCount) ) {
		__xrtErrorSetValue();
		return false;
	}
	return true;
}



/* 完整验证并统计一个 Expect 字段值。 */
XRT_API bool xrtHttpExpectCount(
	xstrview Value,
	size_t* pCount
)
{
	size_t iCount = 0;

	if ( !__xrtHttpViewValid(Value) ||
		!__xrtRangeValid(pCount, sizeof(iCount)) ||
		__xrtRangesOverlap(
			Value.Data, Value.Size,
			pCount, sizeof(iCount)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pCount, &iCount, sizeof(iCount));
	if ( !__xrtHttpExpectMeasure(Value, &iCount) ) {
		__xrtErrorSetValue();
		return false;
	}
	memcpy(pCount, &iCount, sizeof(iCount));
	return true;
}



/* 验证单字段游标只能由初始化函数和迭代器推进。 */
static bool __xrtHttpExpectCursorValid(
	const xhttpexpectcursor* pCursor,
	size_t iSize
)
{
	return (pCursor->Validated <= 1u) &&
		(pCursor->Offset <= iSize) &&
		!((pCursor->Validated == 0) &&
		  (pCursor->Offset != 0));
}



/* 按线路顺序迭代一个完整 Expect 字段值。 */
XRT_API xhttpnext xrtHttpExpectNext(
	xstrview Value,
	xhttpexpectcursor* pCursor,
	xhttpexpectation* pExpectation
)
{
	xhttpexpectcursor Cursor;
	xhttpexpectation Expectation;
	xhttpnext Next;
	size_t iIgnored;
	size_t iNext;

	if ( !__xrtHttpViewValid(Value) ||
		!__xrtRangeValid(pCursor, sizeof(Cursor)) ||
		!__xrtRangeValid(
			pExpectation, sizeof(Expectation)
		) || __xrtRangesOverlap(
			Value.Data, Value.Size,
			pCursor, sizeof(Cursor)
		) || __xrtRangesOverlap(
			Value.Data, Value.Size,
			pExpectation, sizeof(Expectation)
		) || __xrtRangesOverlap(
			pCursor, sizeof(Cursor),
			pExpectation, sizeof(Expectation)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	memset(&Expectation, 0, sizeof(Expectation));
	memcpy(
		pExpectation, &Expectation, sizeof(Expectation)
	);
	if ( !__xrtHttpExpectCursorValid(
		&Cursor, Value.Size
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( Cursor.Validated == 0 ) {
		if ( !__xrtHttpExpectMeasure(
			Value, &iIgnored
		) ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Validated = 1u;
	}
	Next = __xrtHttpExpectItemNext(
		Value, Cursor.Offset, &iNext, &Expectation
	);
	if ( Next == XHTTP_NEXT_ERROR ) {
		__xrtErrorSetInvalidArgument();
		return Next;
	}
	Cursor.Offset = iNext;
	memcpy(
		pExpectation, &Expectation, sizeof(Expectation)
	);
	memcpy(pCursor, &Cursor, sizeof(Cursor));
	return Next;
}



/* 完整预校验全部重复 Expect 字段行。 */
static bool __xrtHttpExpectFieldsValidate(
	const xhttpfield* pFields,
	size_t iCount
)
{
	xhttpfield Field;
	size_t iItems;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( xrtHttpFieldNameEqual(
			Field.Name, XRT_STR_LITERAL("Expect")
		) && !__xrtHttpExpectMeasure(
			Field.Value, &iItems
		) ) {
			return false;
		}
	}
	return true;
}



/* 验证重复字段游标的跨字段状态。 */
static bool __xrtHttpExpectFieldCursorValid(
	const xhttpexpectfieldcursor* pCursor,
	size_t iCount
)
{
	return (pCursor->Validated <= 1u) &&
		(pCursor->Field <= iCount) &&
		!((pCursor->Validated == 0) &&
		  ((pCursor->Field != 0) ||
		   (pCursor->Offset != 0))) &&
		!((pCursor->Field == iCount) &&
		  (pCursor->Offset != 0));
}



/* 跨重复 Expect 字段行按线路顺序迭代 expectation。 */
XRT_API xhttpnext xrtHttpExpectFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpexpectfieldcursor* pCursor,
	xhttpexpectation* pExpectation
)
{
	xhttpexpectfieldcursor Cursor;
	xhttpexpectation Expectation;
	xhttpfield Field;
	xhttpnext Next;
	size_t iNext;

	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ||
		!__xrtRangeValid(pCursor, sizeof(Cursor)) ||
		!__xrtRangeValid(
			pExpectation, sizeof(Expectation)
		) || __xrtHttpFieldArrayOverlap(
			pFields, iCount, pCursor, sizeof(Cursor)
		) || __xrtHttpFieldArrayOverlap(
			pFields, iCount,
			pExpectation, sizeof(Expectation)
		) || __xrtRangesOverlap(
			pCursor, sizeof(Cursor),
			pExpectation, sizeof(Expectation)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	memset(&Expectation, 0, sizeof(Expectation));
	memcpy(
		pExpectation, &Expectation, sizeof(Expectation)
	);
	if ( !__xrtHttpExpectFieldCursorValid(
		&Cursor, iCount
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( Cursor.Validated == 0 ) {
		if ( !__xrtHttpExpectFieldsValidate(
			pFields, iCount
		) ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Validated = 1u;
	}
	while ( Cursor.Field < iCount ) {
		__xrtHttpFieldLoad(pFields, Cursor.Field, &Field);
		if ( !xrtHttpFieldNameEqual(
			Field.Name, XRT_STR_LITERAL("Expect")
		) ) {
			Cursor.Field++;
			Cursor.Offset = 0;
			continue;
		}
		if ( Cursor.Offset > Field.Value.Size ) {
			__xrtErrorSetInvalidArgument();
			return XHTTP_NEXT_ERROR;
		}
		Next = __xrtHttpExpectItemNext(
			Field.Value,
			Cursor.Offset,
			&iNext,
			&Expectation
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			__xrtErrorSetInvalidArgument();
			return Next;
		}
		if ( Next == XHTTP_NEXT_END ) {
			Cursor.Field++;
			Cursor.Offset = 0;
			continue;
		}
		Cursor.Offset = iNext;
		memcpy(
			pExpectation,
			&Expectation,
			sizeof(Expectation)
		);
		memcpy(pCursor, &Cursor, sizeof(Cursor));
		return XHTTP_NEXT_ITEM;
	}
	memset(&Expectation, 0, sizeof(Expectation));
	memcpy(
		pExpectation, &Expectation, sizeof(Expectation)
	);
	memcpy(pCursor, &Cursor, sizeof(Cursor));
	return XHTTP_NEXT_END;
}



/* 分类全部 Expect 字段中的标准和扩展 expectation。 */
XRT_API xhttpexpectresult xrtHttpExpectFields(
	const xhttpfield* pFields,
	size_t iCount
)
{
	xhttpexpectfieldcursor Cursor;
	xhttpexpectation Expectation;
	xhttpnext Next;
	bool bAny = false;
	bool bUnsupported = false;

	xrtHttpExpectFieldCursorInit(&Cursor);
	for ( ;; ) {
		Next = xrtHttpExpectFieldNext(
			pFields, iCount, &Cursor, &Expectation
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return XHTTP_EXPECT_ERROR;
		}
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		bAny = true;
		if ( !xrtHttpTokenEqual(
			Expectation.Name,
			XRT_STR_LITERAL("100-continue")
		) || (Expectation.Flags != XHTTP_EXPECT_BARE) ) {
			bUnsupported = true;
		}
	}
	if ( !bAny ) {
		return XHTTP_EXPECT_NONE;
	}
	return bUnsupported ? XHTTP_EXPECT_UNSUPPORTED :
		XHTTP_EXPECT_CONTINUE;
}

#endif

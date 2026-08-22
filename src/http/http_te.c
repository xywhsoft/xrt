#include "../internal/xrt_http.h"

#include <xrt/http_te.h>



#if defined(XRT_FEATURE_HTTP_TE)

/* 严格解析一个已经从逗号列表分离的 TE 成员。 */
static bool __xrtHttpTeCodingParseValue(
	xstrview Element,
	xhttptecoding* pOutput
)
{
	xhttptecoding Coding;
	xhttpparam Param;
	xhttpnext Next;
	xstrview Parameters = { NULL, 0 };
	size_t iParameterEnd;
	size_t iBefore;
	size_t iStart;
	size_t iOffset = 0;
	size_t i;
	bool bWeight = false;

	memset(&Coding, 0, sizeof(Coding));
	Coding.Quality = XHTTP_QUALITY_MAX;
	Element = xrtHttpOwsTrim(Element);
	Coding.Element = Element;
	i = 0;
	while ( (i < Element.Size) &&
		__xrtHttpTokenByte((unsigned char)Element.Data[i]) ) {
		i++;
	}
	if ( i == 0 ) {
		return false;
	}
	Coding.Coding = (xstrview){ Element.Data, i };
	while ( (i < Element.Size) &&
		((Element.Data[i] == ' ') ||
		 (Element.Data[i] == '\t')) ) {
		i++;
	}
	if ( i != Element.Size ) {
		if ( Element.Data[i] != ';' ) {
			return false;
		}
		iStart = i + 1u;
		Parameters = xrtHttpOwsTrim((xstrview){
			Element.Data + iStart,
			Element.Size - iStart
		});
		if ( Parameters.Size == 0 ) {
			return false;
		}
	}

	/* trailers 是独立能力标记，不能携带传输参数或权重。 */
	if ( xrtHttpTokenEqual(
		Coding.Coding, XRT_STR_LITERAL("trailers")
	) ) {
		if ( Parameters.Size != 0 ) {
			return false;
		}
		Coding.Flags = XHTTP_TE_CODING_TRAILERS;
		memcpy(pOutput, &Coding, sizeof(Coding));
		return true;
	}

	/* q 必须是最后一个参数，并使用 weight 规定的紧凑 q= 形式。 */
	for ( ;; ) {
		iBefore = iOffset;
		Next = xrtHttpParamNext(
			Parameters, &iOffset, &Param
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		if ( bWeight ||
			((Param.Flags & XHTTP_PARAM_HAS_VALUE) == 0) ) {
			return false;
		}
		if ( xrtHttpTokenEqual(
			Param.Name, XRT_STR_LITERAL("q")
		) ) {
			if ( ((Param.Flags & XHTTP_PARAM_QUOTED) != 0) ||
				(Param.Name.Data + Param.Name.Size >=
				 Parameters.Data + Parameters.Size) ||
				(Param.Name.Data[Param.Name.Size] != '=') ||
				(Param.Value.Data !=
				 Param.Name.Data + Param.Name.Size + 1u) ||
				!xrtHttpQualityParse(
					Param.Value, &Coding.Quality
				) ) {
				return false;
			}
			bWeight = true;
			Coding.Flags |= XHTTP_TE_CODING_HAS_WEIGHT;
			iParameterEnd = (iBefore == 0) ?
				0 : (iBefore - 1u);
			Coding.Parameters = xrtHttpOwsTrim((xstrview){
				Parameters.Data, iParameterEnd
			});
			continue;
		}
		if ( Coding.ParameterCount == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		Coding.ParameterCount++;
	}
	if ( !bWeight ) {
		Coding.Parameters = Parameters;
	}
	if ( Coding.ParameterCount != 0 ) {
		Coding.Flags |= XHTTP_TE_CODING_HAS_PARAMETERS;
	}
	memcpy(pOutput, &Coding, sizeof(Coding));
	return true;
}



/* 从字段值读取下一个 TE 成员，不修改公开错误状态。 */
static xhttpnext __xrtHttpTeItemNext(
	xstrview Value,
	size_t iOffset,
	size_t* pNext,
	xhttptecoding* pCoding
)
{
	xhttptecoding Coding;
	xstrview Element;
	xhttpnext Next;

	memset(&Coding, 0, sizeof(Coding));
	Next = __xrtHttpQuotedListNext(
		Value, iOffset, pNext, &Element
	);
	if ( Next != XHTTP_NEXT_ITEM ) {
		memcpy(pCoding, &Coding, sizeof(Coding));
		return Next;
	}
	if ( !__xrtHttpTeCodingParseValue(
		Element, &Coding
	) ) {
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pCoding, &Coding, sizeof(Coding));
	return XHTTP_NEXT_ITEM;
}



/* 完整验证一个 TE 字段值，并可同时统计非空成员。 */
static bool __xrtHttpTeMeasure(
	xstrview Value,
	size_t* pCount
)
{
	xhttptecoding Coding;
	xhttpnext Next;
	size_t iNext;
	size_t iOffset = 0;
	size_t iCount = 0;

	for ( ;; ) {
		Next = __xrtHttpTeItemNext(
			Value, iOffset, &iNext, &Coding
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



/* 验证单字段游标只能由初始化函数和迭代器推进。 */
static bool __xrtHttpTeCursorValid(
	const xhttptecursor* pCursor,
	size_t iSize
)
{
	return (pCursor->Validated <= 1u) &&
		(pCursor->Offset <= iSize) &&
		!((pCursor->Validated == 0) &&
		  (pCursor->Offset != 0));
}



/* 完整预校验全部重复 TE 字段行。 */
static bool __xrtHttpTeFieldsValidate(
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
			Field.Name, XRT_STR_LITERAL("TE")
		) && !__xrtHttpTeMeasure(
			Field.Value, &iItems
		) ) {
			return false;
		}
	}
	return true;
}



/* 验证重复字段游标的跨字段状态。 */
static bool __xrtHttpTeFieldCursorValid(
	const xhttptefieldcursor* pCursor,
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



/* 初始化单字段 TE 游标。 */
XRT_API void xrtHttpTeCursorInit(xhttptecursor* pCursor)
{
	xhttptecursor Cursor;

	if ( !__xrtRangeValid(pCursor, sizeof(Cursor)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Cursor, 0, sizeof(Cursor));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
}



/* 初始化重复字段 TE 游标。 */
XRT_API void xrtHttpTeFieldCursorInit(
	xhttptefieldcursor* pCursor
)
{
	xhttptefieldcursor Cursor;

	if ( !__xrtRangeValid(pCursor, sizeof(Cursor)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Cursor, 0, sizeof(Cursor));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
}



/* 严格解析一个 TE 成员。 */
XRT_API bool xrtHttpTeCodingParse(
	xstrview Element,
	xhttptecoding* pCoding
)
{
	xhttptecoding Coding;

	memset(&Coding, 0, sizeof(Coding));
	if ( !__xrtHttpViewValid(Element) ||
		!__xrtRangeValid(pCoding, sizeof(Coding)) ||
		__xrtRangesOverlap(
			Element.Data, Element.Size,
			pCoding, sizeof(Coding)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pCoding, &Coding, sizeof(Coding));
	if ( !__xrtHttpTeCodingParseValue(
		Element, &Coding
	) ) {
		__xrtErrorSetValue();
		return false;
	}
	memcpy(pCoding, &Coding, sizeof(Coding));
	return true;
}



/* 完整验证一个 TE 字段值。 */
XRT_API bool xrtHttpTeValid(xstrview Value)
{
	size_t iCount;

	if ( !__xrtHttpViewValid(Value) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpTeMeasure(Value, &iCount) ) {
		__xrtErrorSetValue();
		return false;
	}
	return true;
}



/* 完整验证并统计一个 TE 字段值。 */
XRT_API bool xrtHttpTeCount(
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
	if ( !__xrtHttpTeMeasure(Value, &iCount) ) {
		__xrtErrorSetValue();
		return false;
	}
	memcpy(pCount, &iCount, sizeof(iCount));
	return true;
}



/* 按线路顺序迭代一个完整 TE 字段值。 */
XRT_API xhttpnext xrtHttpTeNext(
	xstrview Value,
	xhttptecursor* pCursor,
	xhttptecoding* pCoding
)
{
	xhttptecursor Cursor;
	xhttptecoding Coding;
	xhttpnext Next;
	size_t iIgnored;
	size_t iNext;

	if ( !__xrtHttpViewValid(Value) ||
		!__xrtRangeValid(pCursor, sizeof(Cursor)) ||
		!__xrtRangeValid(pCoding, sizeof(Coding)) ||
		__xrtRangesOverlap(
			Value.Data, Value.Size,
			pCursor, sizeof(Cursor)
		) || __xrtRangesOverlap(
			Value.Data, Value.Size,
			pCoding, sizeof(Coding)
		) || __xrtRangesOverlap(
			pCursor, sizeof(Cursor),
			pCoding, sizeof(Coding)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	memset(&Coding, 0, sizeof(Coding));
	memcpy(pCoding, &Coding, sizeof(Coding));
	if ( !__xrtHttpTeCursorValid(&Cursor, Value.Size) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( Cursor.Validated == 0 ) {
		if ( !__xrtHttpTeMeasure(Value, &iIgnored) ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Validated = 1u;
	}
	Next = __xrtHttpTeItemNext(
		Value, Cursor.Offset, &iNext, &Coding
	);
	if ( Next == XHTTP_NEXT_ERROR ) {
		__xrtErrorSetInvalidArgument();
		return Next;
	}
	Cursor.Offset = iNext;
	memcpy(pCoding, &Coding, sizeof(Coding));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
	return Next;
}



/* 跨重复 TE 字段行按线路顺序迭代全部成员。 */
XRT_API xhttpnext xrtHttpTeFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttptefieldcursor* pCursor,
	xhttptecoding* pCoding
)
{
	xhttptefieldcursor Cursor;
	xhttptecoding Coding;
	xhttpfield Field;
	xhttpnext Next;
	size_t iNext;

	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ||
		!__xrtRangeValid(pCursor, sizeof(Cursor)) ||
		!__xrtRangeValid(pCoding, sizeof(Coding)) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount, pCursor, sizeof(Cursor)
		) || __xrtHttpFieldArrayOverlap(
			pFields, iCount, pCoding, sizeof(Coding)
		) || __xrtRangesOverlap(
			pCursor, sizeof(Cursor),
			pCoding, sizeof(Coding)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	memset(&Coding, 0, sizeof(Coding));
	memcpy(pCoding, &Coding, sizeof(Coding));
	if ( !__xrtHttpTeFieldCursorValid(&Cursor, iCount) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( Cursor.Validated == 0 ) {
		if ( !__xrtHttpTeFieldsValidate(
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
			Field.Name, XRT_STR_LITERAL("TE")
		) ) {
			Cursor.Field++;
			Cursor.Offset = 0;
			continue;
		}
		if ( Cursor.Offset > Field.Value.Size ) {
			__xrtErrorSetInvalidArgument();
			return XHTTP_NEXT_ERROR;
		}
		Next = __xrtHttpTeItemNext(
			Field.Value, Cursor.Offset, &iNext, &Coding
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
		memcpy(pCoding, &Coding, sizeof(Coding));
		memcpy(pCursor, &Cursor, sizeof(Cursor));
		return XHTTP_NEXT_ITEM;
	}
	memset(&Coding, 0, sizeof(Coding));
	memcpy(pCoding, &Coding, sizeof(Coding));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
	return XHTTP_NEXT_END;
}



/* 完整解析重复 TE 字段并建立能力汇总。 */
XRT_API bool xrtHttpTeParse(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpteinfo* pOutput
)
{
	xhttptefieldcursor Cursor;
	xhttptecoding Coding;
	xhttpteinfo Info;
	xhttpfield Field;
	xhttpnext Next;
	size_t i;

	memset(&Info, 0, sizeof(Info));
	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ||
		!__xrtRangeValid(pOutput, sizeof(Info)) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount, pOutput, sizeof(Info)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pOutput, &Info, sizeof(Info));
	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( !xrtHttpFieldNameEqual(
			Field.Name, XRT_STR_LITERAL("TE")
		) ) {
			continue;
		}
		if ( Info.FieldCount == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		Info.FieldCount++;
		Info.Flags |= XHTTP_TE_PRESENT;
	}
	xrtHttpTeFieldCursorInit(&Cursor);
	for ( ;; ) {
		Next = xrtHttpTeFieldNext(
			pFields, iCount, &Cursor, &Coding
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			memcpy(pOutput, &Info, sizeof(Info));
			return true;
		}
		if ( Info.CodingCount == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		Info.CodingCount++;
		if ( (Coding.Flags &
			  XHTTP_TE_CODING_TRAILERS) != 0 ) {
			Info.Flags |= XHTTP_TE_ACCEPTS_TRAILERS;
			continue;
		}
		if ( Info.TransferCodingCount == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		Info.TransferCodingCount++;
		Info.Flags |= XHTTP_TE_HAS_TRANSFER_CODINGS;
	}
}



/* 查询一个传输编码在全部 TE 字段中的最高权重。 */
XRT_API uint16 xrtHttpTeQuality(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Wanted
)
{
	xhttptefieldcursor Cursor;
	xhttptecoding Coding;
	xhttpnext Next;
	uint16 iQuality = 0;

	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ||
		!xrtHttpTokenValid(Wanted) ||
		xrtHttpTokenEqual(
			Wanted, XRT_STR_LITERAL("trailers")
		) ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	xrtHttpTeFieldCursorInit(&Cursor);
	for ( ;; ) {
		Next = xrtHttpTeFieldNext(
			pFields, iCount, &Cursor, &Coding
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return 0;
		}
		if ( Next == XHTTP_NEXT_END ) {
			return iQuality;
		}
		if ( ((Coding.Flags &
			  XHTTP_TE_CODING_TRAILERS) == 0) &&
			xrtHttpTokenEqual(Coding.Coding, Wanted) &&
			(Coding.Quality > iQuality) ) {
			iQuality = Coding.Quality;
		}
	}
}



/* 判断全部有效 TE 字段是否包含裸 trailers 成员。 */
XRT_API xhttpnext xrtHttpTeAcceptsTrailers(
	const xhttpfield* pFields,
	size_t iCount
)
{
	xhttpteinfo Info;

	if ( !xrtHttpTeParse(pFields, iCount, &Info) ) {
		return XHTTP_NEXT_ERROR;
	}
	return (Info.Flags & XHTTP_TE_ACCEPTS_TRAILERS) != 0 ?
		XHTTP_NEXT_ITEM : XHTTP_NEXT_END;
}

#endif

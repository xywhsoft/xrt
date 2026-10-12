#include "../internal/xrt_http.h"

#include <xrt/http_via.h>



#if defined(XHTTP_FEATURE_HTTP_VIA)

/* 判断字节是否属于 Via 使用的线性空白。 */
static bool __xrtHttpViaRwsByte(uint8 iByte)
{
	return (iByte == (uint8)' ') || (iByte == (uint8)'\t');
}



/* 判断字节是否能直接出现在 HTTP comment 中。 */
static bool __xrtHttpViaCommentByte(uint8 iByte)
{
	return (iByte == (uint8)'\t') ||
		(iByte == (uint8)' ') ||
		((iByte >= UINT8_C(0x21)) &&
		 (iByte <= UINT8_C(0x27))) ||
		((iByte >= UINT8_C(0x2A)) &&
		 (iByte <= UINT8_C(0x5B))) ||
		((iByte >= UINT8_C(0x5D)) &&
		 (iByte <= UINT8_C(0x7E))) ||
		(iByte >= UINT8_C(0x80));
}



/* 严格扫描一个支持嵌套和 quoted-pair 的完整 comment。 */
static bool __xrtHttpViaCommentEnd(
	xstrview Text,
	size_t iStart,
	size_t* pEnd
)
{
	size_t iDepth = 1;
	size_t i = iStart + 1u;

	if ( (iStart >= Text.Size) ||
		(Text.Data[iStart] != '(') ) {
		return false;
	}
	while ( i < Text.Size ) {
		uint8 iByte = (uint8)Text.Data[i++];

		if ( iByte == (uint8)'(' ) {
			iDepth++;
			continue;
		}
		if ( iByte == (uint8)')' ) {
			iDepth--;
			if ( iDepth == 0 ) {
				*pEnd = i;
				return true;
			}
			continue;
		}
		if ( iByte == (uint8)'\\' ) {
			if ( (i >= Text.Size) ||
				!__xhttpQuotedPairByte(
					(unsigned char)Text.Data[i]
				) ) {
				return false;
			}
			i++;
			continue;
		}
		if ( !__xrtHttpViaCommentByte(iByte) ) {
			return false;
		}
	}
	return false;
}



/* 从当前位置扫描一个非空 token 并返回尾后位置。 */
static bool __xrtHttpViaTokenEnd(
	xstrview Text,
	size_t iStart,
	size_t* pEnd
)
{
	size_t i = iStart;

	while ( (i < Text.Size) &&
		__xhttpTokenByte((unsigned char)Text.Data[i]) ) {
		i++;
	}
	if ( i == iStart ) {
		return false;
	}
	*pEnd = i;
	return true;
}



/* 初始化单字段游标。 */
XHTTP_API void xrtHttpViaCursorInit(xhttpviacursor* pCursor)
{
	xhttpviacursor Cursor;

	if ( !xrtMemRangeValid(pCursor, sizeof(Cursor)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return;
	}
	memset(&Cursor, 0, sizeof(Cursor));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
}



/* 初始化重复字段游标。 */
XHTTP_API void xrtHttpViaFieldCursorInit(
	xhttpviafieldcursor* pCursor
)
{
	xhttpviafieldcursor Cursor;

	if ( !xrtMemRangeValid(pCursor, sizeof(Cursor)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return;
	}
	memset(&Cursor, 0, sizeof(Cursor));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
}



/* 严格解析一个 Via 元素。 */
XHTTP_API bool xrtHttpViaElementParse(
	xstrview Element,
	xhttpvia* pOutput
)
{
	xhttpvia Via;
	xstrview Text;
	size_t iFirst;
	size_t iSecond;
	size_t iBy;
	size_t iEnd;
	size_t i;

	if ( !__xhttpViewValid(Element) ||
		!xrtMemRangeValid(pOutput, sizeof(Via)) ||
		xrtMemRangesOverlap(
			pOutput, sizeof(Via), Element.Data, Element.Size
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memset(&Via, 0, sizeof(Via));
	Text = xrtHttpOwsTrim(Element);
	Via.Element = Text;
	if ( (Text.Size == 0) ||
		!__xrtHttpViaTokenEnd(Text, 0, &iFirst) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	i = iFirst;
	if ( (i < Text.Size) && (Text.Data[i] == '/') ) {
		if ( !__xrtHttpViaTokenEnd(Text, i + 1u, &iSecond) ) {
			xrtSetErrorKind(XERR_VALUE);
			return false;
		}
		Via.ProtocolName = (xstrview){ Text.Data, i };
		Via.ProtocolVersion = (xstrview){
			Text.Data + i + 1u,
			iSecond - i - 1u
		};
		Via.Flags |= XHTTP_VIA_HAS_PROTOCOL_NAME;
		i = iSecond;
	} else {
		Via.ProtocolVersion = (xstrview){ Text.Data, i };
	}
	if ( (i >= Text.Size) ||
		!__xrtHttpViaRwsByte((uint8)Text.Data[i]) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	while ( (i < Text.Size) &&
		__xrtHttpViaRwsByte((uint8)Text.Data[i]) ) {
		i++;
	}
	iBy = i;
	if ( !__xrtHttpViaTokenEnd(Text, i, &iEnd) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	Via.Pseudonym = (xstrview){ Text.Data + i, iEnd - i };
	i = iEnd;
	if ( (i < Text.Size) && (Text.Data[i] == ':') ) {
		size_t iPort = ++i;

		while ( (i < Text.Size) &&
			(Text.Data[i] >= '0') &&
			(Text.Data[i] <= '9') ) {
			i++;
		}
		Via.Port = (xstrview){
			Text.Data + iPort, i - iPort
		};
		Via.Flags |= XHTTP_VIA_HAS_PORT;
	}
	Via.ReceivedBy = (xstrview){ Text.Data + iBy, i - iBy };
	if ( i < Text.Size ) {
		if ( !__xrtHttpViaRwsByte((uint8)Text.Data[i]) ) {
			xrtSetErrorKind(XERR_VALUE);
			return false;
		}
		while ( (i < Text.Size) &&
			__xrtHttpViaRwsByte((uint8)Text.Data[i]) ) {
			i++;
		}
		if ( !__xrtHttpViaCommentEnd(Text, i, &iEnd) ||
			(iEnd != Text.Size) ) {
			xrtSetErrorKind(XERR_VALUE);
			return false;
		}
		Via.Comment = (xstrview){ Text.Data + i, iEnd - i };
		Via.Flags |= XHTTP_VIA_HAS_COMMENT;
	}
	memcpy(pOutput, &Via, sizeof(Via));
	return true;
}



/* 读取下一个非空 Via 列表成员并正确跳过 comment 中的逗号。 */
static xhttpnext __xrtHttpViaMemberNext(
	xstrview Value,
	size_t iOffset,
	size_t* pNext,
	xstrview* pElement
)
{
	size_t iStart;
	size_t iDepth;
	size_t i;

	i = iOffset;
	while ( i < Value.Size ) {
		while ( (i < Value.Size) &&
			__xrtHttpViaRwsByte((uint8)Value.Data[i]) ) {
			i++;
		}
		if ( (i >= Value.Size) || (Value.Data[i] != ',') ) {
			break;
		}
		i++;
	}
	if ( i == Value.Size ) {
		*pNext = i;
		*pElement = (xstrview){ NULL, 0 };
		return XHTTP_NEXT_END;
	}
	iStart = i;
	iDepth = 0;
	while ( i < Value.Size ) {
		uint8 iByte = (uint8)Value.Data[i];

		if ( (iByte == (uint8)',') && (iDepth == 0) ) {
			break;
		}
		if ( iByte == (uint8)'(' ) {
			iDepth++;
			i++;
			continue;
		}
		if ( iByte == (uint8)')' ) {
			if ( iDepth == 0 ) {
				xrtSetErrorKind(XERR_VALUE);
				return XHTTP_NEXT_ERROR;
			}
			iDepth--;
			i++;
			continue;
		}
		if ( (iByte == (uint8)'\\') && (iDepth != 0) ) {
			if ( (i + 1u) >= Value.Size ) {
				xrtSetErrorKind(XERR_VALUE);
				return XHTTP_NEXT_ERROR;
			}
			i += 2u;
			continue;
		}
		i++;
	}
	if ( iDepth != 0 ) {
		xrtSetErrorKind(XERR_VALUE);
		return XHTTP_NEXT_ERROR;
	}
	*pElement = xrtHttpOwsTrim((xstrview){
		Value.Data + iStart, i - iStart
	});
	*pNext = (i < Value.Size) ? (i + 1u) : i;
	if ( pElement->Size == 0 ) {
		xrtSetErrorKind(XERR_VALUE);
		return XHTTP_NEXT_ERROR;
	}
	return XHTTP_NEXT_ITEM;
}



/* 完整验证一个 Via 字段值。 */
static bool __xrtHttpViaValueValidate(xstrview Value)
{
	xhttpvia Via;
	xstrview Element;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iNext;

	if ( !__xhttpViewValid(Value) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	for ( ;; ) {
		Next = __xrtHttpViaMemberNext(
			Value, iOffset, &iNext, &Element
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		if ( !xrtHttpViaElementParse(Element, &Via) ) {
			return false;
		}
		iOffset = iNext;
	}
	return true;
}



/* 严格验证完整 Via 字段值。 */
XHTTP_API bool xrtHttpViaValid(xstrview Value)
{
	return __xrtHttpViaValueValidate(Value);
}



/* 验证单字段游标状态。 */
static bool __xrtHttpViaCursorValid(
	const xhttpviacursor* pCursor,
	xstrview Value
)
{
	if ( pCursor->Validated > 1u ) {
		return false;
	}
	if ( pCursor->Validated == 0 ) {
		return (pCursor->Source == NULL) &&
			(pCursor->Size == 0) &&
			(pCursor->Offset == 0);
	}
	return (pCursor->Source == Value.Data) &&
		(pCursor->Size == Value.Size) &&
		(pCursor->Offset <= Value.Size);
}



/* 迭代一个完整 Via 字段值。 */
XHTTP_API xhttpnext xrtHttpViaNext(
	xstrview Value,
	xhttpviacursor* pCursor,
	xhttpvia* pOutput
)
{
	xhttpviacursor Cursor;
	xhttpvia Via;
	xstrview Element;
	xhttpnext Next;
	size_t iNext;

	if ( !__xhttpViewValid(Value) ||
		!xrtMemRangeValid(pCursor, sizeof(Cursor)) ||
		!xrtMemRangeValid(pOutput, sizeof(Via)) ||
		xrtMemRangesOverlap(
			pCursor, sizeof(Cursor), pOutput, sizeof(Via)
		) || xrtMemRangesOverlap(
			pCursor, sizeof(Cursor), Value.Data, Value.Size
		) || xrtMemRangesOverlap(
			pOutput, sizeof(Via), Value.Data, Value.Size
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	memset(&Via, 0, sizeof(Via));
	memcpy(pOutput, &Via, sizeof(Via));
	if ( !__xrtHttpViaCursorValid(&Cursor, Value) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_NEXT_ERROR;
	}
	if ( Cursor.Validated == 0 ) {
		if ( !__xrtHttpViaValueValidate(Value) ) {
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Source = Value.Data;
		Cursor.Size = Value.Size;
		Cursor.Validated = 1u;
	}
	Next = __xrtHttpViaMemberNext(
		Value, Cursor.Offset, &iNext, &Element
	);
	if ( Next == XHTTP_NEXT_END ) {
		Cursor.Offset = iNext;
		memcpy(pOutput, &Via, sizeof(Via));
		memcpy(pCursor, &Cursor, sizeof(Cursor));
		return Next;
	}
	if ( (Next == XHTTP_NEXT_ERROR) ||
		!xrtHttpViaElementParse(Element, &Via) ) {
		return XHTTP_NEXT_ERROR;
	}
	Cursor.Offset = iNext;
	memcpy(pOutput, &Via, sizeof(Via));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
	return XHTTP_NEXT_ITEM;
}



/* 完整预校验所有重复 Via 字段行。 */
static bool __xrtHttpViaFieldsValidate(
	const xhttpfield* pFields,
	size_t iCount
)
{
	xhttpfield Field;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		__xhttpFieldLoad(pFields, i, &Field);
		if ( xrtHttpFieldNameEqual(
			Field.Name, XRT_STR_LITERAL("Via")
		) && !__xrtHttpViaValueValidate(Field.Value) ) {
			return false;
		}
	}
	return true;
}



/* 验证重复字段游标状态。 */
static bool __xrtHttpViaFieldCursorValid(
	const xhttpviafieldcursor* pCursor,
	const xhttpfield* pFields,
	size_t iCount
)
{
	if ( pCursor->Validated > 1u ) {
		return false;
	}
	if ( pCursor->Validated == 0 ) {
		return (pCursor->Source == NULL) &&
			(pCursor->Count == 0) &&
			(pCursor->Field == 0) &&
			(pCursor->Offset == 0);
	}
	return (pCursor->Source == pFields) &&
		(pCursor->Count == iCount) &&
		(pCursor->Field <= iCount) &&
		!((pCursor->Field == iCount) &&
		  (pCursor->Offset != 0));
}



/* 跨重复 Via 字段行迭代代理节点。 */
XHTTP_API xhttpnext xrtHttpViaFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpviafieldcursor* pCursor,
	xhttpvia* pOutput
)
{
	xhttpviafieldcursor Cursor;
	xhttpfield Field;
	xhttpvia Via;
	xstrview Element;
	xhttpnext Next;
	size_t iNext;

	if ( !__xhttpFieldArrayValid(pFields, iCount) ||
		!xrtMemRangeValid(pCursor, sizeof(Cursor)) ||
		!xrtMemRangeValid(pOutput, sizeof(Via)) ||
		__xhttpFieldArrayOverlap(
			pFields, iCount, pCursor, sizeof(Cursor)
		) || __xhttpFieldArrayOverlap(
			pFields, iCount, pOutput, sizeof(Via)
		) || xrtMemRangesOverlap(
			pCursor, sizeof(Cursor), pOutput, sizeof(Via)
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	memset(&Via, 0, sizeof(Via));
	memcpy(pOutput, &Via, sizeof(Via));
	if ( !__xrtHttpViaFieldCursorValid(
		&Cursor, pFields, iCount
	) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_NEXT_ERROR;
	}
	if ( Cursor.Validated == 0 ) {
		if ( !__xrtHttpViaFieldsValidate(pFields, iCount) ) {
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Source = pFields;
		Cursor.Count = iCount;
		Cursor.Validated = 1u;
	}
	while ( Cursor.Field < iCount ) {
		__xhttpFieldLoad(pFields, Cursor.Field, &Field);
		if ( !xrtHttpFieldNameEqual(
			Field.Name, XRT_STR_LITERAL("Via")
		) ) {
			Cursor.Field++;
			Cursor.Offset = 0;
			continue;
		}
		Next = __xrtHttpViaMemberNext(
			Field.Value, Cursor.Offset, &iNext, &Element
		);
		if ( Next == XHTTP_NEXT_END ) {
			Cursor.Field++;
			Cursor.Offset = 0;
			continue;
		}
		if ( (Next == XHTTP_NEXT_ERROR) ||
			!xrtHttpViaElementParse(Element, &Via) ) {
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Offset = iNext;
		memcpy(pOutput, &Via, sizeof(Via));
		memcpy(pCursor, &Cursor, sizeof(Cursor));
		return XHTTP_NEXT_ITEM;
	}
	memcpy(pCursor, &Cursor, sizeof(Cursor));
	return XHTTP_NEXT_END;
}



/* 解码完整 Via comment。 */
XHTTP_API bool xrtHttpViaCommentDecode(
	xstrview Comment,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	size_t iRequired = 0;
	size_t iEnd;
	size_t iPosition = 0;
	size_t i;
	bytes pBytes = (bytes)pOutput;

	if ( !__xhttpViewValid(Comment) ||
		!xrtMemRangeValid(pSize, sizeof(iRequired)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) &&
		 !xrtMemRangeValid(pOutput, iCapacity)) ||
		xrtMemRangesOverlap(
			pSize, sizeof(iRequired), Comment.Data, Comment.Size
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( !__xrtHttpViaCommentEnd(Comment, 0, &iEnd) ||
		(iEnd != Comment.Size) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	for ( i = 1u; (i + 1u) < Comment.Size; i++ ) {
		if ( Comment.Data[i] == '\\' ) {
			i++;
		}
		iRequired++;
	}
	if ( (pOutput != NULL) &&
		(xrtMemRangesOverlap(
			pOutput, iRequired, Comment.Data, Comment.Size
		 ) || xrtMemRangesOverlap(
			pOutput, iRequired, pSize, sizeof(iRequired)
		 )) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(pSize, &iRequired, sizeof(iRequired));
	if ( pOutput == NULL ) {
		return true;
	}
	if ( iCapacity < iRequired ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	for ( i = 1u; (i + 1u) < Comment.Size; i++ ) {
		if ( Comment.Data[i] == '\\' ) {
			i++;
		}
		pBytes[iPosition++] = (uint8)Comment.Data[i];
	}
	return true;
}

#endif

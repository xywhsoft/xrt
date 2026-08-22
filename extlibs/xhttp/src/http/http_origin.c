#include "../internal/xrt_http_origin.h"



#if defined(XHTTP_FEATURE_HTTP_ORIGIN)

/* 判断来源 URL 是否能提取出可比较的 Origin 三元组。 */
static bool __xrtHttpOriginSourceUrlValid(const xurl* pUrl)
{
	const uint32 iRequired = XURL_HAS_SCHEME |
		XURL_HAS_AUTHORITY | XURL_HAS_HOST;
	bool bExplicitPort;

	if ( !__xrtUrlValueValid(pUrl) ) {
		return false;
	}
	bExplicitPort = ((pUrl->Flags & XURL_HAS_PORT) != 0) &&
		((pUrl->Flags & XURL_PORT_EMPTY) == 0);
	return (!bExplicitPort ||
		((pUrl->Flags & XURL_PORT_VALUE) != 0)) &&
		((pUrl->Flags & iRequired) == iRequired) &&
		((pUrl->Flags & XURL_HAS_USERINFO) == 0) &&
		(pUrl->Scheme.Size != 0) &&
		(pUrl->Host.Size != 0);
}



/* 判断 Origin 结构的存在位、文本和 URL 保持一致。 */
bool __xrtHttpOriginValueValid(
	const xhttporigin* pOrigin
)
{
	if ( !__xrtHttpViewValid(pOrigin->Text) ||
		((pOrigin->Flags & ~XHTTP_ORIGIN_NULL) != 0) ) {
		return false;
	}
	if ( (pOrigin->Flags & XHTTP_ORIGIN_NULL) == 0 ) {
		return __xrtHttpOriginSourceUrlValid(&pOrigin->Url) &&
			(pOrigin->Url.Path.Size == 0) &&
			((pOrigin->Url.Flags & (
				XURL_HAS_QUERY | XURL_HAS_FRAGMENT
			)) == 0);
	}
	return (pOrigin->Url.Flags == 0) &&
		(pOrigin->Url.Port == 0) &&
		(pOrigin->Url.Scheme.Size == 0) &&
		(pOrigin->Url.Authority.Size == 0) &&
		(pOrigin->Url.UserInfo.Size == 0) &&
		(pOrigin->Url.Host.Size == 0) &&
		(pOrigin->Url.PortText.Size == 0) &&
		(pOrigin->Url.Path.Size == 0) &&
		(pOrigin->Url.Query.Size == 0) &&
		(pOrigin->Url.Fragment.Size == 0);
}



/* 按 ASCII 大小写不敏感规则比较两个合法视图。 */
static bool __xrtHttpOriginCaseEqual(
	xstrview Left,
	xstrview Right
)
{
	size_t i;

	if ( Left.Size != Right.Size ) {
		return false;
	}
	for ( i = 0; i < Left.Size; i++ ) {
		if ( __xhttpAsciiLower((uint8)Left.Data[i]) !=
			__xhttpAsciiLower((uint8)Right.Data[i]) ) {
			return false;
		}
	}
	return true;
}



/* 比较同 scheme 的显式端口与默认端口。 */
static bool __xrtHttpOriginPortSame(
	const xurl* pLeft,
	const xurl* pRight
)
{
	bool bLeft = (pLeft->Flags & (
		XURL_HAS_PORT | XURL_PORT_EMPTY
	)) == XURL_HAS_PORT;
	bool bRight = (pRight->Flags & (
		XURL_HAS_PORT | XURL_PORT_EMPTY
	)) == XURL_HAS_PORT;
	uint16 iDefault;

	if ( bLeft && bRight ) {
		return pLeft->Port == pRight->Port;
	}
	if ( bLeft == bRight ) {
		return true;
	}
	iDefault = xrtUrlDefaultPort(pLeft->Scheme);
	if ( iDefault == 0 ) {
		return false;
	}
	return (bLeft ? pLeft->Port : pRight->Port) == iDefault;
}



/* 比较两个已经验证的非 null Origin。 */
bool __xrtHttpOriginTupleSame(
	const xhttporigin* pLeft,
	const xhttporigin* pRight
)
{
	return __xrtHttpOriginCaseEqual(
		pLeft->Url.Scheme, pRight->Url.Scheme
	) && xrtHttpHostEqual(
		pLeft->Url.Host, pRight->Url.Host
	) && __xrtHttpOriginPortSame(
		&pLeft->Url, &pRight->Url
	);
}



/* 初始化 Origin 列表游标。 */
XRT_API void xrtHttpOriginCursorInit(
	xhttporigincursor* pCursor
)
{
	xhttporigincursor Cursor;

	if ( !__xrtRangeValid(pCursor, sizeof(Cursor)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Cursor, 0, sizeof(Cursor));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
}



/* 建立没有三元组身份的 null Origin。 */
XRT_API void xrtHttpOriginNull(xhttporigin* pOrigin)
{
	xhttporigin Origin;

	if ( !__xrtRangeValid(pOrigin, sizeof(Origin)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Origin, 0, sizeof(Origin));
	Origin.Flags = XHTTP_ORIGIN_NULL;
	memcpy(pOrigin, &Origin, sizeof(Origin));
}



/* 严格解析一个 null 或 serialized-origin。 */
XRT_API bool xrtHttpOriginParse(
	xstrview Text,
	xhttporigin* pOutput
)
{
	xhttporigin Origin;
	xstrview Value;

	if ( !__xrtHttpViewValid(Text) ||
		!__xrtRangeValid(pOutput, sizeof(Origin)) ||
		__xrtRangesOverlap(
			pOutput, sizeof(Origin), Text.Data, Text.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Origin, 0, sizeof(Origin));
	Value = xrtHttpOwsTrim(Text);
	Origin.Text = Value;
	if ( __xrtHttpViewEqual(Value, XRT_STR_LITERAL("null")) ) {
		Origin.Flags = XHTTP_ORIGIN_NULL;
		memcpy(pOutput, &Origin, sizeof(Origin));
		return true;
	}
	if ( !__xrtUrlParseValue(Value, &Origin.Url) ||
		!__xrtHttpOriginSourceUrlValid(&Origin.Url) ||
		(Origin.Url.Path.Size != 0) ||
		((Origin.Url.Flags & (XURL_HAS_QUERY |
		 XURL_HAS_FRAGMENT)) != 0) ) {
		__xrtErrorSetValue();
		return false;
	}
	memcpy(pOutput, &Origin, sizeof(Origin));
	return true;
}



/* 判断输出是否覆盖 Origin 描述符或它借用的任一组件。 */
bool __xrtHttpOriginOverlap(
	const xhttporigin* pOrigin,
	const void* pOutput,
	size_t iSize
)
{
	const xurl* pUrl = &pOrigin->Url;
	xstrview Views[9];
	size_t i;

	if ( __xrtRangesOverlap(
		pOrigin, sizeof(*pOrigin), pOutput, iSize
	) ) {
		return true;
	}
	Views[0] = pOrigin->Text;
	Views[1] = pUrl->Scheme;
	Views[2] = pUrl->Authority;
	Views[3] = pUrl->UserInfo;
	Views[4] = pUrl->Host;
	Views[5] = pUrl->PortText;
	Views[6] = pUrl->Path;
	Views[7] = pUrl->Query;
	Views[8] = pUrl->Fragment;
	for ( i = 0; i < 9u; i++ ) {
		if ( __xrtRangesOverlap(
			Views[i].Data, Views[i].Size, pOutput, iSize
		) ) {
			return true;
		}
	}
	return false;
}



/* 从 URL 提取不含资源路径的 Origin 三元组。 */
XRT_API bool xrtHttpOriginFromUrl(
	const xurl* pUrl,
	xhttporigin* pOutput
)
{
	xhttporigin Origin;
	xurl Url;

	if ( !__xrtRangeValid(pUrl, sizeof(Url)) ||
		!__xrtRangeValid(pOutput, sizeof(Origin)) ||
		__xrtRangesOverlap(
			pUrl, sizeof(Url), pOutput, sizeof(Origin)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Url, pUrl, sizeof(Url));
	if ( !__xrtHttpOriginSourceUrlValid(&Url) ) {
		__xrtErrorSetValue();
		return false;
	}
	memset(&Origin, 0, sizeof(Origin));
	Origin.Url = Url;
	Origin.Url.Flags &= ~(XURL_HAS_QUERY | XURL_HAS_FRAGMENT);
	Origin.Url.Path = (xstrview){ NULL, 0 };
	Origin.Url.Query = (xstrview){ NULL, 0 };
	Origin.Url.Fragment = (xstrview){ NULL, 0 };
	if ( __xrtHttpOriginOverlap(
		&Origin, pOutput, sizeof(Origin)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pOutput, &Origin, sizeof(Origin));
	return true;
}



/* 返回去除字段两端 OWS 后的列表边界。 */
static bool __xrtHttpOriginBounds(
	xstrview Value,
	size_t* pStart,
	size_t* pEnd
)
{
	xstrview Trimmed;

	if ( !__xrtHttpViewValid(Value) ) {
		return false;
	}
	Trimmed = xrtHttpOwsTrim(Value);
	*pStart = (Trimmed.Size == 0) ? 0 :
		(size_t)(Trimmed.Data - Value.Data);
	*pEnd = *pStart + Trimmed.Size;
	return Trimmed.Size != 0;
}



/* 从已经去除两端 OWS 的 Origin 列表读取下一项。 */
static xhttpnext __xrtHttpOriginMemberNext(
	xstrview Value,
	size_t iOffset,
	size_t iEnd,
	size_t* pNext,
	xstrview* pText
)
{
	size_t i = iOffset;

	if ( i == iEnd ) {
		*pNext = i;
		*pText = (xstrview){ NULL, 0 };
		return XHTTP_NEXT_END;
	}
	while ( (i < iEnd) && (Value.Data[i] != ' ') ) {
		if ( Value.Data[i] == '\t' ) {
			return XHTTP_NEXT_ERROR;
		}
		i++;
	}
	*pText = (xstrview){ Value.Data + iOffset, i - iOffset };
	if ( pText->Size == 0 ) {
		return XHTTP_NEXT_ERROR;
	}
	if ( i == iEnd ) {
		*pNext = i;
		return XHTTP_NEXT_ITEM;
	}
	if ( ((i + 1u) == iEnd) || (Value.Data[i + 1u] == ' ') ||
		(Value.Data[i + 1u] == '\t') ) {
		return XHTTP_NEXT_ERROR;
	}
	*pNext = i + 1u;
	return XHTTP_NEXT_ITEM;
}



/* 完整验证 Origin 字段值并返回可迭代边界。 */
static bool __xrtHttpOriginListValidate(
	xstrview Value,
	size_t* pStart,
	size_t* pEnd
)
{
	xhttporigin Origin;
	xstrview Text;
	xhttpnext Next;
	size_t iOffset;
	size_t iNext;
	bool bNull = false;
	size_t iCount = 0;

	if ( !__xrtHttpOriginBounds(Value, pStart, pEnd) ) {
		__xrtErrorSetValue();
		return false;
	}
	iOffset = *pStart;
	for ( ;; ) {
		Next = __xrtHttpOriginMemberNext(
			Value, iOffset, *pEnd, &iNext, &Text
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			__xrtErrorSetValue();
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		if ( !xrtHttpOriginParse(Text, &Origin) ) {
			return false;
		}
		bNull = bNull ||
			((Origin.Flags & XHTTP_ORIGIN_NULL) != 0);
		iCount++;
		iOffset = iNext;
	}
	if ( (iCount == 0) || (bNull && (iCount != 1u)) ) {
		__xrtErrorSetValue();
		return false;
	}
	return true;
}



/* 严格验证完整 Origin 字段值。 */
XRT_API bool xrtHttpOriginValid(xstrview Value)
{
	size_t iStart;
	size_t iEnd;

	if ( !__xrtHttpViewValid(Value) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtHttpOriginListValidate(
		Value, &iStart, &iEnd
	);
}



/* 验证 Origin 列表游标状态。 */
static bool __xrtHttpOriginCursorValid(
	const xhttporigincursor* pCursor,
	xstrview Value
)
{
	if ( pCursor->Validated > 1u ) {
		return false;
	}
	if ( pCursor->Validated == 0 ) {
		return (pCursor->Source == NULL) &&
			(pCursor->Size == 0) &&
			(pCursor->Offset == 0) &&
			(pCursor->End == 0);
	}
	return (pCursor->Source == Value.Data) &&
		(pCursor->Size == Value.Size) &&
		(pCursor->Offset <= pCursor->End) &&
		(pCursor->End <= Value.Size);
}



/* 完整预校验后迭代 Origin 字段值。 */
XRT_API xhttpnext xrtHttpOriginNext(
	xstrview Value,
	xhttporigincursor* pCursor,
	xhttporigin* pOutput
)
{
	xhttporigincursor Cursor;
	xhttporigin Origin;
	xstrview Text;
	xhttpnext Next;
	size_t iNext;
	size_t iStart;
	size_t iEnd;

	if ( !__xrtHttpViewValid(Value) ||
		!__xrtRangeValid(pCursor, sizeof(Cursor)) ||
		!__xrtRangeValid(pOutput, sizeof(Origin)) ||
		__xrtRangesOverlap(
			pCursor, sizeof(Cursor), pOutput, sizeof(Origin)
		) || __xrtRangesOverlap(
			pCursor, sizeof(Cursor), Value.Data, Value.Size
		) || __xrtRangesOverlap(
			pOutput, sizeof(Origin), Value.Data, Value.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	memset(&Origin, 0, sizeof(Origin));
	memcpy(pOutput, &Origin, sizeof(Origin));
	if ( !__xrtHttpOriginCursorValid(&Cursor, Value) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( Cursor.Validated == 0 ) {
		if ( !__xrtHttpOriginListValidate(
			Value, &iStart, &iEnd
		) ) {
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Source = Value.Data;
		Cursor.Size = Value.Size;
		Cursor.Offset = iStart;
		Cursor.End = iEnd;
		Cursor.Validated = 1u;
	}
	Next = __xrtHttpOriginMemberNext(
		Value, Cursor.Offset, Cursor.End, &iNext, &Text
	);
	if ( Next == XHTTP_NEXT_END ) {
		memcpy(pOutput, &Origin, sizeof(Origin));
		memcpy(pCursor, &Cursor, sizeof(Cursor));
		return Next;
	}
	if ( (Next == XHTTP_NEXT_ERROR) ||
		!xrtHttpOriginParse(Text, &Origin) ) {
		return XHTTP_NEXT_ERROR;
	}
	Cursor.Offset = iNext;
	memcpy(pOutput, &Origin, sizeof(Origin));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
	return XHTTP_NEXT_ITEM;
}



/* 读取唯一 Origin 字段和单一值。 */
XRT_API xhttpnext xrtHttpOriginFields(
	const xhttpfield* pFields,
	size_t iCount,
	xhttporigin* pOutput
)
{
	const xhttpfield* pFound = NULL;
	xhttpfield Field;
	xhttporigin Origin;
	xhttpnext Next;

	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ||
		!__xrtRangeValid(pOutput, sizeof(Origin)) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount, pOutput, sizeof(Origin)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memset(&Origin, 0, sizeof(Origin));
	Next = xrtHttpFieldGetUnique(
		pFields, iCount, XRT_STR_LITERAL("Origin"), &pFound
	);
	if ( Next != XHTTP_NEXT_ITEM ) {
		memcpy(pOutput, &Origin, sizeof(Origin));
		return Next;
	}
	memcpy(&Field, pFound, sizeof(Field));
	if ( !xrtHttpOriginParse(Field.Value, &Origin) ) {
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pOutput, &Origin, sizeof(Origin));
	return XHTTP_NEXT_ITEM;
}



/* 比较两个 Origin 三元组。 */
XRT_API bool xrtHttpOriginSame(
	const xhttporigin* pLeft,
	const xhttporigin* pRight
)
{
	xhttporigin Left;
	xhttporigin Right;

	if ( !__xrtRangeValid(pLeft, sizeof(Left)) ||
		!__xrtRangeValid(pRight, sizeof(Right)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Left, pLeft, sizeof(Left));
	memcpy(&Right, pRight, sizeof(Right));
	if ( !__xrtHttpOriginValueValid(&Left) ||
		!__xrtHttpOriginValueValid(&Right) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( ((Left.Flags | Right.Flags) &
		XHTTP_ORIGIN_NULL) != 0 ) {
		return false;
	}
	return __xrtHttpOriginTupleSame(&Left, &Right);
}

#endif

#include "../internal/xrt_http_link.h"
#include "../internal/xrt_url.h"



#if defined(XHTTP_FEATURE_HTTP_LINK)

/* 判断参数带有值。 */
static bool __xrtHttpLinkParamHasValue(const xhttpparam* pParam)
{
	return (pParam->Flags & XHTTP_PARAM_HAS_VALUE) != 0;
}



/* 判断参数的解码值非空。 */
static bool __xrtHttpLinkParamNonempty(const xhttpparam* pParam)
{
	size_t iSize;

	return __xrtHttpLinkParamHasValue(pParam) &&
		xrtHttpParamValueWrite(pParam, NULL, 0, &iSize) &&
		(iSize != 0);
}



/* 记录首个已知参数，后续重复项按 RFC 8288 忽略。 */
static bool __xrtHttpLinkKnownFirst(
	xhttplink* pLink,
	const xhttpparam* pParam,
	uint32 iFlag,
	xhttpparam* pSlot
)
{
	if ( (pLink->Flags & iFlag) != 0 ) {
		return false;
	}
	*pSlot = *pParam;
	pLink->Flags |= iFlag;
	return true;
}



/* 验证并记录一个 Link 参数的标准语义。 */
static bool __xrtHttpLinkParamApply(
	xhttplink* pLink,
	const xhttpparam* pParam
)
{
	xhttpextvalue ExtValue;

	if ( xrtHttpTokenEqual(
		pParam->Name, XRT_STR_LITERAL("rel")
	) ) {
		if ( !__xrtHttpLinkKnownFirst(
			pLink, pParam, XHTTP_LINK_HAS_REL, &pLink->Rel
		) ) {
			return true;
		}
		return __xrtHttpLinkRelationsParamValid(pParam);
	}
	if ( xrtHttpTokenEqual(
		pParam->Name, XRT_STR_LITERAL("anchor")
	) ) {
		if ( !__xrtHttpLinkKnownFirst(
			pLink, pParam,
			XHTTP_LINK_HAS_ANCHOR, &pLink->Anchor
		) ) {
			return true;
		}
		return __xrtHttpLinkUriParamValid(pParam);
	}
	if ( xrtHttpTokenEqual(
		pParam->Name, XRT_STR_LITERAL("rev")
	) ) {
		if ( !__xrtHttpLinkKnownFirst(
			pLink, pParam, XHTTP_LINK_HAS_REV, &pLink->Rev
		) ) {
			return true;
		}
		return __xrtHttpLinkRelationsParamValid(pParam);
	}
	if ( xrtHttpTokenEqual(
		pParam->Name, XRT_STR_LITERAL("hreflang")
	) ) {
		if ( !__xrtHttpLinkLanguageParamValid(pParam) ||
			(pLink->HrefLangCount == SIZE_MAX) ) {
			return false;
		}
		pLink->HrefLangCount++;
		return true;
	}
	if ( xrtHttpTokenEqual(
		pParam->Name, XRT_STR_LITERAL("media")
	) ) {
		if ( !__xrtHttpLinkKnownFirst(
			pLink, pParam,
			XHTTP_LINK_HAS_MEDIA, &pLink->Media
		) ) {
			return true;
		}
		return __xrtHttpLinkParamNonempty(pParam);
	}
	if ( xrtHttpTokenEqual(
		pParam->Name, XRT_STR_LITERAL("title")
	) ) {
		if ( !__xrtHttpLinkKnownFirst(
			pLink, pParam,
			XHTTP_LINK_HAS_TITLE, &pLink->Title
		) ) {
			return true;
		}
		return __xrtHttpLinkParamHasValue(pParam);
	}
	if ( xrtHttpTokenEqual(
		pParam->Name, XRT_STR_LITERAL("title*")
	) ) {
		if ( !__xrtHttpLinkKnownFirst(
			pLink, pParam,
			XHTTP_LINK_HAS_TITLE_EXT, &pLink->TitleExt
		) ) {
			return true;
		}
		return __xrtHttpLinkParamHasValue(pParam) &&
			((pParam->Flags & XHTTP_PARAM_QUOTED) == 0) &&
			__xrtHttpExtValueSplit(pParam->Value, &ExtValue);
	}
	if ( xrtHttpTokenEqual(
		pParam->Name, XRT_STR_LITERAL("type")
	) ) {
		if ( !__xrtHttpLinkKnownFirst(
			pLink, pParam,
			XHTTP_LINK_HAS_TYPE, &pLink->Type
		) ) {
			return true;
		}
		return __xrtHttpLinkTypeParamValid(pParam);
	}
	return true;
}



/* 严格解析一个 link-value 到局部结果。 */
static bool __xrtHttpLinkElementParseValue(
	xstrview Element,
	xhttplink* pLink
)
{
	xhttplink Link;
	xhttpparam Param;
	xhttpnext Next;
	xstrview Rest;
	xurl Target;
	cstr sClose;
	size_t iOffset = 0;
	size_t iRest = 0;

	memset(&Link, 0, sizeof(Link));
	Element = xrtHttpOwsTrim(Element);
	if ( (Element.Size < 2u) ||
		(Element.Data[0] != '<') ) {
		return false;
	}
	sClose = (cstr)memchr(
		Element.Data + 1u, '>', Element.Size - 1u
	);
	if ( sClose == NULL ) {
		return false;
	}
	Link.Element = Element;
	Link.Target = (xstrview){
		Element.Data + 1u,
		(size_t)(sClose - Element.Data - 1u)
	};
	if ( !__xrtUrlParseValue(Link.Target, &Target) ) {
		return false;
	}
	Rest = (xstrview){
		sClose + 1u,
		Element.Size - (size_t)(sClose + 1u - Element.Data)
	};
	while ( (iRest < Rest.Size) &&
		((Rest.Data[iRest] == ' ') ||
		 (Rest.Data[iRest] == '\t')) ) {
		iRest++;
	}
	if ( iRest == Rest.Size ) {
		return false;
	}
	if ( Rest.Data[iRest] != ';' ) {
		return false;
	}
	Link.Parameters = xrtHttpOwsTrim((xstrview){
		Rest.Data + iRest + 1u,
		Rest.Size - iRest - 1u
	});
	if ( Link.Parameters.Size == 0 ) {
		return false;
	}
	for ( ;; ) {
		Next = xrtHttpParamNext(
			Link.Parameters, &iOffset, &Param
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		if ( !__xrtHttpLinkParamApply(&Link, &Param) ||
			(Link.ParamCount == SIZE_MAX) ) {
			return false;
		}
		Link.ParamCount++;
	}
	if ( (Link.Flags & XHTTP_LINK_HAS_REL) == 0 ) {
		return false;
	}
	*pLink = Link;
	return true;
}



/* 严格解析一个 link-value。 */
XRT_API bool xrtHttpLinkElementParse(
	xstrview Element,
	xhttplink* pOutput
)
{
	xhttplink Link;

	if ( !__xrtHttpViewValid(Element) ||
		!__xrtRangeValid(pOutput, sizeof(Link)) ||
		__xrtRangesOverlap(
			pOutput, sizeof(Link), Element.Data, Element.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpLinkElementParseValue(Element, &Link) ) {
		if ( xrtGetError() == NULL ) {
			__xrtErrorSetValue();
		}
		return false;
	}
	memcpy(pOutput, &Link, sizeof(Link));
	return true;
}



/* 从逗号列表读取下一个非空 link-value。 */
static xhttpnext __xrtHttpLinkMemberNext(
	xstrview Value,
	size_t iOffset,
	size_t* pNext,
	xstrview* pElement
)
{
	xstrview Element;
	size_t iStart;
	size_t i;
	bool bTarget;
	bool bQuoted = false;

	if ( iOffset > Value.Size ) {
		return XHTTP_NEXT_ERROR;
	}
	i = iOffset;
	for ( ;; ) {
		while ( (i < Value.Size) &&
			((Value.Data[i] == ' ') ||
			 (Value.Data[i] == '\t') ||
			 (Value.Data[i] == ',')) ) {
			i++;
		}
		if ( i == Value.Size ) {
			*pNext = i;
			memset(pElement, 0, sizeof(*pElement));
			return XHTTP_NEXT_END;
		}
		iStart = i;
		bTarget = Value.Data[i] == '<';
		bQuoted = false;
		while ( i < Value.Size ) {
			if ( bTarget ) {
				if ( Value.Data[i] == '>' ) {
					bTarget = false;
				}
				i++;
				continue;
			}
			if ( bQuoted && (Value.Data[i] == '\\') ) {
				i += ((i + 1u) < Value.Size) ? 2u : 1u;
				continue;
			}
			if ( Value.Data[i] == '"' ) {
				bQuoted = !bQuoted;
				i++;
				continue;
			}
			if ( !bQuoted && (Value.Data[i] == ',') ) {
				break;
			}
			i++;
		}
		Element = xrtHttpOwsTrim((xstrview){
			Value.Data + iStart, i - iStart
		});
		*pNext = (i < Value.Size) ? i + 1u : i;
		if ( Element.Size != 0 ) {
			*pElement = Element;
			return XHTTP_NEXT_ITEM;
		}
	}
}



/* 完整验证一个 Link 字段值。 */
static bool __xrtHttpLinkValueValidate(xstrview Value)
{
	xhttplink Link;
	xstrview Element;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iNext;

	if ( !__xrtHttpViewValid(Value) ) {
		return false;
	}
	for ( ;; ) {
		Next = __xrtHttpLinkMemberNext(
			Value, iOffset, &iNext, &Element
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			return true;
		}
		if ( !__xrtHttpLinkElementParseValue(
			Element, &Link
		) ) {
			return false;
		}
		iOffset = iNext;
	}
}



/* 初始化单字段游标。 */
XRT_API void xrtHttpLinkCursorInit(xhttplinkcursor* pCursor)
{
	xhttplinkcursor Cursor;

	if ( !__xrtRangeValid(pCursor, sizeof(Cursor)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Cursor, 0, sizeof(Cursor));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
}



/* 初始化重复字段游标。 */
XRT_API void xrtHttpLinkFieldCursorInit(
	xhttplinkfieldcursor* pCursor
)
{
	xhttplinkfieldcursor Cursor;

	if ( !__xrtRangeValid(pCursor, sizeof(Cursor)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Cursor, 0, sizeof(Cursor));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
}



/* 严格验证完整 Link 字段值。 */
XRT_API bool xrtHttpLinkValid(xstrview Value)
{
	if ( !__xrtHttpViewValid(Value) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpLinkValueValidate(Value) ) {
		if ( xrtGetError() == NULL ) {
			__xrtErrorSetValue();
		}
		return false;
	}
	return true;
}



/* 验证单字段游标状态。 */
static bool __xrtHttpLinkCursorValid(
	const xhttplinkcursor* pCursor,
	xstrview Value
)
{
	if ( pCursor->Validated == 0 ) {
		return (pCursor->Source == NULL) &&
			(pCursor->SourceSize == 0) &&
			(pCursor->Offset == 0);
	}
	return (pCursor->Validated == 1u) &&
		(pCursor->Source == (const void*)Value.Data) &&
		(pCursor->SourceSize == Value.Size) &&
		(pCursor->Offset <= Value.Size);
}



/* 迭代一个完整 Link 字段值。 */
XRT_API xhttpnext xrtHttpLinkNext(
	xstrview Value,
	xhttplinkcursor* pCursor,
	xhttplink* pOutput
)
{
	xhttplinkcursor Cursor;
	xhttplink Link;
	xstrview Element;
	xhttpnext Next;
	size_t iNext;

	if ( !__xrtHttpViewValid(Value) ||
		!__xrtRangeValid(pCursor, sizeof(Cursor)) ||
		!__xrtRangeValid(pOutput, sizeof(Link)) ||
		__xrtRangesOverlap(
			pCursor, sizeof(Cursor), pOutput, sizeof(Link)
		) || __xrtRangesOverlap(
			pCursor, sizeof(Cursor), Value.Data, Value.Size
		) || __xrtRangesOverlap(
			pOutput, sizeof(Link), Value.Data, Value.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	if ( !__xrtHttpLinkCursorValid(&Cursor, Value) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( Cursor.Validated == 0 ) {
		if ( !__xrtHttpLinkValueValidate(Value) ) {
			if ( xrtGetError() == NULL ) {
				__xrtErrorSetValue();
			}
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Source = Value.Data;
		Cursor.SourceSize = Value.Size;
		Cursor.Validated = 1u;
	}
	Next = __xrtHttpLinkMemberNext(
		Value, Cursor.Offset, &iNext, &Element
	);
	if ( Next == XHTTP_NEXT_ERROR ) {
		__xrtErrorSetInvalidArgument();
		return Next;
	}
	memset(&Link, 0, sizeof(Link));
	if ( Next == XHTTP_NEXT_END ) {
		Cursor.Offset = iNext;
		memcpy(pOutput, &Link, sizeof(Link));
		memcpy(pCursor, &Cursor, sizeof(Cursor));
		return Next;
	}
	if ( !__xrtHttpLinkElementParseValue(Element, &Link) ) {
		__xrtErrorSetValue();
		return XHTTP_NEXT_ERROR;
	}
	Cursor.Offset = iNext;
	memcpy(pOutput, &Link, sizeof(Link));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
	return XHTTP_NEXT_ITEM;
}



/* 完整预校验全部重复 Link 字段行。 */
static bool __xrtHttpLinkFieldsValidate(
	const xhttpfield* pFields,
	size_t iCount
)
{
	xhttpfield Field;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( xrtHttpFieldNameEqual(
			Field.Name, XRT_STR_LITERAL("Link")
		) && !__xrtHttpLinkValueValidate(Field.Value) ) {
			return false;
		}
	}
	return true;
}



/* 验证重复字段游标的跨字段状态。 */
static bool __xrtHttpLinkFieldCursorValid(
	const xhttplinkfieldcursor* pCursor,
	const xhttpfield* pFields,
	size_t iCount
)
{
	if ( pCursor->Validated == 0 ) {
		return (pCursor->Source == NULL) &&
			(pCursor->SourceSize == 0) &&
			(pCursor->Field == 0) &&
			(pCursor->Offset == 0);
	}
	return (pCursor->Validated == 1u) &&
		(pCursor->Source == (const void*)pFields) &&
		(pCursor->SourceSize == iCount) &&
		(pCursor->Field <= iCount) &&
		!((pCursor->Field == iCount) &&
		  (pCursor->Offset != 0));
}



/* 跨重复 Link 字段行迭代链接。 */
XRT_API xhttpnext xrtHttpLinkFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttplinkfieldcursor* pCursor,
	xhttplink* pOutput
)
{
	xhttplinkfieldcursor Cursor;
	xhttplink Link;
	xhttpfield Field;
	xstrview Element;
	xhttpnext Next;
	size_t iNext;

	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ||
		!__xrtRangeValid(pCursor, sizeof(Cursor)) ||
		!__xrtRangeValid(pOutput, sizeof(Link)) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount, pCursor, sizeof(Cursor)
		) || __xrtHttpFieldArrayOverlap(
			pFields, iCount, pOutput, sizeof(Link)
		) || __xrtRangesOverlap(
			pCursor, sizeof(Cursor), pOutput, sizeof(Link)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(&Cursor, pCursor, sizeof(Cursor));
	if ( !__xrtHttpLinkFieldCursorValid(
		&Cursor, pFields, iCount
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	if ( Cursor.Validated == 0 ) {
		if ( !__xrtHttpLinkFieldsValidate(pFields, iCount) ) {
			if ( xrtGetError() == NULL ) {
				__xrtErrorSetValue();
			}
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Source = pFields;
		Cursor.SourceSize = iCount;
		Cursor.Validated = 1u;
	}
	while ( Cursor.Field < iCount ) {
		__xrtHttpFieldLoad(pFields, Cursor.Field, &Field);
		if ( !xrtHttpFieldNameEqual(
			Field.Name, XRT_STR_LITERAL("Link")
		) ) {
			Cursor.Field++;
			Cursor.Offset = 0;
			continue;
		}
		if ( Cursor.Offset > Field.Value.Size ) {
			__xrtErrorSetInvalidArgument();
			return XHTTP_NEXT_ERROR;
		}
		Next = __xrtHttpLinkMemberNext(
			Field.Value, Cursor.Offset, &iNext, &Element
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
		if ( !__xrtHttpLinkElementParseValue(
			Element, &Link
		) ) {
			__xrtErrorSetValue();
			return XHTTP_NEXT_ERROR;
		}
		Cursor.Offset = iNext;
		memcpy(pOutput, &Link, sizeof(Link));
		memcpy(pCursor, &Cursor, sizeof(Cursor));
		return XHTTP_NEXT_ITEM;
	}
	memset(&Link, 0, sizeof(Link));
	memcpy(pOutput, &Link, sizeof(Link));
	memcpy(pCursor, &Cursor, sizeof(Cursor));
	return XHTTP_NEXT_END;
}



/* 按语义比较两个借用字符串视图。 */
static bool __xrtHttpLinkViewEqual(xstrview Left, xstrview Right)
{
	return (Left.Data == Right.Data) &&
		(Left.Size == Right.Size);
}



/* 按语义比较两个借用参数描述符，不读取结构填充字节。 */
static bool __xrtHttpLinkParamEqual(
	const xhttpparam* pLeft,
	const xhttpparam* pRight
)
{
	return __xrtHttpLinkViewEqual(pLeft->Name, pRight->Name) &&
		__xrtHttpLinkViewEqual(pLeft->Value, pRight->Value) &&
		(pLeft->Flags == pRight->Flags);
}



/* 按全部公开语义字段比较两个 Link 描述符。 */
static bool __xrtHttpLinkDescriptorEqual(
	const xhttplink* pLeft,
	const xhttplink* pRight
)
{
	return __xrtHttpLinkViewEqual(
		pLeft->Element, pRight->Element
	) && __xrtHttpLinkViewEqual(
		pLeft->Target, pRight->Target
	) && __xrtHttpLinkViewEqual(
		pLeft->Parameters, pRight->Parameters
	) && __xrtHttpLinkParamEqual(
		&pLeft->Rel, &pRight->Rel
	) && __xrtHttpLinkParamEqual(
		&pLeft->Anchor, &pRight->Anchor
	) && __xrtHttpLinkParamEqual(
		&pLeft->Rev, &pRight->Rev
	) && __xrtHttpLinkParamEqual(
		&pLeft->Media, &pRight->Media
	) && __xrtHttpLinkParamEqual(
		&pLeft->Title, &pRight->Title
	) && __xrtHttpLinkParamEqual(
		&pLeft->TitleExt, &pRight->TitleExt
	) && __xrtHttpLinkParamEqual(
		&pLeft->Type, &pRight->Type
	) && (pLeft->ParamCount == pRight->ParamCount) &&
		(pLeft->HrefLangCount == pRight->HrefLangCount) &&
		(pLeft->Flags == pRight->Flags);
}



/* 重新解析并加载可信 Link 描述符。 */
bool __xrtHttpLinkDescriptorLoad(
	const xhttplink* pLink,
	xhttplink* pOutput
)
{
	xhttplink Link;
	xhttplink Parsed;

	if ( !__xrtRangeValid(pLink, sizeof(Link)) ||
		(pOutput == NULL) ) {
		return false;
	}
	memcpy(&Link, pLink, sizeof(Link));
	if ( !__xrtHttpViewValid(Link.Element) ||
		__xrtRangesOverlap(
			pLink, sizeof(Link),
			Link.Element.Data, Link.Element.Size
		) || !__xrtHttpLinkElementParseValue(
			Link.Element, &Parsed
		) || !__xrtHttpLinkDescriptorEqual(&Link, &Parsed) ) {
		return false;
	}
	*pOutput = Link;
	return true;
}

#endif

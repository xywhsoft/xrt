#include "../internal/xrt_http_link.h"
#include "../internal/xrt_url.h"



#if defined(XHTTP_FEATURE_HTTP_LINK)

/* 参数语义读取器在 quoted-string 上跳过 quoted-pair 的反斜线。 */
typedef struct xrt_http_link_reader {
	const xhttpparam* Param;
	xhttpparamvaluecursor Cursor;
} xrt_http_link_reader;



/* 初始化参数语义读取器。 */
static void __xrtHttpLinkReaderInit(
	xrt_http_link_reader* pReader,
	const xhttpparam* pParam
)
{
	pReader->Param = pParam;
	xrtHttpParamValueCursorInit(&pReader->Cursor);
}



/* 读取下一个已经解码的参数字节。 */
static bool __xrtHttpLinkRead(
	xrt_http_link_reader* pReader,
	uint8* pByte
)
{
	return xrtHttpParamValueNext(
		pReader->Param, &pReader->Cursor, pByte
	) == XHTTP_NEXT_ITEM;
}



/* 判断字节是否为 ASCII 字母。 */
static bool __xrtHttpLinkAlpha(uint8 iByte)
{
	return ((iByte >= (uint8)'A') &&
		(iByte <= (uint8)'Z')) ||
		((iByte >= (uint8)'a') &&
		 (iByte <= (uint8)'z'));
}



/* 判断字节是否为 ASCII 小写字母。 */
static bool __xrtHttpLinkLower(uint8 iByte)
{
	return (iByte >= (uint8)'a') &&
		(iByte <= (uint8)'z');
}



/* 判断字节是否为 ASCII 数字。 */
static bool __xrtHttpLinkDigit(uint8 iByte)
{
	return (iByte >= (uint8)'0') &&
		(iByte <= (uint8)'9');
}



/* 判断字节是否为 ASCII 十六进制数字。 */
static bool __xrtHttpLinkHex(uint8 iByte)
{
	return __xrtHttpLinkDigit(iByte) ||
		((iByte >= (uint8)'A') &&
		 (iByte <= (uint8)'F')) ||
		((iByte >= (uint8)'a') &&
		 (iByte <= (uint8)'f'));
}



/* 判断字节是否属于 RFC 3986 unreserved。 */
static bool __xrtHttpLinkUnreserved(uint8 iByte)
{
	return __xrtHttpLinkAlpha(iByte) ||
		__xrtHttpLinkDigit(iByte) ||
		(iByte == (uint8)'-') ||
		(iByte == (uint8)'.') ||
		(iByte == (uint8)'_') ||
		(iByte == (uint8)'~');
}



/* 判断字节是否属于 RFC 3986 reserved。 */
static bool __xrtHttpLinkReserved(uint8 iByte)
{
	return (iByte == (uint8)':') ||
		(iByte == (uint8)'/') ||
		(iByte == (uint8)'?') ||
		(iByte == (uint8)'#') ||
		(iByte == (uint8)'[') ||
		(iByte == (uint8)']') ||
		(iByte == (uint8)'@') ||
		(iByte == (uint8)'!') ||
		(iByte == (uint8)'$') ||
		(iByte == (uint8)'&') ||
		(iByte == (uint8)'\'') ||
		(iByte == (uint8)'(') ||
		(iByte == (uint8)')') ||
		(iByte == (uint8)'*') ||
		(iByte == (uint8)'+') ||
		(iByte == (uint8)',') ||
		(iByte == (uint8)';') ||
		(iByte == (uint8)'=');
}



/* 无转义参数可以直接复用原值视图。 */
static bool __xrtHttpLinkParamDirect(
	const xhttpparam* pParam,
	xstrview* pValue
)
{
	if ( (pParam->Flags & XHTTP_PARAM_HAS_VALUE) == 0 ) {
		return false;
	}
	if ( ((pParam->Flags & XHTTP_PARAM_QUOTED) != 0) &&
		(memchr(
			pParam->Value.Data, '\\', pParam->Value.Size
		) != NULL) ) {
		return false;
	}
	*pValue = pParam->Value;
	return true;
}



/* 验证一个连续关系类型是注册名称或绝对 URI。 */
static bool __xrtHttpLinkRelationValueValid(xstrview Relation)
{
	xurl Url;
	size_t i;
	bool bRegistered;

	if ( !__xrtHttpViewValid(Relation) ||
		(Relation.Size == 0) ) {
		return false;
	}
	bRegistered = __xrtHttpLinkLower(
		(uint8)Relation.Data[0]
	);
	for ( i = 1u; bRegistered && (i < Relation.Size); i++ ) {
		uint8 iByte = (uint8)Relation.Data[i];

		bRegistered = __xrtHttpLinkLower(iByte) ||
			__xrtHttpLinkDigit(iByte) ||
			(iByte == (uint8)'.') ||
			(iByte == (uint8)'-');
	}
	if ( bRegistered ) {
		return true;
	}
	if ( !__xrtUrlParseValue(Relation, &Url) ) {
		return false;
	}
	return ((Url.Flags & XURL_HAS_SCHEME) != 0) &&
		((Url.Flags & XURL_HAS_FRAGMENT) == 0);
}



/* 验证查询关系；注册名称允许调用方使用任意 ASCII 大小写。 */
static bool __xrtHttpLinkRelationQueryValid(xstrview Relation)
{
	size_t i;
	bool bRegistered;

	if ( !__xrtHttpViewValid(Relation) ||
		(Relation.Size == 0) ) {
		return false;
	}
	bRegistered = __xrtHttpLinkAlpha(
		(uint8)Relation.Data[0]
	);
	for ( i = 1u; bRegistered && (i < Relation.Size); i++ ) {
		uint8 iByte = (uint8)Relation.Data[i];

		bRegistered = __xrtHttpLinkAlpha(iByte) ||
			__xrtHttpLinkDigit(iByte) ||
			(iByte == (uint8)'.') ||
			(iByte == (uint8)'-');
	}
	return bRegistered ||
		__xrtHttpLinkRelationValueValid(Relation);
}



/* 验证写入侧已经解码的关系类型列表。 */
bool __xrtHttpLinkRelationsValueValid(xstrview Relations)
{
	size_t iStart = 0;
	size_t i = 0;

	if ( !__xrtHttpViewValid(Relations) ||
		(Relations.Size == 0) ||
		(Relations.Data[0] == ' ') ||
		(Relations.Data[Relations.Size - 1u] == ' ') ) {
		return false;
	}
	while ( i <= Relations.Size ) {
		if ( (i != Relations.Size) &&
			(Relations.Data[i] != ' ') ) {
			i++;
			continue;
		}
		if ( (i == iStart) ||
			!__xrtHttpLinkRelationValueValid((xstrview){
				Relations.Data + iStart, i - iStart
			}) ) {
			return false;
		}
		if ( i == Relations.Size ) {
			return true;
		}
		while ( (i < Relations.Size) &&
			(Relations.Data[i] == ' ') ) {
			i++;
		}
		iStart = i;
	}
	return false;
}



/* 流式关系项状态支持不分配地验证 quoted-pair。 */
typedef struct xrt_http_link_relation_state {
	size_t Size;
	uint8 Percent;
	bool Registered;
	bool Scheme;
	bool Colon;
} xrt_http_link_relation_state;



/* 初始化一项关系类型的语法状态。 */
static void __xrtHttpLinkRelationStateInit(
	xrt_http_link_relation_state* pState
)
{
	memset(pState, 0, sizeof(*pState));
	pState->Registered = true;
	pState->Scheme = true;
}



/* 向关系类型状态追加一个语义字节。 */
static bool __xrtHttpLinkRelationStateByte(
	xrt_http_link_relation_state* pState,
	uint8 iByte
)
{
	if ( pState->Percent != 0 ) {
		if ( !__xrtHttpLinkHex(iByte) ) {
			return false;
		}
		pState->Percent--;
		pState->Size++;
		return true;
	}
	if ( pState->Size == 0 ) {
		pState->Registered = __xrtHttpLinkLower(iByte);
		pState->Scheme = __xrtHttpLinkAlpha(iByte);
	} else {
		pState->Registered = pState->Registered &&
			(__xrtHttpLinkLower(iByte) ||
			 __xrtHttpLinkDigit(iByte) ||
			 (iByte == (uint8)'.') ||
			 (iByte == (uint8)'-'));
		if ( !pState->Colon ) {
			if ( iByte == (uint8)':' ) {
				pState->Colon = pState->Scheme;
			} else {
				pState->Scheme = pState->Scheme &&
					(__xrtHttpLinkAlpha(iByte) ||
					 __xrtHttpLinkDigit(iByte) ||
					 (iByte == (uint8)'+') ||
					 (iByte == (uint8)'-') ||
					 (iByte == (uint8)'.'));
			}
		}
	}
	if ( pState->Colon ) {
		if ( iByte == (uint8)'#' ) {
			return false;
		}
		if ( iByte == (uint8)'%' ) {
			pState->Percent = 2u;
		} else if ( !__xrtHttpLinkUnreserved(iByte) &&
			!__xrtHttpLinkReserved(iByte) ) {
			return false;
		}
	}
	pState->Size++;
	return true;
}



/* 判断当前流式关系项是否完整。 */
static bool __xrtHttpLinkRelationStateValid(
	const xrt_http_link_relation_state* pState
)
{
	return (pState->Size != 0) &&
		(pState->Percent == 0) &&
		(pState->Registered || pState->Colon);
}



/* 验证一个解码关系项；扩展关系必须是无片段的绝对 URI。 */
static bool __xrtHttpLinkRelationRangeValid(
	const xhttpparam* pParam,
	size_t iStart,
	size_t iEnd,
	const xrt_http_link_relation_state* pState
)
{
	xhttpparam Range;

	if ( !__xrtHttpLinkRelationStateValid(pState) ) {
		return false;
	}
	if ( pState->Registered ) {
		return true;
	}
	memset(&Range, 0, sizeof(Range));
	Range.Value.Data = pParam->Value.Data + iStart;
	Range.Value.Size = iEnd - iStart;
	Range.Flags = pParam->Flags;
	return xrtUrlParamValid(&Range);
}



/* 验证 rel 或 rev 参数解码后的关系类型列表。 */
bool __xrtHttpLinkRelationsParamValid(const xhttpparam* pParam)
{
	xrt_http_link_relation_state State;
	xrt_http_link_reader Reader;
	xstrview Direct;
	uint8 iByte;
	size_t iStart = 0;
	size_t iBefore;
	bool bSeparator = false;

	if ( __xrtHttpLinkParamDirect(pParam, &Direct) ) {
		return __xrtHttpLinkRelationsValueValid(Direct);
	}
	if ( (pParam->Flags & XHTTP_PARAM_HAS_VALUE) == 0 ) {
		return false;
	}
	__xrtHttpLinkReaderInit(&Reader, pParam);
	__xrtHttpLinkRelationStateInit(&State);
	for ( ;; ) {
		iBefore = Reader.Cursor.Offset;
		if ( !__xrtHttpLinkRead(&Reader, &iByte) ) {
			break;
		}
		if ( iByte == (uint8)' ' ) {
			if ( !bSeparator &&
				!__xrtHttpLinkRelationRangeValid(
					pParam, iStart, iBefore, &State
				) ) {
				return false;
			}
			bSeparator = true;
			continue;
		}
		if ( bSeparator ) {
			__xrtHttpLinkRelationStateInit(&State);
			iStart = iBefore;
			bSeparator = false;
		}
		if ( !__xrtHttpLinkRelationStateByte(
			&State, iByte
		) ) {
			return false;
		}
	}
	return !bSeparator &&
		__xrtHttpLinkRelationRangeValid(
			pParam, iStart, pParam->Value.Size, &State
		);
}



/* 验证参数解码后的 URI-reference。 */
bool __xrtHttpLinkUriParamValid(const xhttpparam* pParam)
{
	return xrtUrlParamValid(pParam);
}



/* 验证参数解码后的基本语言标签。 */
bool __xrtHttpLinkLanguageParamValid(const xhttpparam* pParam)
{
	xrt_http_link_reader Reader;
	xstrview Direct;
	uint8 iByte;
	size_t iSegment = 0;
	size_t iSegments = 0;

	if ( __xrtHttpLinkParamDirect(pParam, &Direct) ) {
		return xrtHttpLanguageTagValid(Direct);
	}
	if ( (pParam->Flags & XHTTP_PARAM_HAS_VALUE) == 0 ) {
		return false;
	}
	__xrtHttpLinkReaderInit(&Reader, pParam);
	while ( __xrtHttpLinkRead(&Reader, &iByte) ) {
		if ( iByte == (uint8)'-' ) {
			if ( (iSegment == 0) || (iSegment > 8u) ) {
				return false;
			}
			iSegments++;
			iSegment = 0;
			continue;
		}
		if ( (iSegment == 8u) ||
			((iSegments == 0) ?
			 !__xrtHttpLinkAlpha(iByte) :
			 (!__xrtHttpLinkAlpha(iByte) &&
			  !__xrtHttpLinkDigit(iByte))) ) {
			return false;
		}
		iSegment++;
	}
	return iSegment != 0;
}



/* 验证参数解码后的 type-name/subtype-name。 */
bool __xrtHttpLinkTypeParamValid(const xhttpparam* pParam)
{
	xrt_http_link_reader Reader;
	uint8 iByte;
	size_t iType = 0;
	size_t iSubtype = 0;
	bool bSlash = false;

	if ( (pParam->Flags & XHTTP_PARAM_HAS_VALUE) == 0 ) {
		return false;
	}
	__xrtHttpLinkReaderInit(&Reader, pParam);
	while ( __xrtHttpLinkRead(&Reader, &iByte) ) {
		if ( iByte == (uint8)'/' ) {
			if ( bSlash || (iType == 0) ) {
				return false;
			}
			bSlash = true;
			continue;
		}
		if ( !__xrtHttpTokenByte(iByte) ) {
			return false;
		}
		if ( bSlash ) {
			iSubtype++;
		} else {
			iType++;
		}
	}
	return bSlash && (iSubtype != 0);
}



/* 验证公共 Helper 的输出不覆盖输入描述符。 */
static bool __xrtHttpLinkHelperOutputValid(
	const xhttplink* pLink,
	const void* pOutput,
	size_t iCapacity,
	const size_t* pSize
)
{
	return __xrtRangeValid(pSize, sizeof(*pSize)) &&
		((pOutput != NULL) || (iCapacity == 0)) &&
		!__xrtRangesOverlap(
			pLink, sizeof(*pLink), pSize, sizeof(*pSize)
		) && ((pOutput == NULL) ||
		 (!__xrtRangesOverlap(
			pLink, sizeof(*pLink), pOutput, iCapacity
		 ) && !__xrtRangesOverlap(
			pSize, sizeof(*pSize), pOutput, iCapacity
		 )));
}



/* 验证 Helper 输出不覆盖 Link 借用的原字段。 */
static bool __xrtHttpLinkHelperBorrowValid(
	const xhttplink* pLink,
	const void* pOutput,
	size_t iCapacity,
	const size_t* pSize
)
{
	return !__xrtRangesOverlap(
		pLink->Element.Data, pLink->Element.Size,
		pSize, sizeof(*pSize)
	) && ((pOutput == NULL) ||
		!__xrtRangesOverlap(
			pLink->Element.Data, pLink->Element.Size,
			pOutput, iCapacity
		));
}



/* 严格查找第一个 Link 参数。 */
XRT_API xhttpnext xrtHttpLinkParam(
	const xhttplink* pLink,
	xstrview Name,
	xhttpparam* pOutput
)
{
	xhttplink Link;

	if ( !__xrtRangeValid(pOutput, sizeof(*pOutput)) ||
		__xrtRangesOverlap(
			pLink, sizeof(*pLink), pOutput, sizeof(*pOutput)
		) || !xrtHttpTokenValid(Name) ||
		!__xrtHttpLinkDescriptorLoad(pLink, &Link) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	return xrtHttpParamFind(Link.Parameters, Name, pOutput);
}



/* 按大小写不敏感规则查找一个关系类型。 */
XRT_API xhttpnext xrtHttpLinkRelationFind(
	const xhttplink* pLink,
	xstrview Relation
)
{
	xhttplink Link;
	xrt_http_link_reader Reader;
	uint8 iByte;
	size_t iMatch = 0;
	bool bEqual = true;
	bool bItem = false;

	if ( !__xrtHttpLinkRelationQueryValid(Relation) ||
		!__xrtHttpLinkDescriptorLoad(pLink, &Link) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	__xrtHttpLinkReaderInit(&Reader, &Link.Rel);
	while ( __xrtHttpLinkRead(&Reader, &iByte) ) {
		if ( iByte == (uint8)' ' ) {
			if ( bItem && bEqual &&
				(iMatch == Relation.Size) ) {
				return XHTTP_NEXT_ITEM;
			}
			bItem = false;
			bEqual = true;
			iMatch = 0;
			continue;
		}
		bItem = true;
		if ( (iMatch >= Relation.Size) ||
			(__xhttpAsciiLower(iByte) !=
			 __xhttpAsciiLower(
				(uint8)Relation.Data[iMatch]
			 )) ) {
			bEqual = false;
		}
		iMatch++;
	}
	return (bItem && bEqual &&
		(iMatch == Relation.Size)) ?
		XHTTP_NEXT_ITEM : XHTTP_NEXT_END;
}



/* 解码 anchor 参数。 */
XRT_API bool xrtHttpLinkAnchorWrite(
	const xhttplink* pLink,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttplink Link;

	if ( !__xrtHttpLinkHelperOutputValid(
		pLink, pOutput, iCapacity, pSize
	) || !__xrtHttpLinkDescriptorLoad(pLink, &Link) ||
		!__xrtHttpLinkHelperBorrowValid(
			&Link, pOutput, iCapacity, pSize
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (Link.Flags & XHTTP_LINK_HAS_ANCHOR) == 0 ) {
		__xrtErrorSetValue();
		return false;
	}
	return xrtHttpParamValueWrite(
		&Link.Anchor, pOutput, iCapacity, pSize
	);
}



/* 构建零结尾 anchor。 */
XRT_API str xrtHttpLinkAnchorBuild(
	const xhttplink* pLink,
	size_t* pSize
)
{
	xhttplink Link;
	str sOutput;
	size_t iRequired;

	if ( !__xrtHttpLinkDescriptorLoad(pLink, &Link) ||
		((pSize != NULL) &&
		 (!__xrtRangeValid(pSize, sizeof(iRequired)) ||
		  __xrtRangesOverlap(
			pLink, sizeof(*pLink), pSize, sizeof(iRequired)
		  ) || __xrtRangesOverlap(
			Link.Element.Data, Link.Element.Size,
			pSize, sizeof(iRequired)
		  ))) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtHttpLinkAnchorWrite(
		pLink, NULL, 0, &iRequired
	) || (iRequired == SIZE_MAX) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtHttpLinkAnchorWrite(
		pLink, sOutput, iRequired, &iRequired
	) ) {
		xrtFree(sOutput);
		return NULL;
	}
	sOutput[iRequired] = '\0';
	if ( pSize != NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
	}
	return sOutput;
}



/* 读取 UTF-8 标题并按规范回退普通 title。 */
XRT_API bool xrtHttpLinkTitleWrite(
	const xhttplink* pLink,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpextvalue ExtValue;
	xhttplink Link;

	if ( !__xrtHttpLinkHelperOutputValid(
		pLink, pOutput, iCapacity, pSize
	) || !__xrtHttpLinkDescriptorLoad(pLink, &Link) ||
		!__xrtHttpLinkHelperBorrowValid(
			&Link, pOutput, iCapacity, pSize
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (Link.Flags & XHTTP_LINK_HAS_TITLE_EXT) != 0 ) {
		if ( __xrtHttpExtValueSplit(
			Link.TitleExt.Value, &ExtValue
		) && xrtHttpTokenEqual(
			ExtValue.Charset, XRT_STR_LITERAL("UTF-8")
		) ) {
			return xrtHttpExtValueRead(
				&ExtValue, pOutput, iCapacity, pSize
			);
		}
	}
	if ( (Link.Flags & XHTTP_LINK_HAS_TITLE) != 0 ) {
		return xrtHttpParamValueWrite(
			&Link.Title, pOutput, iCapacity, pSize
		);
	}
	__xrtErrorSetValue();
	return false;
}



/* 构建零结尾 UTF-8 标题。 */
XRT_API str xrtHttpLinkTitleBuild(
	const xhttplink* pLink,
	size_t* pSize
)
{
	xhttplink Link;
	str sOutput;
	size_t iRequired;

	if ( !__xrtHttpLinkDescriptorLoad(pLink, &Link) ||
		((pSize != NULL) &&
		 (!__xrtRangeValid(pSize, sizeof(iRequired)) ||
		  __xrtRangesOverlap(
			pLink, sizeof(*pLink), pSize, sizeof(iRequired)
		  ) || __xrtRangesOverlap(
			Link.Element.Data, Link.Element.Size,
			pSize, sizeof(iRequired)
		  ))) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtHttpLinkTitleWrite(
		pLink, NULL, 0, &iRequired
	) || (iRequired == SIZE_MAX) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtHttpLinkTitleWrite(
		pLink, sOutput, iRequired, &iRequired
	) ) {
		xrtFree(sOutput);
		return NULL;
	}
	sOutput[iRequired] = '\0';
	if ( pSize != NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
	}
	return sOutput;
}



/* 使用 Base 解析链接目标。 */
XRT_API bool xrtHttpLinkTargetResolve(
	const xhttplink* pLink,
	const xurl* pBase,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttplink Link;

	if ( !__xrtRangeValid(pBase, sizeof(*pBase)) ||
		!__xrtHttpLinkHelperOutputValid(
		pLink, pOutput, iCapacity, pSize
	) || !__xrtHttpLinkDescriptorLoad(pLink, &Link) ||
		!__xrtHttpLinkHelperBorrowValid(
			&Link, pOutput, iCapacity, pSize
		) || __xrtRangesOverlap(
			pBase, sizeof(*pBase), pSize, sizeof(*pSize)
		) || ((pOutput != NULL) &&
		 __xrtRangesOverlap(
			pBase, sizeof(*pBase), pOutput, iCapacity
		) ) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return xrtUrlResolve(
		pBase, Link.Target,
		pOutput, iCapacity, pSize
	);
}



/* 分配并解析链接目标。 */
XRT_API str xrtHttpLinkTargetResolveBuild(
	const xhttplink* pLink,
	const xurl* pBase,
	size_t* pSize
)
{
	xhttplink Link;
	str sOutput;
	size_t iSize;

	if ( !__xrtRangeValid(pBase, sizeof(*pBase)) ||
		!__xrtHttpLinkDescriptorLoad(pLink, &Link) ||
		((pSize != NULL) &&
		 (!__xrtRangeValid(pSize, sizeof(iSize)) ||
		  __xrtRangesOverlap(
			pLink, sizeof(*pLink), pSize, sizeof(iSize)
		  ) || __xrtRangesOverlap(
			Link.Element.Data, Link.Element.Size,
			pSize, sizeof(iSize)
		  ) || __xrtRangesOverlap(
			pBase, sizeof(*pBase), pSize, sizeof(iSize)
		  ))) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	sOutput = xrtUrlResolveBuild(pBase, Link.Target, &iSize);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( pSize != NULL ) {
		memcpy(pSize, &iSize, sizeof(iSize));
	}
	return sOutput;
}

#endif

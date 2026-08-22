#include "../internal/xrt_http_route.h"

#include <xrt/http_route.h>



#if defined(XHTTP_FEATURE_HTTP_ROUTE)

/* 判断字节是否为 ASCII 字母。 */
static bool __xrtHttpRouteAlpha(unsigned char iByte)
{
	return ((iByte >= (unsigned char)'A') &&
		(iByte <= (unsigned char)'Z')) ||
		((iByte >= (unsigned char)'a') &&
		 (iByte <= (unsigned char)'z'));
}



/* 判断字节是否为 ASCII 十进制数字。 */
static bool __xrtHttpRouteDigit(unsigned char iByte)
{
	return (iByte >= (unsigned char)'0') &&
		(iByte <= (unsigned char)'9');
}



/* 判断字节是否为 ASCII 十六进制数字。 */
static bool __xrtHttpRouteHex(unsigned char iByte)
{
	return __xrtHttpRouteDigit(iByte) ||
		((iByte >= (unsigned char)'A') &&
		 (iByte <= (unsigned char)'F')) ||
		((iByte >= (unsigned char)'a') &&
		 (iByte <= (unsigned char)'f'));
}



/* 判断字节是否属于 RFC 3986 unreserved。 */
static bool __xrtHttpRouteUnreserved(unsigned char iByte)
{
	return __xrtHttpRouteAlpha(iByte) ||
		__xrtHttpRouteDigit(iByte) ||
		(iByte == (unsigned char)'-') ||
		(iByte == (unsigned char)'.') ||
		(iByte == (unsigned char)'_') ||
		(iByte == (unsigned char)'~');
}



/* 判断字节是否属于 RFC 3986 sub-delims。 */
static bool __xrtHttpRouteSubDelimiter(unsigned char iByte)
{
	return (iByte == (unsigned char)'!') ||
		(iByte == (unsigned char)'$') ||
		(iByte == (unsigned char)'&') ||
		(iByte == (unsigned char)'\'') ||
		(iByte == (unsigned char)'(') ||
		(iByte == (unsigned char)')') ||
		(iByte == (unsigned char)'*') ||
		(iByte == (unsigned char)'+') ||
		(iByte == (unsigned char)',') ||
		(iByte == (unsigned char)';') ||
		(iByte == (unsigned char)'=');
}



/* 验证静态模板段或输入路径段中的 RFC 3986 pchar。 */
static bool __xrtHttpRoutePathBytesValid(xstrview Text)
{
	size_t i;

	for ( i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( __xrtHttpRouteUnreserved(iByte) ||
			__xrtHttpRouteSubDelimiter(iByte) ||
			(iByte == (unsigned char)':') ||
			(iByte == (unsigned char)'@') ||
			(iByte == (unsigned char)'/') ) {
			continue;
		}
		if ( (iByte == (unsigned char)'%') &&
			((i + 2u) < Text.Size) &&
			__xrtHttpRouteHex((unsigned char)Text.Data[i + 1u]) &&
			__xrtHttpRouteHex((unsigned char)Text.Data[i + 2u]) ) {
			i += 2u;
			continue;
		}
		return false;
	}
	return true;
}



/* 从可能未对齐的捕获数组读取一个固定描述符。 */
static void __xrtHttpRouteParamLoad(
	const xhttprouteparam* pParams,
	size_t iIndex,
	xhttprouteparam* pParam
)
{
	memcpy(
		pParam,
		(const uint8*)(const void*)pParams +
			(iIndex * sizeof(*pParam)),
		sizeof(*pParam)
	);
}



/* 向可能未对齐的捕获数组写入一个固定描述符。 */
static void __xrtHttpRouteParamStore(
	xhttprouteparam* pParams,
	size_t iIndex,
	xstrview Name,
	xstrview Value
)
{
	xhttprouteparam Param;

	Param.Name = Name;
	Param.Value = Value;
	memcpy(
		(uint8*)(void*)pParams + (iIndex * sizeof(Param)),
		&Param,
		sizeof(Param)
	);
}



/* 初始化保留重复斜杠和尾斜杠的路径段游标。 */
void __xrtHttpRouteCursorInit(
	xrt_http_route_cursor* pCursor,
	xstrview Text
)
{
	pCursor->Text = Text;
	pCursor->Position = Text.Size == 1u ? 2u : 1u;
}



/* 读取下一个路径段；空段是有效结果，根路径没有任何段。 */
bool __xrtHttpRouteCursorNext(
	xrt_http_route_cursor* pCursor,
	xstrview* pSegment
)
{
	size_t iStart;
	size_t iEnd;

	if ( pCursor->Position > pCursor->Text.Size ) {
		return false;
	}
	iStart = pCursor->Position;
	iEnd = iStart;
	while ( (iEnd < pCursor->Text.Size) &&
		(pCursor->Text.Data[iEnd] != '/') ) {
		iEnd++;
	}
	pSegment->Data = pCursor->Text.Data + iStart;
	pSegment->Size = iEnd - iStart;
	pCursor->Position = iEnd < pCursor->Text.Size ?
		iEnd + 1u : pCursor->Text.Size + 1u;
	return true;
}



/* 判断参数名是否符合稳定且便于语言绑定的 ASCII 标识符规则。 */
static bool __xrtHttpRouteNameValid(xstrview Name)
{
	size_t i;

	if ( (Name.Size == 0) ||
		(!__xrtHttpRouteAlpha((unsigned char)Name.Data[0]) &&
		 (Name.Data[0] != '_')) ) {
		return false;
	}
	for ( i = 1u; i < Name.Size; i++ ) {
		unsigned char iByte = (unsigned char)Name.Data[i];

		if ( !__xrtHttpRouteAlpha(iByte) &&
			!__xrtHttpRouteDigit(iByte) &&
			(iByte != (unsigned char)'_') ) {
			return false;
		}
	}
	return true;
}



/* 把已经切分的模板段解析为静态段、参数段或末尾参数。 */
bool __xrtHttpRouteSegmentParse(
	xstrview Text,
	bool bLast,
	xrt_http_route_segment* pSegment
)
{
	size_t i;

	memset(pSegment, 0, sizeof(*pSegment));
	pSegment->Text = Text;
	for ( i = 0; i < Text.Size; i++ ) {
		if ( (Text.Data[i] == '{') || (Text.Data[i] == '}') ) {
			break;
		}
	}
	if ( i == Text.Size ) {
		if ( !__xrtHttpRoutePathBytesValid(Text) ) {
			return false;
		}
		pSegment->Kind = XRT_HTTP_ROUTE_STATIC;
		return true;
	}
	if ( (Text.Size < 3u) || (Text.Data[0] != '{') ||
		(Text.Data[Text.Size - 1u] != '}') ) {
		return false;
	}
	pSegment->Name.Data = Text.Data + 1u;
	pSegment->Name.Size = Text.Size - 2u;
	if ( (pSegment->Name.Size >= 3u) &&
		(memcmp(
			pSegment->Name.Data + pSegment->Name.Size - 3u,
			"...", 3u
		 ) == 0) ) {
		pSegment->Kind = XRT_HTTP_ROUTE_TAIL;
		pSegment->Name.Size -= 3u;
		if ( !bLast ) {
			return false;
		}
	} else {
		pSegment->Kind = XRT_HTTP_ROUTE_PARAMETER;
	}
	return __xrtHttpRouteNameValid(pSegment->Name);
}



/* 在当前段之前查找同名参数，防止匹配结果出现有歧义的重复名称。 */
static bool __xrtHttpRouteNameSeen(
	xstrview Pattern,
	size_t iEnd,
	xstrview Name
)
{
	xrt_http_route_cursor Cursor;
	xrt_http_route_segment Segment;
	xstrview Text;

	__xrtHttpRouteCursorInit(&Cursor, Pattern);
	while ( (Cursor.Position < iEnd) &&
		__xrtHttpRouteCursorNext(&Cursor, &Text) ) {
		if ( !__xrtHttpRouteSegmentParse(
			Text, Cursor.Position > Pattern.Size, &Segment
		) ) {
			return false;
		}
		if ( (Segment.Kind != XRT_HTTP_ROUTE_STATIC) &&
			(Segment.Name.Size == Name.Size) &&
			(memcmp(Segment.Name.Data, Name.Data, Name.Size) == 0) ) {
			return true;
		}
	}
	return false;
}



/* 验证完整模板并统计唯一参数。 */
static bool __xrtHttpRouteValidate(
	xstrview Pattern,
	size_t* pParameters
)
{
	xrt_http_route_cursor Cursor;
	xrt_http_route_segment Segment;
	xstrview Text;
	size_t iCount = 0;

	if ( (Pattern.Size == 0) || (Pattern.Data[0] != '/') ) {
		return false;
	}
	__xrtHttpRouteCursorInit(&Cursor, Pattern);
	while ( __xrtHttpRouteCursorNext(&Cursor, &Text) ) {
		size_t iCurrent = (size_t)(Text.Data - Pattern.Data);

		if ( !__xrtHttpRouteSegmentParse(
			Text, Cursor.Position > Pattern.Size, &Segment
		) ) {
			return false;
		}
		if ( Segment.Kind == XRT_HTTP_ROUTE_STATIC ) {
			continue;
		}
		if ( __xrtHttpRouteNameSeen(
			Pattern, iCurrent, Segment.Name
		) ) {
			return false;
		}
		if ( iCount == SIZE_MAX ) {
			return false;
		}
		iCount++;
	}
	*pParameters = iCount;
	return true;
}



/* 验证调用方提供的是不含 query 或 fragment 的绝对 RFC 3986 路径。 */
bool __xrtHttpRoutePathValid(xstrview Path)
{
	return (Path.Size != 0) && (Path.Data[0] == '/') &&
		__xrtHttpRoutePathBytesValid(Path);
}



/* 执行一次无写入匹配，命中时返回尾参数原始起点。 */
xhttproutestatus __xrtHttpRouteMatchValidated(
	xstrview Pattern,
	xstrview Path,
	xhttprouteparam* pParams
)
{
	xrt_http_route_cursor PatternCursor;
	xrt_http_route_cursor PathCursor;
	xrt_http_route_segment Route;
	xstrview PatternSegment;
	xstrview PathSegment;
	size_t iParameter = 0;

	__xrtHttpRouteCursorInit(&PatternCursor, Pattern);
	__xrtHttpRouteCursorInit(&PathCursor, Path);
	while ( __xrtHttpRouteCursorNext(
		&PatternCursor, &PatternSegment
	) ) {
		size_t iTailStart;

		(void)__xrtHttpRouteSegmentParse(
			PatternSegment,
			PatternCursor.Position > Pattern.Size,
			&Route
		);
		if ( Route.Kind == XRT_HTTP_ROUTE_TAIL ) {
			iTailStart = PathCursor.Position;
			if ( iTailStart > Path.Size ) {
				if ( (PatternSegment.Data == (Pattern.Data + 1u)) &&
					(Path.Size == 1u) ) {
					iTailStart = 1u;
				} else {
					return XHTTP_ROUTE_MISS;
				}
			}
			if ( pParams != NULL ) {
				xstrview Value;

				Value.Data = Path.Data + iTailStart;
				Value.Size = Path.Size - iTailStart;
				__xrtHttpRouteParamStore(
					pParams, iParameter, Route.Name, Value
				);
			}
			return XHTTP_ROUTE_MATCH;
		}
		if ( !__xrtHttpRouteCursorNext(
			&PathCursor, &PathSegment
		) ) {
			return XHTTP_ROUTE_MISS;
		}
		if ( Route.Kind == XRT_HTTP_ROUTE_STATIC ) {
			if ( (Route.Text.Size != PathSegment.Size) ||
				(memcmp(
					Route.Text.Data,
					PathSegment.Data,
					Route.Text.Size
				 ) != 0) ) {
				return XHTTP_ROUTE_MISS;
			}
			continue;
		}
		if ( PathSegment.Size == 0 ) {
			return XHTTP_ROUTE_MISS;
		}
		if ( pParams != NULL ) {
			__xrtHttpRouteParamStore(
				pParams, iParameter, Route.Name, PathSegment
			);
		}
		iParameter++;
	}
	if ( __xrtHttpRouteCursorNext(&PathCursor, &PathSegment) ) {
		return XHTTP_ROUTE_MISS;
	}
	return XHTTP_ROUTE_MATCH;
}



/* 验证绝对模板、参数语法和参数名称唯一性。 */
XRT_API bool xrtHttpRouteValidate(
	xstrview Pattern,
	size_t* pParameters
)
{
	size_t iParameters = 0;

	if ( !__xrtRangeValid(pParameters, sizeof(iParameters)) ||
		!__xrtHttpViewValid(Pattern) ||
		__xrtRangesOverlap(
			pParameters, sizeof(iParameters),
			Pattern.Data, Pattern.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pParameters, &iParameters, sizeof(iParameters));
	if ( !__xrtHttpRouteValidate(Pattern, &iParameters) ) {
		__xrtErrorSetValue();
		return false;
	}
	memcpy(pParameters, &iParameters, sizeof(iParameters));
	return true;
}



/* 严格匹配原始路径，并在容量足够时一次性写入全部借用捕获。 */
XRT_API xhttproutestatus xrtHttpRouteMatch(
	xstrview Pattern,
	xstrview Path,
	xhttprouteparam* pParams,
	size_t iCapacity,
	size_t* pCount
)
{
	xhttproutestatus Status;
	size_t iParameters;
	size_t iBytes;
	size_t iResult = 0;

	if ( !__xrtRangeValid(pCount, sizeof(iResult)) ||
		!__xrtHttpViewValid(Pattern) ||
		!__xrtHttpViewValid(Path) ||
		((pParams == NULL) && (iCapacity != 0)) ||
		(iCapacity > (SIZE_MAX / sizeof(*pParams))) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_ROUTE_ERROR;
	}
	iBytes = iCapacity * sizeof(*pParams);
	if ( !__xrtRangeValid(pParams, iBytes) ||
		__xrtRangesOverlap(
		pCount, sizeof(iResult), Pattern.Data, Pattern.Size
	) || __xrtRangesOverlap(
		pCount, sizeof(iResult), Path.Data, Path.Size
	) || __xrtRangesOverlap(
		pCount, sizeof(iResult), pParams, iBytes
	) || __xrtRangesOverlap(
		pParams, iBytes, Pattern.Data, Pattern.Size
	) || __xrtRangesOverlap(
		pParams, iBytes, Path.Data, Path.Size
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_ROUTE_ERROR;
	}
	memcpy(pCount, &iResult, sizeof(iResult));
	if ( !__xrtHttpRouteValidate(Pattern, &iParameters) ||
		!__xrtHttpRoutePathValid(Path) ) {
		__xrtErrorSetValue();
		return XHTTP_ROUTE_ERROR;
	}
	Status = __xrtHttpRouteMatchValidated(Pattern, Path, NULL);
	if ( Status != XHTTP_ROUTE_MATCH ) {
		return Status;
	}
	iResult = iParameters;
	memcpy(pCount, &iResult, sizeof(iResult));
	if ( iCapacity < iParameters ) {
		return XHTTP_ROUTE_MORE;
	}
	if ( iParameters != 0 ) {
		Status = __xrtHttpRouteMatchValidated(
			Pattern, Path, pParams
		);
		if ( Status != XHTTP_ROUTE_MATCH ) {
			iResult = 0;
			memcpy(pCount, &iResult, sizeof(iResult));
			__xrtErrorSetInternal();
			return XHTTP_ROUTE_ERROR;
		}
	}
	return XHTTP_ROUTE_MATCH;
}



/* 按参数名查找捕获，正常未找到不会污染线程错误。 */
XRT_API const xhttprouteparam* xrtHttpRouteParam(
	const xhttprouteparam* pParams,
	size_t iCount,
	xstrview Name
)
{
	xhttprouteparam Param;
	size_t iBytes;
	size_t i;

	if ( ((pParams == NULL) && (iCount != 0)) ||
		(iCount > (SIZE_MAX / sizeof(*pParams))) ||
		!__xrtHttpViewValid(Name) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	iBytes = iCount * sizeof(*pParams);
	if ( !__xrtRangeValid(pParams, iBytes) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpRouteParamLoad(pParams, i, &Param);
		if ( !__xrtHttpViewValid(Param.Name) ||
			!__xrtHttpViewValid(Param.Value) ) {
			__xrtErrorSetInvalidArgument();
			return NULL;
		}
		if ( (Param.Name.Size == Name.Size) &&
			((Name.Size == 0) || (memcmp(
				Param.Name.Data, Name.Data, Name.Size
			 ) == 0)) ) {
			return (const xhttprouteparam*)(const void*)(
				(const uint8*)(const void*)pParams +
				(i * sizeof(Param))
			);
		}
	}
	return NULL;
}

#endif

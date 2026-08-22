#include "../internal/xrt_http_cors_client.h"



#if defined(XRT_FEATURE_HTTP_CORS_CLIENT)

/* 判断字段名是否为必须保持单值语义的 safelist 字段。 */
static bool __xrtHttpCorsSingleRequestName(xstrview Name)
{
	return xrtHttpFieldNameEqual(
		Name, XRT_STR_LITERAL("Content-Type")
	) || xrtHttpFieldNameEqual(
		Name, XRT_STR_LITERAL("Range")
	);
}



/* 判断指定名称是否在请求字段中重复。 */
static bool __xrtHttpCorsRequestNameRepeated(
	const xhttpfield* pFields,
	size_t iCount,
	size_t iIndex
)
{
	xhttpfield Field;
	xhttpfield Other;
	size_t i;

	__xrtHttpFieldLoad(pFields, iIndex, &Field);
	for ( i = 0; i < iCount; i++ ) {
		if ( i == iIndex ) {
			continue;
		}
		__xrtHttpFieldLoad(pFields, i, &Other);
		if ( xrtHttpFieldNameEqual(
			Field.Name, Other.Name
		) ) {
			return true;
		}
	}
	return false;
}



/* 按完整 header-list 语义判断一个字段项是否安全。 */
static bool __xrtHttpCorsRequestFieldSafelisted(
	const xhttpfield* pFields,
	size_t iCount,
	size_t iIndex
)
{
	xhttpfield Field;

	__xrtHttpFieldLoad(pFields, iIndex, &Field);
	if ( __xrtHttpCorsSingleRequestName(Field.Name) &&
		__xrtHttpCorsRequestNameRepeated(
			pFields, iCount, iIndex
		) ) {
		return false;
	}
	return xrtHttpCorsRequestHeaderSafelisted(
		Field.Name, Field.Value
	);
}



/* 判断一个字段项是否属于最终非安全名称集合。 */
bool __xrtHttpCorsRequestFieldUnsafe(
	const xhttpfield* pFields,
	size_t iCount,
	const xrt_http_cors_request_info* pInfo,
	size_t iIndex
)
{
	return pInfo->SafeOverflow ||
		!__xrtHttpCorsRequestFieldSafelisted(
			pFields, iCount, iIndex
		);
}



/* 判断当前名称是否已在更早的字段项出现。 */
static bool __xrtHttpCorsRequestNameSeen(
	const xhttpfield* pFields,
	size_t iIndex,
	xstrview Name
)
{
	xhttpfield Previous;
	size_t i;

	for ( i = 0; i < iIndex; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Previous);
		if ( xrtHttpFieldNameEqual(
			Previous.Name, Name
		) ) {
			return true;
		}
	}
	return false;
}



/* 判断一个唯一字段名是否包含至少一个非安全项。 */
static bool __xrtHttpCorsRequestNameUnsafe(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	bool bSafeOverflow
)
{
	xhttpfield Field;
	size_t i;

	if ( bSafeOverflow ) {
		return true;
	}
	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( xrtHttpFieldNameEqual(Field.Name, Name) &&
			!__xrtHttpCorsRequestFieldSafelisted(
				pFields, iCount, i
			) ) {
			return true;
		}
	}
	return false;
}



/* 完整验证请求字段并计算 Fetch 非安全名称集合大小。 */
bool __xrtHttpCorsRequestInspect(
	const xhttpfield* pFields,
	size_t iCount,
	xrt_http_cors_request_info* pInfo
)
{
	xrt_http_cors_request_info Info = { 0 };
	xhttpfield Field;
	size_t iSafeSize = 0;
	size_t i;

	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( !xrtHttpTokenValid(Field.Name) ||
			!xrtHttpFieldValueValid(Field.Value) ) {
			__xrtErrorSetValue();
			return false;
		}
		if ( !__xrtHttpCorsRequestFieldSafelisted(
			pFields, iCount, i
		) ) {
			continue;
		}
		if ( Field.Value.Size > (1024u - iSafeSize) ) {
			Info.SafeOverflow = true;
		} else {
			iSafeSize += Field.Value.Size;
		}
	}
	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( __xrtHttpCorsRequestNameSeen(
			pFields, i, Field.Name
		) ) {
			continue;
		}
		if ( __xrtHttpCorsRequestNameUnsafe(
			pFields, iCount, Field.Name,
			Info.SafeOverflow
		) ) {
			Info.UnsafeCount++;
		}
	}
	*pInfo = Info;
	return true;
}



/* 从请求检查事实构造唯一的预检计划语义。 */
void __xrtHttpCorsPreflightMake(
	xstrview Method,
	bool bForce,
	const xrt_http_cors_request_info* pInfo,
	xhttpcorspreflightplan* pPlan
)
{
	xhttpcorspreflightplan Plan = { 0 };

	Plan.HeaderCount = pInfo->UnsafeCount;
	if ( bForce ) {
		Plan.Flags |= XHTTP_CORS_PREFLIGHT_FORCED;
	}
	if ( !xrtHttpCorsMethodSafelisted(Method) ) {
		Plan.Flags |= XHTTP_CORS_PREFLIGHT_METHOD;
	}
	if ( pInfo->UnsafeCount != 0 ) {
		Plan.Flags |= XHTTP_CORS_PREFLIGHT_HEADERS;
	}
	if ( Plan.Flags != 0 ) {
		Plan.Flags |= XHTTP_CORS_PREFLIGHT_REQUIRED;
	}
	*pPlan = Plan;
}



/* 从指定位置读取下一个唯一非安全请求字段名。 */
xhttpnext __xrtHttpCorsUnsafeNameNext(
	const xhttpfield* pFields,
	size_t iCount,
	const xrt_http_cors_request_info* pInfo,
	size_t* pOffset,
	xstrview* pName
)
{
	xhttpfield Field;
	size_t i;

	for ( i = *pOffset; i < iCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( __xrtHttpCorsRequestNameSeen(
			pFields, i, Field.Name
		) || !__xrtHttpCorsRequestNameUnsafe(
			pFields, iCount, Field.Name,
			pInfo->SafeOverflow
		) ) {
			continue;
		}
		*pOffset = i + 1u;
		*pName = Field.Name;
		return XHTTP_NEXT_ITEM;
	}
	*pOffset = iCount;
	*pName = (xstrview){ NULL, 0 };
	return XHTTP_NEXT_END;
}



/* 判断请求 Origin 是否与 Allow-Origin 的显式值相同。 */
static bool __xrtHttpCorsClientOriginSame(
	const xhttporigin* pRequest,
	const xhttporigin* pAllowed
)
{
	bool bRequestNull = (pRequest->Flags &
		XHTTP_ORIGIN_NULL) != 0;
	bool bAllowedNull = (pAllowed->Flags &
		XHTTP_ORIGIN_NULL) != 0;

	if ( bRequestNull || bAllowedNull ) {
		return bRequestNull && bAllowedNull;
	}
	return __xrtHttpOriginTupleSame(pRequest, pAllowed);
}



/* 读取并完整验证允许方法列表。 */
static bool __xrtHttpCorsClientMethodAllowed(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Method,
	bool* pExplicit,
	bool* pWildcard
)
{
	xhttpcorscursor Cursor;
	xstrview Item;
	xhttpnext Next;

	*pExplicit = false;
	*pWildcard = false;
	xrtHttpCorsCursorInit(&Cursor);
	while ( (Next = xrtHttpCorsAllowMethodNext(
		pFields, iCount, &Cursor, &Item
	)) == XHTTP_NEXT_ITEM ) {
		if ( xrtHttpMethodEqual(Item, Method) ) {
			*pExplicit = true;
		}
		if ( (Item.Size == 1u) &&
			(Item.Data[0] == '*') ) {
			*pWildcard = true;
		}
	}
	return Next != XHTTP_NEXT_ERROR;
}



/* 读取并完整验证允许字段列表。 */
static bool __xrtHttpCorsClientHeaderAllowed(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	bool* pExplicit,
	bool* pWildcard
)
{
	xhttpcorscursor Cursor;
	xstrview Item;
	xhttpnext Next;

	*pExplicit = false;
	*pWildcard = false;
	xrtHttpCorsCursorInit(&Cursor);
	while ( (Next = xrtHttpCorsAllowHeaderNext(
		pFields, iCount, &Cursor, &Item
	)) == XHTTP_NEXT_ITEM ) {
		if ( xrtHttpFieldNameEqual(Item, Name) ) {
			*pExplicit = true;
		}
		if ( (Item.Size == 1u) &&
			(Item.Data[0] == '*') ) {
			*pWildcard = true;
		}
	}
	return Next != XHTTP_NEXT_ERROR;
}



/* 完整验证允许字段列表，即使当前请求没有非安全字段也不能跳过。 */
static bool __xrtHttpCorsClientHeadersValid(
	const xhttpfield* pFields,
	size_t iCount
)
{
	xhttpcorscursor Cursor;
	xstrview Item;
	xhttpnext Next;

	xrtHttpCorsCursorInit(&Cursor);
	while ( (Next = xrtHttpCorsAllowHeaderNext(
		pFields, iCount, &Cursor, &Item
	)) == XHTTP_NEXT_ITEM ) {
	}
	return Next != XHTTP_NEXT_ERROR;
}



/* 无错误副作用地按 Fetch 规则读取预检缓存时间。 */
static uint64 __xrtHttpCorsClientMaxAge(
	const xhttpfield* pFields,
	size_t iCount
)
{
	xhttpfield Field;
	xstrview Text = { NULL, 0 };
	uint64 iValue = 0;
	size_t iFound = 0;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpFieldLoad(pFields, i, &Field);
		if ( !xrtHttpFieldNameEqual(
			Field.Name,
			XRT_STR_LITERAL("Access-Control-Max-Age")
		) ) {
			continue;
		}
		iFound++;
		Text = xrtHttpOwsTrim(Field.Value);
	}
	if ( (iFound != 1u) || (Text.Size == 0) ) {
		return 5u;
	}
	for ( i = 0; i < Text.Size; i++ ) {
		uint8 iDigit;

		if ( (Text.Data[i] < '0') ||
			(Text.Data[i] > '9') ) {
			return 5u;
		}
		iDigit = (uint8)(Text.Data[i] - '0');
		if ( iValue > ((UINT64_MAX - iDigit) / 10u) ) {
			return 5u;
		}
		iValue = (iValue * 10u) + iDigit;
	}
	return iValue;
}



/* 发布一个语义拒绝结果。 */
static void __xrtHttpCorsClientReject(
	xhttpcorsclientresult* pResult,
	xhttpcorsclientreject Reject,
	bool bPreflight
)
{
	memset(pResult, 0, sizeof(*pResult));
	pResult->Reject = Reject;
	if ( bPreflight ) {
		pResult->Flags = XHTTP_CORS_CLIENT_PREFLIGHT;
	}
}



/* 规划是否需要发送 CORS 预检。 */
XRT_API bool xrtHttpCorsPreflightPlan(
	xstrview Method,
	const xhttpfield* pFields,
	size_t iCount,
	bool bForce,
	xhttpcorspreflightplan* pOutput
)
{
	xhttpcorspreflightplan Output = { 0 };
	xrt_http_cors_request_info Info;

	if ( !__xrtHttpViewValid(Method) ||
		!__xrtRangeValid(pOutput, sizeof(Output)) ||
		!__xrtHttpFieldArrayValid(pFields, iCount) ||
		__xrtRangesOverlap(
			Method.Data, Method.Size,
			pOutput, sizeof(Output)
		) || __xrtHttpFieldArrayOverlap(
			pFields, iCount, pOutput, sizeof(Output)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pOutput, &Output, sizeof(Output));
	if ( !xrtHttpTokenValid(Method) ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( !__xrtHttpCorsRequestInspect(
		pFields, iCount, &Info
	) ) {
		return false;
	}
	__xrtHttpCorsPreflightMake(
		Method, bForce, &Info, &Output
	);
	memcpy(pOutput, &Output, sizeof(Output));
	return true;
}



/* 校验实际 CORS 响应。 */
XRT_API bool xrtHttpCorsClientCheck(
	const xhttporigin* pRequestOrigin,
	bool bCredentials,
	const xhttpfield* pResponseFields,
	size_t iResponseFieldCount,
	xhttpcorsclientresult* pOutput
)
{
	xhttporigin RequestOrigin;
	xhttpcorsorigin Allowed;
	xhttpcorsclientresult Output = { 0 };
	xhttpnext Next;

	if ( !__xrtRangeValid(
		pRequestOrigin, sizeof(RequestOrigin)
	) || !__xrtRangeValid(pOutput, sizeof(Output)) ||
		!__xrtHttpFieldArrayValid(
			pResponseFields, iResponseFieldCount
		) || __xrtRangesOverlap(
			pRequestOrigin, sizeof(RequestOrigin),
			pOutput, sizeof(Output)
		) || __xrtHttpFieldArrayOverlap(
			pResponseFields, iResponseFieldCount,
			pOutput, sizeof(Output)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&RequestOrigin, pRequestOrigin, sizeof(RequestOrigin));
	memcpy(pOutput, &Output, sizeof(Output));
	if ( !__xrtHttpOriginValueValid(&RequestOrigin) ||
		__xrtHttpOriginOverlap(
			&RequestOrigin, pOutput, sizeof(Output)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Next = xrtHttpCorsAllowOriginFields(
		pResponseFields, iResponseFieldCount, &Allowed
	);
	if ( Next == XHTTP_NEXT_ERROR ) {
		return false;
	}
	if ( Next == XHTTP_NEXT_END ) {
		__xrtHttpCorsClientReject(
			&Output,
			XHTTP_CORS_CLIENT_REJECT_ORIGIN,
			false
		);
		memcpy(pOutput, &Output, sizeof(Output));
		return true;
	}
	if ( (Allowed.Flags &
		XHTTP_CORS_ORIGIN_WILDCARD) != 0 ) {
		if ( bCredentials ) {
			__xrtHttpCorsClientReject(
				&Output,
				XHTTP_CORS_CLIENT_REJECT_ORIGIN,
				false
			);
			memcpy(pOutput, &Output, sizeof(Output));
			return true;
		}
	} else if ( !__xrtHttpCorsClientOriginSame(
		&RequestOrigin, &Allowed.Origin
	) ) {
		__xrtHttpCorsClientReject(
			&Output,
			XHTTP_CORS_CLIENT_REJECT_ORIGIN,
			false
		);
		memcpy(pOutput, &Output, sizeof(Output));
		return true;
	}
	if ( bCredentials ) {
		Next = xrtHttpCorsAllowCredentialsFields(
			pResponseFields, iResponseFieldCount
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XHTTP_NEXT_END ) {
			__xrtHttpCorsClientReject(
				&Output,
				XHTTP_CORS_CLIENT_REJECT_CREDENTIALS,
				false
			);
			memcpy(pOutput, &Output, sizeof(Output));
			return true;
		}
	}
	Output.Flags = XHTTP_CORS_CLIENT_ALLOW;
	memcpy(pOutput, &Output, sizeof(Output));
	return true;
}



/* 校验 CORS 预检响应。 */
XRT_API bool xrtHttpCorsPreflightCheck(
	uint16 iStatus,
	const xhttporigin* pRequestOrigin,
	xstrview Method,
	const xhttpfield* pRequestFields,
	size_t iRequestFieldCount,
	bool bCredentials,
	const xhttpfield* pResponseFields,
	size_t iResponseFieldCount,
	xhttpcorsclientresult* pOutput
)
{
	xhttporigin RequestOrigin;
	xhttpcorsclientresult Output = { 0 };
	xrt_http_cors_request_info Info;
	xstrview Name;
	size_t iOffset = 0;
	bool bExplicit;
	bool bWildcard;

	if ( (iStatus < 100u) || (iStatus > 999u) ||
		!__xrtRangeValid(
			pRequestOrigin, sizeof(RequestOrigin)
		) ||
		!__xrtHttpViewValid(Method) ||
		!__xrtRangeValid(pOutput, sizeof(Output)) ||
		!__xrtHttpFieldArrayValid(
			pRequestFields, iRequestFieldCount
		) || !__xrtHttpFieldArrayValid(
			pResponseFields, iResponseFieldCount
		) || __xrtRangesOverlap(
			pRequestOrigin, sizeof(RequestOrigin),
			pOutput, sizeof(Output)
		) || __xrtRangesOverlap(
			Method.Data, Method.Size,
			pOutput, sizeof(Output)
		) || __xrtHttpFieldArrayOverlap(
			pRequestFields, iRequestFieldCount,
			pOutput, sizeof(Output)
		) || __xrtHttpFieldArrayOverlap(
			pResponseFields, iResponseFieldCount,
			pOutput, sizeof(Output)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&RequestOrigin, pRequestOrigin, sizeof(RequestOrigin));
	memcpy(pOutput, &Output, sizeof(Output));
	if ( !__xrtHttpOriginValueValid(&RequestOrigin) ||
		__xrtHttpOriginOverlap(
			&RequestOrigin, pOutput, sizeof(Output)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtHttpTokenValid(Method) ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( !__xrtHttpCorsRequestInspect(
		pRequestFields, iRequestFieldCount, &Info
	) || !xrtHttpCorsClientCheck(
		&RequestOrigin,
		bCredentials,
		pResponseFields,
		iResponseFieldCount,
		&Output
	) ) {
		return false;
	}
	Output.Flags |= XHTTP_CORS_CLIENT_PREFLIGHT;
	if ( (Output.Flags & XHTTP_CORS_CLIENT_ALLOW) == 0 ) {
		memcpy(pOutput, &Output, sizeof(Output));
		return true;
	}
	if ( (iStatus < 200u) || (iStatus >= 300u) ) {
		__xrtHttpCorsClientReject(
			&Output,
			XHTTP_CORS_CLIENT_REJECT_STATUS,
			true
		);
		memcpy(pOutput, &Output, sizeof(Output));
		return true;
	}
	if ( !__xrtHttpCorsClientMethodAllowed(
		pResponseFields,
		iResponseFieldCount,
		Method,
		&bExplicit,
		&bWildcard
	) ) {
		return false;
	}
	if ( !xrtHttpCorsMethodSafelisted(Method) &&
		!bExplicit && (bCredentials || !bWildcard) ) {
		__xrtHttpCorsClientReject(
			&Output,
			XHTTP_CORS_CLIENT_REJECT_METHOD,
			true
		);
		memcpy(pOutput, &Output, sizeof(Output));
		return true;
	}
	if ( !__xrtHttpCorsClientHeadersValid(
		pResponseFields, iResponseFieldCount
	) ) {
		return false;
	}
	while ( __xrtHttpCorsUnsafeNameNext(
		pRequestFields,
		iRequestFieldCount,
		&Info,
		&iOffset,
		&Name
	) == XHTTP_NEXT_ITEM ) {
		if ( !__xrtHttpCorsClientHeaderAllowed(
			pResponseFields,
			iResponseFieldCount,
			Name,
			&bExplicit,
			&bWildcard
		) ) {
			return false;
		}
		if ( !bExplicit &&
			(bCredentials || !bWildcard ||
			 xrtHttpCorsRequestHeaderNonWildcard(Name)) ) {
			__xrtHttpCorsClientReject(
				&Output,
				XHTTP_CORS_CLIENT_REJECT_HEADER,
				true
			);
			memcpy(pOutput, &Output, sizeof(Output));
			return true;
		}
	}
	Output.MaxAge = __xrtHttpCorsClientMaxAge(
		pResponseFields, iResponseFieldCount
	);
	memcpy(pOutput, &Output, sizeof(Output));
	return true;
}

#endif

#include "../internal/xrt_http.h"

#include <xrt/http_cors.h>



#if defined(XRT_FEATURE_HTTP_CORS)

/* 读取唯一字段值，并兼容未对齐字段数组。 */
static xhttpnext __xrtHttpCorsFieldValue(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xstrview* pValue
)
{
	const xhttpfield* pFound = NULL;
	xhttpfield Field;
	xhttpnext Next;

	memset(pValue, 0, sizeof(*pValue));
	Next = xrtHttpFieldGetUnique(
		pFields, iCount, Name, &pFound
	);
	if ( Next != XHTTP_NEXT_ITEM ) {
		return Next;
	}
	memcpy(&Field, pFound, sizeof(Field));
	*pValue = Field.Value;
	return XHTTP_NEXT_ITEM;
}



/* 严格解析无符号十进制秒数。 */
static bool __xrtHttpCorsSecondsParse(
	xstrview Text,
	uint64* pSeconds
)
{
	uint64 iValue = 0;
	size_t i;

	if ( Text.Size == 0 ) {
		return false;
	}
	for ( i = 0; i < Text.Size; i++ ) {
		uint8 iDigit;

		if ( (Text.Data[i] < '0') ||
			(Text.Data[i] > '9') ) {
			return false;
		}
		iDigit = (uint8)(Text.Data[i] - '0');
		if ( iValue > ((UINT64_MAX - iDigit) / 10u) ) {
			return false;
		}
		iValue = (iValue * 10u) + iDigit;
	}
	*pSeconds = iValue;
	return true;
}



/* 汇总一个可重复 CORS token-list 字段。 */
static bool __xrtHttpCorsListRead(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	uint32 iFlag,
	uint32* pFlags,
	size_t* pItems
)
{
	size_t iFields;
	size_t iTokens;

	iFields = xrtHttpFieldCount(pFields, iCount, Name);
	if ( !xrtHttpFieldTokenCount(
		pFields, iCount, Name, &iTokens
	) ) {
		return false;
	}
	if ( iFields != 0 ) {
		*pFlags |= iFlag;
	}
	*pItems = iTokens;
	return true;
}



/* 严格读取一个 CORS 方法。 */
XRT_API bool xrtHttpCorsMethodParse(
	xstrview Value,
	xstrview* pMethod
)
{
	xstrview Method;

	if ( !__xrtHttpViewValid(Value) ||
		!__xrtRangeValid(pMethod, sizeof(Method)) ||
		__xrtRangesOverlap(
			Value.Data, Value.Size, pMethod, sizeof(Method)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Method, 0, sizeof(Method));
	memcpy(pMethod, &Method, sizeof(Method));
	Method = xrtHttpOwsTrim(Value);
	if ( !xrtHttpTokenValid(Method) ) {
		__xrtErrorSetValue();
		return false;
	}
	memcpy(pMethod, &Method, sizeof(Method));
	return true;
}



/* 严格读取 CORS 允许源。 */
XRT_API bool xrtHttpCorsAllowOriginParse(
	xstrview Value,
	xhttpcorsorigin* pOutput
)
{
	xhttpcorsorigin Output;
	xstrview Text;

	if ( !__xrtHttpViewValid(Value) ||
		!__xrtRangeValid(pOutput, sizeof(Output)) ||
		__xrtRangesOverlap(
			Value.Data, Value.Size, pOutput, sizeof(Output)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Output, 0, sizeof(Output));
	memcpy(pOutput, &Output, sizeof(Output));
	Text = xrtHttpOwsTrim(Value);
	if ( (Text.Size == 1) && (Text.Data[0] == '*') ) {
		Output.Flags = XHTTP_CORS_ORIGIN_WILDCARD;
	} else if ( !xrtHttpOriginParse(Text, &Output.Origin) ) {
		return false;
	}
	memcpy(pOutput, &Output, sizeof(Output));
	return true;
}



/* 验证 CORS 凭据许可的唯一路线值。 */
XRT_API bool xrtHttpCorsAllowCredentialsParse(xstrview Value)
{
	xstrview Text;

	if ( !__xrtHttpViewValid(Value) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Text = xrtHttpOwsTrim(Value);
	if ( (Text.Size != 4) ||
		(memcmp(Text.Data, "true", 4) != 0) ) {
		__xrtErrorSetValue();
		return false;
	}
	return true;
}



/* 严格读取 CORS 预检缓存秒数。 */
XRT_API bool xrtHttpCorsMaxAgeParse(
	xstrview Value,
	uint64* pSeconds
)
{
	xstrview Text;
	uint64 iSeconds = 0;

	if ( !__xrtHttpViewValid(Value) ||
		!__xrtRangeValid(pSeconds, sizeof(iSeconds)) ||
		__xrtRangesOverlap(
			Value.Data, Value.Size,
			pSeconds, sizeof(iSeconds)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(pSeconds, &iSeconds, sizeof(iSeconds));
	Text = xrtHttpOwsTrim(Value);
	if ( !__xrtHttpCorsSecondsParse(Text, &iSeconds) ) {
		__xrtErrorSetValue();
		return false;
	}
	memcpy(pSeconds, &iSeconds, sizeof(iSeconds));
	return true;
}



/* 读取唯一 CORS 允许源字段。 */
XRT_API xhttpnext xrtHttpCorsAllowOriginFields(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcorsorigin* pOutput
)
{
	xhttpcorsorigin Output;
	xstrview Value;
	xhttpnext Next;

	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ||
		!__xrtRangeValid(pOutput, sizeof(Output)) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount, pOutput, sizeof(Output)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memset(&Output, 0, sizeof(Output));
	memcpy(pOutput, &Output, sizeof(Output));
	Next = __xrtHttpCorsFieldValue(
		pFields,
		iCount,
		XRT_STR_LITERAL("Access-Control-Allow-Origin"),
		&Value
	);
	if ( Next != XHTTP_NEXT_ITEM ) {
		return Next;
	}
	if ( !xrtHttpCorsAllowOriginParse(Value, &Output) ) {
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pOutput, &Output, sizeof(Output));
	return XHTTP_NEXT_ITEM;
}



/* 读取唯一 CORS 预检方法字段。 */
XRT_API xhttpnext xrtHttpCorsRequestMethodFields(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview* pMethod
)
{
	xstrview Method;
	xstrview Value;
	xhttpnext Next;

	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ||
		!__xrtRangeValid(pMethod, sizeof(Method)) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount, pMethod, sizeof(Method)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memset(&Method, 0, sizeof(Method));
	memcpy(pMethod, &Method, sizeof(Method));
	Next = __xrtHttpCorsFieldValue(
		pFields,
		iCount,
		XRT_STR_LITERAL("Access-Control-Request-Method"),
		&Value
	);
	if ( Next != XHTTP_NEXT_ITEM ) {
		return Next;
	}
	if ( !xrtHttpCorsMethodParse(Value, &Method) ) {
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pMethod, &Method, sizeof(Method));
	return XHTTP_NEXT_ITEM;
}



/* 读取唯一 CORS 凭据许可字段。 */
XRT_API xhttpnext xrtHttpCorsAllowCredentialsFields(
	const xhttpfield* pFields,
	size_t iCount
)
{
	xstrview Value;
	xhttpnext Next;

	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	Next = __xrtHttpCorsFieldValue(
		pFields,
		iCount,
		XRT_STR_LITERAL("Access-Control-Allow-Credentials"),
		&Value
	);
	if ( Next != XHTTP_NEXT_ITEM ) {
		return Next;
	}
	return xrtHttpCorsAllowCredentialsParse(Value) ?
		XHTTP_NEXT_ITEM : XHTTP_NEXT_ERROR;
}



/* 读取唯一 CORS 预检缓存字段。 */
XRT_API xhttpnext xrtHttpCorsMaxAgeFields(
	const xhttpfield* pFields,
	size_t iCount,
	uint64* pSeconds
)
{
	xstrview Value;
	xhttpnext Next;
	uint64 iSeconds = 0;

	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ||
		!__xrtRangeValid(pSeconds, sizeof(iSeconds)) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount, pSeconds, sizeof(iSeconds)
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pSeconds, &iSeconds, sizeof(iSeconds));
	Next = __xrtHttpCorsFieldValue(
		pFields,
		iCount,
		XRT_STR_LITERAL("Access-Control-Max-Age"),
		&Value
	);
	if ( Next != XHTTP_NEXT_ITEM ) {
		return Next;
	}
	if ( !xrtHttpCorsMaxAgeParse(Value, &iSeconds) ) {
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pSeconds, &iSeconds, sizeof(iSeconds));
	return XHTTP_NEXT_ITEM;
}



/* 初始化 CORS 列表游标。 */
XRT_API void xrtHttpCorsCursorInit(xhttpcorscursor* pCursor)
{
	xrtHttpFieldTokenCursorInit(pCursor);
}



/* 读取请求头名称列表。 */
XRT_API xhttpnext xrtHttpCorsRequestHeaderNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcorscursor* pCursor,
	xstrview* pName
)
{
	return __xrtHttpFieldTokenNextRequired(
		pFields,
		iCount,
		XRT_STR_LITERAL("Access-Control-Request-Headers"),
		pCursor,
		pName
	);
}



/* 读取允许方法列表。 */
XRT_API xhttpnext xrtHttpCorsAllowMethodNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcorscursor* pCursor,
	xstrview* pMethod
)
{
	return xrtHttpFieldTokenNext(
		pFields,
		iCount,
		XRT_STR_LITERAL("Access-Control-Allow-Methods"),
		pCursor,
		pMethod
	);
}



/* 读取允许请求头列表。 */
XRT_API xhttpnext xrtHttpCorsAllowHeaderNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcorscursor* pCursor,
	xstrview* pName
)
{
	return xrtHttpFieldTokenNext(
		pFields,
		iCount,
		XRT_STR_LITERAL("Access-Control-Allow-Headers"),
		pCursor,
		pName
	);
}



/* 读取暴露响应头列表。 */
XRT_API xhttpnext xrtHttpCorsExposeHeaderNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcorscursor* pCursor,
	xstrview* pName
)
{
	return xrtHttpFieldTokenNext(
		pFields,
		iCount,
		XRT_STR_LITERAL("Access-Control-Expose-Headers"),
		pCursor,
		pName
	);
}



/* 读取并验证请求侧 CORS 字段组合。 */
XRT_API bool xrtHttpCorsRequestRead(
	xstrview Method,
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcorsrequest* pOutput
)
{
	xhttpcorsrequest Output;
	xhttpnext OriginNext;
	xhttpnext MethodNext;
	size_t iHeaderFields;

	if ( !xrtHttpTokenValid(Method) ||
		!__xrtHttpFieldArrayValid(pFields, iCount) ||
		!__xrtRangeValid(pOutput, sizeof(Output)) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount, pOutput, sizeof(Output)
		) || __xrtRangesOverlap(
			Method.Data, Method.Size, pOutput, sizeof(Output)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Output, 0, sizeof(Output));
	memcpy(pOutput, &Output, sizeof(Output));
	Output.Method = Method;
	OriginNext = xrtHttpOriginFields(
		pFields, iCount, &Output.Origin
	);
	if ( OriginNext == XHTTP_NEXT_ERROR ) {
		return false;
	}
	if ( OriginNext == XHTTP_NEXT_ITEM ) {
		Output.Flags |= XHTTP_CORS_REQUEST_ORIGIN;
	}
	MethodNext = xrtHttpCorsRequestMethodFields(
		pFields, iCount, &Output.RequestMethod
	);
	if ( MethodNext == XHTTP_NEXT_ERROR ) {
		return false;
	}
	iHeaderFields = xrtHttpFieldCount(
		pFields,
		iCount,
		XRT_STR_LITERAL("Access-Control-Request-Headers")
	);
	if ( !xrtHttpFieldTokenCount(
		pFields,
		iCount,
		XRT_STR_LITERAL("Access-Control-Request-Headers"),
		&Output.HeaderCount
	) ) {
		return false;
	}
	if ( MethodNext == XHTTP_NEXT_ITEM ) {
		if ( (OriginNext != XHTTP_NEXT_ITEM) ||
			!xrtHttpMethodEqual(
				Method, XRT_STR_LITERAL("OPTIONS")
			) ) {
			__xrtErrorSetValue();
			return false;
		}
		Output.Flags |= XHTTP_CORS_REQUEST_PREFLIGHT;
	}
	if ( iHeaderFields != 0 ) {
		if ( (MethodNext != XHTTP_NEXT_ITEM) ||
			(Output.HeaderCount == 0) ) {
			__xrtErrorSetValue();
			return false;
		}
		Output.Flags |= XHTTP_CORS_REQUEST_HEADERS;
	}
	memcpy(pOutput, &Output, sizeof(Output));
	return true;
}



/* 读取并验证响应侧 CORS 字段组合。 */
XRT_API bool xrtHttpCorsResponseRead(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcorsresponse* pOutput
)
{
	xhttpcorsresponse Output;
	xhttpnext Next;

	if ( !__xrtHttpFieldArrayValid(pFields, iCount) ||
		!__xrtRangeValid(pOutput, sizeof(Output)) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount, pOutput, sizeof(Output)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Output, 0, sizeof(Output));
	memcpy(pOutput, &Output, sizeof(Output));
	Next = xrtHttpCorsAllowOriginFields(
		pFields, iCount, &Output.AllowOrigin
	);
	if ( Next == XHTTP_NEXT_ERROR ) {
		return false;
	}
	if ( Next == XHTTP_NEXT_ITEM ) {
		Output.Flags |= XHTTP_CORS_RESPONSE_ALLOW_ORIGIN;
	}
	Next = xrtHttpCorsAllowCredentialsFields(pFields, iCount);
	if ( Next == XHTTP_NEXT_ERROR ) {
		return false;
	}
	if ( Next == XHTTP_NEXT_ITEM ) {
		Output.Flags |= XHTTP_CORS_RESPONSE_CREDENTIALS;
	}
	Next = xrtHttpCorsMaxAgeFields(
		pFields, iCount, &Output.MaxAge
	);
	if ( Next == XHTTP_NEXT_ERROR ) {
		return false;
	}
	if ( Next == XHTTP_NEXT_ITEM ) {
		Output.Flags |= XHTTP_CORS_RESPONSE_MAX_AGE;
	}
	if ( !__xrtHttpCorsListRead(
		pFields,
		iCount,
		XRT_STR_LITERAL("Access-Control-Allow-Methods"),
		XHTTP_CORS_RESPONSE_ALLOW_METHODS,
		&Output.Flags,
		&Output.MethodCount
	) || !__xrtHttpCorsListRead(
		pFields,
		iCount,
		XRT_STR_LITERAL("Access-Control-Allow-Headers"),
		XHTTP_CORS_RESPONSE_ALLOW_HEADERS,
		&Output.Flags,
		&Output.HeaderCount
	) || !__xrtHttpCorsListRead(
		pFields,
		iCount,
		XRT_STR_LITERAL("Access-Control-Expose-Headers"),
		XHTTP_CORS_RESPONSE_EXPOSE_HEADERS,
		&Output.Flags,
		&Output.ExposeCount
	) ) {
		return false;
	}
	memcpy(pOutput, &Output, sizeof(Output));
	return true;
}

#endif

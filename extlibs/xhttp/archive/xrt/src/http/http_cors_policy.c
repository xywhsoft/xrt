#include "../internal/xrt_http_cors.h"



#if defined(XRT_FEATURE_HTTP_CORS_POLICY)

#define XRT_HTTP_CORS_POLICY_FLAGS ( \
	XHTTP_CORS_POLICY_ANY_ORIGIN | \
	XHTTP_CORS_POLICY_CREDENTIALS | \
	XHTTP_CORS_POLICY_ANY_METHOD | \
	XHTTP_CORS_POLICY_ANY_HEADER | \
	XHTTP_CORS_POLICY_MAX_AGE )

#define XRT_HTTP_CORS_DECISION_FLAGS ( \
	XHTTP_CORS_DECISION_ALLOW | \
	XHTTP_CORS_DECISION_PREFLIGHT | \
	XHTTP_CORS_DECISION_CREDENTIALS | \
	XHTTP_CORS_DECISION_ALLOW_HEADERS | \
	XHTTP_CORS_DECISION_EXPOSE_HEADERS | \
	XHTTP_CORS_DECISION_MAX_AGE | \
	XHTTP_CORS_DECISION_VARY_ORIGIN )



/* 验证固定大小数组范围并返回总字节数。 */
static bool __xrtHttpCorsArrayValid(
	const void* pItems,
	size_t iCount,
	size_t iItemSize,
	size_t* pBytes
)
{
	if ( iCount > (SIZE_MAX / iItemSize) ) {
		return false;
	}
	*pBytes = iCount * iItemSize;
	return __xrtRangeValid(pItems, *pBytes);
}



/* 验证 token 视图数组。 */
static bool __xrtHttpCorsTokenArrayValid(
	const xstrview* pItems,
	size_t iCount,
	bool bWildcard
)
{
	xstrview Item;
	size_t iBytes;
	size_t i;

	if ( !__xrtHttpCorsArrayValid(
		pItems, iCount, sizeof(Item), &iBytes
	) ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		memcpy(&Item, &pItems[i], sizeof(Item));
		if ( !xrtHttpTokenValid(Item) ||
			(!bWildcard && (Item.Size == 1u) &&
			 (Item.Data[0] == '*')) ) {
			return false;
		}
	}
	return true;
}



/* 验证预解析 Origin 数组。 */
static bool __xrtHttpCorsOriginArrayValid(
	const xhttporigin* pOrigins,
	size_t iCount
)
{
	xhttporigin Origin;
	size_t iBytes;
	size_t i;

	if ( !__xrtHttpCorsArrayValid(
		pOrigins, iCount, sizeof(Origin), &iBytes
	) ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		memcpy(&Origin, &pOrigins[i], sizeof(Origin));
		if ( !__xrtHttpOriginValueValid(&Origin) ) {
			return false;
		}
	}
	return true;
}



/* 验证策略描述符及全部借用数组。 */
static bool __xrtHttpCorsPolicyValid(const xhttpcorspolicy* pPolicy)
{
	return ((pPolicy->Flags &
		~XRT_HTTP_CORS_POLICY_FLAGS) == 0) &&
		(((pPolicy->Flags & XHTTP_CORS_POLICY_MAX_AGE) != 0) ||
		 (pPolicy->MaxAge == 0)) &&
		__xrtHttpCorsOriginArrayValid(
			pPolicy->Origins, pPolicy->OriginCount
		) && __xrtHttpCorsTokenArrayValid(
			pPolicy->Methods, pPolicy->MethodCount, false
		) && __xrtHttpCorsTokenArrayValid(
			pPolicy->Headers, pPolicy->HeaderCount, false
		) && __xrtHttpCorsTokenArrayValid(
			pPolicy->ExposeHeaders, pPolicy->ExposeCount, true
		);
}



/* 判断 Origin 是否命中显式策略数组。 */
static bool __xrtHttpCorsOriginAllowed(
	const xhttpcorspolicy* pPolicy,
	const xhttporigin* pRequest
)
{
	xhttporigin Allowed;
	size_t i;

	if ( (pPolicy->Flags &
		XHTTP_CORS_POLICY_ANY_ORIGIN) != 0 ) {
		return true;
	}
	for ( i = 0; i < pPolicy->OriginCount; i++ ) {
		memcpy(&Allowed, &pPolicy->Origins[i], sizeof(Allowed));
		if ( ((Allowed.Flags & XHTTP_ORIGIN_NULL) != 0) ||
			((pRequest->Flags & XHTTP_ORIGIN_NULL) != 0) ) {
			if ( ((Allowed.Flags & XHTTP_ORIGIN_NULL) != 0) &&
				((pRequest->Flags & XHTTP_ORIGIN_NULL) != 0) ) {
				return true;
			}
			continue;
		}
		if ( __xrtHttpOriginTupleSame(&Allowed, pRequest) ) {
			return true;
		}
	}
	return false;
}



/* 判断区分大小写的方法是否命中策略数组。 */
static bool __xrtHttpCorsMethodAllowed(
	const xhttpcorspolicy* pPolicy,
	xstrview Method
)
{
	xstrview Allowed;
	size_t i;

	if ( (pPolicy->Flags &
		XHTTP_CORS_POLICY_ANY_METHOD) != 0 ) {
		return true;
	}
	for ( i = 0; i < pPolicy->MethodCount; i++ ) {
		memcpy(&Allowed, &pPolicy->Methods[i], sizeof(Allowed));
		if ( xrtHttpMethodEqual(Allowed, Method) ) {
			return true;
		}
	}
	return false;
}



/* 判断大小写不敏感的字段名是否命中策略数组。 */
static bool __xrtHttpCorsHeaderAllowed(
	const xhttpcorspolicy* pPolicy,
	xstrview Name
)
{
	xstrview Allowed;
	size_t i;

	if ( (pPolicy->Flags &
		XHTTP_CORS_POLICY_ANY_HEADER) != 0 ) {
		return true;
	}
	for ( i = 0; i < pPolicy->HeaderCount; i++ ) {
		memcpy(&Allowed, &pPolicy->Headers[i], sizeof(Allowed));
		if ( xrtHttpFieldNameEqual(Allowed, Name) ) {
			return true;
		}
	}
	return false;
}



/* 判断输出是否覆盖策略描述符或任一借用配置。 */
static bool __xrtHttpCorsPolicyOverlap(
	const xhttpcorspolicy* pPolicy,
	const void* pMemory,
	size_t iSize
)
{
	xhttporigin Origin;
	xstrview Item;
	size_t iBytes;
	size_t i;

	if ( __xrtRangesOverlap(
		pPolicy, sizeof(*pPolicy), pMemory, iSize
	) ) {
		return true;
	}
	iBytes = pPolicy->OriginCount * sizeof(Origin);
	if ( __xrtRangesOverlap(
		pPolicy->Origins, iBytes, pMemory, iSize
	) ) {
		return true;
	}
	for ( i = 0; i < pPolicy->OriginCount; i++ ) {
		memcpy(&Origin, &pPolicy->Origins[i], sizeof(Origin));
		if ( __xrtHttpOriginOverlap(
			&Origin, pMemory, iSize
		) ) {
			return true;
		}
	}
	for ( i = 0; i < 3u; i++ ) {
		const xstrview* pItems;
		size_t iCount;
		size_t j;

		if ( i == 0 ) {
			pItems = pPolicy->Methods;
			iCount = pPolicy->MethodCount;
		} else if ( i == 1 ) {
			pItems = pPolicy->Headers;
			iCount = pPolicy->HeaderCount;
		} else {
			pItems = pPolicy->ExposeHeaders;
			iCount = pPolicy->ExposeCount;
		}
		iBytes = iCount * sizeof(Item);
		if ( __xrtRangesOverlap(
			pItems, iBytes, pMemory, iSize
		) ) {
			return true;
		}
		for ( j = 0; j < iCount; j++ ) {
			memcpy(&Item, &pItems[j], sizeof(Item));
			if ( __xrtRangesOverlap(
				Item.Data, Item.Size, pMemory, iSize
			) ) {
				return true;
			}
		}
	}
	return false;
}



/* 判断 CORS Origin 描述符是否为空。 */
static bool __xrtHttpCorsOriginEmpty(const xhttpcorsorigin* pOrigin)
{
	return (pOrigin->Flags == 0) &&
		(pOrigin->Origin.Flags == 0) &&
		(pOrigin->Origin.Text.Size == 0) &&
		(pOrigin->Origin.Url.Flags == 0) &&
		(pOrigin->Origin.Url.Port == 0) &&
		(pOrigin->Origin.Url.Scheme.Size == 0) &&
		(pOrigin->Origin.Url.Authority.Size == 0) &&
		(pOrigin->Origin.Url.UserInfo.Size == 0) &&
		(pOrigin->Origin.Url.Host.Size == 0) &&
		(pOrigin->Origin.Url.PortText.Size == 0) &&
		(pOrigin->Origin.Url.Path.Size == 0) &&
		(pOrigin->Origin.Url.Query.Size == 0) &&
		(pOrigin->Origin.Url.Fragment.Size == 0);
}



/* 验证决策描述符和写出所需的请求字段。 */
bool __xrtHttpCorsDecisionValid(
	const xhttpcorsdecision* pDecision,
	const xhttpfield* pRequestFields,
	size_t iRequestFieldCount
)
{
	bool bAllow;
	bool bWildcard;
	bool bPreflight;
	bool bHeaders;
	bool bExpose;

	if ( ((pDecision->Flags &
		~XRT_HTTP_CORS_DECISION_FLAGS) != 0) ||
		(pDecision->Reject < XHTTP_CORS_REJECT_NONE) ||
		(pDecision->Reject > XHTTP_CORS_REJECT_HEADER) ||
		!__xrtHttpFieldArrayValid(
			pRequestFields, iRequestFieldCount
		) ) {
		return false;
	}
	bAllow = (pDecision->Flags & XHTTP_CORS_DECISION_ALLOW) != 0;
	bPreflight = (pDecision->Flags &
		XHTTP_CORS_DECISION_PREFLIGHT) != 0;
	bHeaders = (pDecision->Flags &
		XHTTP_CORS_DECISION_ALLOW_HEADERS) != 0;
	bExpose = (pDecision->Flags &
		XHTTP_CORS_DECISION_EXPOSE_HEADERS) != 0;
	bWildcard = (pDecision->AllowOrigin.Flags &
		XHTTP_CORS_ORIGIN_WILDCARD) != 0;
	if ( !bAllow ) {
		return (pDecision->Flags == 0) &&
		__xrtHttpCorsOriginEmpty(&pDecision->AllowOrigin) &&
		(pDecision->AllowMethod.Size == 0) &&
		(pDecision->ExposeCount == 0) &&
		(pDecision->HeaderCount == 0) &&
		(pDecision->MaxAge == 0);
	}
	if ( pDecision->Reject != XHTTP_CORS_REJECT_NONE ) {
		return false;
	}
	if ( bWildcard ) {
		if ( (pDecision->AllowOrigin.Flags !=
			XHTTP_CORS_ORIGIN_WILDCARD) ||
			!__xrtHttpCorsOriginEmpty(&(xhttpcorsorigin){
				.Origin = pDecision->AllowOrigin.Origin
			}) ) {
			return false;
		}
	} else if ( (pDecision->AllowOrigin.Flags != 0) ||
		!__xrtHttpOriginValueValid(
			&pDecision->AllowOrigin.Origin
		) ) {
		return false;
	}
	if ( bPreflight ) {
		if ( !xrtHttpTokenValid(pDecision->AllowMethod) ) {
			return false;
		}
	} else if ( pDecision->AllowMethod.Size != 0 ) {
		return false;
	}
	if ( bHeaders != (pDecision->HeaderCount != 0) ||
		(bHeaders && !bPreflight) ) {
		return false;
	}
	if ( bExpose != (pDecision->ExposeCount != 0) ||
		(bExpose && bPreflight) ||
		!__xrtHttpCorsTokenArrayValid(
			pDecision->ExposeHeaders,
			pDecision->ExposeCount,
			true
		) ) {
		return false;
	}
	if ( (((pDecision->Flags &
		XHTTP_CORS_DECISION_MAX_AGE) != 0) &&
		 !bPreflight) || (((pDecision->Flags &
		 XHTTP_CORS_DECISION_MAX_AGE) == 0) &&
		 (pDecision->MaxAge != 0)) ||
		(bWildcard && ((pDecision->Flags &
		 XHTTP_CORS_DECISION_CREDENTIALS) != 0)) ) {
		return false;
	}
	return true;
}



/* 判断输出是否覆盖决策或任一借用输入。 */
bool __xrtHttpCorsDecisionOverlap(
	const xhttpcorsdecision* pDecision,
	const xhttpfield* pRequestFields,
	size_t iRequestFieldCount,
	const void* pMemory,
	size_t iSize
)
{
	xstrview Item;
	size_t iBytes = pDecision->ExposeCount * sizeof(Item);
	size_t i;

	if ( __xrtRangesOverlap(
		pDecision, sizeof(*pDecision), pMemory, iSize
	) || __xrtHttpFieldArrayOverlap(
		pRequestFields,
		iRequestFieldCount,
		pMemory,
		iSize
	) || __xrtHttpOriginOverlap(
		&pDecision->AllowOrigin.Origin, pMemory, iSize
	) || __xrtRangesOverlap(
		pDecision->AllowMethod.Data,
		pDecision->AllowMethod.Size,
		pMemory,
		iSize
	) || __xrtRangesOverlap(
		pDecision->ExposeHeaders,
		iBytes,
		pMemory,
		iSize
	) ) {
		return true;
	}
	for ( i = 0; i < pDecision->ExposeCount; i++ ) {
		memcpy(&Item, &pDecision->ExposeHeaders[i], sizeof(Item));
		if ( __xrtRangesOverlap(
			Item.Data, Item.Size, pMemory, iSize
		) ) {
			return true;
		}
	}
	return false;
}



/* 读取请求并执行无分配 CORS 数组策略。 */
XRT_API bool xrtHttpCorsPolicyCheck(
	const xhttpcorspolicy* pPolicyInput,
	xstrview Method,
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcorsdecision* pOutput
)
{
	xhttpcorspolicy Policy;
	xhttpcorsrequest Request;
	xhttpcorsdecision Output;
	xhttpcorscursor Cursor;
	xstrview Header;
	xhttpnext Next;

	if ( !__xrtRangeValid(pPolicyInput, sizeof(Policy)) ||
		!__xrtRangeValid(pOutput, sizeof(Output)) ||
		!__xrtHttpViewValid(Method) ||
		!__xrtHttpFieldArrayValid(pFields, iCount) ||
		__xrtRangesOverlap(
			pPolicyInput, sizeof(Policy),
			pOutput, sizeof(Output)
		) || __xrtRangesOverlap(
			Method.Data, Method.Size,
			pOutput, sizeof(Output)
		) || __xrtHttpFieldArrayOverlap(
			pFields, iCount, pOutput, sizeof(Output)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Policy, pPolicyInput, sizeof(Policy));
	memset(&Output, 0, sizeof(Output));
	memcpy(pOutput, &Output, sizeof(Output));
	if ( !__xrtHttpCorsPolicyValid(&Policy) ||
		__xrtHttpCorsPolicyOverlap(
			&Policy, pOutput, sizeof(Output)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtHttpCorsRequestRead(
		Method, pFields, iCount, &Request
	) ) {
		return false;
	}
	if ( (Request.Flags &
		XHTTP_CORS_REQUEST_ORIGIN) == 0 ) {
		return true;
	}
	if ( !__xrtHttpCorsOriginAllowed(
		&Policy, &Request.Origin
	) ) {
		Output.Reject = XHTTP_CORS_REJECT_ORIGIN;
		memcpy(pOutput, &Output, sizeof(Output));
		return true;
	}
	if ( (Request.Flags &
		XHTTP_CORS_REQUEST_PREFLIGHT) != 0 ) {
		if ( !__xrtHttpCorsMethodAllowed(
			&Policy, Request.RequestMethod
		) ) {
			Output.Reject = XHTTP_CORS_REJECT_METHOD;
			memcpy(pOutput, &Output, sizeof(Output));
			return true;
		}
		xrtHttpCorsCursorInit(&Cursor);
		while ( (Next = xrtHttpCorsRequestHeaderNext(
			pFields, iCount, &Cursor, &Header
		)) == XHTTP_NEXT_ITEM ) {
			if ( !__xrtHttpCorsHeaderAllowed(
				&Policy, Header
			) ) {
				Output.Reject = XHTTP_CORS_REJECT_HEADER;
				memcpy(pOutput, &Output, sizeof(Output));
				return true;
			}
		}
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
	}
	Output.Flags = XHTTP_CORS_DECISION_ALLOW;
	if ( ((Policy.Flags &
		XHTTP_CORS_POLICY_ANY_ORIGIN) != 0) &&
		((Policy.Flags &
		 XHTTP_CORS_POLICY_CREDENTIALS) == 0) ) {
		Output.AllowOrigin.Flags =
			XHTTP_CORS_ORIGIN_WILDCARD;
	} else {
		Output.AllowOrigin.Origin = Request.Origin;
		Output.Flags |= XHTTP_CORS_DECISION_VARY_ORIGIN;
	}
	if ( (Policy.Flags &
		XHTTP_CORS_POLICY_CREDENTIALS) != 0 ) {
		Output.Flags |= XHTTP_CORS_DECISION_CREDENTIALS;
	}
	if ( (Request.Flags &
		XHTTP_CORS_REQUEST_PREFLIGHT) != 0 ) {
		Output.Flags |= XHTTP_CORS_DECISION_PREFLIGHT;
		Output.AllowMethod = Request.RequestMethod;
		Output.HeaderCount = Request.HeaderCount;
		if ( Request.HeaderCount != 0 ) {
			Output.Flags |=
				XHTTP_CORS_DECISION_ALLOW_HEADERS;
		}
		if ( (Policy.Flags &
			XHTTP_CORS_POLICY_MAX_AGE) != 0 ) {
			Output.Flags |= XHTTP_CORS_DECISION_MAX_AGE;
			Output.MaxAge = Policy.MaxAge;
		}
	} else if ( Policy.ExposeCount != 0 ) {
		Output.Flags |= XHTTP_CORS_DECISION_EXPOSE_HEADERS;
		Output.ExposeHeaders = Policy.ExposeHeaders;
		Output.ExposeCount = Policy.ExposeCount;
	}
	memcpy(pOutput, &Output, sizeof(Output));
	return true;
}

#endif

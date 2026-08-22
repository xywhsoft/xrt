#include "../internal/xrt_http_cache_validate.h"
#include "../internal/xrt_http_origin.h"
#include "../internal/xrt_url.h"



#if defined(XRT_FEATURE_HTTP_CACHE_VALIDATE)

/* 验证 HTTP 缓存目标是可确定 origin 的绝对 HTTP(S) URI。 */
static bool __xrtHttpCacheTargetUrl(
	xstrview Target,
	xurl* pUrl
)
{
	if ( !__xrtUrlParseValue(Target, pUrl) ) {
		return false;
	}
	if ( ((pUrl->Flags & (
			XURL_HAS_SCHEME |
			XURL_HAS_AUTHORITY |
			XURL_HAS_HOST
		 )) != (
			XURL_HAS_SCHEME |
			XURL_HAS_AUTHORITY |
			XURL_HAS_HOST
		 )) ||
		((pUrl->Flags & (
			XURL_HAS_USERINFO |
			XURL_HAS_FRAGMENT
		 )) != 0) ||
		(pUrl->Host.Size == 0) ) {
		return false;
	}
	return xrtUrlSchemeIs(
		pUrl, XRT_STR_LITERAL("http")
	) || xrtUrlSchemeIs(
		pUrl, XRT_STR_LITERAL("https")
	);
}



/* 从完整 URL 投影出不含资源组件的 Origin 三元组。 */
static bool __xrtHttpCacheOriginFromUrl(
	const xurl* pUrl,
	xhttporigin* pOrigin
)
{
	memset(pOrigin, 0, sizeof(*pOrigin));
	pOrigin->Url = *pUrl;
	pOrigin->Url.Flags &= ~(
		XURL_HAS_QUERY | XURL_HAS_FRAGMENT
	);
	pOrigin->Url.Path = (xstrview){ NULL, 0 };
	pOrigin->Url.Query = (xstrview){ NULL, 0 };
	pOrigin->Url.Fragment = (xstrview){ NULL, 0 };
	return __xrtHttpOriginValueValid(pOrigin);
}



/* 判断 URI-reference 解析并解析后仍与目标 URI 同源。 */
static bool __xrtHttpCacheReferenceSameOrigin(
	const xurl* pTarget,
	xstrview Reference,
	xurl* pReference
)
{
	xhttporigin TargetOrigin;
	xhttporigin ReferenceOrigin;

	if ( !__xrtUrlParseValue(
		Reference, pReference
	) ) {
		return false;
	}
	if ( (pReference->Flags &
		 XURL_HAS_SCHEME) != 0 ) {
		if ( (pReference->Flags &
			 XURL_HAS_AUTHORITY) == 0 ) {
			return false;
		}
	}
	if ( (pReference->Flags &
		 XURL_HAS_AUTHORITY) == 0 ) {
		return (pReference->Flags &
			XURL_HAS_SCHEME) == 0;
	}
	if ( ((pReference->Flags & (
			XURL_HAS_HOST |
			XURL_HAS_USERINFO
		 )) != XURL_HAS_HOST) ||
		(pReference->Host.Size == 0) ) {
		return false;
	}
	if ( (pReference->Flags & XURL_HAS_SCHEME) == 0 ) {
		pReference->Scheme = pTarget->Scheme;
		pReference->Flags |= XURL_HAS_SCHEME;
	}
	return __xrtHttpCacheOriginFromUrl(
		pTarget, &TargetOrigin
	) && __xrtHttpCacheOriginFromUrl(
		pReference, &ReferenceOrigin
	) &&
		__xrtHttpOriginTupleSame(
			&TargetOrigin, &ReferenceOrigin
		);
}



/* 判断当前位置字段属于可选的同源失效候选。 */
static bool __xrtHttpCacheLocationItem(
	const xurl* pTarget,
	const xhttpfield* pFields,
	size_t iCount,
	size_t iIndex,
	xhttpcacheinvalidateitem* pItem
)
{
	xstrview Name;
	xstrview Reference;
	xurl Parsed;
	xhttpcacheinvalidatekind Kind;

	Name = pFields[iIndex].Name;
	if ( xrtHttpFieldNameEqual(
		Name, XRT_STR_LITERAL("Location")
	) ) {
		if ( xrtHttpFieldCount(
			pFields, iCount,
			XRT_STR_LITERAL("Location")
		) != 1 ) {
			return false;
		}
		Kind = XHTTP_CACHE_INVALIDATE_LOCATION;
	} else if ( xrtHttpFieldNameEqual(
		Name, XRT_STR_LITERAL("Content-Location")
	) ) {
		if ( xrtHttpFieldCount(
			pFields, iCount,
			XRT_STR_LITERAL("Content-Location")
		) != 1 ) {
			return false;
		}
		Kind = XHTTP_CACHE_INVALIDATE_CONTENT_LOCATION;
	} else {
		return false;
	}
	Reference = xrtHttpOwsTrim(
		pFields[iIndex].Value
	);
	if ( !__xrtHttpCacheReferenceSameOrigin(
		pTarget, Reference, &Parsed
	) ) {
		return false;
	}
	pItem->Reference = Reference;
	pItem->Field = iIndex;
	pItem->Kind = Kind;
	return true;
}



/* 判断失效迭代器输入和输出边界不会相互覆盖。 */
static bool __xrtHttpCacheInvalidationInputValid(
	xstrview Method,
	xstrview Target,
	const xhttpfield* pFields,
	size_t iCount,
	const xhttpcacheinvalidatecursor* pCursor,
	const xhttpcacheinvalidateitem* pItem
)
{
	if ( !__xrtHttpViewValid(Method) ||
		!xrtHttpTokenValid(Method) ||
		!__xrtHttpViewValid(Target) ||
		!__xrtHttpCacheFieldsValid(
			pFields, iCount
		) ||
		(pCursor == NULL) ||
		(pItem == NULL) ||
		(pCursor->Field > iCount) ||
		__xrtRangesOverlap(
			pCursor, sizeof(*pCursor),
			pItem, sizeof(*pItem)
		) ||
		__xrtRangesOverlap(
			pCursor, sizeof(*pCursor),
			Method.Data, Method.Size
		) ||
		__xrtRangesOverlap(
			pCursor, sizeof(*pCursor),
			Target.Data, Target.Size
		) ||
		__xrtRangesOverlap(
			pItem, sizeof(*pItem),
			Method.Data, Method.Size
		) ||
		__xrtRangesOverlap(
			pItem, sizeof(*pItem),
			Target.Data, Target.Size
		) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount,
			pCursor, sizeof(*pCursor)
		) ||
		__xrtHttpFieldArrayOverlap(
			pFields, iCount,
			pItem, sizeof(*pItem)
		) ) {
		return false;
	}
	return true;
}



/* 初始化可重复使用的缓存失效候选游标。 */
XRT_API void xrtHttpCacheInvalidationCursorInit(
	xhttpcacheinvalidatecursor* pCursor
)
{
	if ( pCursor == NULL ) {
		return;
	}
	pCursor->Field = 0;
	pCursor->Target = true;
}



/* 迭代 unsafe 方法成功响应产生的同源失效候选。 */
XRT_API xhttpnext xrtHttpCacheInvalidationNext(
	xstrview Method,
	uint16 iStatus,
	xstrview Target,
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcacheinvalidatecursor* pCursor,
	xhttpcacheinvalidateitem* pItem
)
{
	xurl TargetUrl;
	size_t i;

	if ( !__xrtHttpCacheInvalidationInputValid(
		Method,
		Target,
		pFields,
		iCount,
		pCursor,
		pItem
	) || (iStatus < 100u) || (iStatus > 599u) ||
		!__xrtHttpCacheTargetUrl(
			Target, &TargetUrl
		) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_NEXT_ERROR;
	}
	memset(pItem, 0, sizeof(*pItem));
	pItem->Field = XRT_NPOS;
	if ( xrtHttpMethodSafe(Method) ||
		(iStatus < 200u) || (iStatus >= 400u) ) {
		pCursor->Target = false;
		pCursor->Field = iCount;
		return XHTTP_NEXT_END;
	}
	if ( pCursor->Target ) {
		pCursor->Target = false;
		pItem->Reference = Target;
		pItem->Field = XRT_NPOS;
		pItem->Kind = XHTTP_CACHE_INVALIDATE_TARGET;
		return XHTTP_NEXT_ITEM;
	}
	for ( i = pCursor->Field; i < iCount; i++ ) {
		pCursor->Field = i + 1u;
		if ( __xrtHttpCacheLocationItem(
			&TargetUrl,
			pFields,
			iCount,
			i,
			pItem
		) ) {
			return XHTTP_NEXT_ITEM;
		}
	}
	return XHTTP_NEXT_END;
}



/* 截去 URI-reference 中不会参与 HTTP 缓存键的 fragment。 */
static xstrview __xrtHttpCacheFragmentRemove(
	xstrview Reference
)
{
	size_t i;

	for ( i = 0; i < Reference.Size; i++ ) {
		if ( Reference.Data[i] == '#' ) {
			Reference.Size = i;
			break;
		}
	}
	return Reference;
}



/* 判断两个字符串视图按字节精确相同。 */
static bool __xrtHttpCacheReferenceEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(
			Left.Data, Right.Data, Left.Size
		 ) == 0));
}



/* 验证手工构造的失效候选仍满足来源和同源约束。 */
static bool __xrtHttpCacheInvalidationItemValid(
	const xurl* pTarget,
	xstrview Target,
	const xhttpcacheinvalidateitem* pItem
)
{
	xurl Reference;

	if ( (pItem == NULL) ||
		!__xrtHttpViewValid(pItem->Reference) ) {
		return false;
	}
	if ( pItem->Kind == XHTTP_CACHE_INVALIDATE_TARGET ) {
		return (pItem->Field == XRT_NPOS) &&
			__xrtHttpCacheReferenceEqual(
				pItem->Reference, Target
			);
	}
	if ( ((pItem->Kind !=
		  XHTTP_CACHE_INVALIDATE_LOCATION) &&
		 (pItem->Kind !=
		  XHTTP_CACHE_INVALIDATE_CONTENT_LOCATION)) ||
		(pItem->Field == XRT_NPOS) ) {
		return false;
	}
	return __xrtHttpCacheReferenceSameOrigin(
		pTarget, pItem->Reference, &Reference
	);
}



/* 把失效候选解析为不含 fragment 的绝对 URI。 */
XRT_API bool xrtHttpCacheInvalidationWrite(
	xstrview Target,
	const xhttpcacheinvalidateitem* pItem,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xurl TargetUrl;
	xstrview Reference;

	if ( (pSize == NULL) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		!__xrtHttpViewValid(Target) ||
		!__xrtHttpCacheTargetUrl(
			Target, &TargetUrl
		) ||
		!__xrtHttpCacheInvalidationItemValid(
			&TargetUrl, Target, pItem
		) ||
		__xrtRangesOverlap(
			pSize, sizeof(*pSize),
			pItem, sizeof(*pItem)
		) ||
		__xrtRangesOverlap(
			pSize, sizeof(*pSize),
			Target.Data, Target.Size
		) ||
		__xrtRangesOverlap(
			pSize, sizeof(*pSize),
			pItem->Reference.Data,
			pItem->Reference.Size
		) ||
		__xrtRangesOverlap(
			pOutput, iCapacity,
			pSize, sizeof(*pSize)
		) ||
		__xrtRangesOverlap(
			pOutput, iCapacity,
			pItem, sizeof(*pItem)
		) ||
		__xrtRangesOverlap(
			pOutput, iCapacity,
			Target.Data, Target.Size
		) ||
		__xrtRangesOverlap(
			pOutput, iCapacity,
			pItem->Reference.Data,
			pItem->Reference.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pItem->Kind == XHTTP_CACHE_INVALIDATE_TARGET ) {
		return xrtUrlWrite(
			&TargetUrl,
			pOutput,
			iCapacity,
			pSize
		);
	}
	Reference = __xrtHttpCacheFragmentRemove(
		pItem->Reference
	);
	return xrtUrlResolve(
		&TargetUrl,
		Reference,
		pOutput,
		iCapacity,
		pSize
	);
}

#endif

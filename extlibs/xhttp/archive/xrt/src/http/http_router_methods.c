#include "../internal/xrt_http_router.h"



#if defined(XRT_FEATURE_HTTP_ROUTER)

#define XRT_HTTP_ROUTER_LOCAL_METHODS ((size_t)16u)



/* 返回路由记录借用的方法。 */
static xstrview __xrtHttpRouterRouteMethod(
	const xhttprouter* pRouter,
	const xrt_http_router_route* pRoute
)
{
	return __xrtHttpRouterView(
		pRouter, pRoute->Method, pRoute->MethodSize
	);
}



/* 判断两个 HTTP 方法是否逐字节相同。 */
static bool __xrtHttpRouterMethodsEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		(memcmp(Left.Data, Right.Data, Left.Size) == 0);
}



/* 判断一个冻结路由模板是否匹配原始路径。 */
static bool __xrtHttpRouterRouteMatches(
	const xhttprouter* pRouter,
	const xrt_http_router_route* pRoute,
	xstrview Path
)
{
	xstrview Pattern = __xrtHttpRouterView(
		pRouter, pRoute->Pattern, pRoute->PatternSize
	);

	return __xrtHttpRouteMatchValidated(
		Pattern, Path, NULL
	) == XHTTP_ROUTE_MATCH;
}



/* 在小型方法集合中查找重复方法。 */
static bool __xrtHttpRouterMethodsContains(
	const xstrview* pMethods,
	size_t iCount,
	xstrview Method
)
{
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		if ( __xrtHttpRouterMethodsEqual(
			pMethods[i], Method
		) ) {
			return true;
		}
	}
	return false;
}



/* 向可能未对齐的方法数组写入一个借用视图。 */
static void __xrtHttpRouterMethodStore(
	xstrview* pMethods,
	size_t iIndex,
	xstrview Method
)
{
	memcpy(
		(uint8*)(void*)pMethods + (iIndex * sizeof(Method)),
		&Method,
		sizeof(Method)
	);
}



/* 向固定本地集合追加唯一方法，空间不足时报告需要慢速精确扫描。 */
static bool __xrtHttpRouterMethodsLocalAdd(
	xstrview* pMethods,
	size_t* pCount,
	xstrview Method
)
{
	if ( __xrtHttpRouterMethodsContains(
		pMethods, *pCount, Method
	) ) {
		return true;
	}
	if ( *pCount == XRT_HTTP_ROUTER_LOCAL_METHODS ) {
		return false;
	}
	pMethods[(*pCount)++] = Method;
	return true;
}



/* 常见方法集合用单次路由扫描收集到固定本地数组。 */
static bool __xrtHttpRouterMethodsLocal(
	const xhttprouter* pRouter,
	xstrview Path,
	xstrview* pMethods,
	size_t* pCount
)
{
	static const xstrview Get = XRT_STR_INIT("GET");
	static const xstrview Head = XRT_STR_INIT("HEAD");
	size_t iCount = 0;
	size_t i;

	for ( i = 0; i < pRouter->RouteCount; i++ ) {
		const xrt_http_router_route* pRoute =
			&pRouter->Routes[i];
		xstrview Method;

		if ( !__xrtHttpRouterRouteMatches(
			pRouter, pRoute, Path
		) ) {
			continue;
		}
		Method = __xrtHttpRouterRouteMethod(pRouter, pRoute);
		if ( !__xrtHttpRouterMethodsLocalAdd(
			pMethods, &iCount, Method
		) ) {
			return false;
		}
		if ( __xrtHttpRouterMethodsEqual(Method, Get) &&
			!__xrtHttpRouterMethodsLocalAdd(
				pMethods, &iCount, Head
			) ) {
			return false;
		}
	}
	*pCount = iCount;
	return true;
}



/* 判断指定路由之前是否已有匹配路径发布同一个显式或合成方法。 */
static bool __xrtHttpRouterMethodSeenBefore(
	const xhttprouter* pRouter,
	xstrview Path,
	size_t iEnd,
	xstrview Method
)
{
	static const xstrview Get = XRT_STR_INIT("GET");
	static const xstrview Head = XRT_STR_INIT("HEAD");
	size_t i;

	for ( i = 0; i < iEnd; i++ ) {
		const xrt_http_router_route* pRoute =
			&pRouter->Routes[i];
		xstrview Previous = __xrtHttpRouterRouteMethod(
			pRouter, pRoute
		);
		bool bSame = __xrtHttpRouterMethodsEqual(
			Previous, Method
		) || (__xrtHttpRouterMethodsEqual(Method, Head) &&
			__xrtHttpRouterMethodsEqual(Previous, Get));

		if ( !bSame || !__xrtHttpRouterRouteMatches(
			pRouter, pRoute, Path
		) ) {
			continue;
		}
		return true;
	}
	return false;
}



/* 极端方法集合用无分配嵌套扫描精确计数或写出。 */
static size_t __xrtHttpRouterMethodsLarge(
	const xhttprouter* pRouter,
	xstrview Path,
	xstrview* pMethods
)
{
	static const xstrview Get = XRT_STR_INIT("GET");
	static const xstrview Head = XRT_STR_INIT("HEAD");
	size_t iCount = 0;
	size_t i;

	for ( i = 0; i < pRouter->RouteCount; i++ ) {
		const xrt_http_router_route* pRoute =
			&pRouter->Routes[i];
		xstrview Method;

		if ( !__xrtHttpRouterRouteMatches(
			pRouter, pRoute, Path
		) ) {
			continue;
		}
		Method = __xrtHttpRouterRouteMethod(pRouter, pRoute);
		if ( !__xrtHttpRouterMethodSeenBefore(
			pRouter, Path, i, Method
		) ) {
			if ( pMethods != NULL ) {
				__xrtHttpRouterMethodStore(
					pMethods, iCount, Method
				);
			}
			iCount++;
		}
		if ( __xrtHttpRouterMethodsEqual(Method, Get) &&
			!__xrtHttpRouterMethodSeenBefore(
				pRouter, Path, i, Head
			) ) {
			if ( pMethods != NULL ) {
				__xrtHttpRouterMethodStore(
					pMethods, iCount, Head
				);
			}
			iCount++;
		}
	}
	return iCount;
}



/* 验证方法数组和计数输出不会覆盖路径或彼此。 */
static bool __xrtHttpRouterMethodsOutputsValid(
	xstrview Path,
	xstrview* pMethods,
	size_t iBytes,
	size_t* pCount
)
{
	return !__xrtRangesOverlap(
		pCount, sizeof(*pCount), Path.Data, Path.Size
	) && !__xrtRangesOverlap(
		pCount, sizeof(*pCount), pMethods, iBytes
	) && !__xrtRangesOverlap(
		pMethods, iBytes, Path.Data, Path.Size
	);
}



/* 列出匹配结构路径的唯一方法，并为 GET 合成稳定 HEAD 能力。 */
XRT_API xhttprouterstatus xrtHttpRouterMethods(
	const xhttprouter* pRouter,
	xstrview Path,
	xstrview* pMethods,
	size_t iCapacity,
	size_t* pCount
)
{
	xstrview Local[XRT_HTTP_ROUTER_LOCAL_METHODS];
	size_t iCount = 0;
	size_t iBytes;
	bool bLocal;

	if ( (pRouter == NULL) || !pRouter->Frozen ||
		!__xrtRangeValid(pCount, sizeof(iCount)) ||
		!__xrtHttpViewValid(Path) ||
		((pMethods == NULL) && (iCapacity != 0)) ||
		(iCapacity > (SIZE_MAX / sizeof(*pMethods))) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_ROUTER_ERROR;
	}
	iBytes = iCapacity * sizeof(*pMethods);
	if ( !__xrtRangeValid(pMethods, iBytes) ||
		!__xrtHttpRouterMethodsOutputsValid(
		Path, pMethods, iBytes, pCount
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_ROUTER_ERROR;
	}
	memcpy(pCount, &iCount, sizeof(iCount));
	if ( !__xrtHttpRoutePathValid(Path) ) {
		__xrtErrorSetValue();
		return XHTTP_ROUTER_ERROR;
	}
	bLocal = __xrtHttpRouterMethodsLocal(
		pRouter, Path, Local, &iCount
	);
	if ( !bLocal ) {
		iCount = __xrtHttpRouterMethodsLarge(
			pRouter, Path, NULL
		);
	}
	memcpy(pCount, &iCount, sizeof(iCount));
	if ( iCount == 0 ) {
		return XHTTP_ROUTER_NOT_FOUND;
	}
	if ( iCapacity < iCount ) {
		return XHTTP_ROUTER_MORE;
	}
	if ( bLocal ) {
		memcpy(pMethods, Local, iCount * sizeof(*pMethods));
	} else {
		(void)__xrtHttpRouterMethodsLarge(
			pRouter, Path, pMethods
		);
	}
	return XHTTP_ROUTER_MATCH;
}

#endif

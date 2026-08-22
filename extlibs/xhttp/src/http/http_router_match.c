#include "../internal/xrt_http_router.h"



#if defined(XHTTP_FEATURE_HTTP_ROUTER)

/* 按字典序比较冻结静态边与输入路径段。 */
static int __xrtHttpRouterStaticCompare(
	const xhttprouter* pRouter,
	const xrt_http_router_edge* pEdge,
	xstrview Segment
)
{
	size_t iCommon = pEdge->Size < Segment.Size ?
		pEdge->Size : Segment.Size;
	int iCompare = memcmp(
		pRouter->Arena + pEdge->Text,
		Segment.Data,
		iCommon
	);

	if ( iCompare != 0 ) {
		return iCompare;
	}
	return pEdge->Size < Segment.Size ? -1 :
		(pEdge->Size > Segment.Size ? 1 : 0);
}



/* 在节点连续静态边区间中执行二分查找。 */
static size_t __xrtHttpRouterStaticFind(
	const xhttprouter* pRouter,
	const xrt_http_router_node* pNode,
	xstrview Segment
)
{
	size_t iLeft = 0;
	size_t iRight = pNode->EdgeCount;

	while ( iLeft < iRight ) {
		size_t iMiddle = iLeft + ((iRight - iLeft) / 2u);
		const xrt_http_router_edge* pEdge =
			&pRouter->Edges[pNode->EdgeStart + iMiddle];
		int iCompare = __xrtHttpRouterStaticCompare(
			pRouter, pEdge, Segment
		);

		if ( iCompare < 0 ) {
			iLeft = iMiddle + 1u;
		} else if ( iCompare > 0 ) {
			iRight = iMiddle;
		} else {
			return pEdge->Child;
		}
	}
	return XRT_HTTP_ROUTER_NONE;
}



/* 统计严格路径段数量；根路径为零，重复和尾斜杠均产生空段。 */
static size_t __xrtHttpRouterPathDepth(xstrview Path)
{
	xrt_http_route_cursor Cursor;
	xstrview Segment;
	size_t iDepth = 0;

	__xrtHttpRouteCursorInit(&Cursor, Path);
	while ( __xrtHttpRouteCursorNext(&Cursor, &Segment) ) {
		iDepth++;
	}
	return iDepth;
}



/* 仅在回溯时把路径游标恢复到指定的已消费段数。 */
static bool __xrtHttpRouterPathReplay(
	xstrview Path,
	size_t iCount,
	xrt_http_route_cursor* pCursor,
	xstrview* pSegment
)
{
	size_t i;

	__xrtHttpRouteCursorInit(pCursor, Path);
	for ( i = 0; i < iCount; i++ ) {
		if ( !__xrtHttpRouteCursorNext(pCursor, pSegment) ) {
			return false;
		}
	}
	return true;
}



/* 判断方法视图是否与编译路由方法逐字节相同。 */
static bool __xrtHttpRouterMethodEqual(
	const xhttprouter* pRouter,
	const xrt_http_router_route* pRoute,
	xstrview Method
)
{
	return (pRoute->MethodSize == Method.Size) &&
		(memcmp(
			pRouter->Arena + pRoute->Method,
			Method.Data,
			Method.Size
		 ) == 0);
}



/* 在同一结构叶上按精确方法、HEAD 回退、任意方法顺序选择。 */
static size_t __xrtHttpRouterRouteSelect(
	const xhttprouter* pRouter,
	size_t iNode,
	xstrview Method,
	uint32* pFlags,
	bool* pHasMethod
)
{
	static const xstrview Head = XRT_STR_INIT("HEAD");
	static const xstrview Get = XRT_STR_INIT("GET");
	static const xstrview Any = XRT_STR_INIT("*");
	size_t iRoute = pRouter->Nodes[iNode].Route;
	size_t iGet = XRT_HTTP_ROUTER_NONE;
	size_t iAny = XRT_HTTP_ROUTER_NONE;
	bool bHead = (Method.Size == Head.Size) &&
		(memcmp(Method.Data, Head.Data, Head.Size) == 0);

	if ( iRoute != XRT_HTTP_ROUTER_NONE ) {
		*pHasMethod = true;
	}
	while ( iRoute != XRT_HTTP_ROUTER_NONE ) {
		const xrt_http_router_route* pRoute =
			&pRouter->Routes[iRoute];

		if ( __xrtHttpRouterMethodEqual(
			pRouter, pRoute, Method
		) ) {
			*pFlags = 0;
			return iRoute;
		}
		if ( bHead && __xrtHttpRouterMethodEqual(
			pRouter, pRoute, Get
		) ) {
			iGet = iRoute;
		} else if ( __xrtHttpRouterMethodEqual(
			pRouter, pRoute, Any
		) ) {
			iAny = iRoute;
		}
		iRoute = pRoute->Next;
	}
	if ( iGet != XRT_HTTP_ROUTER_NONE ) {
		*pFlags = XHTTP_ROUTER_HEAD_FALLBACK;
		return iGet;
	}
	if ( iAny != XRT_HTTP_ROUTER_NONE ) {
		*pFlags = XHTTP_ROUTER_ANY_METHOD;
		return iAny;
	}
	return XRT_HTTP_ROUTER_NONE;
}



/* 尝试节点叶上的方法集合并累计结构路径存在事实。 */
static size_t __xrtHttpRouterCandidate(
	const xhttprouter* pRouter,
	size_t iNode,
	xstrview Method,
	uint32* pFlags,
	bool* pHasMethod
)
{
	return __xrtHttpRouterRouteSelect(
		pRouter, iNode, Method, pFlags, pHasMethod
	);
}



/*
	按静态、参数、尾参数优先级迭代深度优先搜索。
	子节点父关系携带回溯状态，路径位置只在回溯时按深度重放。
*/
static size_t __xrtHttpRouterSearch(
	const xhttprouter* pRouter,
	xstrview Method,
	xstrview Path,
	uint32* pFlags,
	bool* pHasMethod
)
{
	xrt_http_route_cursor PathCursor;
	size_t iPathDepth = __xrtHttpRouterPathDepth(Path);
	size_t iNode = 0;
	bool bEnter = true;

	__xrtHttpRouteCursorInit(&PathCursor, Path);
	for ( ;; ) {
		const xrt_http_router_node* pNode = &pRouter->Nodes[iNode];

		if ( bEnter ) {
			xstrview Segment;
			size_t iChild;
			size_t iRoute;

			if ( pNode->Depth == iPathDepth ) {
				iRoute = __xrtHttpRouterCandidate(
					pRouter, iNode, Method,
					pFlags, pHasMethod
				);
				if ( iRoute != XRT_HTTP_ROUTER_NONE ) {
					return iRoute;
				}
				if ( (iNode == 0) &&
					(pNode->Tail != XRT_HTTP_ROUTER_NONE) ) {
					iRoute = __xrtHttpRouterCandidate(
						pRouter, pNode->Tail, Method,
						pFlags, pHasMethod
					);
					if ( iRoute != XRT_HTTP_ROUTER_NONE ) {
						return iRoute;
					}
				}
				bEnter = false;
				continue;
			}
			if ( !__xrtHttpRouteCursorNext(
				&PathCursor, &Segment
			) ) {
				return XRT_HTTP_ROUTER_NONE;
			}
			iChild = __xrtHttpRouterStaticFind(
				pRouter, pNode, Segment
			);
			if ( iChild != XRT_HTTP_ROUTER_NONE ) {
				iNode = iChild;
				continue;
			}
			if ( (pNode->Parameter != XRT_HTTP_ROUTER_NONE) &&
				(Segment.Size != 0) ) {
				iNode = pNode->Parameter;
				continue;
			}
			if ( pNode->Tail != XRT_HTTP_ROUTER_NONE ) {
				iRoute = __xrtHttpRouterCandidate(
					pRouter, pNode->Tail, Method,
					pFlags, pHasMethod
				);
				if ( iRoute != XRT_HTTP_ROUTER_NONE ) {
					return iRoute;
				}
			}
			bEnter = false;
			continue;
		}
		if ( iNode == 0 ) {
			break;
		}
		{
			size_t iParent = pNode->Parent;
			uint8 iKind = pNode->ParentKind;
			const xrt_http_router_node* pParent =
				&pRouter->Nodes[iParent];
			xstrview Segment;
			size_t iRoute;

			iNode = iParent;
			if ( !__xrtHttpRouterPathReplay(
				Path, pParent->Depth + 1u,
				&PathCursor, &Segment
			) ) {
				return XRT_HTTP_ROUTER_NONE;
			}
			if ( (iKind == XRT_HTTP_ROUTER_PARENT_STATIC) &&
				(pParent->Parameter != XRT_HTTP_ROUTER_NONE) &&
				(Segment.Size != 0) ) {
				iNode = pParent->Parameter;
				bEnter = true;
				continue;
			}
			if ( pParent->Tail != XRT_HTTP_ROUTER_NONE ) {
				iRoute = __xrtHttpRouterCandidate(
					pRouter, pParent->Tail, Method,
					pFlags, pHasMethod
				);
				if ( iRoute != XRT_HTTP_ROUTER_NONE ) {
					return iRoute;
				}
			}
			bEnter = false;
		}
	}
	return XRT_HTTP_ROUTER_NONE;
}



/* 验证输出互不覆盖输入和彼此，避免借用匹配过程中自修改。 */
static bool __xrtHttpRouterOutputsValid(
	xstrview Method,
	xstrview Path,
	xhttprouteparam* pParams,
	size_t iBytes,
	size_t* pCount,
	xhttproutermatch* pMatch
)
{
	return !__xrtRangesOverlap(
		pCount, sizeof(*pCount), Method.Data, Method.Size
	) && !__xrtRangesOverlap(
		pCount, sizeof(*pCount), Path.Data, Path.Size
	) && !__xrtRangesOverlap(
		pCount, sizeof(*pCount), pParams, iBytes
	) && !__xrtRangesOverlap(
		pCount, sizeof(*pCount), pMatch, sizeof(*pMatch)
	) && !__xrtRangesOverlap(
		pMatch, sizeof(*pMatch), Method.Data, Method.Size
	) && !__xrtRangesOverlap(
		pMatch, sizeof(*pMatch), Path.Data, Path.Size
	) && !__xrtRangesOverlap(
		pMatch, sizeof(*pMatch), pParams, iBytes
	) && !__xrtRangesOverlap(
		pParams, iBytes, Method.Data, Method.Size
	) && !__xrtRangesOverlap(
		pParams, iBytes, Path.Data, Path.Size
	);
}



/* 匹配冻结索引，并仅在容量足够时写入借用参数。 */
XRT_API xhttprouterstatus xrtHttpRouterMatch(
	const xhttprouter* pRouter,
	xstrview Method,
	xstrview Path,
	xhttprouteparam* pParams,
	size_t iCapacity,
	size_t* pCount,
	xhttproutermatch* pMatch
)
{
	const xrt_http_router_route* pRoute;
	xhttproutermatch Match;
	xhttproutestatus RouteStatus;
	size_t iCount = 0;
	size_t iRoute;
	size_t iBytes;
	uint32 iFlags = 0;
	bool bHasMethod = false;

	memset(&Match, 0, sizeof(Match));
	if ( (pRouter == NULL) || !pRouter->Frozen ||
		!__xrtRangeValid(pCount, sizeof(iCount)) ||
		!__xrtRangeValid(pMatch, sizeof(Match)) ||
		!__xrtHttpViewValid(Method) ||
		!__xrtHttpViewValid(Path) ||
		(Method.Size == 0) || !xrtHttpTokenValid(Method) ||
		((pParams == NULL) && (iCapacity != 0)) ||
		(iCapacity > (SIZE_MAX / sizeof(*pParams))) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_ROUTER_ERROR;
	}
	iBytes = iCapacity * sizeof(*pParams);
	if ( !__xrtRangeValid(pParams, iBytes) ||
		!__xrtHttpRouterOutputsValid(
		Method, Path, pParams, iBytes, pCount, pMatch
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP_ROUTER_ERROR;
	}
	memcpy(pCount, &iCount, sizeof(iCount));
	memcpy(pMatch, &Match, sizeof(Match));
	if ( !__xrtHttpRoutePathValid(Path) ) {
		__xrtErrorSetValue();
		return XHTTP_ROUTER_ERROR;
	}
	iRoute = __xrtHttpRouterSearch(
		pRouter, Method, Path, &iFlags, &bHasMethod
	);
	if ( iRoute == XRT_HTTP_ROUTER_NONE ) {
		return bHasMethod ? XHTTP_ROUTER_METHOD_NOT_ALLOWED :
			XHTTP_ROUTER_NOT_FOUND;
	}
	pRoute = &pRouter->Routes[iRoute];
	Match.Flags = iFlags;
	Match.Method = __xrtHttpRouterView(
		pRouter, pRoute->Method, pRoute->MethodSize
	);
	Match.Pattern = __xrtHttpRouterView(
		pRouter, pRoute->Pattern, pRoute->PatternSize
	);
	Match.Value = pRoute->Value;
	iCount = pRoute->Parameters;
	memcpy(pMatch, &Match, sizeof(Match));
	memcpy(pCount, &iCount, sizeof(iCount));
	if ( iCapacity < pRoute->Parameters ) {
		return XHTTP_ROUTER_MORE;
	}
	if ( pRoute->Parameters != 0 ) {
		RouteStatus = __xrtHttpRouteMatchValidated(
			Match.Pattern, Path, pParams
		);
		if ( RouteStatus != XHTTP_ROUTE_MATCH ) {
			iCount = 0;
			memset(&Match, 0, sizeof(Match));
			memcpy(pCount, &iCount, sizeof(iCount));
			memcpy(pMatch, &Match, sizeof(Match));
			__xrtErrorSetInternal();
			return XHTTP_ROUTER_ERROR;
		}
	}
	return XHTTP_ROUTER_MATCH;
}

#endif

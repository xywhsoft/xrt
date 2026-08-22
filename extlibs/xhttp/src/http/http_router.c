#include "../internal/xrt_http_router.h"

#include <xrt/memory.h>



#if defined(XHTTP_FEATURE_HTTP_ROUTER)

#define XRT_HTTP_ROUTER_DEFAULT_ROUTES ((size_t)4096u)
#define XRT_HTTP_ROUTER_DEFAULT_NODES ((size_t)16384u)
#define XRT_HTTP_ROUTER_DEFAULT_BYTES ((size_t)4194304u)



/* 安全累加注册预检尺寸。 */
static bool __xrtHttpRouterAddSize(
	size_t iLeft,
	size_t iRight,
	size_t* pResult
)
{
	if ( iLeft > (SIZE_MAX - iRight) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pResult = iLeft + iRight;
	return true;
}



/* 按上限扩展任意连续内部数组，失败不改变原指针和容量。 */
static bool __xrtHttpRouterReserve(
	ptr* ppData,
	size_t* pCapacity,
	size_t iRequired,
	size_t iMaximum,
	size_t iUnit
)
{
	ptr pData;
	size_t iCapacity;

	if ( (iRequired > iMaximum) ||
		(iRequired > (SIZE_MAX / iUnit)) ) {
		__xrtErrorSetRange();
		return false;
	}
	if ( iRequired <= *pCapacity ) {
		return true;
	}
	iCapacity = *pCapacity != 0 ? *pCapacity :
		(iMaximum < 8u ? iMaximum : 8u);
	while ( iCapacity < iRequired ) {
		size_t iNext = iCapacity > (iMaximum / 2u) ?
			iMaximum : iCapacity * 2u;

		if ( iNext <= iCapacity ) {
			iCapacity = iRequired;
			break;
		}
		iCapacity = iNext;
	}
	pData = xrtRealloc(*ppData, iCapacity * iUnit);
	if ( pData == NULL ) {
		return false;
	}
	*ppData = pData;
	*pCapacity = iCapacity;
	return true;
}



/* 在线性注册链中查找同一父节点下的静态边。 */
static size_t __xrtHttpRouterBuildStatic(
	const xhttprouter* pRouter,
	size_t iNode,
	xstrview Text
)
{
	size_t iEdge = pRouter->Nodes[iNode].BuildEdge;

	while ( iEdge != XRT_HTTP_ROUTER_NONE ) {
		const xrt_http_router_build_edge* pEdge =
			&pRouter->BuildEdges[iEdge];

		if ( (pEdge->Size == Text.Size) &&
			(memcmp(
				pRouter->Arena + pEdge->Text,
				Text.Data,
				Text.Size
			 ) == 0) ) {
			return pEdge->Child;
		}
		iEdge = pEdge->Next;
	}
	return XRT_HTTP_ROUTER_NONE;
}



/* 在叶节点查找已经注册的同名方法。 */
static bool __xrtHttpRouterMethodExists(
	const xhttprouter* pRouter,
	size_t iNode,
	xstrview Method
)
{
	size_t iRoute = pRouter->Nodes[iNode].Route;

	while ( iRoute != XRT_HTTP_ROUTER_NONE ) {
		const xrt_http_router_route* pRoute =
			&pRouter->Routes[iRoute];

		if ( (pRoute->MethodSize == Method.Size) &&
			(memcmp(
				pRouter->Arena + pRoute->Method,
				Method.Data,
				Method.Size
			 ) == 0) ) {
			return true;
		}
		iRoute = pRoute->Next;
	}
	return false;
}



/* 预检结构路径，精确计算新增节点、静态边和静态文本字节。 */
static bool __xrtHttpRouterPreflight(
	const xhttprouter* pRouter,
	xstrview Pattern,
	xstrview Method,
	size_t* pNodes,
	size_t* pEdges,
	size_t* pBytes
)
{
	xrt_http_route_cursor Cursor;
	xrt_http_route_segment Segment;
	xstrview Text;
	size_t iNode = 0;
	size_t iNodes = 0;
	size_t iEdges = 0;
	size_t iBytes = 0;
	bool bMissing = false;

	__xrtHttpRouteCursorInit(&Cursor, Pattern);
	while ( __xrtHttpRouteCursorNext(&Cursor, &Text) ) {
		size_t iChild = XRT_HTTP_ROUTER_NONE;

		(void)__xrtHttpRouteSegmentParse(
			Text, Cursor.Position > Pattern.Size, &Segment
		);
		if ( !bMissing ) {
			if ( Segment.Kind == XRT_HTTP_ROUTE_STATIC ) {
				iChild = __xrtHttpRouterBuildStatic(
					pRouter, iNode, Text
				);
			} else if ( Segment.Kind ==
				XRT_HTTP_ROUTE_PARAMETER ) {
				iChild = pRouter->Nodes[iNode].Parameter;
			} else {
				iChild = pRouter->Nodes[iNode].Tail;
			}
			if ( iChild != XRT_HTTP_ROUTER_NONE ) {
				iNode = iChild;
				continue;
			}
			bMissing = true;
		}
		if ( !__xrtHttpRouterAddSize(iNodes, 1u, &iNodes) ) {
			return false;
		}
		if ( Segment.Kind == XRT_HTTP_ROUTE_STATIC ) {
			if ( !__xrtHttpRouterAddSize(
				iEdges, 1u, &iEdges
			) || !__xrtHttpRouterAddSize(
				iBytes, Text.Size, &iBytes
			) ) {
				return false;
			}
		}
	}
	if ( !bMissing && __xrtHttpRouterMethodExists(
		pRouter, iNode, Method
	) ) {
		__xrtErrorSetValue();
		return false;
	}
	*pNodes = iNodes;
	*pEdges = iEdges;
	*pBytes = iBytes;
	return true;
}



/* 追加一个已经预留容量的空节点。 */
static size_t __xrtHttpRouterNodeAppend(
	xhttprouter* pRouter,
	size_t iParent,
	uint8 iParentKind
)
{
	xrt_http_router_node* pNode =
		&pRouter->Nodes[pRouter->NodeCount];
	size_t iNode = pRouter->NodeCount++;

	memset(pNode, 0, sizeof(*pNode));
	pNode->Parent = iParent;
	pNode->Depth = pRouter->Nodes[iParent].Depth + 1u;
	pNode->BuildEdge = XRT_HTTP_ROUTER_NONE;
	pNode->Parameter = XRT_HTTP_ROUTER_NONE;
	pNode->Tail = XRT_HTTP_ROUTER_NONE;
	pNode->Route = XRT_HTTP_ROUTER_NONE;
	pNode->ParentKind = iParentKind;
	return iNode;
}



/* 在已经预留的 Arena 尾部复制借用文本并返回偏移。 */
static size_t __xrtHttpRouterTextAppend(
	xhttprouter* pRouter,
	xstrview Text
)
{
	size_t iOffset = pRouter->ArenaSize;

	if ( Text.Size != 0 ) {
		memcpy(pRouter->Arena + iOffset, Text.Data, Text.Size);
	}
	pRouter->ArenaSize += Text.Size;
	return iOffset;
}



/* 追加静态边和子节点，并挂到父节点注册链表头。 */
static size_t __xrtHttpRouterStaticAppend(
	xhttprouter* pRouter,
	size_t iParent,
	xstrview Text
)
{
	xrt_http_router_build_edge* pEdge =
		&pRouter->BuildEdges[pRouter->BuildEdgeCount];
	size_t iChild = __xrtHttpRouterNodeAppend(
		pRouter, iParent, XRT_HTTP_ROUTER_PARENT_STATIC
	);
	size_t iEdge = pRouter->BuildEdgeCount++;

	pEdge->Parent = iParent;
	pEdge->Child = iChild;
	pEdge->Text = __xrtHttpRouterTextAppend(pRouter, Text);
	pEdge->Size = Text.Size;
	pEdge->Next = pRouter->Nodes[iParent].BuildEdge;
	pRouter->Nodes[iParent].BuildEdge = iEdge;
	return iChild;
}



/* 按已验证模板创建全部缺失节点并返回叶节点。 */
static size_t __xrtHttpRouterBuildPath(
	xhttprouter* pRouter,
	xstrview Pattern
)
{
	xrt_http_route_cursor Cursor;
	xrt_http_route_segment Segment;
	xstrview Text;
	size_t iNode = 0;

	__xrtHttpRouteCursorInit(&Cursor, Pattern);
	while ( __xrtHttpRouteCursorNext(&Cursor, &Text) ) {
		size_t iChild;

		(void)__xrtHttpRouteSegmentParse(
			Text, Cursor.Position > Pattern.Size, &Segment
		);
		if ( Segment.Kind == XRT_HTTP_ROUTE_STATIC ) {
			iChild = __xrtHttpRouterBuildStatic(
				pRouter, iNode, Text
			);
			if ( iChild == XRT_HTTP_ROUTER_NONE ) {
				iChild = __xrtHttpRouterStaticAppend(
					pRouter, iNode, Text
				);
			}
		} else if ( Segment.Kind == XRT_HTTP_ROUTE_PARAMETER ) {
			iChild = pRouter->Nodes[iNode].Parameter;
			if ( iChild == XRT_HTTP_ROUTER_NONE ) {
				iChild = __xrtHttpRouterNodeAppend(
					pRouter, iNode,
					XRT_HTTP_ROUTER_PARENT_PARAMETER
				);
				pRouter->Nodes[iNode].Parameter = iChild;
			}
		} else {
			iChild = pRouter->Nodes[iNode].Tail;
			if ( iChild == XRT_HTTP_ROUTER_NONE ) {
				iChild = __xrtHttpRouterNodeAppend(
					pRouter, iNode,
					XRT_HTTP_ROUTER_PARENT_PARAMETER
				);
				pRouter->Nodes[iNode].Tail = iChild;
			}
		}
		iNode = iChild;
	}
	return iNode;
}



/* 比较两个冻结静态边的原始段文本。 */
static int __xrtHttpRouterEdgeCompare(
	const xhttprouter* pRouter,
	const xrt_http_router_edge* pLeft,
	const xrt_http_router_edge* pRight
)
{
	size_t iCommon = pLeft->Size < pRight->Size ?
		pLeft->Size : pRight->Size;
	int iCompare = memcmp(
		pRouter->Arena + pLeft->Text,
		pRouter->Arena + pRight->Text,
		iCommon
	);

	if ( iCompare != 0 ) {
		return iCompare;
	}
	return pLeft->Size < pRight->Size ? -1 :
		(pLeft->Size > pRight->Size ? 1 : 0);
}



/* 交换同一节点静态边区间中的两个元素。 */
static void __xrtHttpRouterEdgeSwap(
	xhttprouter* pRouter,
	const xrt_http_router_node* pNode,
	size_t iLeft,
	size_t iRight
)
{
	xrt_http_router_edge Edge =
		pRouter->Edges[pNode->EdgeStart + iLeft];

	pRouter->Edges[pNode->EdgeStart + iLeft] =
		pRouter->Edges[pNode->EdgeStart + iRight];
	pRouter->Edges[pNode->EdgeStart + iRight] = Edge;
}



/* 在静态边最大堆中向下恢复字典序。 */
static void __xrtHttpRouterEdgeSift(
	xhttprouter* pRouter,
	const xrt_http_router_node* pNode,
	size_t iRoot,
	size_t iEnd
)
{
	if ( iEnd == 0 ) {
		return;
	}
	while ( iRoot <= ((iEnd - 1u) / 2u) ) {
		size_t iChild = (iRoot * 2u) + 1u;

		if ( (iChild < iEnd) &&
			(__xrtHttpRouterEdgeCompare(
				pRouter,
				&pRouter->Edges[pNode->EdgeStart + iChild],
				&pRouter->Edges[pNode->EdgeStart + iChild + 1u]
			 ) < 0) ) {
			iChild++;
		}
		if ( __xrtHttpRouterEdgeCompare(
			pRouter,
			&pRouter->Edges[pNode->EdgeStart + iRoot],
			&pRouter->Edges[pNode->EdgeStart + iChild]
		) >= 0 ) {
			return;
		}
		__xrtHttpRouterEdgeSwap(
			pRouter, pNode, iRoot, iChild
		);
		iRoot = iChild;
	}
}



/* 对单个节点的静态边执行无分配堆排序。 */
static void __xrtHttpRouterEdgesSort(
	xhttprouter* pRouter,
	xrt_http_router_node* pNode
)
{
	size_t i;

	if ( pNode->EdgeCount < 2u ) {
		return;
	}
	for ( i = pNode->EdgeCount / 2u; i != 0; i-- ) {
		__xrtHttpRouterEdgeSift(
			pRouter, pNode, i - 1u, pNode->EdgeCount - 1u
		);
	}
	for ( i = pNode->EdgeCount - 1u; i != 0; i-- ) {
		__xrtHttpRouterEdgeSwap(pRouter, pNode, 0, i);
		__xrtHttpRouterEdgeSift(
			pRouter, pNode, 0, i - 1u
		);
	}
}



/* 初始化适合中型服务的显式默认限额。 */
XRT_API void xrtHttpRouterConfigInit(xhttprouterconfig* pConfig)
{
	const xhttprouterconfig Config = {
		XRT_HTTP_ROUTER_DEFAULT_ROUTES,
		XRT_HTTP_ROUTER_DEFAULT_NODES,
		XRT_HTTP_ROUTER_DEFAULT_BYTES
	};

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 创建只持有一个根节点的空 Router。 */
XRT_API xhttprouter* xrtHttpRouterCreate(
	const xhttprouterconfig* pConfig
)
{
	xhttprouterconfig Config;
	xhttprouter* pRouter;

	if ( pConfig == NULL ) {
		xrtHttpRouterConfigInit(&Config);
	} else {
		if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
			__xrtErrorSetInvalidArgument();
			return NULL;
		}
		memcpy(&Config, pConfig, sizeof(Config));
	}
	if ( (Config.MaxRoutes == 0) ||
		(Config.MaxNodes == 0) ||
		(Config.MaxBytes == 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pRouter = (xhttprouter*)xrtMalloc(sizeof(*pRouter));
	if ( pRouter == NULL ) {
		return NULL;
	}
	memset(pRouter, 0, sizeof(*pRouter));
	pRouter->Config = Config;
	if ( !__xrtHttpRouterReserve(
		(ptr*)&pRouter->Nodes,
		&pRouter->NodeCapacity,
		1u,
		pRouter->Config.MaxNodes,
		sizeof(*pRouter->Nodes)
	) ) {
		xrtHttpRouterDestroy(pRouter);
		return NULL;
	}
	memset(&pRouter->Nodes[0], 0, sizeof(pRouter->Nodes[0]));
	pRouter->Nodes[0].Parent = XRT_HTTP_ROUTER_NONE;
	pRouter->Nodes[0].BuildEdge = XRT_HTTP_ROUTER_NONE;
	pRouter->Nodes[0].Parameter = XRT_HTTP_ROUTER_NONE;
	pRouter->Nodes[0].Tail = XRT_HTTP_ROUTER_NONE;
	pRouter->Nodes[0].Route = XRT_HTTP_ROUTER_NONE;
	pRouter->NodeCount = 1u;
	return pRouter;
}



/* 释放注册图或冻结索引及其全部拥有文本。 */
XRT_API void xrtHttpRouterDestroy(xhttprouter* pRouter)
{
	if ( pRouter == NULL ) {
		return;
	}
	xrtFree(pRouter->Nodes);
	xrtFree(pRouter->BuildEdges);
	xrtFree(pRouter->Edges);
	xrtFree(pRouter->Routes);
	xrtFree(pRouter->Arena);
	xrtFree(pRouter);
}



/* 原子预留后把一个方法和结构路径提交到注册图。 */
XRT_API bool xrtHttpRouterAdd(
	xhttprouter* pRouter,
	xstrview Method,
	xstrview Pattern,
	ptr pValue
)
{
	xrt_http_router_route* pRoute;
	size_t iParameters;
	size_t iAddNodes;
	size_t iAddEdges;
	size_t iAddBytes;
	size_t iNodes;
	size_t iEdges;
	size_t iRoutes;
	size_t iBytes;
	size_t iLeaf;

	if ( (pRouter == NULL) || pRouter->Frozen ||
		!__xrtHttpViewValid(Method) ||
		(Method.Size == 0) || !xrtHttpTokenValid(Method) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtHttpRouteValidate(Pattern, &iParameters) ||
		!__xrtHttpRouterPreflight(
			pRouter, Pattern, Method,
			&iAddNodes, &iAddEdges, &iAddBytes
		) || !__xrtHttpRouterAddSize(
			pRouter->NodeCount, iAddNodes, &iNodes
		) || !__xrtHttpRouterAddSize(
			pRouter->BuildEdgeCount, iAddEdges, &iEdges
		) || !__xrtHttpRouterAddSize(
			pRouter->RouteCount, 1u, &iRoutes
		) || !__xrtHttpRouterAddSize(
			iAddBytes, Method.Size, &iAddBytes
		) || !__xrtHttpRouterAddSize(
			iAddBytes, Pattern.Size, &iAddBytes
		) || !__xrtHttpRouterAddSize(
			pRouter->ArenaSize, iAddBytes, &iBytes
		) ) {
		return false;
	}
	if ( !__xrtHttpRouterReserve(
		(ptr*)&pRouter->Nodes, &pRouter->NodeCapacity,
		iNodes, pRouter->Config.MaxNodes,
		sizeof(*pRouter->Nodes)
	) || !__xrtHttpRouterReserve(
		(ptr*)&pRouter->BuildEdges,
		&pRouter->BuildEdgeCapacity,
		iEdges,
		pRouter->Config.MaxNodes - 1u,
		sizeof(*pRouter->BuildEdges)
	) || !__xrtHttpRouterReserve(
		(ptr*)&pRouter->Routes, &pRouter->RouteCapacity,
		iRoutes, pRouter->Config.MaxRoutes,
		sizeof(*pRouter->Routes)
	) || !__xrtHttpRouterReserve(
		(ptr*)&pRouter->Arena, &pRouter->ArenaCapacity,
		iBytes, pRouter->Config.MaxBytes, sizeof(char)
	) ) {
		return false;
	}
	iLeaf = __xrtHttpRouterBuildPath(pRouter, Pattern);
	pRoute = &pRouter->Routes[pRouter->RouteCount++];
	pRoute->Next = pRouter->Nodes[iLeaf].Route;
	pRoute->Method = __xrtHttpRouterTextAppend(pRouter, Method);
	pRoute->MethodSize = Method.Size;
	pRoute->Pattern = __xrtHttpRouterTextAppend(pRouter, Pattern);
	pRoute->PatternSize = Pattern.Size;
	pRoute->Parameters = iParameters;
	pRoute->Value = pValue;
	pRouter->Nodes[iLeaf].Route = pRouter->RouteCount - 1u;
	return true;
}



/* 构建节点连续有序静态边，成功后丢弃注册链表。 */
XRT_API bool xrtHttpRouterFreeze(xhttprouter* pRouter)
{
	xrt_http_router_edge* pEdges = NULL;
	size_t iOffset = 0;
	size_t i;

	if ( pRouter == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pRouter->Frozen ) {
		return true;
	}
	if ( pRouter->BuildEdgeCount != 0 ) {
		if ( pRouter->BuildEdgeCount >
			(SIZE_MAX / sizeof(*pEdges)) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		pEdges = (xrt_http_router_edge*)xrtMalloc(
			pRouter->BuildEdgeCount * sizeof(*pEdges)
		);
		if ( pEdges == NULL ) {
			return false;
		}
	}
	for ( i = 0; i < pRouter->NodeCount; i++ ) {
		pRouter->Nodes[i].EdgeCount = 0;
	}
	for ( i = 0; i < pRouter->BuildEdgeCount; i++ ) {
		pRouter->Nodes[pRouter->BuildEdges[i].Parent].EdgeCount++;
	}
	for ( i = 0; i < pRouter->NodeCount; i++ ) {
		pRouter->Nodes[i].EdgeStart = iOffset;
		pRouter->Nodes[i].EdgeWrite = iOffset;
		iOffset += pRouter->Nodes[i].EdgeCount;
	}
	for ( i = 0; i < pRouter->BuildEdgeCount; i++ ) {
		const xrt_http_router_build_edge* pBuild =
			&pRouter->BuildEdges[i];
		xrt_http_router_node* pNode =
			&pRouter->Nodes[pBuild->Parent];
		xrt_http_router_edge* pEdge =
			&pEdges[pNode->EdgeWrite++];

		pEdge->Child = pBuild->Child;
		pEdge->Text = pBuild->Text;
		pEdge->Size = pBuild->Size;
	}
	pRouter->Edges = pEdges;
	for ( i = 0; i < pRouter->NodeCount; i++ ) {
		__xrtHttpRouterEdgesSort(pRouter, &pRouter->Nodes[i]);
	}
	xrtFree(pRouter->BuildEdges);
	pRouter->BuildEdges = NULL;
	pRouter->BuildEdgeCount = 0;
	pRouter->BuildEdgeCapacity = 0;
	pRouter->Frozen = true;
	return true;
}



/* 返回不可变阶段事实。 */
XRT_API bool xrtHttpRouterFrozen(const xhttprouter* pRouter)
{
	return pRouter != NULL ? pRouter->Frozen : false;
}



/* 返回注册路由数量。 */
XRT_API size_t xrtHttpRouterCount(const xhttprouter* pRouter)
{
	return pRouter != NULL ? pRouter->RouteCount : 0;
}



/* 返回包含根节点的预编译节点数量。 */
XRT_API size_t xrtHttpRouterNodes(const xhttprouter* pRouter)
{
	return pRouter != NULL ? pRouter->NodeCount : 0;
}



/* 返回 Arena 实际使用字节。 */
XRT_API size_t xrtHttpRouterBytes(const xhttprouter* pRouter)
{
	return pRouter != NULL ? pRouter->ArenaSize : 0;
}

#endif

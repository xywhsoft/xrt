#ifndef XRT_INTERNAL_HTTP_ROUTER_H
#define XRT_INTERNAL_HTTP_ROUTER_H

#include "xrt_http_route.h"

#include <xrt/http_router.h>



#if defined(XHTTP_FEATURE_HTTP_ROUTER)

#define XRT_HTTP_ROUTER_NONE SIZE_MAX

#define XRT_HTTP_ROUTER_PARENT_STATIC ((uint8)1u)
#define XRT_HTTP_ROUTER_PARENT_PARAMETER ((uint8)2u)



/* 注册期静态边使用父节点链表，冻结后转换为节点连续排序区间。 */
typedef struct xrt_http_router_build_edge {
	size_t Parent;
	size_t Child;
	size_t Text;
	size_t Size;
	size_t Next;
} xrt_http_router_build_edge;



/* 冻结静态边只保留二分查找需要的文本和子节点。 */
typedef struct xrt_http_router_edge {
	size_t Child;
	size_t Text;
	size_t Size;
} xrt_http_router_edge;



/* 节点同时保留注册图、冻结区间和无栈回溯需要的父关系。 */
typedef struct xrt_http_router_node {
	size_t Parent;
	size_t Depth;
	size_t BuildEdge;
	size_t Parameter;
	size_t Tail;
	size_t Route;
	size_t EdgeStart;
	size_t EdgeCount;
	size_t EdgeWrite;
	uint8 ParentKind;
} xrt_http_router_node;



/* 路由记录用 Arena 偏移持有稳定方法和模板，并借用用户 Value。 */
typedef struct xrt_http_router_route {
	size_t Next;
	size_t Method;
	size_t MethodSize;
	size_t Pattern;
	size_t PatternSize;
	size_t Parameters;
	ptr Value;
} xrt_http_router_route;



/* Router 在冻结前持有 BuildEdges，冻结后持有排序 Edges。 */
struct xhttprouter {
	xhttprouterconfig Config;
	xrt_http_router_node* Nodes;
	size_t NodeCount;
	size_t NodeCapacity;
	xrt_http_router_build_edge* BuildEdges;
	size_t BuildEdgeCount;
	size_t BuildEdgeCapacity;
	xrt_http_router_edge* Edges;
	xrt_http_router_route* Routes;
	size_t RouteCount;
	size_t RouteCapacity;
	char* Arena;
	size_t ArenaSize;
	size_t ArenaCapacity;
	bool Frozen;
};



/* 返回 Arena 中的借用文本。 */
static inline xstrview __xrtHttpRouterView(
	const xhttprouter* pRouter,
	size_t iOffset,
	size_t iSize
)
{
	return (xstrview){ pRouter->Arena + iOffset, iSize };
}

#endif

#endif

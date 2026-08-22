#ifndef XRT_INTERNAL_HTTP_ROUTE_H
#define XRT_INTERNAL_HTTP_ROUTE_H

#include "xrt_http.h"

#include <xrt/http_route.h>



#if defined(XRT_FEATURE_HTTP_ROUTE)

/* 路由段类别由纯匹配器与预编译 Router 共享。 */
typedef enum xrt_http_route_segment_kind {
	XRT_HTTP_ROUTE_STATIC = 0,
	XRT_HTTP_ROUTE_PARAMETER,
	XRT_HTTP_ROUTE_TAIL
} xrt_http_route_segment_kind;



/* 路由段保留原始文本及去除花括号和尾标记后的参数名。 */
typedef struct xrt_http_route_segment {
	xrt_http_route_segment_kind Kind;
	xstrview Text;
	xstrview Name;
} xrt_http_route_segment;



/* Cursor 使用 Size + 1 表示结束，从而保留根路径之外的尾空段。 */
typedef struct xrt_http_route_cursor {
	xstrview Text;
	size_t Position;
} xrt_http_route_cursor;



/* 初始化严格路径段游标。 */
void __xrtHttpRouteCursorInit(
	xrt_http_route_cursor* pCursor,
	xstrview Text
);



/* 读取下一个严格路径段。 */
bool __xrtHttpRouteCursorNext(
	xrt_http_route_cursor* pCursor,
	xstrview* pSegment
);



/* 解析一个已经切分的模板段。 */
bool __xrtHttpRouteSegmentParse(
	xstrview Text,
	bool bLast,
	xrt_http_route_segment* pSegment
);



/* 验证不含 query 或 fragment 的绝对 RFC 3986 路径。 */
bool __xrtHttpRoutePathValid(xstrview Path);



/* 匹配已经验证的模板和路径；调用方保证捕获容量充足。 */
xhttproutestatus __xrtHttpRouteMatchValidated(
	xstrview Pattern,
	xstrview Path,
	xhttprouteparam* pParams
);

#endif

#endif

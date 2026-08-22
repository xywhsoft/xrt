#ifndef XRT_INTERNAL_HTTP_SERVER_ROUTER_H
#define XRT_INTERNAL_HTTP_SERVER_ROUTER_H

#include "xrt_internal.h"

#include <string.h>

#include <xrt/memory.h>
#include <xrt/http_server_router.h>

#if defined(XRT_FEATURE_HTTP_SERVER_MIDDLEWARE)
	#include <xrt/http_server_middleware.h>
#endif



#if defined(XRT_FEATURE_HTTP_SERVER_ROUTER)

#define XRT_HTTP_SERVER_ROUTER_LOCAL_PARAMS ((size_t)8u)



/* 路由记录与通用 Router 的 Value 索引一一对应。 */
typedef struct xrt_http_server_route_entry {
	xhttpserverrouteevents Events;
} xrt_http_server_route_entry;



#if defined(XRT_FEATURE_HTTP_SERVER_MIDDLEWARE)

/* 中间件记录复制回调和用户数据，并可在 Router 终态释放拥有型数据。 */
typedef struct xrt_http_server_middleware_entry {
	xhttpservermiddlewareproc Handle;
	xhttpserverrouterreleaseproc Release;
	ptr Data;
} xrt_http_server_middleware_entry;

#endif



/* 运行时适配器持有 Router 引用，并保存调用方原始事件和 Data。 */
typedef struct xrt_http_server_router_runtime {
	xhttpserverrouter* Router;
	xhttpserverevents Events;
} xrt_http_server_router_runtime;



/* 服务端 Router 在冻结前拥有可增长回调表，冻结后只读共享。 */
struct xhttpserverrouter {
	volatile int32 References;
	xhttprouter* Index;
	xrt_http_server_route_entry* Entries;
	size_t Count;
	size_t Capacity;
	size_t MaxRoutes;
	#if defined(XRT_FEATURE_HTTP_SERVER_MIDDLEWARE)
		xrt_http_server_middleware_entry* Middleware;
		size_t MiddlewareCount;
		size_t MiddlewareCapacity;
	#endif
};



/* 一次分发结果使用八个栈上参数，极端模板才按精确数量分配。 */
typedef struct xrt_http_server_route_match {
	xhttprouterstatus Status;
	const xrt_http_server_route_entry* Entry;
	xhttprouteparam* Params;
	size_t Count;
	xstrview Method;
	xstrview Path;
	xhttprouteparam Local[XRT_HTTP_SERVER_ROUTER_LOCAL_PARAMS];
} xrt_http_server_route_match;



/* 设置带操作名和可选原因链的稳定服务端路由错误。 */
void __xrtHttpServerRouterSetError(
	xerrkind Kind,
	xhttpserverroutererror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 原因链缺失或未分类时返回调用点给出的保守错误种类。 */
xerrkind __xrtHttpServerRouterCauseKind(
	const xerror* pCause,
	xerrkind Default
);



/* 把当前线程错误保留为原因，并发布服务端 Router 顶层错误。 */
void __xrtHttpServerRouterWrapError(
	xerrkind Default,
	xhttpserverroutererror Code,
	cstr sOperation,
	cstr sMessage
);



/* 建立持有 Router 引用的完整 Server 事件适配器。 */
xrt_http_server_router_runtime* __xrtHttpServerRouterRuntimeCreate(
	xhttpserverrouter* pRouter,
	const xhttpserverevents* pEvents,
	xhttpserverevents* pOutput
);



/* 释放尚未交给运行中 Server 或已经完成 Shutdown 的适配器。 */
void __xrtHttpServerRouterRuntimeDestroy(
	xrt_http_server_router_runtime* pRuntime
);



/* 使用指定 Router 和回退事件处理 Header 阶段，供固定路由与 Host Mux 共用。 */
xhttpserverbodypolicy __xrtHttpServerRouterDispatchHeaders(
	const xhttpserverrouter* pRouter,
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttpserverevents* pEvents,
	xrt_http_server_route_match** ppMatch
);



/* 使用指定 Router 和回退事件处理一个流式正文片段。 */
bool __xrtHttpServerRouterDispatchBody(
	const xhttpserverrouter* pRouter,
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	xbytesview Data,
	const xhttpserverevents* pEvents,
	const xrt_http_server_route_match* pMatch
);



/* 使用指定 Router 和回退事件处理完整请求。 */
void __xrtHttpServerRouterDispatchRequest(
	const xhttpserverrouter* pRouter,
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttpserverevents* pEvents,
	const xrt_http_server_route_match* pMatch
);



/* 报告 Router 适配失败并尽力提交固定 500 响应。 */
void __xrtHttpServerRouterDispatchFail(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverevents* pEvents
);



/* 执行已经匹配的最终路由、用户回退或标准响应，不再次进入中间件。 */
bool __xrtHttpServerRouterDispatchTerminal(
	const xhttpserverrouter* pRouter,
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xrt_http_server_route_match* pMatch,
	const xhttpserverevents* pEvents
);



#if defined(XRT_FEATURE_HTTP_SERVER_MIDDLEWARE)

/* 用冻结的中间件链包裹一次已经匹配的完整请求终端分派。 */
bool __xrtHttpServerMiddlewareDispatch(
	const xhttpserverrouter* pRouter,
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xrt_http_server_route_match* pMatch,
	const xhttpserverevents* pEvents
);



/* 按洋葱逆序释放 Router 拥有的中间件数据和记录数组。 */
void __xrtHttpServerMiddlewareClear(xhttpserverrouter* pRouter);

#endif



/* 解析请求目标并执行容量完整的 Router 匹配。 */
bool __xrtHttpServerRouterMatch(
	const xhttpserverrouter* pRouter,
	const xhttpserverrequest* pRequest,
	xrt_http_server_route_match* pMatch
);



/* 释放极端参数模板使用的临时描述符数组。 */
void __xrtHttpServerRouterMatchClear(
	xrt_http_server_route_match* pMatch
);



/* 把栈上匹配结果转移为流式请求期拥有对象。 */
xrt_http_server_route_match* __xrtHttpServerRouterMatchTake(
	xrt_http_server_route_match* pMatch
);



/* 释放流式请求期匹配结果。 */
void __xrtHttpServerRouterMatchDestroy(ptr pData);



/* 为 404、405 和自动 OPTIONS 提交默认响应。 */
bool __xrtHttpServerRouterDefault(
	const xhttpserverrouter* pRouter,
	xhttpconn* pConnection,
	const xrt_http_server_route_match* pMatch
);

#endif

#endif

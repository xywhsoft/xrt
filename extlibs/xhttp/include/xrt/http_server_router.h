#ifndef XRT_HTTP_SERVER_ROUTER_H
#define XRT_HTTP_SERVER_ROUTER_H

#include <xrt/http_router.h>
#include <xrt/http_server_runtime.h>



#if defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER) && \
	(!defined(XHTTP_FEATURE_HTTP_ROUTER) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER))
	#error "XRT HTTP server router requires HTTP router and server support"
#endif



#if defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER)

typedef struct xhttpserverrouter xhttpserverrouter;



/* 服务端路由错误区分参数、生命周期、目标、分配、响应和内部索引故障。 */
typedef enum xhttpserverroutererror {
	XHTTP_SERVER_ROUTER_ERROR_ARGUMENT = 1,
	XHTTP_SERVER_ROUTER_ERROR_STATE,
	XHTTP_SERVER_ROUTER_ERROR_TARGET,
	XHTTP_SERVER_ROUTER_ERROR_LIMIT,
	XHTTP_SERVER_ROUTER_ERROR_MEMORY,
	XHTTP_SERVER_ROUTER_ERROR_RESPONSE,
	XHTTP_SERVER_ROUTER_ERROR_START,
	XHTTP_SERVER_ROUTER_ERROR_INTERNAL
} xhttpserverroutererror;



/* Header 路由回调可按请求选择正文策略，并可在连接上设置细化正文限额。 */
typedef xhttpserverbodypolicy (*xhttpserverrouteheadersproc)(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
);



/* Body 路由回调只借用当前片段和参数描述符，返回 false 会终止请求。 */
typedef bool (*xhttpserverroutebodyproc)(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	xbytesview Data,
	ptr pData
);



/* Request 路由回调接收完整请求；允许同步响应或保留 Connection 后异步响应。 */
typedef void (*xhttpserverrouteproc)(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
);



/* Router 最后释放成功注册的路由时调用一次 Data 清理器。 */
typedef void (*xhttpserverrouterreleaseproc)(ptr pData);



/* 每条路由复制自己的分阶段回调、用户数据和可选清理器。 */
typedef struct xhttpserverrouteevents {
	xhttpserverrouteheadersproc Headers;
	xhttpserverroutebodyproc Body;
	xhttpserverrouteproc Request;
	xhttpserverrouterreleaseproc Release;
	ptr Data;
} xhttpserverrouteevents;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER)

/*
	初始化空路由事件；完整请求回调应在注册前设置。
	事件描述符支持未对齐存储，输出范围无效时设置结构化参数错误。
*/
XRT_API void xrtHttpServerRouteEventsInit(
	xhttpserverrouteevents* pEvents
);



/*
	创建拥有回调记录和通用结构索引的服务端 Router。
	可选配置在返回前完整复制，支持未对齐的固定配置描述符。
*/
XRT_API xhttpserverrouter* xrtHttpServerRouterCreate(
	const xhttprouterconfig* pConfig
);



/* 增加 Router 引用；冻结后的 Router 可由多个运行中 Server 共享。 */
XRT_API xhttpserverrouter* xrtHttpServerRouterRef(
	xhttpserverrouter* pRouter
);



/* 释放 Router 引用；空指针是安全的空操作。 */
XRT_API void xrtHttpServerRouterDestroy(
	xhttpserverrouter* pRouter
);



/*
	复制并注册一组分阶段路由回调；Request 不能为空。
	成功后 Router 接管一次 Release(Data) 责任，失败时所有权仍属于调用方。
	事件表在返回前完整复制，支持未对齐的固定描述符。
*/
XRT_API bool xrtHttpServerRouteEvents(
	xhttpserverrouter* pRouter,
	xstrview Method,
	xstrview Pattern,
	const xhttpserverrouteevents* pEvents
);



/* 注册只处理完整请求的常用路由。 */
XRT_API bool xrtHttpServerRoute(
	xhttpserverrouter* pRouter,
	xstrview Method,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
);



/* 注册常用 GET 路由；HEAD 自动回退到同一路径的 GET。 */
XRT_API bool xrtHttpServerGet(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
);



/* 注册常用 POST 路由。 */
XRT_API bool xrtHttpServerPost(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
);



/* 注册常用 PUT 路由。 */
XRT_API bool xrtHttpServerPut(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
);



/* 注册常用 PATCH 路由。 */
XRT_API bool xrtHttpServerPatch(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
);



/* 注册常用 DELETE 路由。 */
XRT_API bool xrtHttpServerDelete(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
);



/* 注册任意合法 HTTP 方法路由。 */
XRT_API bool xrtHttpServerAny(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
);



/* 冻结 Router；成功后可无锁并发分发且不能继续注册。 */
XRT_API bool xrtHttpServerRouterFreeze(
	xhttpserverrouter* pRouter
);



/* 返回 Router 是否已经冻结。 */
XRT_API bool xrtHttpServerRouterFrozen(
	const xhttpserverrouter* pRouter
);



/* 返回已注册路由数量。 */
XRT_API size_t xrtHttpServerRouterCount(
	const xhttpserverrouter* pRouter
);



/* 对完整请求执行一次分发；未命中时自动响应 404、405 或 OPTIONS。 */
XRT_API xhttprouterstatus xrtHttpServerRouterDispatch(
	const xhttpserverrouter* pRouter,
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest
);



/*
	启动由 Router 分发的明文 HTTP/1 Server。
	Events.Request 在此入口中是未命中回调；其他事件保持原始含义。
*/
XRT_API xhttpserver* xrtHttpServerRouterStart(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	xhttpserverrouter* pRouter,
	const xhttpserverevents* pEvents
);

#endif



XRT_EXTERN_C_END

#endif

#ifndef XRT_HTTP_SERVER_MIDDLEWARE_H
#define XRT_HTTP_SERVER_MIDDLEWARE_H

#include <xrt/http_server_router.h>



#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE) && \
	!defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER)
	#error "XRT HTTP server middleware requires server router support"
#endif



#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE)

typedef struct xhttpservernext xhttpservernext;



/* 中间件错误稳定区分参数、冻结状态、容量、分配、Next 和回调失败。 */
typedef enum xhttpservermiddlewareerror {
	XHTTP_SERVER_MIDDLEWARE_ERROR_ARGUMENT = 1,
	XHTTP_SERVER_MIDDLEWARE_ERROR_STATE,
	XHTTP_SERVER_MIDDLEWARE_ERROR_LIMIT,
	XHTTP_SERVER_MIDDLEWARE_ERROR_MEMORY,
	XHTTP_SERVER_MIDDLEWARE_ERROR_NEXT,
	XHTTP_SERVER_MIDDLEWARE_ERROR_CALLBACK
} xhttpservermiddlewareerror;



/*
	同步中间件可在调用 Next 前后执行逻辑，或不调用 Next 而直接响应或异步接管。
	返回 false 表示不可恢复的应用错误；Next 只在当前回调栈内有效且最多调用一次。
*/
typedef bool (*xhttpservermiddlewareproc)(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	xhttpservernext* pNext,
	ptr pData
);

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE)

/*
	把一层同步中间件追加到 Router；调用顺序与注册顺序相同。
	成功后 Data 仍由调用方管理，且必须覆盖 Router 的完整生命周期。
*/
XRT_API bool xrtHttpServerUse(
	xhttpserverrouter* pRouter,
	xhttpservermiddlewareproc pMiddleware,
	ptr pData
);



/*
	追加一层由 Router 管理 Data 生命周期的同步中间件。
	成功后 Router 在最终销毁时调用一次 Release(Data)，失败时所有权不转移。
*/
XRT_API bool xrtHttpServerUseOwned(
	xhttpserverrouter* pRouter,
	xhttpservermiddlewareproc pMiddleware,
	ptr pData,
	xhttpserverrouterreleaseproc pRelease
);



/* 返回 Router 已注册的中间件数量；空 Router 返回零。 */
XRT_API size_t xrtHttpServerMiddlewareCount(
	const xhttpserverrouter* pRouter
);



/*
	继续执行下一层中间件或最终路由；同一个 Next 最多成功调用一次。
	返回 false 时当前请求进入失败路径，调用方应立即返回 false。
*/
XRT_API bool xrtHttpServerNext(xhttpservernext* pNext);

#endif



XRT_EXTERN_C_END

#endif

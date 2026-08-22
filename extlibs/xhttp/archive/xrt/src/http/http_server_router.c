#include "../internal/xrt_http_server_router.h"



#if defined(XRT_FEATURE_HTTP_SERVER_ROUTER)

/* 原因缺失或没有类别时返回调用点的稳定默认类别。 */
xerrkind __xrtHttpServerRouterCauseKind(
	const xerror* pCause,
	xerrkind Default
)
{
	xerrkind Kind = xrtErrorKind(pCause);

	return Kind != XERR_NONE ? Kind : Default;
}



/* 设置包含稳定域、代码、操作和原因链的服务端路由错误。 */
void __xrtHttpServerRouterSetError(
	xerrkind Kind,
	xhttpserverroutererror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind != XERR_NONE ? Kind :
		__xrtHttpServerRouterCauseKind(
			pCause, XERR_INTERNAL
		);
	Desc.Code = (int32)Code;
	Desc.SystemCode = pCause != NULL ?
		xrtErrorSystemCode(pCause) : 0;
	Desc.Domain = "xrt.http.server.router";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 把当前错误作为原因包装到稳定的服务端 Router 域。 */
void __xrtHttpServerRouterWrapError(
	xerrkind Default,
	xhttpserverroutererror Code,
	cstr sOperation,
	cstr sMessage
)
{
	const xerror* pCause = xrtGetError();

	__xrtHttpServerRouterSetError(
		__xrtHttpServerRouterCauseKind(pCause, Default),
		Code,
		sOperation,
		sMessage,
		pCause
	);
}



/* 初始化空的分阶段路由事件。 */
XRT_API void xrtHttpServerRouteEventsInit(
	xhttpserverrouteevents* pEvents
)
{
	const xhttpserverrouteevents Events = { 0 };

	if ( !__xrtRangeValid(pEvents, sizeof(Events)) ) {
		__xrtHttpServerRouterSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
			"init-http-server-route-events",
			"HTTP server route events range is invalid",
			NULL
		);
		return;
	}
	memcpy(pEvents, &Events, sizeof(Events));
}



/* 创建拥有通用索引和回调表的服务端 Router。 */
XRT_API xhttpserverrouter* xrtHttpServerRouterCreate(
	const xhttprouterconfig* pConfig
)
{
	xhttprouterconfig Config;
	xhttpserverrouter* pRouter;

	xrtHttpRouterConfigInit(&Config);
	if ( pConfig != NULL ) {
		if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
			__xrtHttpServerRouterSetError(
				XERR_ARGUMENT,
				XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
				"create-http-server-router",
				"HTTP server router config range is invalid",
				NULL
			);
			return NULL;
		}
		memcpy(&Config, pConfig, sizeof(Config));
	}
	pRouter = (xhttpserverrouter*)xrtCalloc(
		1, sizeof(*pRouter)
	);
	if ( pRouter == NULL ) {
		__xrtHttpServerRouterWrapError(
			XERR_MEMORY,
			XHTTP_SERVER_ROUTER_ERROR_MEMORY,
			"create-http-server-router",
			"HTTP server router allocation failed"
		);
		return NULL;
	}
	pRouter->Index = xrtHttpRouterCreate(&Config);
	if ( pRouter->Index == NULL ) {
		const xerror* pCause = xrtGetError();
		xerrkind Kind = __xrtHttpServerRouterCauseKind(
			pCause, XERR_INTERNAL
		);
		xhttpserverroutererror Code =
			Kind == XERR_MEMORY ?
				XHTTP_SERVER_ROUTER_ERROR_MEMORY :
			Kind == XERR_RANGE ?
				XHTTP_SERVER_ROUTER_ERROR_LIMIT :
			Kind == XERR_ARGUMENT ?
				XHTTP_SERVER_ROUTER_ERROR_ARGUMENT :
				XHTTP_SERVER_ROUTER_ERROR_INTERNAL;

		__xrtHttpServerRouterWrapError(
			Kind,
			Code,
			"create-http-server-router",
			"HTTP server router index creation failed"
		);
		xrtFree(pRouter);
		return NULL;
	}
	pRouter->References = 1;
	pRouter->MaxRoutes = Config.MaxRoutes;
	return pRouter;
}



/* 增加可跨运行中 Server 持有的 Router 引用。 */
XRT_API xhttpserverrouter* xrtHttpServerRouterRef(
	xhttpserverrouter* pRouter
)
{
	if ( (pRouter == NULL) ||
		(xrtRefRetain(&pRouter->References) < 0) ) {
		__xrtHttpServerRouterSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
			"retain-http-server-router",
			"HTTP server router is null or released",
			NULL
		);
		return NULL;
	}
	return pRouter;
}



/* 释放最后一个 Router 引用和全部复制回调。 */
XRT_API void xrtHttpServerRouterDestroy(
	xhttpserverrouter* pRouter
)
{
	size_t i;

	if ( (pRouter == NULL) ||
		(xrtRefRelease(&pRouter->References) != 0) ) {
		return;
	}
	for ( i = 0; i < pRouter->Count; i++ ) {
		if ( pRouter->Entries[i].Events.Release != NULL ) {
			pRouter->Entries[i].Events.Release(
				pRouter->Entries[i].Events.Data
			);
		}
	}
	#if defined(XRT_FEATURE_HTTP_SERVER_MIDDLEWARE)
		__xrtHttpServerMiddlewareClear(pRouter);
	#endif
	xrtHttpRouterDestroy(pRouter->Index);
	xrtFree(pRouter->Entries);
	xrtFree(pRouter);
}



/* 为一个新回调记录预留稳定索引空间。 */
static bool __xrtHttpServerRouterReserve(
	xhttpserverrouter* pRouter
)
{
	size_t iRequired = pRouter->Count + 1u;
	size_t iCapacity;
	xrt_http_server_route_entry* pEntries;

	if ( iRequired > pRouter->MaxRoutes ) {
		__xrtHttpServerRouterSetError(
			XERR_RANGE,
			XHTTP_SERVER_ROUTER_ERROR_LIMIT,
			"register-http-server-route",
			"HTTP server route count exceeds the configured limit",
			NULL
		);
		return false;
	}
	if ( iRequired <= pRouter->Capacity ) {
		return true;
	}
	iCapacity = pRouter->Capacity != 0 ?
		pRouter->Capacity :
		(pRouter->MaxRoutes < 8u ? pRouter->MaxRoutes : 8u);
	while ( iCapacity < iRequired ) {
		size_t iNext = iCapacity > (pRouter->MaxRoutes / 2u) ?
			pRouter->MaxRoutes : iCapacity * 2u;

		if ( iNext <= iCapacity ) {
			iCapacity = iRequired;
			break;
		}
		iCapacity = iNext;
	}
	pEntries = (xrt_http_server_route_entry*)xrtRealloc(
		pRouter->Entries,
		iCapacity * sizeof(*pEntries)
	);
	if ( pEntries == NULL ) {
		__xrtHttpServerRouterWrapError(
			XERR_MEMORY,
			XHTTP_SERVER_ROUTER_ERROR_MEMORY,
			"register-http-server-route",
			"HTTP server route callback table allocation failed"
		);
		return false;
	}
	pRouter->Entries = pEntries;
	pRouter->Capacity = iCapacity;
	return true;
}



/* 复制一组路由回调，并用非空整数指针编码稳定记录索引。 */
XRT_API bool xrtHttpServerRouteEvents(
	xhttpserverrouter* pRouter,
	xstrview Method,
	xstrview Pattern,
	const xhttpserverrouteevents* pEvents
)
{
	xhttpserverrouteevents Events;
	size_t iEntry;

	if ( (pRouter == NULL) ||
		!__xrtRangeValid(pEvents, sizeof(Events)) ) {
		__xrtHttpServerRouterSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
			"register-http-server-route",
			"HTTP server router, events or request handler is null",
			NULL
		);
		return false;
	}
	memcpy(&Events, pEvents, sizeof(Events));
	if ( Events.Request == NULL ) {
		__xrtHttpServerRouterSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
			"register-http-server-route",
			"HTTP server route request handler is null",
			NULL
		);
		return false;
	}
	if ( xrtHttpRouterFrozen(pRouter->Index) ) {
		__xrtHttpServerRouterSetError(
			XERR_STATE,
			XHTTP_SERVER_ROUTER_ERROR_STATE,
			"register-http-server-route",
			"HTTP server router is already frozen",
			NULL
		);
		return false;
	}
	if ( !__xrtHttpServerRouterReserve(pRouter) ) {
		return false;
	}
	iEntry = pRouter->Count;
	if ( !xrtHttpRouterAdd(
		pRouter->Index,
		Method,
		Pattern,
		(ptr)(uintptr_t)(iEntry + 1u)
	) ) {
		const xerror* pCause = xrtGetError();
		xerrkind Kind = __xrtHttpServerRouterCauseKind(
			pCause, XERR_PROTOCOL
		);
		xhttpserverroutererror Code =
			Kind == XERR_MEMORY ?
				XHTTP_SERVER_ROUTER_ERROR_MEMORY :
			Kind == XERR_RANGE ?
				XHTTP_SERVER_ROUTER_ERROR_LIMIT :
			Kind == XERR_STATE ?
				XHTTP_SERVER_ROUTER_ERROR_STATE :
				XHTTP_SERVER_ROUTER_ERROR_TARGET;

		__xrtHttpServerRouterWrapError(
			Kind,
			Code,
			"register-http-server-route",
			"HTTP server route method or pattern was rejected"
		);
		return false;
	}
	pRouter->Entries[iEntry].Events = Events;
	pRouter->Count++;
	return true;
}



/* 用完整请求回调构造最常用的服务端路由。 */
XRT_API bool xrtHttpServerRoute(
	xhttpserverrouter* pRouter,
	xstrview Method,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
)
{
	xhttpserverrouteevents Events;

	xrtHttpServerRouteEventsInit(&Events);
	Events.Request = pHandler;
	Events.Data = pData;
	return xrtHttpServerRouteEvents(
		pRouter, Method, Pattern, &Events
	);
}



/* 注册 GET 路由。 */
XRT_API bool xrtHttpServerGet(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
)
{
	return xrtHttpServerRoute(
		pRouter, XRT_STR_LITERAL("GET"),
		Pattern, pHandler, pData
	);
}



/* 注册 POST 路由。 */
XRT_API bool xrtHttpServerPost(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
)
{
	return xrtHttpServerRoute(
		pRouter, XRT_STR_LITERAL("POST"),
		Pattern, pHandler, pData
	);
}



/* 注册 PUT 路由。 */
XRT_API bool xrtHttpServerPut(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
)
{
	return xrtHttpServerRoute(
		pRouter, XRT_STR_LITERAL("PUT"),
		Pattern, pHandler, pData
	);
}



/* 注册 PATCH 路由。 */
XRT_API bool xrtHttpServerPatch(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
)
{
	return xrtHttpServerRoute(
		pRouter, XRT_STR_LITERAL("PATCH"),
		Pattern, pHandler, pData
	);
}



/* 注册 DELETE 路由。 */
XRT_API bool xrtHttpServerDelete(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
)
{
	return xrtHttpServerRoute(
		pRouter, XRT_STR_LITERAL("DELETE"),
		Pattern, pHandler, pData
	);
}



/* 注册任意方法路由。 */
XRT_API bool xrtHttpServerAny(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
)
{
	return xrtHttpServerRoute(
		pRouter, XRT_STR_LITERAL("*"),
		Pattern, pHandler, pData
	);
}



/* 冻结通用结构索引和高层回调表。 */
XRT_API bool xrtHttpServerRouterFreeze(
	xhttpserverrouter* pRouter
)
{
	if ( pRouter == NULL ) {
		__xrtHttpServerRouterSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
			"freeze-http-server-router",
			"HTTP server router is null",
			NULL
		);
		return false;
	}
	if ( !xrtHttpRouterFreeze(pRouter->Index) ) {
		const xerror* pCause = xrtGetError();
		xerrkind Kind = __xrtHttpServerRouterCauseKind(
			pCause, XERR_INTERNAL
		);

		__xrtHttpServerRouterWrapError(
			Kind,
			Kind == XERR_MEMORY ?
				XHTTP_SERVER_ROUTER_ERROR_MEMORY :
			Kind == XERR_RANGE ?
				XHTTP_SERVER_ROUTER_ERROR_LIMIT :
				XHTTP_SERVER_ROUTER_ERROR_INTERNAL,
			"freeze-http-server-router",
			"HTTP server router index freeze failed"
		);
		return false;
	}
	return true;
}



/* 返回高层 Router 是否冻结。 */
XRT_API bool xrtHttpServerRouterFrozen(
	const xhttpserverrouter* pRouter
)
{
	return (pRouter != NULL) &&
		xrtHttpRouterFrozen(pRouter->Index);
}



/* 返回复制回调记录数量。 */
XRT_API size_t xrtHttpServerRouterCount(
	const xhttpserverrouter* pRouter
)
{
	return pRouter != NULL ? pRouter->Count : 0;
}

#endif

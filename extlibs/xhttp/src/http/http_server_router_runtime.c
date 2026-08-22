#include "../internal/xrt_http_server_router.h"
#include "../internal/xrt_http_server_runtime.h"



#if defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER)

/* 转发 Connection 打开事件。 */
static void __xrtHttpServerRouterOpen(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	ptr pData
)
{
	xrt_http_server_router_runtime* pRuntime =
		(xrt_http_server_router_runtime*)pData;

	if ( pRuntime->Events.Open != NULL ) {
		pRuntime->Events.Open(
			pServer,
			pConnection,
			pRuntime->Events.Data
		);
	}
}



/* 报告路由内部失败并尽力提交固定 500，二次失败时异常关闭。 */
void __xrtHttpServerRouterDispatchFail(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverevents* pEvents
)
{
	xerror* pError = xrtErrorRef(xrtGetError());

	if ( (pError != NULL) &&
		(pEvents != NULL) &&
		(pEvents->Error != NULL) ) {
		pEvents->Error(
			pServer,
			pConnection,
			pError,
			pEvents->Data
		);
	}
	if ( xrtHttpConnReply(
		pConnection,
		XHTTP_STATUS_INTERNAL_SERVER_ERROR,
		XRT_STR_LITERAL("text/plain; charset=utf-8"),
		XRT_BYTES_LITERAL("Internal Server Error")
	) != XNET_RESULT_OK ) {
		xrtClearError();
		(void)xrtHttpConnAbort(pConnection);
	}
	xrtErrorFree(pError);
}



/* 执行匹配路由、调用方回退或 Router 标准终端响应。 */
bool __xrtHttpServerRouterDispatchTerminal(
	const xhttpserverrouter* pRouter,
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xrt_http_server_route_match* pMatch,
	const xhttpserverevents* pEvents
)
{
	if ( pMatch->Status == XHTTP_ROUTER_MATCH ) {
		pMatch->Entry->Events.Request(
			pServer,
			pConnection,
			pRequest,
			pMatch->Params,
			pMatch->Count,
			pMatch->Entry->Events.Data
		);
		return true;
	}
	if ( (pEvents != NULL) &&
		(pEvents->Request != NULL) ) {
		pEvents->Request(
			pServer,
			pConnection,
			pRequest,
			pEvents->Data
		);
		return true;
	}
	return __xrtHttpServerRouterDefault(
		pRouter, pConnection, pMatch
	);
}



/* 对未命中路由选择用户回退策略或立即提交标准响应并拒绝正文。 */
static xhttpserverbodypolicy __xrtHttpServerRouterMissingHeaders(
	const xhttpserverrouter* pRouter,
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttpserverevents* pEvents,
	xrt_http_server_route_match* pMatch
)
{
	if ( pEvents->Request != NULL ) {
		if ( pEvents->Headers != NULL ) {
			return pEvents->Headers(
				pServer,
				pConnection,
				pRequest,
				pEvents->Data
			);
		}
		return XHTTP_SERVER_BODY_BUFFER;
	}
	#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE)
		if ( pRouter->MiddlewareCount != 0 ) {
			return XHTTP_SERVER_BODY_DISCARD;
		}
	#endif
	if ( !__xrtHttpServerRouterDefault(
		pRouter, pConnection, pMatch
	) ) {
		__xrtHttpServerRouterDispatchFail(
			pServer, pConnection, pEvents
		);
	}
	return XHTTP_SERVER_BODY_REJECT;
}



/* 在 Header 完整后使用指定 Router 选择正文策略。 */
xhttpserverbodypolicy __xrtHttpServerRouterDispatchHeaders(
	const xhttpserverrouter* pRouter,
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttpserverevents* pEvents,
	xrt_http_server_route_match** ppMatch
)
{
	xrt_http_server_route_match Match;
	xhttpserverbodypolicy Policy;
	xrt_http_server_route_match* pStored = NULL;

	if ( ppMatch != NULL ) {
		*ppMatch = NULL;
	}

	if ( !__xrtHttpServerRouterMatch(
		pRouter, pRequest, &Match
	) ) {
		__xrtHttpServerRouterDispatchFail(
			pServer, pConnection, pEvents
		);
		return XHTTP_SERVER_BODY_REJECT;
	}
	if ( Match.Status != XHTTP_ROUTER_MATCH ) {
		Policy = __xrtHttpServerRouterMissingHeaders(
			pRouter,
			pServer,
			pConnection,
			pRequest,
			pEvents,
			&Match
		);
	} else if ( Match.Entry->Events.Headers != NULL ) {
		Policy = Match.Entry->Events.Headers(
			pServer,
			pConnection,
			pRequest,
			Match.Params,
			Match.Count,
			Match.Entry->Events.Data
		);
	} else if ( pEvents->Headers != NULL ) {
		Policy = pEvents->Headers(
			pServer,
			pConnection,
			pRequest,
			pEvents->Data
		);
	} else {
		Policy = XHTTP_SERVER_BODY_BUFFER;
	}
	if ( Policy == XHTTP_SERVER_BODY_STREAM ) {
		pStored = __xrtHttpServerRouterMatchTake(&Match);
		if ( pStored == NULL ) {
			__xrtHttpServerRouterDispatchFail(
				pServer, pConnection, pEvents
			);
			Policy = XHTTP_SERVER_BODY_REJECT;
		}
	}
	__xrtHttpServerRouterMatchClear(&Match);
	if ( ppMatch != NULL ) {
		*ppMatch = pStored;
	} else {
		__xrtHttpServerRouterMatchDestroy(pStored);
	}
	return Policy;
}



/* 把流式正文片段交给指定 Router，未命中时使用原始 Body 事件。 */
bool __xrtHttpServerRouterDispatchBody(
	const xhttpserverrouter* pRouter,
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	xbytesview Data,
	const xhttpserverevents* pEvents,
	const xrt_http_server_route_match* pCached
)
{
	xrt_http_server_route_match Match;
	const xrt_http_server_route_match* pMatch = pCached;
	bool bLocal = false;
	bool bResult;

	if ( pMatch == NULL ) {
		if ( !__xrtHttpServerRouterMatch(
			pRouter, pRequest, &Match
		) ) {
			return false;
		}
		pMatch = &Match;
		bLocal = true;
	}
	if ( (pMatch->Status == XHTTP_ROUTER_MATCH) &&
		(pMatch->Entry->Events.Body != NULL) ) {
		bResult = pMatch->Entry->Events.Body(
			pServer,
			pConnection,
			pRequest,
			pMatch->Params,
			pMatch->Count,
			Data,
			pMatch->Entry->Events.Data
		);
	} else if ( pEvents->Body != NULL ) {
		bResult = pEvents->Body(
			pServer,
			pConnection,
			pRequest,
			Data,
			pEvents->Data
		);
	} else {
		__xrtHttpServerRouterSetError(
			XERR_STATE,
			XHTTP_SERVER_ROUTER_ERROR_STATE,
			"stream-http-server-route-body",
			"HTTP route selected streaming without a Body handler",
			NULL
		);
		bResult = false;
	}
	if ( bLocal ) {
		__xrtHttpServerRouterMatchClear(&Match);
	}
	return bResult;
}



/* 使用指定 Router 分发完整请求；原始 Request 事件仅处理未命中路由。 */
void __xrtHttpServerRouterDispatchRequest(
	const xhttpserverrouter* pRouter,
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttpserverevents* pEvents,
	const xrt_http_server_route_match* pCached
)
{
	xrt_http_server_route_match Match;
	const xrt_http_server_route_match* pMatch = pCached;
	bool bLocal = false;

	if ( pMatch == NULL ) {
		if ( !__xrtHttpServerRouterMatch(
			pRouter, pRequest, &Match
		) ) {
			__xrtHttpServerRouterDispatchFail(
				pServer, pConnection, pEvents
			);
			return;
		}
		pMatch = &Match;
		bLocal = true;
	}
	#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE)
		if ( !__xrtHttpServerMiddlewareDispatch(
			pRouter,
			pServer,
			pConnection,
			pRequest,
			pMatch,
			pEvents
		) ) {
			__xrtHttpServerRouterDispatchFail(
				pServer, pConnection, pEvents
			);
		}
	#else
		if ( !__xrtHttpServerRouterDispatchTerminal(
			pRouter,
			pServer,
			pConnection,
			pRequest,
			pMatch,
			pEvents
		) ) {
			__xrtHttpServerRouterDispatchFail(
				pServer, pConnection, pEvents
			);
		}
	#endif
	if ( bLocal ) {
		__xrtHttpServerRouterMatchClear(&Match);
	}
}



/* 固定 Router 适配器把 Header 阶段转发给共用分发核心。 */
static xhttpserverbodypolicy __xrtHttpServerRouterHeaders(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	xrt_http_server_router_runtime* pRuntime =
		(xrt_http_server_router_runtime*)pData;
	xrt_http_server_route_match* pMatch = NULL;
	xhttpserverbodypolicy Policy;

	Policy = __xrtHttpServerRouterDispatchHeaders(
		pRuntime->Router,
		pServer,
		pConnection,
		pRequest,
		&pRuntime->Events,
		&pMatch
	);
	if ( (pMatch != NULL) && !__xrtHttpConnAdapterSet(
		pConnection,
		pMatch,
		__xrtHttpServerRouterMatchDestroy
	) ) {
		__xrtHttpServerRouterMatchDestroy(pMatch);
		__xrtHttpServerRouterSetError(
			XERR_STATE,
			XHTTP_SERVER_ROUTER_ERROR_STATE,
			"cache-http-server-route",
			"HTTP server connection already has a route cache",
			NULL
		);
		__xrtHttpServerRouterDispatchFail(
			pServer, pConnection, &pRuntime->Events
		);
		return XHTTP_SERVER_BODY_REJECT;
	}
	return Policy;
}



/* 固定 Router 适配器把正文片段转发给共用分发核心。 */
static bool __xrtHttpServerRouterBody(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	xbytesview Data,
	ptr pData
)
{
	xrt_http_server_router_runtime* pRuntime =
		(xrt_http_server_router_runtime*)pData;

	return __xrtHttpServerRouterDispatchBody(
		pRuntime->Router,
		pServer,
		pConnection,
		pRequest,
		Data,
		&pRuntime->Events,
		(const xrt_http_server_route_match*)
			__xrtHttpConnAdapterData(pConnection)
	);
}



/* 固定 Router 适配器把完整请求转发给共用分发核心。 */
static void __xrtHttpServerRouterRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	xrt_http_server_router_runtime* pRuntime =
		(xrt_http_server_router_runtime*)pData;
	xrt_http_server_route_match* pMatch =
		(xrt_http_server_route_match*)
		__xrtHttpConnAdapterTake(pConnection);

	__xrtHttpServerRouterDispatchRequest(
		pRuntime->Router,
		pServer,
		pConnection,
		pRequest,
		&pRuntime->Events,
		pMatch
	);
	__xrtHttpServerRouterMatchDestroy(pMatch);
}



/* 转发底层 Server 错误事件。 */
static void __xrtHttpServerRouterError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	xrt_http_server_router_runtime* pRuntime =
		(xrt_http_server_router_runtime*)pData;

	if ( pRuntime->Events.Error != NULL ) {
		pRuntime->Events.Error(
			pServer,
			pConnection,
			pError,
			pRuntime->Events.Data
		);
	}
}



/* 转发 Connection 唯一关闭事件。 */
static void __xrtHttpServerRouterClose(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	xrt_http_server_router_runtime* pRuntime =
		(xrt_http_server_router_runtime*)pData;
	ptr pMatch = __xrtHttpConnAdapterTake(pConnection);

	__xrtHttpServerRouterMatchDestroy(pMatch);

	if ( pRuntime->Events.Close != NULL ) {
		pRuntime->Events.Close(
			pServer,
			pConnection,
			Result,
			pError,
			pRuntime->Events.Data
		);
	}
}



/* 在底层 Server 发布最终关闭后转发 Shutdown 并释放适配器资产。 */
static void __xrtHttpServerRouterShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	xrt_http_server_router_runtime* pRuntime =
		(xrt_http_server_router_runtime*)pData;

	if ( pRuntime->Events.Shutdown != NULL ) {
		pRuntime->Events.Shutdown(
			pServer,
			pRuntime->Events.Data
		);
	}
	__xrtHttpServerRouterRuntimeDestroy(pRuntime);
}



/* 释放适配器内存及其持有的 Router 引用。 */
void __xrtHttpServerRouterRuntimeDestroy(
	xrt_http_server_router_runtime* pRuntime
)
{
	xhttpserverrouter* pRouter;

	if ( pRuntime == NULL ) {
		return;
	}
	pRouter = pRuntime->Router;
	xrtFree(pRuntime);
	xrtHttpServerRouterDestroy(pRouter);
}



/* 建立持有 Router 的完整事件适配器。 */
xrt_http_server_router_runtime* __xrtHttpServerRouterRuntimeCreate(
	xhttpserverrouter* pRouter,
	const xhttpserverevents* pEvents,
	xhttpserverevents* pOutput
)
{
	xhttpserverevents Events = { 0 };
	xrt_http_server_router_runtime* pRuntime;
	bool bArguments = (pRouter == NULL) ||
		!__xrtRangeValid(pOutput, sizeof(Events)) ||
		((pEvents != NULL) &&
		 !__xrtRangeValid(pEvents, sizeof(Events)));

	if ( bArguments || !xrtHttpServerRouterFrozen(pRouter) ) {
		__xrtHttpServerRouterSetError(
			bArguments ? XERR_ARGUMENT : XERR_STATE,
			bArguments ?
				XHTTP_SERVER_ROUTER_ERROR_ARGUMENT :
				XHTTP_SERVER_ROUTER_ERROR_STATE,
			"start-http-server-router",
			bArguments ?
				"HTTP server router event ranges are invalid" :
				"HTTP server router must be frozen",
			NULL
		);
		return NULL;
	}
	if ( pEvents != NULL ) {
		memcpy(&Events, pEvents, sizeof(Events));
	}
	pRuntime = (xrt_http_server_router_runtime*)xrtCalloc(
		1, sizeof(*pRuntime)
	);
	if ( pRuntime == NULL ) {
		__xrtHttpServerRouterWrapError(
			XERR_MEMORY,
			XHTTP_SERVER_ROUTER_ERROR_MEMORY,
			"start-http-server-router",
			"HTTP server router runtime allocation failed"
		);
		return NULL;
	}
	pRuntime->Router = xrtHttpServerRouterRef(pRouter);
	if ( pRuntime->Router == NULL ) {
		xrtFree(pRuntime);
		return NULL;
	}
	pRuntime->Events = Events;
	xrtHttpServerEventsInit(pOutput);
	pOutput->Open = __xrtHttpServerRouterOpen;
	pOutput->Headers = __xrtHttpServerRouterHeaders;
	pOutput->Body = __xrtHttpServerRouterBody;
	pOutput->Request = __xrtHttpServerRouterRequest;
	pOutput->Close = __xrtHttpServerRouterClose;
	pOutput->Error = __xrtHttpServerRouterError;
	pOutput->Shutdown = __xrtHttpServerRouterShutdown;
	pOutput->Data = pRuntime;
	return pRuntime;
}



/* 启动明文 Router Server；启动失败时同步释放适配器引用。 */
XRT_API xhttpserver* xrtHttpServerRouterStart(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	xhttpserverrouter* pRouter,
	const xhttpserverevents* pEvents
)
{
	xhttpserverevents Events;
	xrt_http_server_router_runtime* pRuntime =
		__xrtHttpServerRouterRuntimeCreate(
			pRouter, pEvents, &Events
		);
	xhttpserver* pServer;

	if ( pRuntime == NULL ) {
		return NULL;
	}
	pServer = xrtHttpServerStart(
		pEngine, pConfig, &Events
	);
	if ( pServer == NULL ) {
		__xrtHttpServerRouterRuntimeDestroy(pRuntime);
		__xrtHttpServerRouterWrapError(
			XERR_IO,
			XHTTP_SERVER_ROUTER_ERROR_START,
			"start-http-server-router",
			"HTTP server router listener start failed"
		);
	}
	return pServer;
}

#endif

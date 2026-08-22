#include "../internal/xrt_http_server_mux.h"



#if defined(XRT_FEATURE_HTTP_SERVER_MUX)

/* 释放一个 Connection 固定持有的当前请求 Router。 */
static void __xrtHttpServerMuxConnectionDestroy(ptr pData)
{
	xrt_http_server_mux_connection* pContext =
		(xrt_http_server_mux_connection*)pData;

	if ( pContext == NULL ) {
		return;
	}
	__xrtHttpServerRouterMatchDestroy(pContext->Match);
	xrtHttpServerRouterDestroy(pContext->Router);
	xrtFree(pContext);
}



/* 报告 Mux 适配错误并异常关闭尚无可响应请求的连接。 */
static void __xrtHttpServerMuxOpenFail(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xrt_http_server_mux_runtime* pRuntime
)
{
	xerror* pError = xrtErrorRef(xrtGetError());

	if ( (pError != NULL) &&
		(pRuntime->Events.Error != NULL) ) {
		pRuntime->Events.Error(
			pServer,
			pConnection,
			pError,
			pRuntime->Events.Data
		);
	}
	xrtErrorFree(pError);
	(void)xrtHttpConnAbort(pConnection);
}



/* 为新连接安装 O(1) Mux 上下文，再转发用户 Open 事件。 */
static void __xrtHttpServerMuxOpen(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	ptr pData
)
{
	xrt_http_server_mux_runtime* pRuntime =
		(xrt_http_server_mux_runtime*)pData;
	xrt_http_server_mux_connection* pContext =
		(xrt_http_server_mux_connection*)xrtCalloc(
			1, sizeof(*pContext)
		);

	if ( pContext == NULL ) {
		__xrtHttpServerMuxWrapError(
			XERR_MEMORY,
			XHTTP_SERVER_MUX_ERROR_MEMORY,
			"open-http-server-mux-connection",
			"HTTP server mux connection context allocation failed"
		);
		__xrtHttpServerMuxOpenFail(
			pServer, pConnection, pRuntime
		);
		return;
	}
	if ( !__xrtHttpConnAdapterSet(
		pConnection,
		pContext,
		__xrtHttpServerMuxConnectionDestroy
	) ) {
		__xrtHttpServerMuxConnectionDestroy(pContext);
		__xrtHttpServerMuxSetError(
			XERR_STATE,
			XHTTP_SERVER_MUX_ERROR_CONTEXT,
			"open-http-server-mux-connection",
			"HTTP server connection already has an adapter context",
			NULL
		);
		__xrtHttpServerMuxOpenFail(
			pServer, pConnection, pRuntime
		);
		return;
	}
	if ( pRuntime->Events.Open != NULL ) {
		pRuntime->Events.Open(
			pServer,
			pConnection,
			pRuntime->Events.Data
		);
	}
}



/* 返回当前 Connection 的 Mux 上下文并建立稳定错误。 */
static xrt_http_server_mux_connection* __xrtHttpServerMuxConnection(
	xhttpconn* pConnection,
	cstr sOperation
)
{
	xrt_http_server_mux_connection* pContext =
		(xrt_http_server_mux_connection*)
		__xrtHttpConnAdapterData(pConnection);

	if ( pContext == NULL ) {
		__xrtHttpServerMuxSetError(
			XERR_STATE,
			XHTTP_SERVER_MUX_ERROR_CONTEXT,
			sOperation,
			"HTTP server mux connection context is missing",
			NULL
		);
	}
	return pContext;
}



/* 替换当前请求固定 Router，并在赋值后释放上一请求引用。 */
static void __xrtHttpServerMuxConnectionRouter(
	xrt_http_server_mux_connection* pContext,
	xhttpserverrouter* pRouter
)
{
	xhttpserverrouter* pOld = pContext->Router;

	pContext->Router = pRouter;
	xrtHttpServerRouterDestroy(pOld);
}



/* 未配置目标 Host 时选择用户回退或提交标准 421。 */
static xhttpserverbodypolicy __xrtHttpServerMuxMissing(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	xrt_http_server_mux_runtime* pRuntime
)
{
	if ( pRuntime->Events.Request != NULL ) {
		if ( pRuntime->Events.Headers != NULL ) {
			return pRuntime->Events.Headers(
				pServer,
				pConnection,
				pRequest,
				pRuntime->Events.Data
			);
		}
		return XHTTP_SERVER_BODY_BUFFER;
	}
	if ( xrtHttpConnReply(
		pConnection,
		XHTTP_STATUS_MISDIRECTED_REQUEST,
		XRT_STR_LITERAL("text/plain; charset=utf-8"),
		XRT_BYTES_LITERAL("Misdirected Request")
	) != XNET_RESULT_OK ) {
		__xrtHttpServerRouterDispatchFail(
			pServer, pConnection, &pRuntime->Events
		);
	}
	return XHTTP_SERVER_BODY_REJECT;
}



/* 在 Header 阶段按有效 authority 固定本次请求使用的 Router。 */
static xhttpserverbodypolicy __xrtHttpServerMuxHeaders(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	xrt_http_server_mux_runtime* pRuntime =
		(xrt_http_server_mux_runtime*)pData;
	xrt_http_server_mux_connection* pContext =
		__xrtHttpServerMuxConnection(
			pConnection,
			"route-http-server-mux-headers"
		);
	xhttpserverrouter* pRouter = NULL;
	xhttpservermuxstatus Status;
	xurl Authority;

	if ( pContext == NULL ) {
		__xrtHttpServerRouterDispatchFail(
			pServer, pConnection, &pRuntime->Events
		);
		return XHTTP_SERVER_BODY_REJECT;
	}
	__xrtHttpServerRouterMatchDestroy(pContext->Match);
	pContext->Match = NULL;
	if ( !xrtHttpServerRequestAuthority(
		pRequest, &Authority
	) ) {
		__xrtHttpServerMuxWrapError(
			XERR_PROTOCOL,
			XHTTP_SERVER_MUX_ERROR_HOST,
			"route-http-server-mux-headers",
			"HTTP server request authority could not be parsed"
		);
		__xrtHttpServerMuxConnectionRouter(pContext, NULL);
		__xrtHttpServerRouterDispatchFail(
			pServer, pConnection, &pRuntime->Events
		);
		return XHTTP_SERVER_BODY_REJECT;
	}
	Status = __xrtHttpServerMuxSelect(
		pRuntime->Mux, Authority.Host, &pRouter
	);
	__xrtHttpServerMuxConnectionRouter(pContext, pRouter);
	if ( Status == XHTTP_SERVER_MUX_ERROR ) {
		__xrtHttpServerRouterDispatchFail(
			pServer, pConnection, &pRuntime->Events
		);
		return XHTTP_SERVER_BODY_REJECT;
	}
	if ( Status == XHTTP_SERVER_MUX_NOT_FOUND ) {
		return __xrtHttpServerMuxMissing(
			pServer, pConnection, pRequest, pRuntime
		);
	}
	return __xrtHttpServerRouterDispatchHeaders(
		pContext->Router,
		pServer,
		pConnection,
		pRequest,
		&pRuntime->Events,
		&pContext->Match
	);
}



/* 把正文片段交给 Header 阶段固定的 Router 或用户回退。 */
static bool __xrtHttpServerMuxBody(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	xbytesview Data,
	ptr pData
)
{
	xrt_http_server_mux_runtime* pRuntime =
		(xrt_http_server_mux_runtime*)pData;
	xrt_http_server_mux_connection* pContext =
		__xrtHttpServerMuxConnection(
			pConnection,
			"route-http-server-mux-body"
		);

	if ( pContext == NULL ) {
		return false;
	}
	if ( pContext->Router != NULL ) {
		return __xrtHttpServerRouterDispatchBody(
			pContext->Router,
			pServer,
			pConnection,
			pRequest,
			Data,
			&pRuntime->Events,
			pContext->Match
		);
	}
	if ( pRuntime->Events.Body != NULL ) {
		return pRuntime->Events.Body(
			pServer,
			pConnection,
			pRequest,
			Data,
			pRuntime->Events.Data
		);
	}
	__xrtHttpServerMuxSetError(
		XERR_STATE,
		XHTTP_SERVER_MUX_ERROR_CONTEXT,
		"route-http-server-mux-body",
		"HTTP server mux fallback selected streaming without a Body handler",
		NULL
	);
	return false;
}



/* 把完整请求交给固定 Router，或使用用户未命中回退。 */
static void __xrtHttpServerMuxRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	xrt_http_server_mux_runtime* pRuntime =
		(xrt_http_server_mux_runtime*)pData;
	xrt_http_server_mux_connection* pContext =
		__xrtHttpServerMuxConnection(
			pConnection,
			"route-http-server-mux-request"
		);
	xrt_http_server_route_match* pMatch;

	if ( pContext == NULL ) {
		__xrtHttpServerRouterDispatchFail(
			pServer, pConnection, &pRuntime->Events
		);
		return;
	}
	if ( pContext->Router != NULL ) {
		pMatch = pContext->Match;
		pContext->Match = NULL;
		__xrtHttpServerRouterDispatchRequest(
			pContext->Router,
			pServer,
			pConnection,
			pRequest,
			&pRuntime->Events,
			pMatch
		);
		__xrtHttpServerRouterMatchDestroy(pMatch);
	} else if ( pRuntime->Events.Request != NULL ) {
		pRuntime->Events.Request(
			pServer,
			pConnection,
			pRequest,
			pRuntime->Events.Data
		);
	}
}



/* 转发底层 Server 错误事件。 */
static void __xrtHttpServerMuxError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	xrt_http_server_mux_runtime* pRuntime =
		(xrt_http_server_mux_runtime*)pData;

	if ( pRuntime->Events.Error != NULL ) {
		pRuntime->Events.Error(
			pServer,
			pConnection,
			pError,
			pRuntime->Events.Data
		);
	}
}



/* 先归还 Connection 固定 Router，再转发唯一 Close 事件。 */
static void __xrtHttpServerMuxClose(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	xrt_http_server_mux_runtime* pRuntime =
		(xrt_http_server_mux_runtime*)pData;
	ptr pContext = __xrtHttpConnAdapterTake(pConnection);

	__xrtHttpServerMuxConnectionDestroy(pContext);
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



/* 转发最终 Shutdown 并释放 Server 持有的 Mux 适配器。 */
static void __xrtHttpServerMuxShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	xrt_http_server_mux_runtime* pRuntime =
		(xrt_http_server_mux_runtime*)pData;

	if ( pRuntime->Events.Shutdown != NULL ) {
		pRuntime->Events.Shutdown(
			pServer,
			pRuntime->Events.Data
		);
	}
	__xrtHttpServerMuxRuntimeDestroy(pRuntime);
}



/* 释放适配器与其持有的 Mux 引用。 */
void __xrtHttpServerMuxRuntimeDestroy(
	xrt_http_server_mux_runtime* pRuntime
)
{
	xhttpservermux* pMux;

	if ( pRuntime == NULL ) {
		return;
	}
	pMux = pRuntime->Mux;
	xrtFree(pRuntime);
	xrtHttpServerMuxDestroy(pMux);
}



/* 建立供明文和 TLS Server 共用的完整事件适配器。 */
xrt_http_server_mux_runtime* __xrtHttpServerMuxRuntimeCreate(
	xhttpservermux* pMux,
	const xhttpserverevents* pEvents,
	xhttpserverevents* pOutput
)
{
	xrt_http_server_mux_runtime* pRuntime;

	if ( (pMux == NULL) || (pOutput == NULL) ) {
		__xrtHttpServerMuxSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_MUX_ERROR_ARGUMENT,
			"start-http-server-mux",
			"HTTP server mux and output events are required",
			NULL
		);
		return NULL;
	}
	pRuntime = (xrt_http_server_mux_runtime*)xrtCalloc(
		1, sizeof(*pRuntime)
	);
	if ( pRuntime == NULL ) {
		__xrtHttpServerMuxWrapError(
			XERR_MEMORY,
			XHTTP_SERVER_MUX_ERROR_MEMORY,
			"start-http-server-mux",
			"HTTP server mux runtime allocation failed"
		);
		return NULL;
	}
	pRuntime->Mux = xrtHttpServerMuxRef(pMux);
	if ( pRuntime->Mux == NULL ) {
		xrtFree(pRuntime);
		return NULL;
	}
	if ( pEvents != NULL ) {
		pRuntime->Events = *pEvents;
	}
	xrtHttpServerEventsInit(pOutput);
	pOutput->Open = __xrtHttpServerMuxOpen;
	pOutput->Headers = __xrtHttpServerMuxHeaders;
	pOutput->Body = __xrtHttpServerMuxBody;
	pOutput->Request = __xrtHttpServerMuxRequest;
	pOutput->Close = __xrtHttpServerMuxClose;
	pOutput->Error = __xrtHttpServerMuxError;
	pOutput->Shutdown = __xrtHttpServerMuxShutdown;
	pOutput->Data = pRuntime;
	return pRuntime;
}



/* 启动明文 Mux Server；失败时同步归还适配器引用。 */
XRT_API xhttpserver* xrtHttpServerMuxStart(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	xhttpservermux* pMux,
	const xhttpserverevents* pEvents
)
{
	xhttpserverevents Events;
	xrt_http_server_mux_runtime* pRuntime =
		__xrtHttpServerMuxRuntimeCreate(
			pMux, pEvents, &Events
		);
	xhttpserver* pServer;

	if ( pRuntime == NULL ) {
		return NULL;
	}
	pServer = xrtHttpServerStart(
		pEngine, pConfig, &Events
	);
	if ( pServer == NULL ) {
		__xrtHttpServerMuxRuntimeDestroy(pRuntime);
		__xrtHttpServerMuxWrapError(
			XERR_IO,
			XHTTP_SERVER_MUX_ERROR_START,
			"start-http-server-mux",
			"HTTP server mux listener start failed"
		);
	}
	return pServer;
}

#endif

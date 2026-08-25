#include "../internal/xrt_websocket_server_router.h"



#if defined(XWS_FEATURE_WEBSOCKET_SERVER_ROUTER)

/* 设置带稳定域、代码、操作和可选原因链的 WebSocket Router 错误。 */
static void __xrtWsServerRouterSetError(
	xerrkind Kind,
	xwsserverroutererror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Code = (int32)Code;
	Desc.SystemCode = pCause != NULL ?
		xrtErrorSystemCode(pCause) : 0;
	Desc.Domain = "xrt.websocket.server.router";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 验证固定公开对象占用完整且不回绕的地址区间。 */
static bool __xrtWsServerRouterRangeCheck(
	const void* pObject,
	size_t iSize,
	cstr sOperation,
	cstr sMessage
)
{
	if ( xrtMemRangeValid(pObject, iSize) ) {
		return true;
	}
	__xrtWsServerRouterSetError(
		XERR_ARGUMENT,
		XWS_SERVER_ROUTER_ERROR_ARGUMENT,
		sOperation,
		sMessage,
		NULL
	);
	return false;
}



/* 返回原因错误已有类型，缺失时使用调用点给出的保守类型。 */
static xerrkind __xrtWsServerRouterCauseKind(
	const xerror* pCause,
	xerrkind Default
)
{
	xerrkind Kind = xrtErrorKind(pCause);

	return Kind != XERR_NONE ? Kind : Default;
}



/* 比较大小写敏感且不要求零结尾的 HTTP 方法。 */
static bool __xrtWsServerRouterMethod(
	xstrview Method,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (Method.Size == iSize) &&
		(memcmp(Method.Data, sExpected, iSize) == 0);
}



/* 向固定路由观察者发布一次同步握手或响应错误。 */
static void __xrtWsServerRouterReport(
	xrt_ws_server_route* pRoute,
	xhttpconn* pHttp,
	const xerror* pError
)
{
	if ( pRoute->Config.Error != NULL ) {
		pRoute->Config.Error(
			pHttp, pError, pRoute->Config.Data
		);
	}
}



/* 提交只允许 GET 的方法拒绝响应。 */
static bool __xrtWsServerRouterMethodReply(
	xhttpconn* pHttp,
	uint16 iStatus
)
{
	xhttpreply* pReply = xrtHttpReplyCreate(iStatus);
	xnetresult Result = XNET_RESULT_ERROR;

	if ( (pReply != NULL) &&
		xrtHttpReplySetHeader(
			pReply,
			XRT_STR_LITERAL("Allow"),
			XRT_STR_LITERAL("GET")
		) ) {
		Result = xrtHttpConnRespond(pHttp, pReply);
	}
	xrtHttpReplyDestroy(pReply);
	return Result == XNET_RESULT_OK;
}



/* 提交无正文的固定状态响应。 */
static bool __xrtWsServerRouterStatusReply(
	xhttpconn* pHttp,
	uint16 iStatus
)
{
	xhttpreply* pReply = xrtHttpReplyCreate(iStatus);
	xnetresult Result = XNET_RESULT_ERROR;

	if ( pReply != NULL ) {
		Result = xrtHttpConnRespond(pHttp, pReply);
	}
	xrtHttpReplyDestroy(pReply);
	return Result == XNET_RESULT_OK;
}



/* 包装响应构造或提交失败，通知观察者后异常关闭。 */
static void __xrtWsServerRouterResponseFailed(
	xrt_ws_server_route* pRoute,
	xhttpconn* pHttp,
	cstr sMessage
)
{
	xerror* pCause = xrtErrorRef(xrtGetError());
	xerror* pError;

	__xrtWsServerRouterSetError(
		__xrtWsServerRouterCauseKind(pCause, XERR_IO),
		XWS_SERVER_ROUTER_ERROR_RESPONSE,
		"reject-websocket-server-route",
		sMessage,
		pCause
	);
	xrtErrorFree(pCause);
	pError = xrtErrorRef(xrtGetError());
	__xrtWsServerRouterReport(pRoute, pHttp, pError);
	xrtErrorFree(pError);
	(void)xrtHttpConnAbort(pHttp);
}



/* 报告并提交标准握手拒绝；二次响应失败时异常关闭。 */
static void __xrtWsServerRouterReject(
	xrt_ws_server_route* pRoute,
	xhttpconn* pHttp,
	const xerror* pError
)
{
	__xrtWsServerRouterReport(pRoute, pHttp, pError);
	if ( xrtWsServerReject(
		pHttp, pError
	) != XNET_RESULT_OK ) {
		__xrtWsServerRouterResponseFailed(
			pRoute,
			pHttp,
			"WebSocket handshake rejection response failed"
		);
	}
}



/* 拒绝不符合 Origin 或业务授权策略的握手。 */
static void __xrtWsServerRouterAuthorizationReject(
	xrt_ws_server_route* pRoute,
	xhttpconn* pHttp,
	cstr sMessage
)
{
	xerror* pError;

	__xrtWsServerRouterSetError(
		XERR_PERMISSION,
		XWS_SERVER_ROUTER_ERROR_AUTHORIZATION,
		"authorize-websocket-server-route",
		sMessage,
		NULL
	);
	pError = xrtErrorRef(xrtGetError());
	__xrtWsServerRouterReport(pRoute, pHttp, pError);
	xrtErrorFree(pError);
	if ( !__xrtWsServerRouterStatusReply(
		pHttp, XHTTP_STATUS_FORBIDDEN
	) ) {
		__xrtWsServerRouterResponseFailed(
			pRoute,
			pHttp,
			"WebSocket authorization response failed"
		);
	}
}



/* 比较浏览器 Origin 与当前 HTTP 请求的 scheme、host 和有效端口。 */
static bool __xrtWsServerRouterOriginSame(
	xhttpconn* pHttp,
	const xhttpserverrequest* pRequest,
	const xhttporigin* pOrigin
)
{
	xhttpauthority Authority;
	xstrview Scheme = xrtHttpConnSecure(pHttp) ?
		XRT_STR_LITERAL("https") : XRT_STR_LITERAL("http");
	uint16 iDefault = xrtHttpConnSecure(pHttp) ? 443u : 80u;
	uint16 iOriginPort;
	uint16 iRequestPort;

	if ( ((pOrigin->Flags & XHTTP_ORIGIN_NULL) != 0) ||
		!xrtUrlSchemeIs(&pOrigin->Url, Scheme) ||
		!xrtHttpServerRequestAuthority(pRequest, &Authority) ||
		!xrtHttpHostEqual(pOrigin->Url.Host, Authority.Host) ||
		!xrtUrlPort(&pOrigin->Url, &iOriginPort) ||
		!xrtHttpAuthorityPort(
			&Authority, iDefault, &iRequestPort
		) ) {
		return false;
	}
	return iOriginPort == iRequestPort;
}



/* 执行固定路由 Origin 策略，ANY 保留给已在外层授权的组合。 */
static bool __xrtWsServerRouterOriginAllowed(
	xrt_ws_server_route* pRoute,
	xhttpconn* pHttp,
	const xhttpserverrequest* pRequest
)
{
	xhttporigin Origin;
	xhttpnext Next;

	if ( pRoute->Config.Origin == XWS_SERVER_ORIGIN_ANY ) {
		return true;
	}
	Next = xrtHttpOriginFields(
		xrtHttpServerRequestHeaderData(pRequest),
		xrtHttpServerRequestHeaderCount(pRequest),
		&Origin
	);
	if ( Next == XHTTP_NEXT_END ) {
		return pRoute->Config.Origin ==
			XWS_SERVER_ORIGIN_SAME_HOST_OR_ABSENT;
	}
	return (Next == XHTTP_NEXT_ITEM) &&
		__xrtWsServerRouterOriginSame(
			pHttp, pRequest, &Origin
		);
}



/* 判断请求是否声明或已经携带正文；WebSocket Upgrade 不接受这些请求。 */
static bool __xrtWsServerRouterHasBody(
	const xhttpserverrequest* pRequest
)
{
	return (xrtHttpServerRequestHeader(
		pRequest,
		XRT_STR_LITERAL("Content-Length")
	) != NULL) || (xrtHttpServerRequestHeader(
		pRequest,
		XRT_STR_LITERAL("Transfer-Encoding")
	) != NULL) || (xrtHttpServerRequestBodyMode(pRequest) !=
		XHTTP1_BODY_NONE) ||
		(xrtHttpServerRequestBodyBytes(pRequest) != 0);
}



/* 在 Header 阶段处理方法和正文门禁；无正文请求进入完整请求阶段。 */
static xhttpserverbodypolicy __xrtWsServerRouterHeaders(
	xhttpserver* pServer,
	xhttpconn* pHttp,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	xrt_ws_server_route* pRoute =
		(xrt_ws_server_route*)pData;
	xstrview Method = xrtHttpServerRequestMethod(pRequest);
	xwsserverhandshake Handshake;
	xerror* pError;

	(void)pServer;
	(void)pParams;
	(void)iParamCount;
	if ( !__xrtWsServerRouterMethod(Method, "GET") ) {
		if ( !__xrtWsServerRouterMethodReply(
			pHttp, XHTTP_STATUS_METHOD_NOT_ALLOWED
		) ) {
			__xrtWsServerRouterResponseFailed(
				pRoute,
				pHttp,
				"WebSocket method rejection response failed"
			);
		}
		return XHTTP_SERVER_BODY_REJECT;
	}
	if ( __xrtWsServerRouterHasBody(pRequest) ) {
		(void)xrtWsServerCheck(
			pRequest,
			&pRoute->Config.Server,
			&Handshake
		);
		pError = xrtErrorRef(xrtGetError());
		__xrtWsServerRouterReject(pRoute, pHttp, pError);
		xrtErrorFree(pError);
		return XHTTP_SERVER_BODY_REJECT;
	}
	return XHTTP_SERVER_BODY_BUFFER;
}



/* 在完整请求阶段提交 Upgrade，确保未消费字节只属于下一层协议。 */
static void __xrtWsServerRouterRequest(
	xhttpserver* pServer,
	xhttpconn* pHttp,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	xrt_ws_server_route* pRoute =
		(xrt_ws_server_route*)pData;
	xrt_ws_server_route_connection* pContext;
	xwsserverhandshake Handshake;
	xerror* pError;
	xnetresult Result;
	xerrkind Kind;
	xwsserverroutererror Code;

	(void)pServer;
	if ( !xrtWsServerCheck(
		pRequest, &pRoute->Config.Server, &Handshake
	) ) {
		pError = xrtErrorRef(xrtGetError());
		__xrtWsServerRouterReject(pRoute, pHttp, pError);
		xrtErrorFree(pError);
		return;
	}
	if ( !__xrtWsServerRouterOriginAllowed(
		pRoute, pHttp, pRequest
	) ) {
		__xrtWsServerRouterAuthorizationReject(
			pRoute,
			pHttp,
			"WebSocket request Origin is not allowed"
		);
		return;
	}
	if ( (pRoute->Config.Authorize != NULL) &&
		!pRoute->Config.Authorize(
			pHttp,
			pRequest,
			pParams,
			iParamCount,
			&Handshake,
			pRoute->Config.Data
		) ) {
		__xrtWsServerRouterAuthorizationReject(
			pRoute,
			pHttp,
			"WebSocket request was rejected by the authorization callback"
		);
		return;
	}
	pContext = __xrtWsServerRouteConnectionCreate(pRoute);
	if ( pContext == NULL ) {
		pError = xrtErrorRef(xrtGetError());
		Kind = __xrtWsServerRouterCauseKind(
			pError,
			XERR_MEMORY
		);
		Code = Kind == XERR_MEMORY ?
			XWS_SERVER_ROUTER_ERROR_MEMORY :
			XWS_SERVER_ROUTER_ERROR_STATE;
		__xrtWsServerRouterSetError(
			Kind,
			Code,
			"upgrade-websocket-server-route",
			"WebSocket route Upgrade context could not be retained",
			pError
		);
		xrtErrorFree(pError);
		pError = xrtErrorRef(xrtGetError());
		__xrtWsServerRouterReject(pRoute, pHttp, pError);
		xrtErrorFree(pError);
		return;
	}
	Result = xrtWsUpgradeAccept(
		pHttp,
		&pRoute->Config.Server,
		&Handshake,
		NULL,
		&__xrtWsServerRouteEvents,
		pContext,
		__xrtWsServerRouteUpgradeDone,
		pContext
	);
	if ( Result != XNET_RESULT_OK ) {
		pError = xrtErrorRef(xrtGetError());
		__xrtWsServerRouteConnectionRelease(pContext);
		__xrtWsServerRouterReject(pRoute, pHttp, pError);
		xrtErrorFree(pError);
	}
}



/* 初始化固定 WebSocket 路由的默认配置。 */
XRT_API void xrtWsServerRouteConfigInit(
	xwsserverrouteconfig* pConfig
)
{
	xwsserverrouteconfig Config;

	if ( !__xrtWsServerRouterRangeCheck(
		pConfig,
		sizeof(Config),
		"config-init-websocket-server-route",
		"WebSocket server route configuration range is invalid"
	) ) {
		return;
	}
	memset(&Config, 0, sizeof(Config));
	xrtWsServerConfigInit(&Config.Server);
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 复制固定配置和协议列表，并把路由快照所有权移交给 Router。 */
XRT_API bool xrtWsServerRoute(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	const xwsserverrouteconfig* pConfig
)
{
	xwsserverrouteconfig Config;
	xrt_ws_server_route* pRoute;
	xhttpserverrouteevents Events;
	xerror* pCause;
	xerrkind Kind;
	size_t iAllocation;

	if ( pRouter == NULL ) {
		__xrtWsServerRouterSetError(
			XERR_ARGUMENT,
			XWS_SERVER_ROUTER_ERROR_ARGUMENT,
			"register-websocket-server-route",
			"HTTP server Router is null",
			NULL
		);
		return false;
	}
	if ( !xrtMemRangeValid(Pattern.Data, Pattern.Size) ) {
		__xrtWsServerRouterSetError(
			XERR_ARGUMENT,
			XWS_SERVER_ROUTER_ERROR_ARGUMENT,
			"register-websocket-server-route",
			"WebSocket server route pattern range is invalid",
			NULL
		);
		return false;
	}
	xrtWsServerRouteConfigInit(&Config);
	if ( pConfig != NULL ) {
		if ( !__xrtWsServerRouterRangeCheck(
			pConfig,
			sizeof(Config),
			"register-websocket-server-route",
			"WebSocket server route configuration range is invalid"
		) ) {
			return false;
		}
		memcpy(&Config, pConfig, sizeof(Config));
	}
	if ( !xrtWsServerConfigValid(&Config.Server) ) {
		pCause = xrtErrorRef(xrtGetError());
		__xrtWsServerRouterSetError(
			XERR_VALUE,
			XWS_SERVER_ROUTER_ERROR_CONFIG,
			"register-websocket-server-route",
			"WebSocket server route configuration is invalid",
			pCause
		);
		xrtErrorFree(pCause);
		return false;
	}
	if ( (Config.Origin !=
		  XWS_SERVER_ORIGIN_SAME_HOST_OR_ABSENT) &&
		(Config.Origin != XWS_SERVER_ORIGIN_SAME_HOST) &&
		(Config.Origin != XWS_SERVER_ORIGIN_ANY) ) {
		__xrtWsServerRouterSetError(
			XERR_VALUE,
			XWS_SERVER_ROUTER_ERROR_CONFIG,
			"register-websocket-server-route",
			"WebSocket server route Origin policy is invalid",
			NULL
		);
		return false;
	}
	if ( Config.Server.Protocols.Size >
		(SIZE_MAX - sizeof(*pRoute) - 1u) ) {
		__xrtWsServerRouterSetError(
			XERR_RANGE,
			XWS_SERVER_ROUTER_ERROR_CONFIG,
			"register-websocket-server-route",
			"WebSocket server protocol list is too large",
			NULL
		);
		return false;
	}
	iAllocation = sizeof(*pRoute) +
		Config.Server.Protocols.Size + 1u;
	pRoute = (xrt_ws_server_route*)xrtCalloc(
		1, iAllocation
	);
	if ( pRoute == NULL ) {
		pCause = xrtErrorRef(xrtGetError());
		__xrtWsServerRouterSetError(
			XERR_MEMORY,
			XWS_SERVER_ROUTER_ERROR_MEMORY,
			"register-websocket-server-route",
			"WebSocket server route snapshot allocation failed",
			pCause
		);
		xrtErrorFree(pCause);
		return false;
	}
	pRoute->References = 1;
	pRoute->Config = Config;
	if ( Config.Server.Protocols.Size != 0 ) {
		memcpy(
			pRoute->Storage,
			Config.Server.Protocols.Data,
			Config.Server.Protocols.Size
		);
		pRoute->Config.Server.Protocols.Data =
			pRoute->Storage;
	}
	pRoute->Storage[Config.Server.Protocols.Size] = '\0';

	xrtHttpServerRouteEventsInit(&Events);
	Events.Headers = __xrtWsServerRouterHeaders;
	Events.Request = __xrtWsServerRouterRequest;
	Events.Release = __xrtWsServerRouteRelease;
	Events.Data = pRoute;
	if ( !xrtHttpServerRouteEvents(
		pRouter,
		XRT_STR_LITERAL("*"),
		Pattern,
		&Events
	) ) {
		pCause = xrtErrorRef(xrtGetError());
		Kind = __xrtWsServerRouterCauseKind(
			pCause, XERR_INTERNAL
		);
		xrtFree(pRoute);
		__xrtWsServerRouterSetError(
			Kind,
			XWS_SERVER_ROUTER_ERROR_ROUTE,
			"register-websocket-server-route",
			"HTTP server route registration failed",
			pCause
		);
		xrtErrorFree(pCause);
		return false;
	}
	return true;
}

#endif

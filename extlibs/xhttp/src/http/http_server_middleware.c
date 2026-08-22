#include "../internal/xrt_http_server_router.h"



#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE)

/* 一次请求分派持有借用参数和不可变 Router，不产生每请求堆分配。 */
typedef struct xrt_http_server_middleware_dispatch {
	const xhttpserverrouter* Router;
	xhttpserver* Server;
	xhttpconn* Connection;
	const xhttpserverrequest* Request;
	const xrt_http_server_route_match* Match;
	const xhttpserverevents* Events;
	bool Failed;
} xrt_http_server_middleware_dispatch;



/* Next 是严格栈内令牌，记录下一层索引和单次调用状态。 */
struct xhttpservernext {
	xrt_http_server_middleware_dispatch* Dispatch;
	size_t Index;
	bool Called;
	bool Active;
};



/* 发布稳定的中间件错误并保留可选原因链。 */
static void __xrtHttpServerMiddlewareSetError(
	xerrkind Kind,
	xhttpservermiddlewareerror Code,
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
	Desc.Domain = "xrt.http.server.middleware";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 从指定索引执行中间件；末端进入唯一 Router 终端分派。 */
static bool __xrtHttpServerMiddlewareRun(
	xrt_http_server_middleware_dispatch* pDispatch,
	size_t iIndex
)
{
	const xrt_http_server_middleware_entry* pEntry;
	xhttpservernext Next;
	bool bResult;

	if ( iIndex == pDispatch->Router->MiddlewareCount ) {
		return __xrtHttpServerRouterDispatchTerminal(
			pDispatch->Router,
			pDispatch->Server,
			pDispatch->Connection,
			pDispatch->Request,
			pDispatch->Match,
			pDispatch->Events
		);
	}
	pEntry = &pDispatch->Router->Middleware[iIndex];
	memset(&Next, 0, sizeof(Next));
	Next.Dispatch = pDispatch;
	Next.Index = iIndex + 1u;
	Next.Active = true;
	bResult = pEntry->Handle(
		pDispatch->Server,
		pDispatch->Connection,
		pDispatch->Request,
		pDispatch->Match->Count != 0 ?
			pDispatch->Match->Params : NULL,
		pDispatch->Match->Count,
		&Next,
		pEntry->Data
	);
	Next.Active = false;
	if ( pDispatch->Failed ) {
		return false;
	}
	if ( !bResult ) {
		const xerror* pCause = xrtGetError();

		__xrtHttpServerMiddlewareSetError(
			xrtErrorKind(pCause) != XERR_NONE ?
				xrtErrorKind(pCause) : XERR_STATE,
			XHTTP_SERVER_MIDDLEWARE_ERROR_CALLBACK,
			"dispatch-http-server-middleware",
			"HTTP server middleware callback failed",
			pCause
		);
		pDispatch->Failed = true;
		return false;
	}
	return true;
}



/* 继续下一层一次；重复、过期或空 Next 会固定当前请求失败。 */
XRT_API bool xrtHttpServerNext(xhttpservernext* pNext)
{
	if ( (pNext == NULL) || !pNext->Active ||
		(pNext->Dispatch == NULL) || pNext->Called ) {
		__xrtHttpServerMiddlewareSetError(
			XERR_STATE,
			XHTTP_SERVER_MIDDLEWARE_ERROR_NEXT,
			"continue-http-server-middleware",
			"HTTP server middleware Next is null, expired or already called",
			NULL
		);
		if ( (pNext != NULL) &&
			(pNext->Dispatch != NULL) ) {
			pNext->Dispatch->Failed = true;
		}
		return false;
	}
	pNext->Called = true;
	if ( !__xrtHttpServerMiddlewareRun(
		pNext->Dispatch, pNext->Index
	) ) {
		pNext->Dispatch->Failed = true;
		return false;
	}
	return true;
}



/* 为新记录增长回调表，最大数量与 Router 的显式路由限额一致。 */
static bool __xrtHttpServerMiddlewareReserve(
	xhttpserverrouter* pRouter
)
{
	size_t iRequired = pRouter->MiddlewareCount + 1u;
	size_t iCapacity;
	xrt_http_server_middleware_entry* pEntries;

	if ( iRequired > pRouter->MaxRoutes ) {
		__xrtHttpServerMiddlewareSetError(
			XERR_RANGE,
			XHTTP_SERVER_MIDDLEWARE_ERROR_LIMIT,
			"register-http-server-middleware",
			"HTTP server middleware count exceeds the Router limit",
			NULL
		);
		return false;
	}
	if ( iRequired <= pRouter->MiddlewareCapacity ) {
		return true;
	}
	iCapacity = pRouter->MiddlewareCapacity != 0 ?
		pRouter->MiddlewareCapacity :
		(pRouter->MaxRoutes < 4u ? pRouter->MaxRoutes : 4u);
	while ( iCapacity < iRequired ) {
		size_t iNext = iCapacity > (pRouter->MaxRoutes / 2u) ?
			pRouter->MaxRoutes : iCapacity * 2u;

		if ( iNext <= iCapacity ) {
			iCapacity = iRequired;
			break;
		}
		iCapacity = iNext;
	}
	pEntries = (xrt_http_server_middleware_entry*)xrtRealloc(
		pRouter->Middleware,
		iCapacity * sizeof(*pEntries)
	);
	if ( pEntries == NULL ) {
		const xerror* pCause = xrtGetError();

		__xrtHttpServerMiddlewareSetError(
			XERR_MEMORY,
			XHTTP_SERVER_MIDDLEWARE_ERROR_MEMORY,
			"register-http-server-middleware",
			"HTTP server middleware table allocation failed",
			pCause
		);
		return false;
	}
	pRouter->Middleware = pEntries;
	pRouter->MiddlewareCapacity = iCapacity;
	return true;
}



/* 注册一层可选拥有用户数据的同步中间件。 */
XRT_API bool xrtHttpServerUseOwned(
	xhttpserverrouter* pRouter,
	xhttpservermiddlewareproc pMiddleware,
	ptr pData,
	xhttpserverrouterreleaseproc pRelease
)
{
	xrt_http_server_middleware_entry* pEntry;

	if ( (pRouter == NULL) || (pMiddleware == NULL) ) {
		__xrtHttpServerMiddlewareSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_MIDDLEWARE_ERROR_ARGUMENT,
			"register-http-server-middleware",
			"HTTP server Router or middleware callback is null",
			NULL
		);
		return false;
	}
	if ( xrtHttpServerRouterFrozen(pRouter) ) {
		__xrtHttpServerMiddlewareSetError(
			XERR_STATE,
			XHTTP_SERVER_MIDDLEWARE_ERROR_STATE,
			"register-http-server-middleware",
			"HTTP server Router is already frozen",
			NULL
		);
		return false;
	}
	if ( !__xrtHttpServerMiddlewareReserve(pRouter) ) {
		return false;
	}
	pEntry = &pRouter->Middleware[pRouter->MiddlewareCount];
	pEntry->Handle = pMiddleware;
	pEntry->Release = pRelease;
	pEntry->Data = pData;
	pRouter->MiddlewareCount++;
	return true;
}



/* 注册由调用方管理用户数据的常用同步中间件。 */
XRT_API bool xrtHttpServerUse(
	xhttpserverrouter* pRouter,
	xhttpservermiddlewareproc pMiddleware,
	ptr pData
)
{
	return xrtHttpServerUseOwned(
		pRouter, pMiddleware, pData, NULL
	);
}



/* 返回冻结前后都稳定可读的中间件数量。 */
XRT_API size_t xrtHttpServerMiddlewareCount(
	const xhttpserverrouter* pRouter
)
{
	return pRouter != NULL ?
		pRouter->MiddlewareCount : 0;
}



/* 从最外层开始执行一次完整请求中间件链。 */
bool __xrtHttpServerMiddlewareDispatch(
	const xhttpserverrouter* pRouter,
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xrt_http_server_route_match* pMatch,
	const xhttpserverevents* pEvents
)
{
	xrt_http_server_middleware_dispatch Dispatch;

	memset(&Dispatch, 0, sizeof(Dispatch));
	Dispatch.Router = pRouter;
	Dispatch.Server = pServer;
	Dispatch.Connection = pConnection;
	Dispatch.Request = pRequest;
	Dispatch.Match = pMatch;
	Dispatch.Events = pEvents;
	return __xrtHttpServerMiddlewareRun(&Dispatch, 0u);
}



/* 逆序释放拥有型用户数据，再释放中间件记录数组。 */
void __xrtHttpServerMiddlewareClear(xhttpserverrouter* pRouter)
{
	size_t i;

	if ( pRouter == NULL ) {
		return;
	}
	for ( i = pRouter->MiddlewareCount; i > 0; i-- ) {
		xrt_http_server_middleware_entry* pEntry =
			&pRouter->Middleware[i - 1u];

		if ( pEntry->Release != NULL ) {
			pEntry->Release(pEntry->Data);
		}
	}
	xrtFree(pRouter->Middleware);
	pRouter->Middleware = NULL;
	pRouter->MiddlewareCount = 0;
	pRouter->MiddlewareCapacity = 0;
}

#endif

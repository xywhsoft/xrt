#include "../internal/xrt_future_bridge.h"
#include "../internal/xrt_http_server_runtime.h"
#include "../internal/xrt_websocket_http_future.h"



#if defined(XRT_FEATURE_WEBSOCKET_SERVER_FUTURE)

/*
	服务端桥在成功后原位转为 Future 拥有的结果。
	独立 HTTP 引用覆盖取消线程与 Upgrade 完成回调的竞态。
*/
typedef struct xrt_ws_server_future {
	xwsopenresult Result;
	xrt_future_bridge Bridge;
	xhttpconn* Http;
	xatomic32 Cancelled;
} xrt_ws_server_future;



/* Future 的拥有值析构只释放一个连接建立结果引用。 */
static void __xrtWsServerFutureFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtWsOpenResultDestroy((xwsopenresult*)pValue);
}



/* 标记 Future 取消并异常关闭尚未完成的 HTTP Upgrade。 */
static void __xrtWsServerFutureCancel(ptr pData)
{
	xrt_ws_server_future* pContext =
		(xrt_ws_server_future*)pData;

	xrtAtomic32Store(
		&pContext->Cancelled,
		1,
		XMEMORY_RELEASE
	);
	(void)xrtHttpConnAbort(pContext->Http);
}



/* 接收 HTTP Upgrade 唯一终态并完成服务端 Future。 */
static void __xrtWsServerFutureDone(
	xhttpconn* pHttp,
	xnetresult Result,
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	xrt_ws_server_future* pContext =
		(xrt_ws_server_future*)pData;
	xpromise* pPromise = pContext->Bridge.Promise;
	xhttpconn* pHeld = pContext->Http;
	xerror* pFallback = NULL;
	xerror* pFailure = NULL;
	bool bCancelled;
	bool bReady;
	bool bResolved = false;
	bool bSuccess;

	(void)pHttp;
	bReady = __xrtFutureBridgeWait(&pContext->Bridge);
	__xrtFutureBridgeUnwatch(&pContext->Bridge);
	bCancelled = xrtAtomic32Load(
		&pContext->Cancelled,
		XMEMORY_ACQUIRE
	) != 0;
	bSuccess = bReady && !bCancelled &&
		(Result == XNET_RESULT_OK) &&
		(pConnection != NULL);
	if ( !bReady ) {
		__xrtWsOpenConnectionDestroy(pConnection);
		xrtHttpConnDestroy(pHeld);
		xrtPromiseDestroy(pPromise);
		xrtFree(pContext);
		return;
	}
	if ( !bSuccess && !bCancelled &&
		(Result != XNET_RESULT_CANCELLED) ) {
		if ( pError == NULL ) {
			pFallback = __xrtWsOpenErrorCreate(
				XERR_INTERNAL,
				"complete-websocket-upgrade",
				Result == XNET_RESULT_OK ?
					"WebSocket Upgrade succeeded without a connection" :
					"WebSocket Upgrade failed without an error"
			);
			pError = pFallback != NULL ?
				pFallback : xrtGetError();
		}
		pFailure = xrtErrorRef(pError);
	}
	if ( bSuccess ) {
		xrtAtomicPtrStore(
			&pContext->Result.Connection,
			pConnection,
			XMEMORY_RELEASE
		);
	} else {
		__xrtWsOpenConnectionDestroy(pConnection);
		xrtFree(pContext);
	}
	xrtHttpConnDestroy(pHeld);
	if ( bSuccess ) {
		bResolved = xrtPromiseResolveOwned(
			pPromise,
			&pContext->Result,
			__xrtWsServerFutureFree,
			NULL
		);
		if ( !bResolved ) {
			xrtWsOpenResultDestroy(&pContext->Result);
		}
	} else if ( bCancelled ||
		(Result == XNET_RESULT_CANCELLED) ) {
		(void)xrtPromiseCancel(pPromise);
	} else if ( pFailure != NULL ) {
		(void)xrtPromiseReject(pPromise, pFailure);
	} else {
		(void)xrtPromiseClose(pPromise);
	}
	xrtPromiseDestroy(pPromise);
	xrtErrorFree(pFailure);
	xrtErrorFree(pFallback);
}



/* 验证并提交服务端 WebSocket Upgrade Future。 */
XRT_API xfuture* xrtWsUpgradeAsync(
	xhttpconn* pHttp,
	const xwsserverconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
)
{
	xrt_ws_server_future* pContext;
	xwsserverconfig Config;
	xwsconnevents Events;
	xfuture* pFuture;
	xerror* pError;

	if ( !__xrtWsServerConfigSnapshot(
		&Config,
		pConfig,
		"upgrade-websocket-async"
	) || !__xrtWsConnEventsSnapshot(
		&Events,
		pEvents,
		"upgrade-websocket-async"
	) || !__xrtWsHttpRangeCheck(
		pHttp,
		sizeof(*pHttp),
		"upgrade-websocket-async",
		"HTTP server connection range is invalid"
	) ) {
		return NULL;
	}
	pContext = (xrt_ws_server_future*)xrtCalloc(
		1,
		sizeof(*pContext)
	);
	if ( pContext == NULL ) {
		return NULL;
	}
	pContext->Result.References = 1;
	xrtAtomicPtrInit(&pContext->Result.Connection, NULL);
	#if defined(XRT_FEATURE_WEBSOCKET_CLIENT_FUTURE)
		xrtAtomicPtrInit(&pContext->Result.Response, NULL);
	#endif
	xrtAtomic32Init(&pContext->Cancelled, 0);
	pContext->Http = xrtHttpConnRef(pHttp);
	if ( pContext->Http == NULL ) {
		xrtFree(pContext);
		return NULL;
	}
	pFuture = __xrtFutureBridgeCreate(
		&pContext->Bridge,
		NULL
	);
	if ( pFuture == NULL ) {
		xrtHttpConnDestroy(pContext->Http);
		xrtFree(pContext);
		return NULL;
	}
	if ( xrtWsUpgrade(
		pHttp,
		&Config,
		&Events,
		pData,
		__xrtWsServerFutureDone,
		pContext
	) != XNET_RESULT_OK ) {
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(pContext->Bridge.Promise);
		xrtHttpConnDestroy(pContext->Http);
		xrtFree(pContext);
		return NULL;
	}
	if ( !__xrtFutureBridgeWatch(
		&pContext->Bridge,
		__xrtWsServerFutureCancel,
		pContext
	) ) {
		pError = xrtTakeError();
		__xrtFutureBridgeFail(&pContext->Bridge);
		(void)xrtHttpConnAbort(pContext->Http);
		xrtFutureDestroy(pFuture);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return NULL;
	}
	__xrtFutureBridgeReady(&pContext->Bridge);
	return pFuture;
}

#endif

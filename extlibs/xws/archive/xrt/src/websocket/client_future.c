#include "../internal/xrt_http_client_runtime.h"
#include "../internal/xrt_future_bridge.h"
#include "../internal/xrt_websocket_http_future.h"



#if defined(XRT_FEATURE_WEBSOCKET_CLIENT_FUTURE)

/*
	桥接上下文在成功后原位转为 Future 拥有的结果。
	Result 必须保持首成员，使结果析构能够释放整块存储。
*/
typedef struct xrt_ws_client_future {
	xwsopenresult Result;
	xrt_future_bridge Bridge;
	xhttpcall* Call;
} xrt_ws_client_future;



/* 验证 Future 入口并安全取得配置中的父取消令牌。 */
static bool __xrtWsClientFutureInputs(
	xhttpclient* pClient,
	const xwsclientconfig* pConfig,
	const xwsconnevents* pEvents,
	cstr sOperation,
	xwsclientconfig* pConfigOutput,
	xwsconnevents* pEventsOutput,
	xcancel** ppParent
)
{
	*ppParent = NULL;
	if ( !__xrtWsClientConfigSnapshot(
		pConfigOutput,
		pConfig,
		sOperation
	) || !__xrtWsConnEventsSnapshot(
		pEventsOutput,
		pEvents,
		sOperation
	) || !__xrtWsHttpRangeCheck(
		pClient,
		sizeof(*pClient),
		sOperation,
		"HTTP client range is invalid"
	) ) {
		return false;
	}
	*ppParent = pConfigOutput->Http.Cancel;
	return true;
}



/* Future 的拥有值析构只释放一个连接建立结果引用。 */
static void __xrtWsClientFutureFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtWsOpenResultDestroy((xwsopenresult*)pValue);
}



/* 把 Future 的协作取消转发给完整 HTTP Call。 */
static void __xrtWsClientFutureCancel(ptr pData)
{
	xrt_ws_client_future* pContext =
		(xrt_ws_client_future*)pData;

	(void)xrtHttpCallCancel(pContext->Call);
}



/* 回收回调转移但没有进入成功结果的全部对象。 */
static void __xrtWsClientFutureObjectsDestroy(
	xwsconn* pConnection,
	xhttpresponse* pResponse
)
{
	__xrtWsOpenConnectionDestroy(pConnection);
	xrtHttpResponseDestroy(pResponse);
}



/* 把成功回调转为 Future 拥有的连接建立结果。 */
static void __xrtWsClientFutureResolve(
	xrt_ws_client_future* pContext,
	xwsconn* pConnection,
	xhttpresponse* pResponse
)
{
	xpromise* pPromise = pContext->Bridge.Promise;
	xhttpcall* pCall = pContext->Call;
	bool bResolved;

	xrtAtomicPtrStore(
		&pContext->Result.Connection,
		pConnection,
		XMEMORY_RELEASE
	);
	xrtAtomicPtrStore(
		&pContext->Result.Response,
		pResponse,
		XMEMORY_RELEASE
	);
	xrtHttpCallDestroy(pCall);
	bResolved = xrtPromiseResolveOwned(
		pPromise,
		&pContext->Result,
		__xrtWsClientFutureFree,
		NULL
	);
	xrtPromiseDestroy(pPromise);
	if ( !bResolved ) {
		xrtWsOpenResultDestroy(&pContext->Result);
	}
}



/* 把失败或取消回调转为对应 Future 终态并回收响应。 */
static void __xrtWsClientFutureReject(
	xrt_ws_client_future* pContext,
	xnetresult Result,
	xwsconn* pConnection,
	xhttpresponse* pResponse,
	const xerror* pError
)
{
	xpromise* pPromise = pContext->Bridge.Promise;
	xhttpcall* pCall = pContext->Call;
	xerror* pFallback = NULL;
	xerror* pFailure = NULL;

	if ( Result != XNET_RESULT_CANCELLED ) {
		if ( pError == NULL ) {
			pFallback = __xrtWsOpenErrorCreate(
				XERR_INTERNAL,
				"complete-websocket-connect",
				"WebSocket connect failed without an error"
			);
			pError = pFallback != NULL ?
				pFallback : xrtGetError();
		}
		pFailure = xrtErrorRef(pError);
	}
	__xrtWsClientFutureObjectsDestroy(
		pConnection,
		pResponse
	);
	xrtHttpCallDestroy(pCall);
	xrtFree(pContext);
	if ( Result == XNET_RESULT_CANCELLED ) {
		(void)xrtPromiseCancel(pPromise);
	} else if ( pFailure != NULL ) {
		(void)xrtPromiseReject(pPromise, pFailure);
	} else {
		(void)xrtPromiseClose(pPromise);
	}
	xrtErrorFree(pFailure);
	xrtErrorFree(pFallback);
	xrtPromiseDestroy(pPromise);
}



/* 接收 WebSocket 客户端唯一终态并完成 Future。 */
static void __xrtWsClientFutureDone(
	xhttpcall* pCall,
	xnetresult Result,
	xwsconn* pConnection,
	xhttpresponse* pResponse,
	const xerror* pError,
	ptr pData
)
{
	xrt_ws_client_future* pContext =
		(xrt_ws_client_future*)pData;
	bool bReady;

	(void)pCall;
	bReady = __xrtFutureBridgeWait(&pContext->Bridge);
	__xrtFutureBridgeUnwatch(&pContext->Bridge);
	if ( !bReady ) {
		__xrtWsClientFutureObjectsDestroy(
			pConnection,
			pResponse
		);
		xrtHttpCallDestroy(pContext->Call);
		xrtPromiseDestroy(pContext->Bridge.Promise);
		xrtFree(pContext);
		return;
	}
	if ( (Result == XNET_RESULT_OK) &&
		(pConnection != NULL) &&
		(pResponse != NULL) ) {
		__xrtWsClientFutureResolve(
			pContext,
			pConnection,
			pResponse
		);
	} else if ( Result == XNET_RESULT_OK ) {
		xerror* pContractError =
			__xrtWsOpenErrorCreate(
				XERR_INTERNAL,
				"complete-websocket-connect",
				"WebSocket connect succeeded without a connection or response"
			);

		__xrtWsClientFutureReject(
			pContext,
			XNET_RESULT_ERROR,
			pConnection,
			pResponse,
			pContractError != NULL ?
				pContractError : xrtGetError()
		);
		xrtErrorFree(pContractError);
	} else {
		__xrtWsClientFutureReject(
			pContext,
			Result,
			pConnection,
			pResponse,
			pError
		);
	}
}



/* 分配 Promise、连接建立结果和统一取消桥。 */
static xrt_ws_client_future* __xrtWsClientFutureCreate(
	xcancel* pParent,
	xfuture** ppFuture
)
{
	xrt_ws_client_future* pContext;

	pContext = (xrt_ws_client_future*)xrtCalloc(
		1,
		sizeof(*pContext)
	);
	if ( pContext == NULL ) {
		return NULL;
	}
	pContext->Result.References = 1;
	xrtAtomicPtrInit(&pContext->Result.Connection, NULL);
	xrtAtomicPtrInit(&pContext->Result.Response, NULL);
	*ppFuture = __xrtFutureBridgeCreate(
		&pContext->Bridge,
		pParent
	);
	if ( *ppFuture == NULL ) {
		xrtFree(pContext);
		return NULL;
	}
	return pContext;
}



/* 完成底层提交后的取消观察器安装。 */
static bool __xrtWsClientFutureWatch(
	xrt_ws_client_future* pContext,
	xfuture* pFuture
)
{
	xerror* pError;

	if ( __xrtFutureBridgeWatch(
		&pContext->Bridge,
		__xrtWsClientFutureCancel,
		pContext
	) ) {
		__xrtFutureBridgeReady(&pContext->Bridge);
		return true;
	}
	pError = xrtTakeError();
	__xrtFutureBridgeFail(&pContext->Bridge);
	(void)xrtHttpCallCancel(pContext->Call);
	xrtFutureDestroy(pFuture);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return false;
}



/* 使用 URL 提交 WebSocket Client Future。 */
XRT_API xfuture* xrtWsConnectAsync(
	xhttpclient* pClient,
	xstrview Url,
	const xwsclientconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
)
{
	xrt_ws_client_future* pContext;
	xwsclientconfig Config;
	xwsconnevents Events;
	xcancel* pParent;
	xfuture* pFuture;

	if ( !__xrtWsClientFutureInputs(
		pClient,
		pConfig,
		pEvents,
		"connect-websocket-async",
		&Config,
		&Events,
		&pParent
	) ) {
		return NULL;
	}
	pContext = __xrtWsClientFutureCreate(
		pParent,
		&pFuture
	);
	if ( pContext == NULL ) {
		return NULL;
	}
	pContext->Call = xrtWsConnect(
		pClient,
		Url,
		&Config,
		&Events,
		pData,
		__xrtWsClientFutureDone,
		pContext
	);
	if ( pContext->Call == NULL ) {
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(pContext->Bridge.Promise);
		xrtFree(pContext);
		return NULL;
	}
	if ( !__xrtWsClientFutureWatch(
		pContext,
		pFuture
	) ) {
		return NULL;
	}
	return pFuture;
}



/* 使用自定义 GET 请求提交 WebSocket Client Future。 */
XRT_API xfuture* xrtWsConnectRequestAsync(
	xhttpclient* pClient,
	const xhttprequest* pRequest,
	const xwsclientconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
)
{
	xrt_ws_client_future* pContext;
	xwsclientconfig Config;
	xwsconnevents Events;
	xcancel* pParent;
	xfuture* pFuture;

	if ( !__xrtWsClientFutureInputs(
		pClient,
		pConfig,
		pEvents,
		"connect-websocket-request-async",
		&Config,
		&Events,
		&pParent
	) ) {
		return NULL;
	}
	if ( !__xrtWsHttpRangeCheck(
		pRequest,
		sizeof(*pRequest),
		"connect-websocket-request-async",
		"HTTP request range is invalid"
	) ) {
		return NULL;
	}
	pContext = __xrtWsClientFutureCreate(
		pParent,
		&pFuture
	);
	if ( pContext == NULL ) {
		return NULL;
	}
	pContext->Call = xrtWsConnectRequest(
		pClient,
		pRequest,
		&Config,
		&Events,
		pData,
		__xrtWsClientFutureDone,
		pContext
	);
	if ( pContext->Call == NULL ) {
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(pContext->Bridge.Promise);
		xrtFree(pContext);
		return NULL;
	}
	if ( !__xrtWsClientFutureWatch(
		pContext,
		pFuture
	) ) {
		return NULL;
	}
	return pFuture;
}



/* 验证宿主线程可以阻塞等待指定 Client。 */
static bool __xrtWsConnectCanWait(xhttpclient* pClient)
{
	if ( !__xrtWsHttpRangeCheck(
		pClient,
		sizeof(*pClient),
		"wait-websocket-connect",
		"HTTP client range is invalid"
	) ) {
		return false;
	}
	if ( xrtNetEngineCurrent(pClient->Engine) != NULL ) {
		xerror* pError = __xrtWsOpenErrorCreate(
			XERR_STATE,
			"wait-websocket-connect",
			"network Worker cannot block on its own WebSocket connect"
		);

		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return false;
	}
	return true;
}



/* 阻塞等待 Future，并保留一个成功结果引用。 */
static xwsopenresult* __xrtWsConnectWait(
	xfuture* pFuture
)
{
	xfuturestate State;
	xwsopenresult* pResult = NULL;

	if ( pFuture == NULL ) {
		return NULL;
	}
	if ( xrtFutureWait(pFuture) != XWAIT_OK ) {
		xerror* pError = xrtTakeError();

		(void)xrtFutureCancel(pFuture);
		xrtFutureDestroy(pFuture);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return NULL;
	}
	State = xrtFutureState(pFuture);
	if ( State == XFUTURE_RESOLVED ) {
		pResult = xrtWsOpenResultRef(
			(xwsopenresult*)xrtFutureValue(pFuture)
		);
	} else if ( State == XFUTURE_FAILED ) {
		(void)xrtFutureValue(pFuture);
	} else {
		xerror* pError = __xrtWsOpenErrorCreate(
			State == XFUTURE_CANCELLED ?
				XERR_CANCELLED : XERR_INTERNAL,
			"wait-websocket-connect",
			State == XFUTURE_CANCELLED ?
				"WebSocket connect was cancelled while waiting" :
				"WebSocket Future completed without a usable result"
		);

		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
	}
	xrtFutureDestroy(pFuture);
	return pResult;
}



/* 在宿主线程使用 URL 阻塞建立 WebSocket。 */
XRT_API xwsopenresult* xrtWsConnectSync(
	xhttpclient* pClient,
	xstrview Url,
	const xwsclientconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
)
{
	if ( !__xrtWsConnectCanWait(pClient) ) {
		return NULL;
	}
	return __xrtWsConnectWait(
		xrtWsConnectAsync(
			pClient,
			Url,
			pConfig,
			pEvents,
			pData
		)
	);
}



/* 在宿主线程使用自定义 GET 请求阻塞建立 WebSocket。 */
XRT_API xwsopenresult* xrtWsConnectRequestSync(
	xhttpclient* pClient,
	const xhttprequest* pRequest,
	const xwsclientconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
)
{
	if ( !__xrtWsConnectCanWait(pClient) ) {
		return NULL;
	}
	return __xrtWsConnectWait(
		xrtWsConnectRequestAsync(
			pClient,
			pRequest,
			pConfig,
			pEvents,
			pData
		)
	);
}

#endif

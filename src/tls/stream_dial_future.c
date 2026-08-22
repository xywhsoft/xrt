#include "../internal/xrt_tls_stream.h"
#include <xrt/future_bridge.h>



#if defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE)

/* TLS Dial Future 桥接一个受管 TLS Dial 与一个公开 Future。 */
typedef struct xrt_tls_dial_future {
	xfuturebridge Bridge;
	xtlsdial* Dial;
} xrt_tls_dial_future;



/* Future 成功值只持有一个 TLS Stream 调用方引用。 */
static void __xrtTlsDialFutureFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtTlsStreamDestroy((xtlsstream*)pValue);
}



/* Future 的协作取消请求转发给 DNS、TCP 或 TLS 当前有效阶段。 */
static void __xrtTlsDialFutureCancel(ptr pData)
{
	xrt_tls_dial_future* pContext =
		(xrt_tls_dial_future*)pData;

	(void)xrtTlsDialCancel(pContext->Dial);
}



/* 把不可能出现的空成功结果转换为结构化 TLS 内部错误。 */
static xerror* __xrtTlsDialFutureInvalidError(void)
{
	xerror* pError;

	__xrtTlsError(
		XERR_INTERNAL,
		XTLS_ERROR_INTERNAL,
		"complete-tls-dial-future",
		"TLS dial reported success without a stream",
		SIZE_MAX
	);
	pError = xrtTakeError();
	return pError;
}



/* 把 TLS Dial 唯一终态转发到 Promise，并处理装配失败竞态。 */
static void __xrtTlsDialFutureDone(
	xtlsdial* pDial,
	xnetresult Result,
	xtlsstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	xrt_tls_dial_future* pContext =
		(xrt_tls_dial_future*)pData;
	xpromise* pPromise = xrtFutureBridgePromise(&pContext->Bridge);
	xtlsdial* pHeld = pContext->Dial;
	xerror* pFailure = NULL;
	bool bReady;

	(void)pDial;
	bReady = xrtFutureBridgeWait(&pContext->Bridge);
	xrtFutureBridgeUnwatch(&pContext->Bridge);
	if ( (Result != XNET_RESULT_OK) && (pError != NULL) ) {
		pFailure = xrtErrorRef(pError);
	} else if ( (Result == XNET_RESULT_OK) && (pStream == NULL) ) {
		pFailure = __xrtTlsDialFutureInvalidError();
	}
	xrtTlsDialDestroy(pHeld);
	xrtFree(pContext);
	if ( !bReady ) {
		if ( pStream != NULL ) {
			(void)xrtTlsStreamAbort(pStream);
			xrtTlsStreamDestroy(pStream);
		}
	} else if ( (Result == XNET_RESULT_OK) &&
		(pStream != NULL) ) {
		if ( !xrtPromiseResolveOwned(
			pPromise,
			pStream,
			__xrtTlsDialFutureFree,
			NULL
		) ) {
			(void)xrtTlsStreamAbort(pStream);
			xrtTlsStreamDestroy(pStream);
		}
	} else if ( Result == XNET_RESULT_CANCELLED ) {
		(void)xrtPromiseCancel(pPromise);
	} else if ( pFailure != NULL ) {
		(void)xrtPromiseReject(pPromise, pFailure);
	} else {
		(void)xrtPromiseClose(pPromise);
	}
	xrtErrorFree(pFailure);
	xrtPromiseDestroy(pPromise);
}



/* 创建受管 TLS Dial、Promise 和双向取消桥接。 */
XRT_API xfuture* xrtTlsDialAsync(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	cstr sHost,
	uint16 iPort,
	const xtlsclientconfig* pTls,
	const xtlsdialconfig* pConfig,
	const xtlsstreamevents* pStreamEvents,
	ptr pStreamData
)
{
	xrt_tls_dial_future* pContext;
	xfuture* pFuture;
	xerror* pError;

	pContext = (xrt_tls_dial_future*)xrtCalloc(
		1,
		sizeof(*pContext)
	);
	if ( pContext == NULL ) {
		return NULL;
	}
	pFuture = xrtFutureBridgeCreate(&pContext->Bridge, NULL);
	if ( pFuture == NULL ) {
		xrtFree(pContext);
		return NULL;
	}
	pContext->Dial = xrtTlsDial(
		pEngine,
		pResolver,
		sHost,
		iPort,
		pTls,
		pConfig,
		pStreamEvents,
		pStreamData,
		__xrtTlsDialFutureDone,
		pContext
	);
	if ( pContext->Dial == NULL ) {
		pError = xrtTakeError();
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(xrtFutureBridgePromise(&pContext->Bridge));
		xrtFree(pContext);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return NULL;
	}
	if ( !xrtFutureBridgeWatch(
		&pContext->Bridge,
		__xrtTlsDialFutureCancel,
		pContext
	) ) {
		pError = xrtTakeError();
		(void)xrtFutureBridgeFail(&pContext->Bridge);
		(void)xrtTlsDialCancel(pContext->Dial);
		xrtFutureDestroy(pFuture);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return NULL;
	}
	(void)xrtFutureBridgeReady(&pContext->Bridge);
	return pFuture;
}

#endif

#include "../internal/xrt_tcp.h"
#include <xrt/future_bridge.h>



#if defined(XRT_FEATURE_NET_TCP_DIAL_FUTURE)

/* TCP Dial Future 桥接一个底层 Dial 与一个公开 Future。 */
typedef struct xrt_net_dial_future {
	xfuturebridge Bridge;
	volatile int32 References;
	xnetdial* Dial;
} xrt_net_dial_future;



/* Future 成功值只持有一个 Stream 调用方引用。 */
static void __xrtNetDialFutureFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtNetStreamDestroy((xnetstream*)pValue);
}



/* Future 的协作取消请求转发给整个托管连接，而不是单个候选。 */
static void __xrtNetDialFutureCancel(ptr pData)
{
	xrt_net_dial_future* pContext =
		(xrt_net_dial_future*)pData;

	(void)xrtNetDialCancel(pContext->Dial);
}



/* 把 Dial 唯一终态转发到 Promise，并处理装配失败或取消竞态。 */
static void __xrtNetDialFutureDone(
	xnetdial* pDial,
	xnetresult Result,
	xnetstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	xrt_net_dial_future* pContext =
		(xrt_net_dial_future*)pData;
	xpromise* pPromise = xrtFutureBridgePromise(&pContext->Bridge);
	xnetdial* pHeld = pContext->Dial;
	xerror* pFailure = NULL;
	bool bReady;

	(void)pDial;
	bReady = xrtFutureBridgeWait(&pContext->Bridge);
	xrtFutureBridgeUnwatch(&pContext->Bridge);
	if ( (Result != XNET_RESULT_OK) && (pError != NULL) ) {
		pFailure = xrtErrorRef(pError);
	} else if ( (Result == XNET_RESULT_OK) && (pStream == NULL) ) {
		__xrtNetSetError(
			XERR_INTERNAL,
			XNET_ERROR_DIAL_CONNECT,
			"complete-tcp-dial-future",
			"TCP dial reported success without a stream",
			0
		);
		pFailure = xrtTakeError();
	}
	xrtNetDialDestroy(pHeld);
	if ( xrtRefRelease(&pContext->References) == 0 ) {
		xrtFree(pContext);
	}
	if ( !bReady ) {
		if ( pStream != NULL ) {
			(void)xrtNetStreamAbort(pStream);
			xrtNetStreamDestroy(pStream);
		}
	} else if ( (Result == XNET_RESULT_OK) &&
		(pStream != NULL) ) {
		if ( !xrtPromiseResolveOwned(
			pPromise,
			pStream,
			__xrtNetDialFutureFree,
			NULL
		) ) {
			(void)xrtNetStreamAbort(pStream);
			xrtNetStreamDestroy(pStream);
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



/* 创建 Dial、Promise 和双向取消桥接。 */
XRT_API xfuture* xrtNetDialAsync(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	cstr sHost,
	uint16 iPort,
	const xnetdialconfig* pConfig,
	const xnetstreamevents* pStreamEvents,
	ptr pStreamData
)
{
	xrt_net_dial_future* pContext;
	xfuture* pFuture;
	xerror* pError;

	pContext = (xrt_net_dial_future*)xrtCalloc(
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
	pContext->References = 2;
	pContext->Dial = xrtNetDial(
		pEngine,
		pResolver,
		sHost,
		iPort,
		pConfig,
		pStreamEvents,
		pStreamData,
		__xrtNetDialFutureDone,
		pContext
	);
	if ( pContext->Dial == NULL ) {
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(xrtFutureBridgePromise(&pContext->Bridge));
		xrtFree(pContext);
		return NULL;
	}
	if ( !xrtFutureBridgeWatch(
		&pContext->Bridge,
		__xrtNetDialFutureCancel,
		pContext
	) ) {
		pError = xrtTakeError();
		(void)xrtFutureBridgeFail(&pContext->Bridge);
		(void)xrtNetDialCancel(pContext->Dial);
		xrtFutureDestroy(pFuture);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		if ( xrtRefRelease(&pContext->References) == 0 ) {
			xrtFree(pContext);
		}
		return NULL;
	}
	(void)xrtFutureBridgeReady(&pContext->Bridge);
	if ( xrtRefRelease(&pContext->References) == 0 ) {
		xrtFree(pContext);
	}
	return pFuture;
}

#endif

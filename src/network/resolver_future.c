#include "../internal/xrt_net_resolver.h"
#include <xrt/future_bridge.h>



#if defined(XRT_FEATURE_NET_RESOLVER_FUTURE)

/* Resolver Future 桥接一个底层解析操作与一个公开 Future。 */
typedef struct xrt_net_resolver_future {
	xfuturebridge Bridge;
	xnetresolveop* Operation;
} xrt_net_resolver_future;



/* Future 成功值持有一份地址列表引用。 */
static void __xrtNetResolverFutureFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtNetAddrListDestroy((xnetaddrlist*)pValue);
}



/* Future 的协作取消请求转发给底层 Resolver 操作。 */
static void __xrtNetResolverFutureCancel(ptr pData)
{
	xrt_net_resolver_future* pContext =
		(xrt_net_resolver_future*)pData;

	(void)xrtNetResolveOpCancel(pContext->Operation);
}



/* 把解析操作的唯一终态转发到 Promise 并释放全部桥接资源。 */
static void __xrtNetResolverFutureDone(
	xnetresolveop* pOperation,
	ptr pData
)
{
	xrt_net_resolver_future* pContext =
		(xrt_net_resolver_future*)pData;
	xpromise* pPromise = xrtFutureBridgePromise(&pContext->Bridge);
	xnetresolveop* pHeld = pContext->Operation;
	xnetaddrlist* pAddresses = NULL;
	xerror* pError = NULL;
	xnetresolveopstate State;
	bool bReady;

	bReady = xrtFutureBridgeWait(&pContext->Bridge);
	xrtFutureBridgeUnwatch(&pContext->Bridge);
	State = xrtNetResolveOpState(pOperation);
	if ( State == XNET_RESOLVE_RESOLVED ) {
		pAddresses = xrtNetResolveOpResult(pOperation);
		if ( pAddresses == NULL ) {
			__xrtNetSetError(
				XERR_INTERNAL,
				XNET_ERROR_RESOLVER_QUERY,
				"complete-resolver-future",
				"resolver reported success without an address list",
				0
			);
			pError = xrtTakeError();
		}
	} else if ( State == XNET_RESOLVE_FAILED ) {
		pError = xrtErrorRef(xrtNetResolveOpError(pOperation));
		if ( pError == NULL ) {
			__xrtNetSetError(
				XERR_INTERNAL,
				XNET_ERROR_RESOLVER_QUERY,
				"complete-resolver-future",
				"resolver failed without an error",
				0
			);
			pError = xrtTakeError();
		}
	}
	xrtNetResolveOpDestroy(pHeld);
	xrtFree(pContext);
	if ( !bReady ) {
		xrtNetAddrListDestroy(pAddresses);
	} else if ( (State == XNET_RESOLVE_RESOLVED) &&
		(pAddresses != NULL) ) {
		if ( !xrtPromiseResolveOwned(
			pPromise,
			pAddresses,
			__xrtNetResolverFutureFree,
			NULL
		) ) {
			xrtNetAddrListDestroy(pAddresses);
		}
	} else if ( pError != NULL ) {
		(void)xrtPromiseReject(pPromise, pError);
	} else if ( State == XNET_RESOLVE_FAILED ) {
		(void)xrtPromiseClose(pPromise);
	} else {
		(void)xrtPromiseCancel(pPromise);
	}
	xrtErrorFree(pError);
	xrtPromiseDestroy(pPromise);
}



/* 创建 Resolver 操作、Promise 和双向取消桥接。 */
XRT_API xfuture* xrtNetResolveAsync(
	xnetresolver* pResolver,
	cstr sHost,
	xnetfamily Family
)
{
	xrt_net_resolver_future* pContext;
	xfuture* pFuture;
	xerror* pError;

	pContext = (xrt_net_resolver_future*)xrtCalloc(
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
	pContext->Operation = xrtNetResolverResolve(
		pResolver,
		sHost,
		Family,
		__xrtNetResolverFutureDone,
		pContext
	);
	if ( pContext->Operation == NULL ) {
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(xrtFutureBridgePromise(&pContext->Bridge));
		xrtFree(pContext);
		return NULL;
	}
	if ( !xrtFutureBridgeWatch(
		&pContext->Bridge,
		__xrtNetResolverFutureCancel,
		pContext
	) ) {
		pError = xrtTakeError();
		(void)xrtFutureBridgeFail(&pContext->Bridge);
		(void)xrtNetResolveOpCancel(pContext->Operation);
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

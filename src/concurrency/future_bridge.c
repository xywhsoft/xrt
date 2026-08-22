#include "../internal/xrt_internal.h"
#include "../internal/xrt_future_bridge.h"



#if defined(XRT_FEATURE_FUTURE_BRIDGE)

/* 校验桥对象及其初始化标记。 */
static xrt_future_bridge_impl* __xrtFutureBridgeCheck(
	xfuturebridge* pBridge
)
{
	xrt_future_bridge_impl* pImpl;

	if ( !__xrtRangeValid(pBridge, sizeof(*pBridge)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pImpl = __xrtFutureBridgeImpl(pBridge);
	if ( pImpl->Magic != XRT_FUTURE_BRIDGE_MAGIC ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pImpl;
}



/* 使用一个已有 Promise 初始化桥。 */
XRT_API bool xrtFutureBridgeInit(
	xfuturebridge* pBridge,
	xpromise* pPromise
)
{
	xrt_future_bridge_impl* pImpl;

	if ( !__xrtRangeValid(pBridge, sizeof(*pBridge)) ||
		(pPromise == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pBridge, 0, sizeof(*pBridge));
	pImpl = __xrtFutureBridgeImpl(pBridge);
	(void)xrtAtomic32Init(
		&pImpl->Setup,
		XRT_FUTURE_BRIDGE_INSTALLING
	);
	pImpl->Promise = pPromise;
	pImpl->Magic = XRT_FUTURE_BRIDGE_MAGIC;
	return true;
}



/* 创建 Future/Promise 对并把桥置于装配中状态。 */
XRT_API xfuture* xrtFutureBridgeCreate(
	xfuturebridge* pBridge,
	xcancel* pParent
)
{
	xpromise* pPromise;
	xfuture* pFuture = NULL;

	if ( !__xrtRangeValid(pBridge, sizeof(*pBridge)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pPromise = xrtPromiseCreate(&pFuture, pParent);
	if ( pPromise == NULL ) {
		memset(pBridge, 0, sizeof(*pBridge));
		return NULL;
	}
	(void)xrtFutureBridgeInit(pBridge, pPromise);
	return pFuture;
}



/* 返回桥借用的 Promise。 */
XRT_API xpromise* xrtFutureBridgePromise(
	const xfuturebridge* pBridge
)
{
	const xrt_future_bridge_impl* pImpl;

	if ( !__xrtRangeValid(pBridge, sizeof(*pBridge)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pImpl = __xrtFutureBridgeConstImpl(pBridge);
	if ( pImpl->Magic != XRT_FUTURE_BRIDGE_MAGIC ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pImpl->Promise;
}



/* 把 Future 的协作取消请求转发给底层异步操作。 */
XRT_API bool xrtFutureBridgeWatch(
	xfuturebridge* pBridge,
	xcancelproc pCancelProc,
	ptr pCancelData
)
{
	xrt_future_bridge_impl* pImpl;
	xcancel* pCancel;

	pImpl = __xrtFutureBridgeCheck(pBridge);
	if ( (pImpl == NULL) || (pCancelProc == NULL) ) {
		if ( (pImpl != NULL) && (pCancelProc == NULL) ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	if ( pImpl->Watch != NULL ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pCancel = xrtPromiseCancelToken(pImpl->Promise);
	if ( pCancel == NULL ) {
		return false;
	}
	pImpl->Watch = xrtCancelWatch(
		pCancel,
		pCancelProc,
		pCancelData
	);
	xrtCancelDestroy(pCancel);
	return pImpl->Watch != NULL;
}



/* 发布唯一装配终态。 */
static bool __xrtFutureBridgePublish(
	xfuturebridge* pBridge,
	xrt_future_bridge_setup State
)
{
	xrt_future_bridge_impl* pImpl;
	uint32 iExpected = XRT_FUTURE_BRIDGE_INSTALLING;

	pImpl = __xrtFutureBridgeCheck(pBridge);
	if ( pImpl == NULL ) {
		return false;
	}
	if ( !xrtAtomic32CompareExchange(
		&pImpl->Setup,
		&iExpected,
		(uint32)State,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 发布装配成功。 */
XRT_API bool xrtFutureBridgeReady(xfuturebridge* pBridge)
{
	return __xrtFutureBridgePublish(
		pBridge,
		XRT_FUTURE_BRIDGE_READY
	);
}



/* 发布装配失败。 */
XRT_API bool xrtFutureBridgeFail(xfuturebridge* pBridge)
{
	return __xrtFutureBridgePublish(
		pBridge,
		XRT_FUTURE_BRIDGE_FAILED
	);
}



/* 等待极短的监听装配窗口，并返回 Future 是否可接收结果。 */
XRT_API bool xrtFutureBridgeWait(const xfuturebridge* pBridge)
{
	const xrt_future_bridge_impl* pImpl;
	uint32 iSetup;

	if ( !__xrtRangeValid(pBridge, sizeof(*pBridge)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pImpl = __xrtFutureBridgeConstImpl(pBridge);
	if ( pImpl->Magic != XRT_FUTURE_BRIDGE_MAGIC ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	do {
		iSetup = xrtAtomic32Load(
			&pImpl->Setup,
			XMEMORY_ACQUIRE
		);
		if ( iSetup == XRT_FUTURE_BRIDGE_INSTALLING ) {
			xrtThreadYield();
		}
	} while ( iSetup == XRT_FUTURE_BRIDGE_INSTALLING );
	return iSetup == XRT_FUTURE_BRIDGE_READY;
}



/* 注销监听并与可能正在运行的取消回调汇合。 */
XRT_API void xrtFutureBridgeUnwatch(xfuturebridge* pBridge)
{
	xrt_future_bridge_impl* pImpl;

	pImpl = __xrtFutureBridgeCheck(pBridge);
	if ( pImpl == NULL ) {
		return;
	}
	xrtCancelUnwatch(pImpl->Watch);
	pImpl->Watch = NULL;
}

#endif

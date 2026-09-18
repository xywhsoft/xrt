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

/* Publish structural activity before running registration callbacks or waiting
 * for cancellation. Never retain a shared mutation while another thread runs. */
static bool __xrtFutureBridgeMutationBegin(xrt_future_bridge_impl* pImpl,
	xrtownershipscope* pScope, uint32* pPrevious)
{
	if (!xrtOwnershipMutationBegin(pScope)) return false;
	*pPrevious = xrtAtomic32Load(&pImpl->OwnershipState, XMEMORY_ACQUIRE);
	uint32 iExpected = *pPrevious;
	if ((iExpected & XRT_FUTURE_BRIDGE_MUTATING) || !xrtAtomic32CompareExchange(
		&pImpl->OwnershipState, &iExpected, iExpected | XRT_FUTURE_BRIDGE_MUTATING, XMEMORY_ACQ_REL, XMEMORY_ACQUIRE)) {
		if (!xrtOwnershipScopeEnd(pScope)) abort();
		__xrtErrorSetInvalidState(); return false;
	}
	return true;
}
static void __xrtFutureBridgeMutationEnd(xrtownershipscope* pScope)
{ if (!xrtOwnershipScopeEnd(pScope)) abort(); }



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
	(void)xrtAtomic32Init(&pImpl->OwnershipState, 0);
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
static bool __xrtFutureBridgeWatch(
	xfuturebridge* pBridge,
	xcancelproc pCancelProc,
	ptr pCancelData,
	const xcancelwatchownershipv1* pPolicy
)
{
	xrt_future_bridge_impl* pImpl;
	xcancel* pCancel;
	xcancelwatch* pWatch = NULL;
	xrtownershipscope Mutation = {0};
	uint32 iPrevious;

	pImpl = __xrtFutureBridgeCheck(pBridge);
	if ( (pImpl == NULL) || (pCancelProc == NULL) ) {
		if ( (pImpl != NULL) && (pCancelProc == NULL) ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	if (!__xrtFutureBridgeMutationBegin(pImpl, &Mutation, &iPrevious)) return false;
	if ( pImpl->Watch != NULL ) {
		xrtAtomic32Store(&pImpl->OwnershipState, iPrevious, XMEMORY_RELEASE);
		__xrtFutureBridgeMutationEnd(&Mutation);
		__xrtErrorSetInvalidState();
		return false;
	}
	__xrtFutureBridgeMutationEnd(&Mutation);
	pCancel = xrtPromiseCancelToken(pImpl->Promise);
	if ( pCancel != NULL ) {
		pWatch = pPolicy ? xrtCancelWatchOwnedV1(pCancel, pCancelData, pPolicy) :
			xrtCancelWatch(pCancel, pCancelProc, pCancelData);
		xrtCancelDestroy(pCancel);
	}
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	pImpl->Watch = pWatch;
	xrtAtomic32Store(&pImpl->OwnershipState, pWatch && pPolicy ? XRT_FUTURE_BRIDGE_WATCH_OWNED : 0, XMEMORY_RELEASE);
	__xrtFutureBridgeMutationEnd(&Mutation);
	return pWatch != NULL;
}

XRT_API bool xrtFutureBridgeWatch(xfuturebridge* pBridge, xcancelproc pCancelProc, ptr pCancelData)
{ return __xrtFutureBridgeWatch(pBridge, pCancelProc, pCancelData, NULL); }

XRT_API bool xrtFutureBridgeWatchOwnedV1(xfuturebridge* pBridge, ptr pData,
	const xcancelwatchownershipv1* pPolicy)
{
	if (!pData || !pPolicy || pPolicy->size != sizeof(*pPolicy) || !pPolicy->Notify ||
		!pPolicy->Drop || !pPolicy->Ops || !pPolicy->Ops->Count || !pPolicy->Ops->Trace) {
		__xrtErrorSetInvalidArgument(); return false;
	}
	return __xrtFutureBridgeWatch(pBridge, pPolicy->Notify, pData, pPolicy);
}

XRT_API bool xrtFutureBridgeWatchOwnershipV1(const xfuturebridge* pBridge, xrtownershipref* pReference)
{
	if (!pReference || !__xrtRangeValid(pBridge, sizeof(*pBridge))) return false;
	const xrt_future_bridge_impl* pImpl = __xrtFutureBridgeConstImpl(pBridge);
	if (pImpl->Magic != XRT_FUTURE_BRIDGE_MAGIC ||
		xrtAtomic32Load(&pImpl->Setup, XMEMORY_ACQUIRE) == XRT_FUTURE_BRIDGE_INSTALLING) return false;
	uint32 iState = xrtAtomic32Load(&pImpl->OwnershipState, XMEMORY_ACQUIRE);
	if ((iState & XRT_FUTURE_BRIDGE_MUTATING) || (pImpl->Watch && !(iState & XRT_FUTURE_BRIDGE_WATCH_OWNED))) return false;
	*pReference = xrtCancelWatchOwnership(pImpl->Watch); return true;
}



/* 发布唯一装配终态。 */
static bool __xrtFutureBridgePublish(
	xfuturebridge* pBridge,
	xrt_future_bridge_setup State
)
{
	xrt_future_bridge_impl* pImpl;
	uint32 iExpected = XRT_FUTURE_BRIDGE_INSTALLING;
	uint32 iPrevious;
	xrtownershipscope Mutation = {0};

	pImpl = __xrtFutureBridgeCheck(pBridge);
	if ( pImpl == NULL ) {
		return false;
	}
	if (!__xrtFutureBridgeMutationBegin(pImpl, &Mutation, &iPrevious)) return false;
	/* Setup transfers mutation authority to the unique waiter. Restore flags
	 * BEFORE publication; the waiter may immediately unlink the owned Watch.
	 * This mutation still excludes graph inspection until ScopeEnd. */
	xrtAtomic32Store(&pImpl->OwnershipState, iPrevious, XMEMORY_RELEASE);
	if ( !xrtAtomic32CompareExchange(
		&pImpl->Setup,
		&iExpected,
		(uint32)State,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		__xrtFutureBridgeMutationEnd(&Mutation);
		__xrtErrorSetInvalidState();
		return false;
	}
	__xrtFutureBridgeMutationEnd(&Mutation);
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
	xrtownershipscope Mutation = {0};
	uint32 iPrevious;

	pImpl = __xrtFutureBridgeCheck(pBridge);
	if ( pImpl == NULL ) {
		return;
	}
	if (!__xrtFutureBridgeMutationBegin(pImpl, &Mutation, &iPrevious)) return;
	xcancelwatch* pWatch = pImpl->Watch;
	pImpl->Watch = NULL;
	__xrtFutureBridgeMutationEnd(&Mutation);
	xrtCancelUnwatch(pWatch);
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	xrtAtomic32Store(&pImpl->OwnershipState, 0, XMEMORY_RELEASE);
	__xrtFutureBridgeMutationEnd(&Mutation);
}

#endif

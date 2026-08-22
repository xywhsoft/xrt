#include "../internal/xrt_future.h"



#if defined(XRT_FEATURE_FUTURE)

/* Promise 嵌入 Future 对象，创建一对端点只产生一次堆分配。 */
struct xpromise {
	struct xfuture* Future;
};



/* Future 用一个锁保护终态、结果、条件变量和内部等待链。 */
struct xfuture {
	volatile int32 RefCount;
	volatile int32 PromiseRefs;
	xmutex Lock;
	xcond Ready;
	xfuturestate State;
	bool Completing;
	ptr Value;
	xfuturefreeproc Destroy;
	ptr DestroyData;
	struct xfuture* Owner;
	xerror* Error;
	xcancel* Cancel;
	xrt_future_waiter* Waiters;
	xrt_future_waiter* WaitersTail;
	xpromise Promise;
};



/* 同一执行上下文中的完成通知使用迭代队列，避免 Future 链递归耗尽线程栈。 */
typedef struct xrt_future_notify_context {
	xrt_future_waiter* Head;
	xrt_future_waiter* Tail;
} xrt_future_notify_context;



#if defined(_WIN32) || defined(_WIN64)

static DWORD __xrtFutureNotifyKey = FLS_OUT_OF_INDEXES;
static volatile LONG __xrtFutureNotifyKeyState;



/* 进程内只创建一次 Fiber 本地通知槽，使 Windows 协程切换不会混用派发队列。 */
static bool __xrtFutureNotifyKeyEnsure(void)
{
	LONG iState = InterlockedCompareExchange(&__xrtFutureNotifyKeyState, 1, 0);

	if ( iState == 0 ) {
		__xrtFutureNotifyKey = FlsAlloc(NULL);
		InterlockedExchange(
			&__xrtFutureNotifyKeyState,
			__xrtFutureNotifyKey != FLS_OUT_OF_INDEXES ? 2 : 3
		);
		return __xrtFutureNotifyKey != FLS_OUT_OF_INDEXES;
	}
	while ( (iState = InterlockedCompareExchange(
		&__xrtFutureNotifyKeyState, 0, 0
	)) == 1 ) {
		Sleep(0);
	}
	return iState == 2;
}



/* 返回当前 Fiber 正在使用的通知队列。 */
static xrt_future_notify_context* __xrtFutureNotifyContextGet(void)
{
	return __xrtFutureNotifyKeyEnsure() ?
		(xrt_future_notify_context*)FlsGetValue(__xrtFutureNotifyKey) : NULL;
}



/* 切换当前 Fiber 的通知队列；失败时调用方退回直接派发。 */
static bool __xrtFutureNotifyContextSet(xrt_future_notify_context* pContext)
{
	return __xrtFutureNotifyKeyEnsure() &&
		(FlsSetValue(__xrtFutureNotifyKey, pContext) != 0);
}

#elif defined(__TINYC__)

static pthread_key_t __xrtFutureNotifyKey;
static pthread_once_t __xrtFutureNotifyKeyOnce = PTHREAD_ONCE_INIT;
static bool __xrtFutureNotifyKeyReady;



/* 为 TinyCC POSIX 构建创建不带析构器的通知上下文槽。 */
static void __xrtFutureNotifyKeyInit(void)
{
	__xrtFutureNotifyKeyReady =
		pthread_key_create(&__xrtFutureNotifyKey, NULL) == 0;
}



/* 返回当前线程正在使用的通知队列。 */
static xrt_future_notify_context* __xrtFutureNotifyContextGet(void)
{
	(void)pthread_once(&__xrtFutureNotifyKeyOnce, __xrtFutureNotifyKeyInit);
	return __xrtFutureNotifyKeyReady ?
		(xrt_future_notify_context*)pthread_getspecific(__xrtFutureNotifyKey) :
		NULL;
}



/* 切换当前线程的通知队列；失败时调用方退回直接派发。 */
static bool __xrtFutureNotifyContextSet(xrt_future_notify_context* pContext)
{
	(void)pthread_once(&__xrtFutureNotifyKeyOnce, __xrtFutureNotifyKeyInit);
	return __xrtFutureNotifyKeyReady &&
		(pthread_setspecific(__xrtFutureNotifyKey, pContext) == 0);
}

#else

static XRT_THREAD_LOCAL xrt_future_notify_context*
	__xrtFutureNotifyContext;



/* 返回当前线程正在使用的通知队列。 */
static xrt_future_notify_context* __xrtFutureNotifyContextGet(void)
{
	return __xrtFutureNotifyContext;
}



/* 切换当前线程的通知队列。 */
static bool __xrtFutureNotifyContextSet(xrt_future_notify_context* pContext)
{
	__xrtFutureNotifyContext = pContext;
	return true;
}

#endif



/* 释放当前 Future，并返回需要继续释放的透传结果所有者。 */
static xfuture* __xrtFutureFree(xfuture* pFuture)
{
	ptr pValue = pFuture->Value;
	xfuturefreeproc pDestroy = pFuture->Destroy;
	ptr pDestroyData = pFuture->DestroyData;
	xfuture* pOwner = pFuture->Owner;
	xerror* pError = pFuture->Error;
	xcancel* pCancel = pFuture->Cancel;

	(void)xrtCondUnit(&pFuture->Ready);
	(void)xrtMutexUnit(&pFuture->Lock);
	xrtFree(pFuture);
	if ( pDestroy != NULL ) {
		pDestroy(pValue, pDestroyData);
	}
	xrtErrorFree(pError);
	xrtCancelDestroy(pCancel);
	return pOwner;
}



/* 迭代释放 Future 及透传所有者链，避免深延续链递归耗尽线程栈。 */
static void __xrtFutureRelease(xfuture* pFuture)
{
	while ( (pFuture != NULL) &&
		(xrtRefRelease(&pFuture->RefCount) == 0) ) {
		pFuture = __xrtFutureFree(pFuture);
	}
}



/* 执行一个完成通知，并在回调返回后发布节点可移除状态。 */
static void __xrtFutureNotifyOne(xrt_future_waiter* pWaiter)
{
	xfuture* pFuture = pWaiter->NotifyFuture;
	void (*pRelease)(ptr pData) = pWaiter->Release;
	ptr pData = pWaiter->Data;
	bool bReleaseFuture = pWaiter->NotifyRelease;

	pWaiter->Next = NULL;
	pWaiter->NotifyFuture = NULL;
	pWaiter->NotifyRelease = false;
	pWaiter->Proc(pData);
	(void)xrtMutexLock(&pFuture->Lock);
	pWaiter->Calling = false;
	(void)xrtCondBroadcast(&pFuture->Ready);
	(void)xrtMutexUnlock(&pFuture->Lock);
	if ( pRelease != NULL ) {
		pRelease(pData);
	}
	if ( bReleaseFuture ) {
		xrtFutureDestroy(pFuture);
	}
}



/* 在 Future 锁外把完成批次加入当前执行上下文，并由最外层调用迭代排空。 */
static void __xrtFutureNotify(
	xrt_future_waiter* pHead,
	xrt_future_waiter* pTail
)
{
	xrt_future_notify_context tContext;
	xrt_future_notify_context* pContext = __xrtFutureNotifyContextGet();

	if ( pHead == NULL ) {
		return;
	}
	if ( pContext != NULL ) {
		if ( pContext->Tail != NULL ) {
			pContext->Tail->Next = pHead;
		} else {
			pContext->Head = pHead;
		}
		pContext->Tail = pTail;
		return;
	}
	memset(&tContext, 0, sizeof(tContext));
	if ( !__xrtFutureNotifyContextSet(&tContext) ) {
		while ( pHead != NULL ) {
			xrt_future_waiter* pNext = pHead->Next;

			__xrtFutureNotifyOne(pHead);
			pHead = pNext;
		}
		return;
	}
	tContext.Head = pHead;
	tContext.Tail = pTail;
	while ( tContext.Head != NULL ) {
		xrt_future_waiter* pWaiter = tContext.Head;

		tContext.Head = pWaiter->Next;
		if ( tContext.Head == NULL ) {
			tContext.Tail = NULL;
		}
		__xrtFutureNotifyOne(pWaiter);
	}
	(void)__xrtFutureNotifyContextSet(NULL);
}



/* 在持锁状态下发布唯一终态，并摘取全部等待节点。 */
static void __xrtFuturePublishLocked(
	xfuture* pFuture,
	xfuturestate State,
	ptr pValue,
	xfuturefreeproc pDestroy,
	ptr pDestroyData,
	xfuture* pOwner,
	xerror* pError,
	xrt_future_waiter** ppWaiter,
	xrt_future_waiter** ppWaiterTail
)
{
	xrt_future_waiter* pWaiterTail = NULL;

	pFuture->State = State;
	pFuture->Completing = false;
	pFuture->Value = pValue;
	pFuture->Destroy = pDestroy;
	pFuture->DestroyData = pDestroyData;
	pFuture->Owner = pOwner;
	pFuture->Error = pError;
	*ppWaiter = pFuture->Waiters;
	pFuture->Waiters = NULL;
	pFuture->WaitersTail = NULL;
	for ( xrt_future_waiter* pCurrent = *ppWaiter;
		pCurrent != NULL; pCurrent = pCurrent->Next ) {
		pCurrent->Linked = false;
		pCurrent->Calling = true;
		pCurrent->NotifyFuture = pFuture;
		pCurrent->NotifyRelease = false;
		pWaiterTail = pCurrent;
	}
	if ( pWaiterTail != NULL ) {
		pWaiterTail->NotifyRelease = true;
		(void)xrtFutureRef(pFuture);
	}
	*ppWaiterTail = pWaiterTail;
	(void)xrtCondBroadcast(&pFuture->Ready);
}



/* 把 Pending 原子转换为唯一终态，并按成功与失败保存结果。 */
static bool __xrtFutureComplete(
	xfuture* pFuture,
	xfuturestate State,
	ptr pValue,
	xfuturefreeproc pDestroy,
	ptr pDestroyData,
	xfuture* pOwner,
	xerror* pError,
	bool bRequestCancel,
	bool bReportDuplicate
)
{
	bool bCompleted = false;
	bool bReserved = false;
	xrt_future_waiter* pWaiter = NULL;
	xrt_future_waiter* pWaiterTail = NULL;

	if ( pFuture == NULL ) {
		if ( bReportDuplicate ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	if ( !xrtMutexLock(&pFuture->Lock) ) {
		return false;
	}
	if ( (pFuture->State == XFUTURE_PENDING) && !pFuture->Completing ) {
		if ( bRequestCancel ) {
			pFuture->Completing = true;
			bReserved = true;
		} else {
			__xrtFuturePublishLocked(
				pFuture,
				State,
				pValue,
				pDestroy,
				pDestroyData,
				pOwner,
				pError,
				&pWaiter,
				&pWaiterTail
			);
			bCompleted = true;
		}
	}
	(void)xrtMutexUnlock(&pFuture->Lock);

	if ( bReserved ) {
		(void)xrtCancelRequest(pFuture->Cancel);
		if ( !xrtMutexLock(&pFuture->Lock) ) {
			return false;
		}
		__xrtFuturePublishLocked(
			pFuture,
			State,
			pValue,
			pDestroy,
			pDestroyData,
			pOwner,
			pError,
			&pWaiter,
			&pWaiterTail
		);
		bCompleted = true;
		(void)xrtMutexUnlock(&pFuture->Lock);
	}
	if ( bCompleted ) {
		__xrtFutureNotify(pWaiter, pWaiterTail);
	} else if ( bReportDuplicate ) {
		__xrtErrorSetInvalidState();
	}
	return bCompleted;
}



/* 可取消等待在 Future 锁下记录终态竞争结果。 */
typedef struct xrt_future_cancel_wait {
	xfuture* Future;
	bool Cancelled;
} xrt_future_cancel_wait;



/* 取消与 Future 完成共用一把锁，先取得锁的一方确定等待结果。 */
static void __xrtFutureWaitCancelled(ptr pData)
{
	xrt_future_cancel_wait* pWait = (xrt_future_cancel_wait*)pData;
	xfuture* pFuture = pWait->Future;

	if ( xrtMutexLock(&pFuture->Lock) ) {
		if ( pFuture->State == XFUTURE_PENDING ) {
			pWait->Cancelled = true;
			(void)xrtCondBroadcast(&pFuture->Ready);
		}
		(void)xrtMutexUnlock(&pFuture->Lock);
	}
}



/* Future 尚未完成时挂入一个不分配内存的内部等待节点。 */
bool __xrtFutureWaiterAdd(xfuture* pFuture, xrt_future_waiter* pWaiter)
{
	bool bLinked = false;

	if ( (pFuture == NULL) || (pWaiter == NULL) || (pWaiter->Proc == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtMutexLock(&pFuture->Lock) ) {
		return false;
	}
	if ( pWaiter->Linked || pWaiter->Calling ) {
		(void)xrtMutexUnlock(&pFuture->Lock);
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( pFuture->State == XFUTURE_PENDING ) {
		pWaiter->Next = NULL;
		pWaiter->Linked = true;
		if ( pFuture->WaitersTail != NULL ) {
			pFuture->WaitersTail->Next = pWaiter;
		} else {
			pFuture->Waiters = pWaiter;
		}
		pFuture->WaitersTail = pWaiter;
		bLinked = true;
	}
	(void)xrtMutexUnlock(&pFuture->Lock);
	return bLinked;
}



/* 摘除尚未进入完成批次的等待节点，但不等待已经开始的回调。 */
bool __xrtFutureWaiterDetach(xfuture* pFuture, xrt_future_waiter* pWaiter)
{
	xrt_future_waiter** ppWaiter;
	xrt_future_waiter* pPrevious = NULL;
	void (*pRelease)(ptr pData) = NULL;
	ptr pData = NULL;
	bool bDetached = false;

	if ( (pFuture == NULL) || (pWaiter == NULL) ) {
		return false;
	}
	if ( !xrtMutexLock(&pFuture->Lock) ) {
		return false;
	}
	if ( pWaiter->Linked ) {
		ppWaiter = &pFuture->Waiters;
		while ( (*ppWaiter != NULL) && (*ppWaiter != pWaiter) ) {
			pPrevious = *ppWaiter;
			ppWaiter = &(*ppWaiter)->Next;
		}
		if ( *ppWaiter == pWaiter ) {
			*ppWaiter = pWaiter->Next;
			if ( pFuture->WaitersTail == pWaiter ) {
				pFuture->WaitersTail = pPrevious;
			}
			pWaiter->Next = NULL;
			pWaiter->Linked = false;
			pRelease = pWaiter->Release;
			pData = pWaiter->Data;
			bDetached = true;
		}
	}
	(void)xrtMutexUnlock(&pFuture->Lock);
	if ( pRelease != NULL ) {
		pRelease(pData);
	}
	return bDetached;
}



/* 从等待链移除仍然挂接的内部节点。 */
void __xrtFutureWaiterRemove(xfuture* pFuture, xrt_future_waiter* pWaiter)
{
	xrt_future_waiter** ppWaiter;
	xrt_future_waiter* pPrevious = NULL;
	void (*pRelease)(ptr pData) = NULL;
	ptr pData = NULL;

	if ( (pFuture == NULL) || (pWaiter == NULL) ) {
		return;
	}
	if ( !xrtMutexLock(&pFuture->Lock) ) {
		return;
	}
	if ( pWaiter->Linked ) {
		ppWaiter = &pFuture->Waiters;
		while ( (*ppWaiter != NULL) && (*ppWaiter != pWaiter) ) {
			pPrevious = *ppWaiter;
			ppWaiter = &(*ppWaiter)->Next;
		}
		if ( *ppWaiter == pWaiter ) {
			*ppWaiter = pWaiter->Next;
			if ( pFuture->WaitersTail == pWaiter ) {
				pFuture->WaitersTail = pPrevious;
			}
		}
		pWaiter->Next = NULL;
		pWaiter->Linked = false;
		pRelease = pWaiter->Release;
		pData = pWaiter->Data;
	}
	while ( pWaiter->Calling ) {
		(void)xrtCondWait(&pFuture->Ready, &pFuture->Lock);
	}
	(void)xrtMutexUnlock(&pFuture->Lock);
	if ( pRelease != NULL ) {
		pRelease(pData);
	}
}



/* 初始化调用方持有的无分配 Future Watch。 */
XRT_API bool xrtFutureWatchInit(
	xfuturewatch* pWatch,
	xfuturewatchproc pNotify,
	xfuturewatchreleaseproc pRelease,
	ptr pData
)
{
	xrt_future_watch_impl* pImpl;

	if ( !__xrtRangeValid(pWatch, sizeof(*pWatch)) ||
		(pNotify == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pWatch, 0, sizeof(*pWatch));
	pImpl = __xrtFutureWatchImpl(pWatch);
	pImpl->Waiter.Proc = pNotify;
	pImpl->Waiter.Release = pRelease;
	pImpl->Waiter.Data = pData;
	pImpl->Magic = XRT_FUTURE_WATCH_MAGIC;
	return true;
}



/* 把 Watch 挂入仍为 Pending 的 Future。 */
XRT_API xfuturewatchresult xrtFutureWatchAdd(
	xfuture* pFuture,
	xfuturewatch* pWatch
)
{
	xrt_future_watch_impl* pImpl;
	xerror* pPrevious;
	xerror* pCurrent;
	bool bAdded;

	if ( (pFuture == NULL) ||
		!__xrtRangeValid(pWatch, sizeof(*pWatch)) ) {
		__xrtErrorSetInvalidArgument();
		return XFUTURE_WATCH_ERROR;
	}
	pImpl = __xrtFutureWatchImpl(pWatch);
	if ( (pImpl->Magic != XRT_FUTURE_WATCH_MAGIC) ||
		(pImpl->Waiter.Proc == NULL) ) {
		__xrtErrorSetInvalidState();
		return XFUTURE_WATCH_ERROR;
	}
	pPrevious = __xrtErrorSwapOwned(NULL);
	bAdded = __xrtFutureWaiterAdd(pFuture, &pImpl->Waiter);
	pCurrent = __xrtErrorSwapOwned(pPrevious);
	if ( bAdded ) {
		return XFUTURE_WATCH_PENDING;
	}
	if ( pCurrent != NULL ) {
		xrtSetError(pCurrent);
		xrtErrorFree(pCurrent);
		return XFUTURE_WATCH_ERROR;
	}
	return XFUTURE_WATCH_READY;
}



/* 摘除尚未开始回调的 Future Watch。 */
XRT_API bool xrtFutureWatchDetach(
	xfuture* pFuture,
	xfuturewatch* pWatch
)
{
	xrt_future_watch_impl* pImpl;

	if ( (pFuture == NULL) ||
		!__xrtRangeValid(pWatch, sizeof(*pWatch)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pImpl = __xrtFutureWatchImpl(pWatch);
	if ( pImpl->Magic != XRT_FUTURE_WATCH_MAGIC ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return __xrtFutureWaiterDetach(pFuture, &pImpl->Waiter);
}



/* 移除 Future Watch，并与并发中的通知回调汇合。 */
XRT_API void xrtFutureWatchRemove(
	xfuture* pFuture,
	xfuturewatch* pWatch
)
{
	xrt_future_watch_impl* pImpl;

	if ( (pFuture == NULL) ||
		!__xrtRangeValid(pWatch, sizeof(*pWatch)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	pImpl = __xrtFutureWatchImpl(pWatch);
	if ( pImpl->Magic != XRT_FUTURE_WATCH_MAGIC ) {
		__xrtErrorSetInvalidState();
		return;
	}
	__xrtFutureWaiterRemove(pFuture, &pImpl->Waiter);
}



/* 创建共享 Future 与嵌入式 Promise 端点。 */
XRT_API xpromise* xrtPromiseCreate(xfuture** ppFuture, xcancel* pParentCancel)
{
	xfuture* pFuture;

	if ( ppFuture == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	*ppFuture = NULL;
	pFuture = (xfuture*)xrtCalloc(1, sizeof(xfuture));
	if ( pFuture == NULL ) {
		return NULL;
	}
	pFuture->RefCount = 2;
	pFuture->PromiseRefs = 1;
	pFuture->State = XFUTURE_PENDING;
	pFuture->Promise.Future = pFuture;
	if ( !xrtMutexInit(&pFuture->Lock) ) {
		xrtFree(pFuture);
		return NULL;
	}
	if ( !xrtCondInit(&pFuture->Ready) ) {
		(void)xrtMutexUnit(&pFuture->Lock);
		xrtFree(pFuture);
		return NULL;
	}
	pFuture->Cancel = xrtCancelChild(pParentCancel);
	if ( pFuture->Cancel == NULL ) {
		(void)xrtCondUnit(&pFuture->Ready);
		(void)xrtMutexUnit(&pFuture->Lock);
		xrtFree(pFuture);
		return NULL;
	}
	*ppFuture = pFuture;
	return &pFuture->Promise;
}



/* 增加 Promise 生产端引用。 */
XRT_API xpromise* xrtPromiseRef(xpromise* pPromise)
{
	xfuture* pFuture;

	if ( pPromise == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pFuture = pPromise->Future;
	if ( xrtRefRetain(&pFuture->PromiseRefs) < 0 ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	if ( xrtRefRetain(&pFuture->RefCount) < 0 ) {
		(void)xrtRefRelease(&pFuture->PromiseRefs);
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pPromise;
}



/* 释放 Promise，并在最后一个生产端离开时关闭未完成结果。 */
XRT_API void xrtPromiseDestroy(xpromise* pPromise)
{
	xfuture* pFuture;
	int32 iRefs;

	if ( pPromise == NULL ) {
		return;
	}
	pFuture = pPromise->Future;
	iRefs = xrtRefRelease(&pFuture->PromiseRefs);
	if ( iRefs < 0 ) {
		__xrtErrorSetInvalidState();
		return;
	}
	if ( iRefs == 0 ) {
		(void)__xrtFutureComplete(
			pFuture,
			XFUTURE_CLOSED,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			true,
			false
		);
	}
	__xrtFutureRelease(pFuture);
}



/* 增加 Future 消费端引用。 */
XRT_API xfuture* xrtFutureRef(xfuture* pFuture)
{
	if ( pFuture == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( xrtRefRetain(&pFuture->RefCount) < 0 ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pFuture;
}



/* 释放 Future 消费端引用。 */
XRT_API void xrtFutureDestroy(xfuture* pFuture)
{
	__xrtFutureRelease(pFuture);
}



/* 返回 Future 状态快照。 */
XRT_API xfuturestate xrtFutureState(const xfuture* pFuture)
{
	xfuturestate State;

	if ( pFuture == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XFUTURE_CLOSED;
	}
	if ( !xrtMutexLock((xmutex*)&pFuture->Lock) ) {
		return XFUTURE_CLOSED;
	}
	State = pFuture->State;
	(void)xrtMutexUnlock((xmutex*)&pFuture->Lock);
	return State;
}



/* 判断 Future 是否已经进入终态。 */
XRT_API bool xrtFutureDone(const xfuture* pFuture)
{
	return xrtFutureState(pFuture) != XFUTURE_PENDING;
}



/* 复制借用的 Future 结果。 */
XRT_API bool xrtFutureResult(const xfuture* pFuture, xfutureresult* pResult)
{
	if ( (pFuture == NULL) || (pResult == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtMutexLock((xmutex*)&pFuture->Lock) ) {
		return false;
	}
	if ( pFuture->State == XFUTURE_PENDING ) {
		(void)xrtMutexUnlock((xmutex*)&pFuture->Lock);
		__xrtErrorSetAgain();
		return false;
	}
	pResult->State = pFuture->State;
	pResult->Value = pFuture->Value;
	pResult->Error = pFuture->Error;
	(void)xrtMutexUnlock((xmutex*)&pFuture->Lock);
	return true;
}



/* 返回成功值，并把非成功状态映射到当前错误上下文。 */
XRT_API ptr xrtFutureValue(const xfuture* pFuture)
{
	xfuturestate State;
	ptr pValue;
	xerror* pError = NULL;

	if ( pFuture == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtMutexLock((xmutex*)&pFuture->Lock) ) {
		return NULL;
	}
	State = pFuture->State;
	pValue = pFuture->Value;
	if ( State == XFUTURE_FAILED ) {
		pError = xrtErrorRef(pFuture->Error);
	}
	(void)xrtMutexUnlock((xmutex*)&pFuture->Lock);
	if ( State == XFUTURE_RESOLVED ) {
		return pValue;
	}
	if ( State == XFUTURE_FAILED ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	} else if ( State == XFUTURE_CANCELLED ) {
		__xrtErrorSetCancelled();
	} else if ( State == XFUTURE_CLOSED ) {
		__xrtErrorSetClosed();
	} else {
		__xrtErrorSetAgain();
	}
	return NULL;
}



/* 返回失败终态借用的结构化错误。 */
XRT_API const xerror* xrtFutureError(const xfuture* pFuture)
{
	const xerror* pError;

	if ( pFuture == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtMutexLock((xmutex*)&pFuture->Lock) ) {
		return NULL;
	}
	pError = pFuture->State == XFUTURE_FAILED ? pFuture->Error : NULL;
	(void)xrtMutexUnlock((xmutex*)&pFuture->Lock);
	return pError;
}



/* 请求 Future 的生产过程协作取消。 */
XRT_API bool xrtFutureCancel(xfuture* pFuture)
{
	bool bPending;

	if ( pFuture == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtMutexLock(&pFuture->Lock) ) {
		return false;
	}
	bPending = pFuture->State == XFUTURE_PENDING;
	(void)xrtMutexUnlock(&pFuture->Lock);
	return bPending && xrtCancelRequest(pFuture->Cancel);
}



/* 返回 Future 取消令牌的新增引用。 */
XRT_API xcancel* xrtFutureCancelToken(const xfuture* pFuture)
{
	if ( pFuture == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return xrtCancelRef(pFuture->Cancel);
}



/* 返回 Promise 取消令牌的新增引用。 */
XRT_API xcancel* xrtPromiseCancelToken(const xpromise* pPromise)
{
	if ( pPromise == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return xrtCancelRef(pPromise->Future->Cancel);
}



/* 永久等待 Future 进入任一终态。 */
XRT_API xwaitresult xrtFutureWait(xfuture* pFuture)
{
	return xrtFutureWaitUntilCancel(pFuture, XRT_DEADLINE_NEVER, NULL);
}



/* 在相对微秒数内等待 Future。 */
XRT_API xwaitresult xrtFutureWaitFor(xfuture* pFuture, uint64 iTimeout)
{
	return xrtFutureWaitUntilCancel(pFuture, xrtDeadlineAfter(iTimeout), NULL);
}



/* 等待 Future 到指定截止时间。 */
XRT_API xwaitresult xrtFutureWaitUntil(xfuture* pFuture, xdeadline iDeadline)
{
	return xrtFutureWaitUntilCancel(pFuture, iDeadline, NULL);
}



/* 等待 Future、截止时间或外部取消令牌中的首个事件。 */
XRT_API xwaitresult xrtFutureWaitUntilCancel(
	xfuture* pFuture,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xcancelwatch* pWatch = NULL;
	xrt_future_cancel_wait CancelWait;
	xwaitresult Result = XWAIT_OK;

	pFuture = xrtFutureRef(pFuture);
	if ( pFuture == NULL ) {
		return XWAIT_ERROR;
	}
	memset(&CancelWait, 0, sizeof(CancelWait));
	CancelWait.Future = pFuture;
	if ( pCancel != NULL ) {
		pWatch = xrtCancelWatch(
			pCancel,
			__xrtFutureWaitCancelled,
			&CancelWait
		);
		if ( pWatch == NULL ) {
			xrtFutureDestroy(pFuture);
			return XWAIT_ERROR;
		}
	}
	if ( !xrtMutexLock(&pFuture->Lock) ) {
		Result = XWAIT_ERROR;
	} else {
		while ( (pFuture->State == XFUTURE_PENDING) &&
				 !CancelWait.Cancelled ) {
			if ( xrtDeadlineExpired(iDeadline) ) {
				Result = XWAIT_TIMEOUT;
				break;
			}
			Result = xrtCondWaitUntil(&pFuture->Ready, &pFuture->Lock, iDeadline);
			if ( Result == XWAIT_ERROR ) {
				break;
			}
			if ( Result == XWAIT_TIMEOUT ) {
				if ( CancelWait.Cancelled ) {
					Result = XWAIT_CANCELLED;
				} else if ( pFuture->State != XFUTURE_PENDING ) {
					Result = XWAIT_OK;
				}
				break;
			}
		}
		if ( CancelWait.Cancelled ) {
			Result = XWAIT_CANCELLED;
		} else if ( pFuture->State != XFUTURE_PENDING ) {
			Result = XWAIT_OK;
		}
		(void)xrtMutexUnlock(&pFuture->Lock);
	}
	if ( pWatch != NULL ) {
		xrtCancelUnwatch(pWatch);
	}
	xrtFutureDestroy(pFuture);
	return Result;
}



/* 以借用值完成 Promise。 */
XRT_API bool xrtPromiseResolve(xpromise* pPromise, ptr pValue)
{
	if ( pPromise == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtFutureComplete(
		pPromise->Future,
		XFUTURE_RESOLVED,
		pValue,
		NULL,
		NULL,
		NULL,
		NULL,
		false,
		true
	);
}



/* 以转移所有权的值完成 Promise。 */
XRT_API bool xrtPromiseResolveOwned(
	xpromise* pPromise,
	ptr pValue,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
)
{
	if ( (pPromise == NULL) || (pDestroy == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtFutureComplete(
		pPromise->Future,
		XFUTURE_RESOLVED,
		pValue,
		pDestroy,
		pDestroyData,
		NULL,
		NULL,
		false,
		true
	);
}



/* 以增加引用的结构化错误完成 Promise。 */
XRT_API bool xrtPromiseReject(xpromise* pPromise, const xerror* pError)
{
	xerror* pHeldError;
	bool bCompleted;

	if ( (pPromise == NULL) || (pError == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pHeldError = xrtErrorRef(pError);
	if ( pHeldError == NULL ) {
		return false;
	}
	bCompleted = __xrtFutureComplete(
		pPromise->Future,
		XFUTURE_FAILED,
		NULL,
		NULL,
		NULL,
		NULL,
		pHeldError,
		false,
		true
	);
	if ( !bCompleted ) {
		xrtErrorFree(pHeldError);
	}
	return bCompleted;
}



/* 把源终态透传到 Promise，并在成功值借用期间保留源 Future。 */
XRT_API bool xrtPromiseForward(xpromise* pPromise, xfuture* pSource)
{
	xfutureresult tResult;
	bool bCompleted;

	if ( (pPromise == NULL) || (pSource == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pPromise->Future == pSource ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pSource = xrtFutureRef(pSource);
	if ( pSource == NULL ) {
		return false;
	}
	if ( !xrtFutureResult(pSource, &tResult) ) {
		xrtFutureDestroy(pSource);
		return false;
	}
	if ( tResult.State == XFUTURE_RESOLVED ) {
		bCompleted = __xrtFutureComplete(
			pPromise->Future,
			XFUTURE_RESOLVED,
			tResult.Value,
			NULL,
			NULL,
			pSource,
			NULL,
			false,
			true
		);
		if ( !bCompleted ) {
			xrtFutureDestroy(pSource);
		}
		return bCompleted;
	}
	if ( tResult.State == XFUTURE_FAILED ) {
		bCompleted = xrtPromiseReject(pPromise, tResult.Error);
	} else if ( tResult.State == XFUTURE_CANCELLED ) {
		bCompleted = xrtPromiseCancel(pPromise);
	} else {
		bCompleted = xrtPromiseClose(pPromise);
	}
	xrtFutureDestroy(pSource);
	return bCompleted;
}



/* 完成取消终态并同步发出协作取消请求。 */
XRT_API bool xrtPromiseCancel(xpromise* pPromise)
{
	bool bCompleted;

	if ( pPromise == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	bCompleted = __xrtFutureComplete(
		pPromise->Future,
		XFUTURE_CANCELLED,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		true,
		true
	);
	return bCompleted;
}



/* 完成关闭终态并同步发出协作取消请求。 */
XRT_API bool xrtPromiseClose(xpromise* pPromise)
{
	bool bCompleted;

	if ( pPromise == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	bCompleted = __xrtFutureComplete(
		pPromise->Future,
		XFUTURE_CLOSED,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		true,
		true
	);
	return bCompleted;
}



/* 判断 Promise 对应的 Future 是否已经完成。 */
XRT_API bool xrtPromiseDone(const xpromise* pPromise)
{
	if ( pPromise == NULL ) {
		__xrtErrorSetInvalidArgument();
		return true;
	}
	return xrtFutureDone(pPromise->Future);
}

#endif

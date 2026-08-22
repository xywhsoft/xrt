#include "../internal/xrt_tls_stream.h"



#if defined(XRT_FEATURE_TLS_STREAM_LISTENER_FUTURE)

/* 每个等待节点持有 Listener 和 Promise，直到唯一终态完成。 */
struct __xrt_tls_listener_wait {
	__xrt_tls_listener_wait* Next;
	xtlslistener* Listener;
	xpromise* Promise;
	xcancelwatch* Watch;
	bool Linked;
};



/* Future 结果析构时释放完成握手的 TLS Stream 引用。 */
static void __xrtTlsListenerStreamFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtTlsStreamDestroy((xtlsstream*)pValue);
}



/* 从等待链移除指定节点；调用方持有 Listener.Lock。 */
static bool __xrtTlsListenerWaitRemove(
	xtlslistener* pListener,
	__xrt_tls_listener_wait* pWaiter
)
{
	__xrt_tls_listener_wait** ppCurrent = &pListener->WaitHead;
	__xrt_tls_listener_wait* pPrevious = NULL;

	while ( (*ppCurrent != NULL) && (*ppCurrent != pWaiter) ) {
		pPrevious = *ppCurrent;
		ppCurrent = &(*ppCurrent)->Next;
	}
	if ( *ppCurrent == NULL ) {
		return false;
	}
	*ppCurrent = pWaiter->Next;
	if ( pListener->WaitTail == pWaiter ) {
		pListener->WaitTail = pPrevious;
	}
	pWaiter->Next = NULL;
	pWaiter->Linked = false;
	(void)xrtAtomic32FetchSub(
		&pListener->AcceptWaiters,
		1,
		XMEMORY_ACQ_REL
	);
	return true;
}



/* 完成一个等待节点，并转移 Stream 的调用方引用。 */
static void __xrtTlsListenerWaitFinish(
	__xrt_tls_listener_wait* pWaiter,
	xtlsstream* pStream,
	bool bCancelled
)
{
	xtlslistener* pListener = pWaiter->Listener;
	xpromise* pPromise = pWaiter->Promise;

	xrtCancelUnwatch(pWaiter->Watch);
	pWaiter->Watch = NULL;
	pWaiter->Promise = NULL;
	xrtFree(pWaiter);
	xrtTlsListenerDestroy(pListener);
	if ( pStream != NULL ) {
		if ( !xrtPromiseResolveOwned(
			pPromise,
			pStream,
			__xrtTlsListenerStreamFree,
			NULL
		) ) {
			(void)xrtTlsStreamAbort(pStream);
			xrtTlsStreamDestroy(pStream);
		}
	} else if ( bCancelled ) {
		(void)xrtPromiseCancel(pPromise);
	} else {
		(void)xrtPromiseClose(pPromise);
	}
	xrtPromiseDestroy(pPromise);
}



/* 把握手完成连接直接交给最早的等待者。 */
bool __xrtTlsListenerFutureAccept(
	xtlslistener* pListener,
	__xrt_tls_listener_stream* pNode
)
{
	__xrt_tls_listener_wait* pWaiter = NULL;
	xtlsstream* pStream = NULL;

	__xrtSpinLock(&pListener->Lock);
	if ( (xrtTlsListenerState(pListener) == XTLS_LISTENER_OPEN) &&
		(pListener->WaitHead != NULL) ) {
		pWaiter = pListener->WaitHead;
		(void)__xrtTlsListenerWaitRemove(pListener, pWaiter);
		pNode->CallerHeld = false;
		pStream = pNode->Stream;
	}
	__xrtSpinUnlock(&pListener->Lock);
	if ( pWaiter == NULL ) {
		return false;
	}
	__xrtTlsListenerWaitFinish(pWaiter, pStream, false);
	return true;
}



/* Future 取消只移除当前等待，不影响 Listener 和其他消费者。 */
static void __xrtTlsListenerWaitCancel(ptr pData)
{
	__xrt_tls_listener_wait* pWaiter =
		(__xrt_tls_listener_wait*)pData;
	xtlslistener* pListener = pWaiter->Listener;
	bool bRemoved;

	__xrtSpinLock(&pListener->Lock);
	bRemoved = pWaiter->Linked &&
		__xrtTlsListenerWaitRemove(pListener, pWaiter);
	__xrtSpinUnlock(&pListener->Lock);
	if ( bRemoved ) {
		__xrtTlsListenerWaitFinish(pWaiter, NULL, true);
	}
}



/* 按 FIFO 配对完成队列与等待者，并在关闭后终结全部等待。 */
void __xrtTlsListenerFutureNotify(xtlslistener* pListener)
{
	for ( ;; ) {
		__xrt_tls_listener_wait* pWaiter = NULL;
		__xrt_tls_listener_stream* pNode = NULL;
		xtlsstream* pStream = NULL;

		__xrtSpinLock(&pListener->Lock);
		if ( pListener->WaitHead != NULL ) {
			if ( xrtTlsListenerState(pListener) !=
				XTLS_LISTENER_OPEN ) {
				pWaiter = pListener->WaitHead;
				(void)__xrtTlsListenerWaitRemove(
					pListener,
					pWaiter
				);
			} else if ( pListener->AcceptHead != NULL ) {
				pWaiter = pListener->WaitHead;
				(void)__xrtTlsListenerWaitRemove(
					pListener,
					pWaiter
				);
				pNode = __xrtTlsListenerTakeQueued(pListener);
				pNode->CallerHeld = false;
				pStream = pNode->Stream;
			}
		}
		__xrtSpinUnlock(&pListener->Lock);
		if ( pWaiter == NULL ) {
			break;
		}
		__xrtTlsListenerWaitFinish(
			pWaiter,
			pStream,
			false
		);
	}
}



/* 建立一个可取消的 TLS Listener accept Future。 */
XRT_API xfuture* xrtTlsListenerAcceptAsync(xtlslistener* pListener)
{
	__xrt_tls_listener_wait* pWaiter;
	xtlslistener* pOwned;
	xfuture* pFuture = NULL;
	xcancel* pCancel;
	xerror* pError;
	__xrt_tls_listener_stream* pNode = NULL;
	xtlsstream* pStream = NULL;
	bool bClosed = false;

	if ( pListener == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( pListener->Events.Accept != NULL ) {
		__xrtTlsError(
			XERR_STATE,
			XTLS_ERROR_STATE,
			"accept-tls-listener",
			"TLS listener accept Future requires no Accept callback",
			SIZE_MAX
		);
		return NULL;
	}
	pOwned = xrtTlsListenerRef(pListener);
	if ( pOwned == NULL ) {
		return NULL;
	}
	pWaiter = (__xrt_tls_listener_wait*)xrtCalloc(
		1,
		sizeof(*pWaiter)
	);
	if ( pWaiter == NULL ) {
		xrtTlsListenerDestroy(pOwned);
		return NULL;
	}
	pWaiter->Listener = pOwned;
	pWaiter->Promise = xrtPromiseCreate(&pFuture, NULL);
	if ( pWaiter->Promise == NULL ) {
		xrtFree(pWaiter);
		xrtTlsListenerDestroy(pOwned);
		return NULL;
	}
	pCancel = xrtPromiseCancelToken(pWaiter->Promise);
	if ( pCancel != NULL ) {
		pWaiter->Watch = xrtCancelWatch(
			pCancel,
			__xrtTlsListenerWaitCancel,
			pWaiter
		);
		xrtCancelDestroy(pCancel);
	}
	if ( pWaiter->Watch == NULL ) {
		pError = xrtTakeError();
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(pWaiter->Promise);
		xrtFree(pWaiter);
		xrtTlsListenerDestroy(pOwned);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return NULL;
	}
	__xrtSpinLock(&pListener->Lock);
	if ( xrtTlsListenerState(pListener) != XTLS_LISTENER_OPEN ) {
		bClosed = true;
	} else {
		if ( pListener->WaitHead == NULL ) {
			pNode = __xrtTlsListenerTakeQueued(pListener);
			if ( pNode != NULL ) {
				pNode->CallerHeld = false;
				pStream = pNode->Stream;
			}
		}
		if ( pNode == NULL ) {
			pWaiter->Linked = true;
			if ( pListener->WaitTail != NULL ) {
				pListener->WaitTail->Next = pWaiter;
			} else {
				pListener->WaitHead = pWaiter;
			}
			pListener->WaitTail = pWaiter;
			(void)xrtAtomic32FetchAdd(
				&pListener->AcceptWaiters,
				1,
				XMEMORY_ACQ_REL
			);
		}
	}
	__xrtSpinUnlock(&pListener->Lock);
	if ( (pStream != NULL) || bClosed ) {
		__xrtTlsListenerWaitFinish(
			pWaiter,
			pStream,
			false
		);
	}
	return pFuture;
}

#endif

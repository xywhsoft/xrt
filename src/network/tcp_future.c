#include "../internal/xrt_tcp.h"



#if defined(XRT_FEATURE_NET_TCP_FUTURE)

/* 内部等待结果只在线性化取走等待节点后使用。 */
typedef enum __xrt_net_stream_wait_result {
	__XRT_NET_STREAM_WAIT_PENDING = 0,
	__XRT_NET_STREAM_WAIT_READY,
	__XRT_NET_STREAM_WAIT_RECEIVE,
	__XRT_NET_STREAM_WAIT_FAILED,
	__XRT_NET_STREAM_WAIT_CANCELLED,
	__XRT_NET_STREAM_WAIT_CLOSED
} __xrt_net_stream_wait_result;



/* 一个等待节点持有 Stream 和 Promise，直到唯一终态完成。 */
struct __xrt_net_stream_wait {
	__xrt_net_stream_wait* Next;
	xnetstream* Stream;
	xnetworker* CacheWorker;
	xpromise* Promise;
	xcancelwatch* Watch;
	xnetstreamwait Wait;
	size_t MaxBytes;
	size_t MinimumBytes;
	bool Receive;
	bool Linked;
};



/* Listener 等待节点持有 Listener 和 Promise，直到唯一终态完成。 */
struct __xrt_net_listener_wait {
	__xrt_net_listener_wait* Next;
	xnetlistener* Listener;
	xnetworker* CacheWorker;
	xpromise* Promise;
	xcancelwatch* Watch;
	bool Linked;
};

_Static_assert(
	sizeof(__xrt_net_stream_wait) <= 128u,
	"TCP Stream Future waiter left the small-node classes"
);

_Static_assert(
	sizeof(__xrt_net_listener_wait) <= 64u,
	"TCP Listener Future waiter left the 64-byte node class"
);



/* 从 Listener 所属 Worker 的统一缓存取得一个等待节点。 */
static __xrt_net_listener_wait* __xrtNetListenerWaitAlloc(
	xnetworker* pWorker
)
{
	__xrt_net_listener_wait* pWaiter = pWorker != NULL ?
		(__xrt_net_listener_wait*)__xrtNetWorkerNodeAlloc(
			pWorker,
			sizeof(__xrt_net_listener_wait)
		) : (__xrt_net_listener_wait*)xrtCalloc(
			1,
			sizeof(__xrt_net_listener_wait)
		);

	if ( pWaiter != NULL ) {
		pWaiter->CacheWorker = pWorker;
	} else if ( pWorker != NULL ) {
		__xrtNetEngineObjectRelease(pWorker->Engine);
	}
	return pWaiter;
}



/* 在 Listener 引用释放前归还等待节点。 */
static void __xrtNetListenerWaitRecycle(
	xnetlistener* pListener,
	__xrt_net_listener_wait* pWaiter
)
{
	xnetworker* pWorker = pWaiter->CacheWorker;

	(void)pListener;
	if ( pWorker != NULL ) {
		__xrtNetWorkerNodeRecycleHeld(
			pWorker,
			pWaiter,
			sizeof(*pWaiter)
		);
	} else {
		xrtFree(pWaiter);
	}
}



/* 从 Stream 所属 Worker 的统一缓存取得一个等待节点。 */
static __xrt_net_stream_wait* __xrtNetStreamWaitAlloc(
	xnetworker* pWorker
)
{
	__xrt_net_stream_wait* pWaiter = pWorker != NULL ?
		(__xrt_net_stream_wait*)__xrtNetWorkerNodeAlloc(
			pWorker,
			sizeof(__xrt_net_stream_wait)
		) : (__xrt_net_stream_wait*)xrtCalloc(
			1,
			sizeof(__xrt_net_stream_wait)
		);

	if ( pWaiter != NULL ) {
		pWaiter->CacheWorker = pWorker;
	} else if ( pWorker != NULL ) {
		__xrtNetEngineObjectRelease(pWorker->Engine);
	}
	return pWaiter;
}



/* 在 Stream 引用释放前归还等待节点。 */
static void __xrtNetStreamWaitRecycle(
	xnetstream* pStream,
	__xrt_net_stream_wait* pWaiter
)
{
	xnetworker* pWorker = pWaiter->CacheWorker;

	(void)pStream;
	if ( pWorker != NULL ) {
		__xrtNetWorkerNodeRecycleHeld(
			pWorker,
			pWaiter,
			sizeof(*pWaiter)
		);
	} else {
		xrtFree(pWaiter);
	}
}



/* 释放 Future 持有的接收结果。 */
static void __xrtNetStreamBytesFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtNetBytesDestroy((xnetbytes*)pValue);
}



/* 释放 Future 持有的已接受 Stream 引用。 */
static void __xrtNetListenerStreamFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtNetStreamDestroy((xnetstream*)pValue);
}



/* 从 Listener 等待链表中移除指定节点；调用方持有 AcceptLock。 */
static bool __xrtNetListenerWaitRemove(
	xnetlistener* pListener,
	__xrt_net_listener_wait* pWaiter
)
{
	__xrt_net_listener_wait** ppCurrent = &pListener->WaitHead;
	__xrt_net_listener_wait* pPrevious = NULL;

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



/* 完成一个 Listener 等待节点并释放全部持有资源。 */
static void __xrtNetListenerWaitFinish(
	__xrt_net_listener_wait* pWaiter,
	xnetstream* pStream,
	bool bCancelled
)
{
	xnetlistener* pListener = pWaiter->Listener;
	xpromise* pPromise = pWaiter->Promise;

	xrtCancelUnwatch(pWaiter->Watch);
	pWaiter->Watch = NULL;
	pWaiter->Promise = NULL;
	__xrtNetListenerWaitRecycle(pListener, pWaiter);
	xrtNetListenerDestroy(pListener);
	if ( pStream != NULL ) {
		if ( !xrtPromiseResolveOwned(
			pPromise,
			pStream,
			__xrtNetListenerStreamFree,
			NULL
		) ) {
			xrtNetStreamDestroy(pStream);
		}
	} else if ( bCancelled ) {
		(void)xrtPromiseCancel(pPromise);
	} else {
		(void)xrtPromiseClose(pPromise);
	}
	xrtPromiseDestroy(pPromise);
}



/* 在 AcceptLock 内把连接线性化交给最早的异步消费者。 */
bool __xrtNetListenerFutureAccept(
	xnetlistener* pListener,
	xnetstream* pStream
)
{
	__xrt_net_listener_wait* pWaiter = NULL;

	__xrtSpinLock(&pListener->AcceptLock);
	if ( (xrtNetListenerState(pListener) == XNET_LISTENER_OPEN) &&
		 (pListener->WaitHead != NULL) ) {
		pWaiter = pListener->WaitHead;
		(void)__xrtNetListenerWaitRemove(pListener, pWaiter);
	}
	__xrtSpinUnlock(&pListener->AcceptLock);
	if ( pWaiter == NULL ) {
		return false;
	}
	__xrtNetListenerWaitFinish(pWaiter, pStream, false);
	return true;
}



/* 取消只移除当前 Accept 等待，不停止 Listener。 */
static void __xrtNetListenerWaitCancel(ptr pData)
{
	__xrt_net_listener_wait* pWaiter =
		(__xrt_net_listener_wait*)pData;
	xnetlistener* pListener = pWaiter->Listener;
	bool bRemoved;

	__xrtSpinLock(&pListener->AcceptLock);
	bRemoved = pWaiter->Linked &&
		__xrtNetListenerWaitRemove(pListener, pWaiter);
	__xrtSpinUnlock(&pListener->AcceptLock);
	if ( bRemoved ) {
		__xrtNetListenerWaitFinish(pWaiter, NULL, true);
	}
}



/* 按 FIFO 配对排队 Stream 与 Accept Future，并关闭终止后的等待。 */
void __xrtNetListenerFutureNotify(xnetlistener* pListener)
{
	for ( ;; ) {
		__xrt_net_listener_wait* pWaiter = NULL;
		xnetstream* pStream = NULL;

		__xrtSpinLock(&pListener->AcceptLock);
		if ( pListener->WaitHead != NULL ) {
			if ( xrtNetListenerState(pListener) !=
				 XNET_LISTENER_OPEN ) {
				pWaiter = pListener->WaitHead;
				(void)__xrtNetListenerWaitRemove(
					pListener,
					pWaiter
				);
			} else if ( pListener->AcceptHead != NULL ) {
				pWaiter = pListener->WaitHead;
				(void)__xrtNetListenerWaitRemove(
					pListener,
					pWaiter
				);
				pStream = __xrtNetListenerTakeQueued(pListener);
			}
		}
		__xrtSpinUnlock(&pListener->AcceptLock);
		if ( pWaiter == NULL ) {
			break;
		}
		__xrtNetListenerWaitFinish(
			pWaiter,
			pStream,
			false
		);
	}
}



/* 建立一个 Listener 拉取 Accept Future。 */
XRT_API xfuture* xrtNetListenerAcceptAsync(xnetlistener* pListener)
{
	__xrt_net_listener_wait* pWaiter;
	xnetlistener* pOwned;
	xfuture* pFuture = NULL;
	xcancel* pCancel;
	xerror* pError;
	xnetstream* pStream = NULL;
	xnetworker* pCacheWorker = NULL;
	bool bClosed = false;

	if ( pListener == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( pListener->Events.Accept != NULL ) {
		__xrtNetSetError(
			XERR_STATE,
			XNET_ERROR_LISTENER_ACCEPT,
			"accept-listener",
			"TCP accept Future requires a listener without an Accept callback",
			0
		);
		return NULL;
	}
	pOwned = xrtNetListenerRef(pListener);
	if ( pOwned == NULL ) {
		return NULL;
	}
	__xrtSpinLock(&pOwned->AcceptLock);
	if ( !pOwned->WaitClosed ) {
		pCacheWorker = pOwned->Worker;
		if ( !__xrtNetEngineObjectHold(pOwned->Engine) ) {
			__xrtSpinUnlock(&pOwned->AcceptLock);
			xrtNetListenerDestroy(pOwned);
			return NULL;
		}
	}
	__xrtSpinUnlock(&pOwned->AcceptLock);
	pWaiter = __xrtNetListenerWaitAlloc(pCacheWorker);
	if ( pWaiter == NULL ) {
		xrtNetListenerDestroy(pOwned);
		return NULL;
	}
	pWaiter->Listener = pOwned;
	pWaiter->Promise = xrtPromiseCreate(&pFuture, NULL);
	if ( pWaiter->Promise == NULL ) {
		__xrtNetListenerWaitRecycle(pListener, pWaiter);
		xrtNetListenerDestroy(pListener);
		return NULL;
	}
	pCancel = xrtPromiseCancelToken(pWaiter->Promise);
	if ( pCancel != NULL ) {
		pWaiter->Watch = xrtCancelWatch(
			pCancel,
			__xrtNetListenerWaitCancel,
			pWaiter
		);
		xrtCancelDestroy(pCancel);
	}
	if ( pWaiter->Watch == NULL ) {
		pError = xrtTakeError();
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(pWaiter->Promise);
		__xrtNetListenerWaitRecycle(pListener, pWaiter);
		xrtNetListenerDestroy(pListener);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return NULL;
	}
	__xrtSpinLock(&pListener->AcceptLock);
	if ( xrtNetListenerState(pListener) != XNET_LISTENER_OPEN ) {
		bClosed = true;
	} else {
		if ( pListener->WaitHead == NULL ) {
			pStream = __xrtNetListenerTakeQueued(pListener);
		}
		if ( pStream == NULL ) {
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
	__xrtSpinUnlock(&pListener->AcceptLock);
	if ( (pStream != NULL) || bClosed ) {
		__xrtNetListenerWaitFinish(
			pWaiter,
			pStream,
			false
		);
	}
	return pFuture;
}



/* 返回 Stream 终止原因对应的统一 Future 结果。 */
static __xrt_net_stream_wait_result __xrtNetStreamWaitTerminal(
	const xnetstream* pStream
)
{
	if ( pStream->CloseResult == XNET_RESULT_CANCELLED ) {
		return __XRT_NET_STREAM_WAIT_CANCELLED;
	}
	if ( (pStream->Error != NULL) ||
		 (pStream->CloseResult == XNET_RESULT_ERROR) ||
		 (pStream->CloseResult == XNET_RESULT_TIMEOUT) ) {
		return __XRT_NET_STREAM_WAIT_FAILED;
	}
	return __XRT_NET_STREAM_WAIT_CLOSED;
}



/* 在 Stream Worker 上判断一个等待条件是否已经进入终态。 */
static __xrt_net_stream_wait_result __xrtNetStreamWaitResult(
	const __xrt_net_stream_wait* pWaiter
)
{
	const xnetstream* pStream = pWaiter->Stream;
	xnetstreamstate State = xrtNetStreamState(pStream);
	size_t iQueued = xrtNetStreamPending(pStream);

	if ( pWaiter->Wait == XNET_STREAM_WAIT_OPEN ) {
		if ( State == XNET_STREAM_OPEN ) {
			return __XRT_NET_STREAM_WAIT_READY;
		}
		if ( State >= XNET_STREAM_CLOSING ) {
			return __xrtNetStreamWaitTerminal(pStream);
		}
		return __XRT_NET_STREAM_WAIT_PENDING;
	}
	if ( pWaiter->Wait == XNET_STREAM_WAIT_READ ) {
		if ( xrtNetBufSize(&pStream->ReadBuffer) >=
			 pWaiter->MinimumBytes ) {
			return pWaiter->Receive ?
				__XRT_NET_STREAM_WAIT_RECEIVE :
				__XRT_NET_STREAM_WAIT_READY;
		}
		if ( xrtAtomic32Load(
			&pStream->ReadEnded,
			XMEMORY_ACQUIRE
		) || (State >= XNET_STREAM_CLOSING) ) {
			return __xrtNetStreamWaitTerminal(pStream);
		}
		return __XRT_NET_STREAM_WAIT_PENDING;
	}
	if ( pWaiter->Wait == XNET_STREAM_WAIT_WRITE ) {
		if ( (State == XNET_STREAM_OPEN) &&
			 !xrtAtomic32Load(
				&pStream->WriteGate,
				XMEMORY_ACQUIRE
			 ) && !xrtAtomic32Load(
				&pStream->WriteEnded,
				XMEMORY_ACQUIRE
			 ) && ((iQueued <= pStream->Config.WriteLowWater) ||
			 (!xrtAtomic32Load(
				&pStream->WriteBackpressured,
				XMEMORY_ACQUIRE
			 ) && (iQueued < pStream->Config.WriteHighWater))) ) {
			return __XRT_NET_STREAM_WAIT_READY;
		}
		if ( (State >= XNET_STREAM_CLOSING) ||
			 xrtAtomic32Load(
				&pStream->WriteGate,
				XMEMORY_ACQUIRE
			 ) || xrtAtomic32Load(
				&pStream->WriteEnded,
				XMEMORY_ACQUIRE
			 ) ) {
			return __xrtNetStreamWaitTerminal(pStream);
		}
		return __XRT_NET_STREAM_WAIT_PENDING;
	}
	if ( pWaiter->Wait == XNET_STREAM_WAIT_DRAIN ) {
		if ( ((State >= XNET_STREAM_CLOSING) ||
			  pStream->AbortRequested) &&
			 (pStream->CloseResult != XNET_RESULT_OK) ) {
			return __xrtNetStreamWaitTerminal(pStream);
		}
		if ( iQueued == 0 ) {
			return __XRT_NET_STREAM_WAIT_READY;
		}
		if ( State == XNET_STREAM_CLOSED ) {
			return __xrtNetStreamWaitTerminal(pStream);
		}
		return __XRT_NET_STREAM_WAIT_PENDING;
	}
	if ( State == XNET_STREAM_CLOSED ) {
		__xrt_net_stream_wait_result Result =
			__xrtNetStreamWaitTerminal(pStream);

		return Result == __XRT_NET_STREAM_WAIT_CLOSED ?
			__XRT_NET_STREAM_WAIT_READY : Result;
	}
	return __XRT_NET_STREAM_WAIT_PENDING;
}



/* 为没有结构化原因的异常终止补充一个与等待操作匹配的稳定错误。 */
static xerror* __xrtNetStreamWaitError(
	const __xrt_net_stream_wait* pWaiter
)
{
	const xnetstream* pStream = pWaiter->Stream;
	int32 iCode;
	cstr sMessage;

	if ( pStream->Error != NULL ) {
		return xrtErrorRef(pStream->Error);
	}
	if ( pWaiter->Wait == XNET_STREAM_WAIT_OPEN ) {
		iCode = XNET_ERROR_STREAM_CONNECT;
		sMessage = "TCP stream open wait failed";
	} else if ( pWaiter->Wait == XNET_STREAM_WAIT_READ ) {
		iCode = XNET_ERROR_STREAM_READ;
		sMessage = "TCP stream read wait failed";
	} else if ( (pWaiter->Wait == XNET_STREAM_WAIT_WRITE) ||
		 (pWaiter->Wait == XNET_STREAM_WAIT_DRAIN) ) {
		iCode = XNET_ERROR_STREAM_WRITE;
		sMessage = "TCP stream write wait failed";
	} else {
		iCode = XNET_ERROR_STREAM_CLOSE;
		sMessage = "TCP stream close wait failed";
	}
	return xrtErrorCreate(
		pStream->CloseResult == XNET_RESULT_TIMEOUT ?
			XERR_TIMEOUT : XERR_IO,
		"xrt.net",
		iCode,
		sMessage
	);
}



/* 从等待链表中移除指定节点；调用方持有 WaitLock。 */
static bool __xrtNetStreamWaitRemove(
	xnetstream* pStream,
	__xrt_net_stream_wait* pWaiter
)
{
	__xrt_net_stream_wait** ppCurrent = &pStream->WaitHead;
	__xrt_net_stream_wait* pPrevious = NULL;

	while ( (*ppCurrent != NULL) && (*ppCurrent != pWaiter) ) {
		pPrevious = *ppCurrent;
		ppCurrent = &(*ppCurrent)->Next;
	}
	if ( *ppCurrent == NULL ) {
		return false;
	}
	*ppCurrent = pWaiter->Next;
	if ( pStream->WaitTail == pWaiter ) {
		pStream->WaitTail = pPrevious;
	}
	pWaiter->Next = NULL;
	pWaiter->Linked = false;
	if ( pWaiter->Wait == XNET_STREAM_WAIT_READ ) {
		pStream->ReadWaiters--;
	}
	return true;
}



static void __xrtNetStreamWaitTask(xnetworker* pWorker, ptr pData);



/* 用 Stream 内嵌命令无分配地唤醒所属 Worker；调用方持有 WaitLock。 */
static void __xrtNetStreamWaitSchedule(xnetstream* pStream)
{
	if ( pStream->WaitClosed || pStream->WaitPosted ||
		 (pStream->WaitHead == NULL) ) {
		return;
	}
	pStream->WaitPosted = true;
	(void)xrtNetStreamRef(pStream);
	__xrtNetEnginePostInternal(
		pStream->Worker,
		&pStream->WaitCommand,
		__xrtNetStreamWaitTask,
		pStream
	);
}



/* 取走链表中第一个已经满足或终止的等待节点。 */
static __xrt_net_stream_wait* __xrtNetStreamWaitTake(
	xnetstream* pStream,
	__xrt_net_stream_wait_result* pResult
)
{
	__xrt_net_stream_wait* pWaiter;

	__xrtSpinLock(&pStream->WaitLock);
	for ( pWaiter = pStream->WaitHead;
		pWaiter != NULL; pWaiter = pWaiter->Next ) {
		*pResult = __xrtNetStreamWaitResult(pWaiter);
		if ( *pResult != __XRT_NET_STREAM_WAIT_PENDING ) {
			(void)__xrtNetStreamWaitRemove(pStream, pWaiter);
			break;
		}
	}
	__xrtSpinUnlock(&pStream->WaitLock);
	return pWaiter;
}



/* 准备一份当前可用字节，在发布 Future 前完成底层读取。 */
static xnetbytes* __xrtNetStreamWaitReceive(
	__xrt_net_stream_wait* pWaiter,
	bool* pConsumed,
	xerror** ppError
)
{
	xnetstream* pStream = pWaiter->Stream;
	size_t iSize = xrtNetBufSize(&pStream->ReadBuffer);
	xnetbytes* pBytes;
	xnetwspan Span;

	*pConsumed = false;
	*ppError = NULL;

	if ( (pWaiter->MaxBytes != 0) && (iSize > pWaiter->MaxBytes) ) {
		iSize = pWaiter->MaxBytes;
	}
	pBytes = __xrtNetBytesAlloc(iSize, &Span);
	if ( pBytes == NULL ) {
		*ppError = xrtTakeError();
		return NULL;
	}
	if ( xrtNetBufRead(
		&pStream->ReadBuffer,
		Span.Data,
		iSize
	) != iSize ) {
		xrtNetBytesDestroy(pBytes);
		__xrtNetSetError(
			XERR_INTERNAL,
			XNET_ERROR_STREAM_READ,
			"receive-stream",
			"TCP receive buffer changed outside its worker",
			0
		);
		*ppError = xrtTakeError();
		return NULL;
	}
	*pConsumed = true;
	__xrtNetStreamReadRefresh(pStream, false);
	return pBytes;
}



/* 完成一个已从 Stream 取走的等待节点并释放全部持有资源。 */
static bool __xrtNetStreamWaitFinish(
	__xrt_net_stream_wait* pWaiter,
	__xrt_net_stream_wait_result Result
)
{
	xnetstream* pStream = pWaiter->Stream;
	xpromise* pPromise = pWaiter->Promise;
	xnetbytes* pBytes = NULL;
	xerror* pError = NULL;
	bool bConsumed = false;

	if ( Result == __XRT_NET_STREAM_WAIT_RECEIVE ) {
		pBytes = __xrtNetStreamWaitReceive(
			pWaiter,
			&bConsumed,
			&pError
		);
	} else if ( Result == __XRT_NET_STREAM_WAIT_FAILED ) {
		pError = __xrtNetStreamWaitError(pWaiter);
	}
	xrtCancelUnwatch(pWaiter->Watch);
	pWaiter->Watch = NULL;
	pWaiter->Promise = NULL;
	__xrtNetStreamWaitRecycle(pStream, pWaiter);
	xrtNetStreamDestroy(pStream);
	if ( Result == __XRT_NET_STREAM_WAIT_READY ) {
		(void)xrtPromiseResolve(pPromise, NULL);
	} else if ( Result == __XRT_NET_STREAM_WAIT_RECEIVE ) {
		if ( pBytes != NULL ) {
			if ( xrtPromiseResolveOwned(
				pPromise,
				pBytes,
				__xrtNetStreamBytesFree,
				NULL
			) ) {
				pBytes = NULL;
			}
		} else if ( pError != NULL ) {
			(void)xrtPromiseReject(pPromise, pError);
		} else {
			(void)xrtPromiseClose(pPromise);
		}
	} else if ( Result == __XRT_NET_STREAM_WAIT_FAILED ) {
		if ( pError != NULL ) {
			(void)xrtPromiseReject(pPromise, pError);
		} else {
			(void)xrtPromiseClose(pPromise);
		}
	} else if ( Result == __XRT_NET_STREAM_WAIT_CANCELLED ) {
		(void)xrtPromiseCancel(pPromise);
	} else {
		(void)xrtPromiseClose(pPromise);
	}
	xrtFree(pBytes);
	xrtErrorFree(pError);
	xrtPromiseDestroy(pPromise);
	return bConsumed;
}



/* 取消只移除当前等待，不关闭或中断底层 Stream。 */
static void __xrtNetStreamWaitCancel(ptr pData)
{
	__xrt_net_stream_wait* pWaiter =
		(__xrt_net_stream_wait*)pData;
	xnetstream* pStream = pWaiter->Stream;
	bool bRemoved;

	__xrtSpinLock(&pStream->WaitLock);
	bRemoved = pWaiter->Linked &&
		__xrtNetStreamWaitRemove(pStream, pWaiter);
	__xrtSpinUnlock(&pStream->WaitLock);
	if ( bRemoved ) {
		(void)__xrtNetStreamWaitFinish(
			pWaiter,
			__XRT_NET_STREAM_WAIT_CANCELLED
		);
	}
}



/* 在所属 Worker 上迭代完成全部当前可推进的等待。 */
void __xrtNetStreamFutureNotify(xnetstream* pStream, bool bDriveRead)
{
	bool bConsumed = false;

	if ( !xrtNetWorkerIsCurrent(pStream->Worker) ) {
		__xrtSpinLock(&pStream->WaitLock);
		__xrtNetStreamWaitSchedule(pStream);
		__xrtSpinUnlock(&pStream->WaitLock);
		return;
	}
	for ( ;; ) {
		__xrt_net_stream_wait_result Result;
		__xrt_net_stream_wait* pWaiter =
			__xrtNetStreamWaitTake(pStream, &Result);

		if ( pWaiter == NULL ) {
			break;
		}
		bConsumed = __xrtNetStreamWaitFinish(pWaiter, Result) ||
			bConsumed;
	}
	if ( bConsumed && bDriveRead ) {
		__xrtNetStreamReadRefresh(pStream, true);
	}
}



/* 内嵌命令进入 Worker 后允许下一次登记再次投递。 */
static void __xrtNetStreamWaitTask(xnetworker* pWorker, ptr pData)
{
	xnetstream* pStream = (xnetstream*)pData;

	(void)pWorker;
	__xrtSpinLock(&pStream->WaitLock);
	pStream->WaitPosted = false;
	__xrtSpinUnlock(&pStream->WaitLock);
	__xrtNetStreamFutureNotify(pStream, true);
	xrtNetStreamDestroy(pStream);
}



/* 建立 Promise、取消监听和 Stream 等待节点。 */
static xfuture* __xrtNetStreamWaitCreate(
	xnetstream* pStream,
	xnetstreamwait Wait,
	bool bReceive,
	size_t iMaxBytes,
	size_t iMinimumBytes
)
{
	__xrt_net_stream_wait* pWaiter;
	xnetstream* pOwned;
	xfuture* pFuture = NULL;
	xcancel* pCancel;
	xerror* pError;
	xnetworker* pCacheWorker = NULL;
	__xrt_net_stream_wait_result Immediate =
		__XRT_NET_STREAM_WAIT_PENDING;

	if ( (pStream == NULL) || (Wait < XNET_STREAM_WAIT_OPEN) ||
		 (Wait > XNET_STREAM_WAIT_CLOSE) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( (Wait == XNET_STREAM_WAIT_READ) &&
		 (iMinimumBytes > pStream->Config.ReadLimit) ) {
		__xrtNetSetError(
			XERR_RANGE,
			XNET_ERROR_STREAM_READ,
			"wait-stream-available",
			"minimum readable bytes exceed the TCP receive limit",
			0
		);
		return NULL;
	}
	pOwned = xrtNetStreamRef(pStream);
	if ( pOwned == NULL ) {
		return NULL;
	}
	__xrtSpinLock(&pOwned->WaitLock);
	if ( !pOwned->WaitClosed ) {
		pCacheWorker = pOwned->Worker;
		if ( !__xrtNetEngineObjectHold(pOwned->Engine) ) {
			__xrtSpinUnlock(&pOwned->WaitLock);
			xrtNetStreamDestroy(pOwned);
			return NULL;
		}
	}
	__xrtSpinUnlock(&pOwned->WaitLock);
	pWaiter = __xrtNetStreamWaitAlloc(pCacheWorker);
	if ( pWaiter == NULL ) {
		xrtNetStreamDestroy(pOwned);
		return NULL;
	}
	pWaiter->Stream = pOwned;
	pWaiter->Wait = Wait;
	pWaiter->Receive = bReceive;
	pWaiter->MaxBytes = iMaxBytes;
	pWaiter->MinimumBytes = iMinimumBytes;
	pWaiter->Promise = xrtPromiseCreate(&pFuture, NULL);
	if ( pWaiter->Promise == NULL ) {
		__xrtNetStreamWaitRecycle(pStream, pWaiter);
		xrtNetStreamDestroy(pStream);
		return NULL;
	}
	pCancel = xrtPromiseCancelToken(pWaiter->Promise);
	if ( pCancel != NULL ) {
		pWaiter->Watch = xrtCancelWatch(
			pCancel,
			__xrtNetStreamWaitCancel,
			pWaiter
		);
		xrtCancelDestroy(pCancel);
	}
	if ( pWaiter->Watch == NULL ) {
		pError = xrtTakeError();
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(pWaiter->Promise);
		__xrtNetStreamWaitRecycle(pStream, pWaiter);
		xrtNetStreamDestroy(pStream);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return NULL;
	}
	__xrtSpinLock(&pStream->WaitLock);
	if ( (Wait == XNET_STREAM_WAIT_READ) && pStream->ReadPush ) {
		__xrtSpinUnlock(&pStream->WaitLock);
		xrtCancelUnwatch(pWaiter->Watch);
		pWaiter->Watch = NULL;
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(pWaiter->Promise);
		__xrtNetStreamWaitRecycle(pStream, pWaiter);
		xrtNetStreamDestroy(pStream);
		__xrtNetSetError(
			XERR_STATE,
			XNET_ERROR_STREAM_READ,
			"wait-stream",
			"read waits require a stream without a Read callback",
			0
		);
		return NULL;
	}
	pWaiter->Linked = true;
	if ( Wait == XNET_STREAM_WAIT_READ ) {
		pStream->ReadWaiters++;
	}
	if ( pStream->WaitTail != NULL ) {
		pStream->WaitTail->Next = pWaiter;
	} else {
		pStream->WaitHead = pWaiter;
	}
	pStream->WaitTail = pWaiter;
	if ( pStream->WaitClosed ) {
		(void)__xrtNetStreamWaitRemove(pStream, pWaiter);
		Immediate = __xrtNetStreamWaitTerminal(pStream);
		if ( (Wait == XNET_STREAM_WAIT_CLOSE) &&
			 (Immediate == __XRT_NET_STREAM_WAIT_CLOSED) ) {
			Immediate = __XRT_NET_STREAM_WAIT_READY;
		} else if ( (Wait == XNET_STREAM_WAIT_DRAIN) &&
			 (Immediate == __XRT_NET_STREAM_WAIT_CLOSED) &&
			 (xrtNetStreamPending(pStream) == 0) ) {
			Immediate = __XRT_NET_STREAM_WAIT_READY;
		}
	} else {
		__xrtNetStreamWaitSchedule(pStream);
	}
	__xrtSpinUnlock(&pStream->WaitLock);
	if ( Immediate != __XRT_NET_STREAM_WAIT_PENDING ) {
		(void)__xrtNetStreamWaitFinish(pWaiter, Immediate);
	}
	return pFuture;
}



/* 建立一个统一 Stream 条件 Future。 */
XRT_API xfuture* xrtNetStreamWaitAsync(
	xnetstream* pStream,
	xnetstreamwait Wait
)
{
	return __xrtNetStreamWaitCreate(
		pStream,
		Wait,
		false,
		0,
		Wait == XNET_STREAM_WAIT_READ ? 1u : 0u
	);
}



/* 异步等待拉取缓冲增长到指定字节数，不消费当前前缀。 */
XRT_API xfuture* xrtNetStreamWaitAvailableAsync(
	xnetstream* pStream,
	size_t iMinimum
)
{
	return __xrtNetStreamWaitCreate(
		pStream,
		XNET_STREAM_WAIT_READ,
		false,
		0,
		iMinimum
	);
}



/* 建立一个拉取模式字节接收 Future。 */
XRT_API xfuture* xrtNetStreamRecvAsync(
	xnetstream* pStream,
	size_t iMaxBytes
)
{
	return __xrtNetStreamWaitCreate(
		pStream,
		XNET_STREAM_WAIT_READ,
		true,
		iMaxBytes,
		1u
	);
}

#endif

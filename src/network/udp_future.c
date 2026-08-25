#include "../internal/xrt_udp.h"



#if defined(XRT_FEATURE_NET_UDP_FUTURE)

#define XRT_NET_UDP_FUTURE_BATCH_MAX 256u



/* 内部等待结果只在线性化取走等待节点后使用。 */
typedef enum __xrt_net_udp_wait_result {
	__XRT_NET_UDP_WAIT_PENDING = 0,
	__XRT_NET_UDP_WAIT_READY,
	__XRT_NET_UDP_WAIT_RECEIVE,
	__XRT_NET_UDP_WAIT_ERROR,
	__XRT_NET_UDP_WAIT_FAILED,
	__XRT_NET_UDP_WAIT_CANCELLED,
	__XRT_NET_UDP_WAIT_CLOSED
} __xrt_net_udp_wait_result;



/* 批量结果拥有每个尚未转移的数据包。 */
struct xnetudpbatch {
	volatile int32 References;
	size_t Count;
	size_t Capacity;
	xnetudppacket* Packets[1];
};



/* 一个等待节点持有 UDP 和 Promise，直到唯一终态完成。 */
struct __xrt_net_udp_wait {
	__xrt_net_udp_wait* Next;
	__xrt_net_udp_wait* FinishNext;
	xnetudp* Udp;
	xnetworker* CacheWorker;
	xpromise* Promise;
	xcancelwatch* Watch;
	xnetudpbatch* Batch;
	xnetudppacket* Packet;
	xnetudperrorpacket* ErrorPacket;
	xnetudpwait Wait;
	__xrt_net_udp_wait_result Result;
	size_t RequiredBytes;
	bool Receive;
	bool Writable;
	bool Linked;
};

_Static_assert(
	sizeof(__xrt_net_udp_wait) <= 128u,
	"UDP Future waiter left the 128-byte node class"
);



/* 从 UDP 所属 Worker 的统一缓存取得一个等待节点。 */
static __xrt_net_udp_wait* __xrtNetUdpWaitAlloc(
	xnetworker* pWorker
)
{
	__xrt_net_udp_wait* pWaiter = pWorker != NULL ?
		(__xrt_net_udp_wait*)__xrtNetWorkerNodeAlloc(
			pWorker,
			sizeof(__xrt_net_udp_wait)
		) : (__xrt_net_udp_wait*)xrtCalloc(
			1,
			sizeof(__xrt_net_udp_wait)
		);

	if ( pWaiter != NULL ) {
		pWaiter->CacheWorker = pWorker;
	} else if ( pWorker != NULL ) {
		__xrtNetEngineObjectRelease(pWorker->Engine);
	}
	return pWaiter;
}



/* 在 UDP 引用释放前归还等待节点。 */
static void __xrtNetUdpWaitRecycle(
	xnetudp* pUdp,
	__xrt_net_udp_wait* pWaiter
)
{
	xnetworker* pWorker = pWaiter->CacheWorker;

	(void)pUdp;
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



/* 增加批量结果引用。 */
XRT_API xnetudpbatch* xrtNetUdpBatchRef(xnetudpbatch* pBatch)
{
	if ( (pBatch == NULL) ||
		 (xrtRefRetain(&pBatch->References) < 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return pBatch;
}



/* 销毁批量结果及其中仍由结果拥有的数据包。 */
XRT_API void xrtNetUdpBatchDestroy(xnetudpbatch* pBatch)
{
	if ( (pBatch == NULL) ||
		 (xrtRefRelease(&pBatch->References) != 0) ) {
		return;
	}
	for ( size_t i = 0; i < pBatch->Count; i++ ) {
		xrtNetUdpPacketDestroy(pBatch->Packets[i]);
	}
	xrtFree(pBatch);
}



/* 返回批量结果中的数据包数量。 */
XRT_API size_t xrtNetUdpBatchCount(const xnetudpbatch* pBatch)
{
	return pBatch != NULL ? pBatch->Count : 0;
}



/* 返回批量结果中一个借用的数据包。 */
XRT_API xnetudppacket* xrtNetUdpBatchPacket(
	const xnetudpbatch* pBatch,
	size_t iIndex
)
{
	if ( (pBatch == NULL) || (iIndex >= pBatch->Count) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return pBatch->Packets[iIndex];
}



/* 从批量结果转移一个数据包所有权。 */
XRT_API xnetudppacket* xrtNetUdpBatchTake(
	xnetudpbatch* pBatch,
	size_t iIndex
)
{
	xnetudppacket* pPacket;

	if ( (pBatch == NULL) || (iIndex >= pBatch->Count) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pPacket = pBatch->Packets[iIndex];
	pBatch->Packets[iIndex] = NULL;
	return pPacket;
}



/* 分配一个尚未包含数据包的批量结果。 */
static xnetudpbatch* __xrtNetUdpBatchCreate(size_t iCapacity)
{
	xnetudpbatch* pBatch;
	size_t iBase = offsetof(xnetudpbatch, Packets);
	size_t iAllocation;

	if ( iCapacity > ((SIZE_MAX - iBase) / sizeof(xnetudppacket*)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iAllocation = iBase + (iCapacity * sizeof(xnetudppacket*));
	pBatch = (xnetudpbatch*)xrtCalloc(1, iAllocation);
	if ( pBatch != NULL ) {
		pBatch->References = 1;
		pBatch->Capacity = iCapacity;
	}
	return pBatch;
}



/* Future 持有的数据包析构过程。 */
static void __xrtNetUdpPacketFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtNetUdpPacketDestroy((xnetudppacket*)pValue);
}



/* Future 持有的数据报错误包析构过程。 */
static void __xrtNetUdpErrorPacketFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtNetUdpErrorPacketDestroy((xnetudperrorpacket*)pValue);
}



/* Future 持有的批量结果析构过程。 */
static void __xrtNetUdpBatchFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtNetUdpBatchDestroy((xnetudpbatch*)pValue);
}



/* 从等待链表移除指定节点；调用方持有 ReceiveLock。 */
static bool __xrtNetUdpWaitRemove(
	xnetudp* pUdp,
	__xrt_net_udp_wait* pWaiter
)
{
	__xrt_net_udp_wait** ppHead;
	__xrt_net_udp_wait** ppTail;
	__xrt_net_udp_wait** ppCurrent;
	__xrt_net_udp_wait* pPrevious = NULL;

	if ( pWaiter->Receive && (pWaiter->Wait == XNET_UDP_WAIT_ERROR) ) {
		ppHead = &pUdp->Errors->WaitHead;
		ppTail = &pUdp->Errors->WaitTail;
	} else if ( pWaiter->Receive ) {
		ppHead = &pUdp->ReceiveFutureHead;
		ppTail = &pUdp->ReceiveFutureTail;
	} else {
		ppHead = &pUdp->WaitHead;
		ppTail = &pUdp->WaitTail;
	}
	ppCurrent = ppHead;

	while ( (*ppCurrent != NULL) && (*ppCurrent != pWaiter) ) {
		pPrevious = *ppCurrent;
		ppCurrent = &(*ppCurrent)->Next;
	}
	if ( *ppCurrent == NULL ) {
		return false;
	}
	*ppCurrent = pWaiter->Next;
	if ( *ppTail == pWaiter ) {
		*ppTail = pPrevious;
	}
	pWaiter->Next = NULL;
	pWaiter->Linked = false;
	if ( pWaiter->Receive && (pWaiter->Wait == XNET_UDP_WAIT_ERROR) ) {
		(void)xrtAtomic64FetchSub(
			&pUdp->Errors->ErrorWaiters,
			1,
			XMEMORY_ACQ_REL
		);
	} else if ( pWaiter->Receive ) {
		(void)xrtAtomic64FetchSub(
			&pUdp->ReceiveWaiters,
			1,
			XMEMORY_ACQ_REL
		);
	} else if ( !pWaiter->Writable &&
		 (pWaiter->Wait == XNET_UDP_WAIT_ERROR) ) {
		pUdp->Errors->ReadyWaiters--;
	} else if ( !pWaiter->Writable &&
		 (pWaiter->Wait == XNET_UDP_WAIT_RECEIVE) ) {
		pUdp->ReceiveReadyWaiters--;
	}
	return true;
}



/* 按用途把等待节点挂入条件链或消费式接收链；调用方持有 ReceiveLock。 */
static void __xrtNetUdpWaitAppendLocked(
	xnetudp* pUdp,
	__xrt_net_udp_wait* pWaiter
)
{
	__xrt_net_udp_wait** ppHead;
	__xrt_net_udp_wait** ppTail;

	if ( pWaiter->Receive && (pWaiter->Wait == XNET_UDP_WAIT_ERROR) ) {
		ppHead = &pUdp->Errors->WaitHead;
		ppTail = &pUdp->Errors->WaitTail;
	} else if ( pWaiter->Receive ) {
		ppHead = &pUdp->ReceiveFutureHead;
		ppTail = &pUdp->ReceiveFutureTail;
	} else {
		ppHead = &pUdp->WaitHead;
		ppTail = &pUdp->WaitTail;
	}

	pWaiter->Linked = true;
	if ( *ppTail != NULL ) {
		(*ppTail)->Next = pWaiter;
	} else {
		*ppHead = pWaiter;
	}
	*ppTail = pWaiter;
	if ( pWaiter->Receive && (pWaiter->Wait == XNET_UDP_WAIT_ERROR) ) {
		(void)xrtAtomic64FetchAdd(
			&pUdp->Errors->ErrorWaiters,
			1,
			XMEMORY_ACQ_REL
		);
	} else if ( pWaiter->Receive ) {
		(void)xrtAtomic64FetchAdd(
			&pUdp->ReceiveWaiters,
			1,
			XMEMORY_ACQ_REL
		);
	} else if ( !pWaiter->Writable &&
		 (pWaiter->Wait == XNET_UDP_WAIT_ERROR) ) {
		pUdp->Errors->ReadyWaiters++;
	} else if ( !pWaiter->Writable &&
		 (pWaiter->Wait == XNET_UDP_WAIT_RECEIVE) ) {
		pUdp->ReceiveReadyWaiters++;
	}
}



/* 从拉取队列取走一个数据包；调用方持有 ReceiveLock。 */
static xnetudppacket* __xrtNetUdpTakeReceiveLocked(xnetudp* pUdp)
{
	xnetudppacket* pPacket = pUdp->ReceiveHead;

	if ( pPacket == NULL ) {
		return NULL;
	}
	pUdp->ReceiveHead = pPacket->Next;
	if ( pUdp->ReceiveHead == NULL ) {
		pUdp->ReceiveTail = NULL;
	}
	pPacket->Next = NULL;
	(void)xrtAtomic64FetchSub(
		&pUdp->ReceiveQueued,
		1,
		XMEMORY_ACQ_REL
	);
	(void)xrtAtomic64FetchSub(
		&pUdp->ReceiveQueuedBytes,
		(uint64)pPacket->Size,
		XMEMORY_ACQ_REL
	);
	return pPacket;
}



/* 把完成节点压入锁外终结链。 */
static void __xrtNetUdpWaitFinishPush(
	__xrt_net_udp_wait** ppHead,
	__xrt_net_udp_wait* pWaiter,
	__xrt_net_udp_wait_result Result
)
{
	pWaiter->Result = Result;
	pWaiter->FinishNext = *ppHead;
	*ppHead = pWaiter;
}



/* 调用方持有 ReceiveLock 时，按 FIFO 为拉取 Future 分配排队数据包。 */
__xrt_net_udp_wait* __xrtNetUdpFuturePairReceiveLocked(xnetudp* pUdp)
{
	__xrt_net_udp_wait* pFinished = NULL;

	if ( xrtAtomic32Load(
		&pUdp->CloseGate,
		XMEMORY_ACQUIRE
	) ) {
		return NULL;
	}

	while ( (pUdp->ReceiveHead != NULL) &&
		 (pUdp->ReceiveFutureHead != NULL) ) {
		__xrt_net_udp_wait* pWaiter = pUdp->ReceiveFutureHead;

		(void)__xrtNetUdpWaitRemove(pUdp, pWaiter);
		if ( pWaiter->Batch != NULL ) {
			while ( (pWaiter->Batch->Count < pWaiter->Batch->Capacity) &&
				 (pUdp->ReceiveHead != NULL) ) {
				pWaiter->Batch->Packets[pWaiter->Batch->Count++] =
					__xrtNetUdpTakeReceiveLocked(pUdp);
			}
		} else {
			pWaiter->Packet = __xrtNetUdpTakeReceiveLocked(pUdp);
		}
		__xrtNetUdpWaitFinishPush(
			&pFinished,
			pWaiter,
			__XRT_NET_UDP_WAIT_RECEIVE
		);
	}
	return pFinished;
}



/* 调用方持有 ReceiveLock 时，按 FIFO 为拉取 Future 分配排队错误。 */
__xrt_net_udp_wait* __xrtNetUdpFuturePairErrorLocked(xnetudp* pUdp)
{
	__xrt_net_udp_wait* pFinished = NULL;
	__xrt_net_udp_error_state* pState = pUdp->Errors;

	if ( (pState == NULL) || xrtAtomic32Load(
		&pUdp->CloseGate,
		XMEMORY_ACQUIRE
	) ) {
		return NULL;
	}
	while ( (pState->Head != NULL) && (pState->WaitHead != NULL) ) {
		__xrt_net_udp_wait* pWaiter = pState->WaitHead;

		(void)__xrtNetUdpWaitRemove(pUdp, pWaiter);
		pWaiter->ErrorPacket = __xrtNetUdpTakeErrorLocked(pUdp);
		__xrtNetUdpWaitFinishPush(
			&pFinished,
			pWaiter,
			__XRT_NET_UDP_WAIT_ERROR
		);
	}
	return pFinished;
}



/* 返回 UDP 终止原因对应的统一 Future 结果。 */
static __xrt_net_udp_wait_result __xrtNetUdpWaitTerminal(
	const xnetudp* pUdp
)
{
	if ( xrtNetUdpState(pUdp) != XNET_UDP_CLOSED ) {
		return __XRT_NET_UDP_WAIT_PENDING;
	}
	if ( pUdp->CloseResult == XNET_RESULT_CANCELLED ) {
		return __XRT_NET_UDP_WAIT_CANCELLED;
	}
	if ( (pUdp->Error != NULL) ||
		 (pUdp->CloseResult == XNET_RESULT_ERROR) ||
		 (pUdp->CloseResult == XNET_RESULT_TIMEOUT) ) {
		return __XRT_NET_UDP_WAIT_FAILED;
	}
	return __XRT_NET_UDP_WAIT_CLOSED;
}



/* 调用方持有 ReceiveLock 时判断一个等待条件。 */
static __xrt_net_udp_wait_result __xrtNetUdpWaitResult(
	const __xrt_net_udp_wait* pWaiter
)
{
	const xnetudp* pUdp = pWaiter->Udp;
	xnetudpstate State = xrtNetUdpState(pUdp);
	uint64 iQueuedBytes = xrtAtomic64Load(
		&pUdp->QueuedBytes,
		XMEMORY_ACQUIRE
	);
	uint64 iQueuedPackets = xrtAtomic64Load(
		&pUdp->QueuedPackets,
		XMEMORY_ACQUIRE
	);
	bool bClosing = (State >= XNET_UDP_CLOSING) ||
		(xrtAtomic32Load(
			&pUdp->CloseGate,
			XMEMORY_ACQUIRE
		) != 0);
	bool bAborting = xrtAtomic32Load(
		&pUdp->AbortGate,
		XMEMORY_ACQUIRE
	) != 0;

	if ( pWaiter->Writable ) {
		if ( (State == XNET_UDP_OPEN) && !bClosing &&
			 (iQueuedPackets < (uint64)pUdp->Config.SendPacketLimit) &&
			 ((uint64)pWaiter->RequiredBytes <=
			  ((uint64)pUdp->Config.SendLimit - iQueuedBytes)) ) {
			return __XRT_NET_UDP_WAIT_READY;
		}
		if ( State >= XNET_UDP_CLOSING ) {
			return __xrtNetUdpWaitTerminal(pUdp);
		}
		return __XRT_NET_UDP_WAIT_PENDING;
	}
	if ( pWaiter->Wait == XNET_UDP_WAIT_OPEN ) {
		if ( (State == XNET_UDP_OPEN) && !bClosing ) {
			return __XRT_NET_UDP_WAIT_READY;
		}
		if ( State == XNET_UDP_CLOSED ) {
			return __xrtNetUdpWaitTerminal(pUdp);
		}
		return __XRT_NET_UDP_WAIT_PENDING;
	}
	if ( pWaiter->Wait == XNET_UDP_WAIT_RECEIVE ) {
		if ( !bClosing && (pUdp->ReceiveHead != NULL) ) {
			return pWaiter->Receive ?
				__XRT_NET_UDP_WAIT_RECEIVE :
				__XRT_NET_UDP_WAIT_READY;
		}
		if ( State == XNET_UDP_CLOSED ) {
			return __xrtNetUdpWaitTerminal(pUdp);
		}
		return __XRT_NET_UDP_WAIT_PENDING;
	}
	if ( pWaiter->Wait == XNET_UDP_WAIT_ERROR ) {
		if ( !bClosing && (pUdp->Errors != NULL) &&
			 (pUdp->Errors->Head != NULL) ) {
			return pWaiter->Receive ?
				__XRT_NET_UDP_WAIT_ERROR :
				__XRT_NET_UDP_WAIT_READY;
		}
		if ( State == XNET_UDP_CLOSED ) {
			return __xrtNetUdpWaitTerminal(pUdp);
		}
		return __XRT_NET_UDP_WAIT_PENDING;
	}
	if ( pWaiter->Wait == XNET_UDP_WAIT_DRAIN ) {
		if ( bAborting ) {
			return State == XNET_UDP_CLOSED ?
				__xrtNetUdpWaitTerminal(pUdp) :
				__XRT_NET_UDP_WAIT_PENDING;
		}
		if ( iQueuedPackets == 0 ) {
			return __XRT_NET_UDP_WAIT_READY;
		}
		if ( State == XNET_UDP_CLOSED ) {
			return __xrtNetUdpWaitTerminal(pUdp);
		}
		return __XRT_NET_UDP_WAIT_PENDING;
	}
	if ( State == XNET_UDP_CLOSED ) {
		__xrt_net_udp_wait_result Result = __xrtNetUdpWaitTerminal(pUdp);

		return Result == __XRT_NET_UDP_WAIT_CLOSED ?
			__XRT_NET_UDP_WAIT_READY : Result;
	}
	return __XRT_NET_UDP_WAIT_PENDING;
}



/* 为没有结构化原因的异常终止补充稳定错误。 */
static xerror* __xrtNetUdpWaitError(const __xrt_net_udp_wait* pWaiter)
{
	const xnetudp* pUdp = pWaiter->Udp;
	int32 iCode;
	cstr sMessage;

	if ( pUdp->Error != NULL ) {
		return xrtErrorRef(pUdp->Error);
	}
	if ( pWaiter->Wait == XNET_UDP_WAIT_OPEN ) {
		iCode = XNET_ERROR_UDP_CREATE;
		sMessage = "UDP open wait failed";
	} else if ( pWaiter->Wait == XNET_UDP_WAIT_RECEIVE ) {
		iCode = XNET_ERROR_UDP_RECEIVE;
		sMessage = "UDP receive wait failed";
	} else if ( pWaiter->Wait == XNET_UDP_WAIT_ERROR ) {
		iCode = XNET_ERROR_UDP_RECEIVE;
		sMessage = "UDP datagram error wait failed";
	} else if ( pWaiter->Wait == XNET_UDP_WAIT_DRAIN ||
		 pWaiter->Writable ) {
		iCode = XNET_ERROR_UDP_SEND;
		sMessage = "UDP send wait failed";
	} else {
		iCode = XNET_ERROR_UDP_CLOSE;
		sMessage = "UDP close wait failed";
	}
	return xrtErrorCreate(
		pUdp->CloseResult == XNET_RESULT_TIMEOUT ? XERR_TIMEOUT : XERR_IO,
		"xrt.net",
		iCode,
		sMessage
	);
}



/* 终结一个已经从等待链取出的节点并释放全部持有资源。 */
static void __xrtNetUdpWaitFinish(__xrt_net_udp_wait* pWaiter)
{
	xnetudp* pUdp = pWaiter->Udp;
	xpromise* pPromise = pWaiter->Promise;
	xnetudppacket* pPacket = pWaiter->Packet;
	xnetudperrorpacket* pErrorPacket = pWaiter->ErrorPacket;
	xnetudpbatch* pBatch = pWaiter->Batch;
	__xrt_net_udp_wait_result Result = pWaiter->Result;
	xerror* pError = NULL;

	if ( Result == __XRT_NET_UDP_WAIT_FAILED ) {
		pError = __xrtNetUdpWaitError(pWaiter);
	}
	pWaiter->Packet = NULL;
	pWaiter->ErrorPacket = NULL;
	pWaiter->Batch = NULL;
	xrtCancelUnwatch(pWaiter->Watch);
	pWaiter->Watch = NULL;
	pWaiter->Promise = NULL;
	__xrtNetUdpWaitRecycle(pUdp, pWaiter);
	xrtNetUdpDestroy(pUdp);
	if ( Result == __XRT_NET_UDP_WAIT_READY ) {
		(void)xrtPromiseResolve(pPromise, NULL);
	} else if ( Result == __XRT_NET_UDP_WAIT_RECEIVE ) {
		if ( pBatch != NULL ) {
			if ( xrtPromiseResolveOwned(
				pPromise,
				pBatch,
				__xrtNetUdpBatchFree,
				NULL
			) ) {
				pBatch = NULL;
			}
		} else {
			if ( xrtPromiseResolveOwned(
				pPromise,
				pPacket,
				__xrtNetUdpPacketFree,
				NULL
			) ) {
				pPacket = NULL;
			}
		}
	} else if ( Result == __XRT_NET_UDP_WAIT_ERROR ) {
		if ( xrtPromiseResolveOwned(
			pPromise,
			pErrorPacket,
			__xrtNetUdpErrorPacketFree,
			NULL
		) ) {
			pErrorPacket = NULL;
		}
	} else if ( Result == __XRT_NET_UDP_WAIT_FAILED ) {
		if ( pError != NULL ) {
			(void)xrtPromiseReject(pPromise, pError);
		} else {
			(void)xrtPromiseClose(pPromise);
		}
	} else if ( Result == __XRT_NET_UDP_WAIT_CANCELLED ) {
		(void)xrtPromiseCancel(pPromise);
	} else {
		(void)xrtPromiseClose(pPromise);
	}
	xrtNetUdpPacketDestroy(pPacket);
	xrtNetUdpErrorPacketDestroy(pErrorPacket);
	xrtNetUdpBatchDestroy(pBatch);
	xrtErrorFree(pError);
	xrtPromiseDestroy(pPromise);
}



/* 完成一条已经在线性化点取出的 Future 链。 */
void __xrtNetUdpFutureFinishList(__xrt_net_udp_wait* pWaiter)
{
	while ( pWaiter != NULL ) {
		__xrt_net_udp_wait* pNext = pWaiter->FinishNext;

		pWaiter->FinishNext = NULL;
		__xrtNetUdpWaitFinish(pWaiter);
		pWaiter = pNext;
	}
}



static void __xrtNetUdpWaitTask(xnetworker* pWorker, ptr pData);



/* 用 UDP 内嵌命令无分配地唤醒所属 Worker；调用方持有 ReceiveLock。 */
static void __xrtNetUdpWaitScheduleLocked(xnetudp* pUdp)
{
	bool bErrorWait = (pUdp->Errors != NULL) &&
		(pUdp->Errors->WaitHead != NULL);

	if ( pUdp->WaitClosed || pUdp->WaitPosted ||
		 ((pUdp->WaitHead == NULL) &&
		  (pUdp->ReceiveFutureHead == NULL) &&
		  !bErrorWait) ) {
		return;
	}
	pUdp->WaitPosted = true;
	(void)xrtNetUdpRef(pUdp);
	if ( !__xrtNetEnginePostInternal(
		pUdp->Worker,
		&pUdp->WaitCommand,
		__xrtNetUdpWaitTask,
		pUdp
	) ) {
		pUdp->WaitPosted = false;
		xrtNetUdpDestroy(pUdp);
	}
}



/* 推进所有当前可满足或已经终止的 UDP Future。 */
void __xrtNetUdpFutureNotify(xnetudp* pUdp)
{
	__xrt_net_udp_wait* pFinished = NULL;
	__xrt_net_udp_wait* pErrorFinished = NULL;
	__xrt_net_udp_wait* pWaiter;

	if ( !pUdp->ReceiveLockReady ) {
		return;
	}
	if ( !xrtNetWorkerIsCurrent(pUdp->Worker) ) {
		__xrtSpinLock(&pUdp->ReceiveLock);
		__xrtNetUdpWaitScheduleLocked(pUdp);
		__xrtSpinUnlock(&pUdp->ReceiveLock);
		return;
	}
	__xrtSpinLock(&pUdp->ReceiveLock);
	pFinished = __xrtNetUdpFuturePairReceiveLocked(pUdp);
	pErrorFinished = __xrtNetUdpFuturePairErrorLocked(pUdp);
	pWaiter = pUdp->WaitHead;
	while ( pWaiter != NULL ) {
		__xrt_net_udp_wait* pNext = pWaiter->Next;
		__xrt_net_udp_wait_result Result = __xrtNetUdpWaitResult(pWaiter);

		if ( Result != __XRT_NET_UDP_WAIT_PENDING ) {
			(void)__xrtNetUdpWaitRemove(pUdp, pWaiter);
			__xrtNetUdpWaitFinishPush(&pFinished, pWaiter, Result);
		}
		pWaiter = pNext;
	}
	pWaiter = pUdp->ReceiveFutureHead;
	while ( pWaiter != NULL ) {
		__xrt_net_udp_wait* pNext = pWaiter->Next;
		__xrt_net_udp_wait_result Result = __xrtNetUdpWaitResult(pWaiter);

		if ( Result != __XRT_NET_UDP_WAIT_PENDING ) {
			(void)__xrtNetUdpWaitRemove(pUdp, pWaiter);
			__xrtNetUdpWaitFinishPush(&pFinished, pWaiter, Result);
		}
		pWaiter = pNext;
	}
	if ( pUdp->Errors != NULL ) {
		pWaiter = pUdp->Errors->WaitHead;
		while ( pWaiter != NULL ) {
			__xrt_net_udp_wait* pNext = pWaiter->Next;
			__xrt_net_udp_wait_result Result =
				__xrtNetUdpWaitResult(pWaiter);

			if ( Result != __XRT_NET_UDP_WAIT_PENDING ) {
				(void)__xrtNetUdpWaitRemove(pUdp, pWaiter);
				__xrtNetUdpWaitFinishPush(
					&pErrorFinished,
					pWaiter,
					Result
				);
			}
			pWaiter = pNext;
		}
	}
	__xrtSpinUnlock(&pUdp->ReceiveLock);
	__xrtNetUdpFutureFinishList(pFinished);
	__xrtNetUdpFutureFinishList(pErrorFinished);
}



/* 内嵌命令进入 Worker 后允许下一次条件变化再次投递。 */
static void __xrtNetUdpWaitTask(xnetworker* pWorker, ptr pData)
{
	xnetudp* pUdp = (xnetudp*)pData;

	(void)pWorker;
	__xrtSpinLock(&pUdp->ReceiveLock);
	pUdp->WaitPosted = false;
	__xrtSpinUnlock(&pUdp->ReceiveLock);
	__xrtNetUdpFutureNotify(pUdp);
	xrtNetUdpDestroy(pUdp);
}



/* 取消只移除当前等待，不关闭 UDP 或丢弃已经排队的数据包。 */
static void __xrtNetUdpWaitCancel(ptr pData)
{
	__xrt_net_udp_wait* pWaiter = (__xrt_net_udp_wait*)pData;
	xnetudp* pUdp = pWaiter->Udp;
	bool bRemoved;

	__xrtSpinLock(&pUdp->ReceiveLock);
	bRemoved = pWaiter->Linked && __xrtNetUdpWaitRemove(pUdp, pWaiter);
	__xrtSpinUnlock(&pUdp->ReceiveLock);
	if ( bRemoved ) {
		pWaiter->Result = __XRT_NET_UDP_WAIT_CANCELLED;
		__xrtNetUdpFutureNotify(pUdp);
		__xrtNetUdpFutureFinishList(pWaiter);
	}
}



/* 判断拉取消费 Future 和可读条件 Future 是否发生不清晰的混用。 */
static bool __xrtNetUdpWaitModeConflictLocked(
	const xnetudp* pUdp,
	const __xrt_net_udp_wait* pCandidate
)
{
	if ( pCandidate->Receive &&
		 (pCandidate->Wait == XNET_UDP_WAIT_ERROR) ) {
		return pUdp->Errors->ReadyWaiters != 0;
	} else if ( pCandidate->Receive ) {
		return pUdp->ReceiveReadyWaiters != 0;
	} else if ( !pCandidate->Writable &&
		 (pCandidate->Wait == XNET_UDP_WAIT_ERROR) ) {
		return xrtAtomic64Load(
			&pUdp->Errors->ErrorWaiters,
			XMEMORY_RELAXED
		) != 0;
	} else if ( !pCandidate->Writable &&
		 (pCandidate->Wait == XNET_UDP_WAIT_RECEIVE) ) {
		return xrtAtomic64Load(
			&pUdp->ReceiveWaiters,
			XMEMORY_RELAXED
		) != 0;
	}
	return false;
}



/* 清理尚未挂入 UDP 的等待节点，同时保留调用方稍后设置的诊断。 */
static void __xrtNetUdpWaitDiscard(
	__xrt_net_udp_wait* pWaiter,
	xfuture* pFuture
)
{
	xnetudp* pUdp = pWaiter->Udp;

	xrtCancelUnwatch(pWaiter->Watch);
	pWaiter->Watch = NULL;
	xrtFutureDestroy(pFuture);
	xrtPromiseDestroy(pWaiter->Promise);
	xrtNetUdpBatchDestroy(pWaiter->Batch);
	__xrtNetUdpWaitRecycle(pUdp, pWaiter);
	xrtNetUdpDestroy(pUdp);
}



/* 建立 Promise、取消监听和 UDP 等待节点。 */
static xfuture* __xrtNetUdpWaitCreate(
	xnetudp* pUdp,
	xnetudpwait Wait,
	bool bReceive,
	bool bWritable,
	size_t iValue
)
{
	__xrt_net_udp_wait* pWaiter;
	__xrt_net_udp_wait* pFinished = NULL;
	xnetudp* pOwned;
	xfuture* pFuture = NULL;
	xcancel* pCancel;
	xerror* pError;
	xnetworker* pCacheWorker = NULL;
	bool bConflict;

	if ( (pUdp == NULL) || (Wait < XNET_UDP_WAIT_OPEN) ||
		 (Wait > XNET_UDP_WAIT_CLOSE) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( (Wait == XNET_UDP_WAIT_RECEIVE) &&
		 (pUdp->Events.Receive != NULL) ) {
		__xrtNetUdpSetError(
			XERR_STATE,
			XNET_ERROR_UDP_RECEIVE_QUEUE,
			"wait-udp-receive",
			"UDP receive Futures require pull mode without a Receive callback"
		);
		return NULL;
	}
	if ( (Wait == XNET_UDP_WAIT_ERROR) &&
		 ((pUdp->Errors == NULL) ||
		  (pUdp->Errors->Callback != NULL)) ) {
		__xrtNetUdpSetError(
			XERR_STATE,
			XNET_ERROR_UDP_RECEIVE_QUEUE,
			"wait-udp-error",
			"UDP error Futures require pulled datagram errors"
		);
		return NULL;
	}
	if ( bWritable && ((iValue > XNET_UDP_PAYLOAD_MAX) ||
		 (iValue > pUdp->Config.SendLimit)) ) {
		__xrtNetUdpSetError(
			XERR_RANGE,
			XNET_ERROR_UDP_SEND,
			"wait-udp-writable",
			"requested UDP datagram can never fit the configured send limit"
		);
		return NULL;
	}
	pOwned = xrtNetUdpRef(pUdp);
	if ( pOwned == NULL ) {
		return NULL;
	}
	__xrtSpinLock(&pOwned->ReceiveLock);
	if ( !pOwned->WaitClosed ) {
		pCacheWorker = pOwned->Worker;
		if ( !__xrtNetEngineObjectHold(pOwned->Engine) ) {
			__xrtSpinUnlock(&pOwned->ReceiveLock);
			xrtNetUdpDestroy(pOwned);
			return NULL;
		}
	}
	__xrtSpinUnlock(&pOwned->ReceiveLock);
	pWaiter = __xrtNetUdpWaitAlloc(pCacheWorker);
	if ( pWaiter == NULL ) {
		xrtNetUdpDestroy(pOwned);
		return NULL;
	}
	pWaiter->Udp = pOwned;
	if ( bReceive && (iValue != 0) ) {
		pWaiter->Batch = __xrtNetUdpBatchCreate(iValue);
		if ( pWaiter->Batch == NULL ) {
			__xrtNetUdpWaitRecycle(pUdp, pWaiter);
			xrtNetUdpDestroy(pOwned);
			return NULL;
		}
	}
	pWaiter->Wait = Wait;
	pWaiter->Receive = bReceive;
	pWaiter->Writable = bWritable;
	pWaiter->RequiredBytes = bWritable ? iValue : 0;
	pWaiter->Promise = xrtPromiseCreate(&pFuture, NULL);
	if ( pWaiter->Promise == NULL ) {
		xrtNetUdpBatchDestroy(pWaiter->Batch);
		__xrtNetUdpWaitRecycle(pUdp, pWaiter);
		xrtNetUdpDestroy(pOwned);
		return NULL;
	}
	pCancel = xrtPromiseCancelToken(pWaiter->Promise);
	if ( pCancel != NULL ) {
		pWaiter->Watch = xrtCancelWatch(
			pCancel,
			__xrtNetUdpWaitCancel,
			pWaiter
		);
		xrtCancelDestroy(pCancel);
	}
	if ( pWaiter->Watch == NULL ) {
		pError = xrtTakeError();
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(pWaiter->Promise);
		xrtNetUdpBatchDestroy(pWaiter->Batch);
		__xrtNetUdpWaitRecycle(pUdp, pWaiter);
		xrtNetUdpDestroy(pOwned);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return NULL;
	}
	__xrtSpinLock(&pUdp->ReceiveLock);
	bConflict = __xrtNetUdpWaitModeConflictLocked(pUdp, pWaiter);
	if ( !bConflict ) {
		__xrtNetUdpWaitAppendLocked(pUdp, pWaiter);
		if ( pWaiter->Receive ) {
			pFinished = (pWaiter->Wait == XNET_UDP_WAIT_ERROR) ?
				__xrtNetUdpFuturePairErrorLocked(pUdp) :
				__xrtNetUdpFuturePairReceiveLocked(pUdp);
		}
		if ( pWaiter->Linked ) {
			__xrt_net_udp_wait_result Result =
				__xrtNetUdpWaitResult(pWaiter);

			if ( Result != __XRT_NET_UDP_WAIT_PENDING ) {
				(void)__xrtNetUdpWaitRemove(pUdp, pWaiter);
				__xrtNetUdpWaitFinishPush(
					&pFinished,
					pWaiter,
					Result
				);
			} else {
				__xrtNetUdpWaitScheduleLocked(pUdp);
			}
		}
	}
	__xrtSpinUnlock(&pUdp->ReceiveLock);
	if ( bConflict ) {
		bool bErrorWait = Wait == XNET_UDP_WAIT_ERROR;

		__xrtNetUdpWaitDiscard(pWaiter, pFuture);
		__xrtNetUdpSetError(
			XERR_STATE,
			XNET_ERROR_UDP_RECEIVE_QUEUE,
			bErrorWait ? "wait-udp-error" : "wait-udp-receive",
			bErrorWait ?
				"readiness and consuming UDP error Futures cannot be mixed" :
				"readiness and consuming UDP receive Futures cannot be mixed"
		);
		return NULL;
	}
	__xrtNetUdpFutureFinishList(pFinished);
	return pFuture;
}



/* 建立一个统一 UDP 条件 Future。 */
XRT_API xfuture* xrtNetUdpWaitAsync(xnetudp* pUdp, xnetudpwait Wait)
{
	return __xrtNetUdpWaitCreate(pUdp, Wait, false, false, 0);
}



/* 建立一个按数据报大小判断预算的可写 Future。 */
XRT_API xfuture* xrtNetUdpWritableAsync(xnetudp* pUdp, size_t iSize)
{
	return __xrtNetUdpWaitCreate(
		pUdp,
		XNET_UDP_WAIT_DRAIN,
		false,
		true,
		iSize
	);
}



/* 建立一个拉取模式单包接收 Future。 */
XRT_API xfuture* xrtNetUdpReceiveAsync(xnetudp* pUdp)
{
	return __xrtNetUdpWaitCreate(
		pUdp,
		XNET_UDP_WAIT_RECEIVE,
		true,
		false,
		0
	);
}



/* 建立一个拉取模式数据报错误 Future。 */
XRT_API xfuture* xrtNetUdpReceiveErrorAsync(xnetudp* pUdp)
{
	return __xrtNetUdpWaitCreate(
		pUdp,
		XNET_UDP_WAIT_ERROR,
		true,
		false,
		0
	);
}



/* 建立一个拉取模式批量接收 Future。 */
XRT_API xfuture* xrtNetUdpReceiveBatchAsync(
	xnetudp* pUdp,
	size_t iCapacity
)
{
	if ( (iCapacity == 0) ||
		 (iCapacity > XRT_NET_UDP_FUTURE_BATCH_MAX) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return __xrtNetUdpWaitCreate(
		pUdp,
		XNET_UDP_WAIT_RECEIVE,
		true,
		false,
		iCapacity
	);
}

#endif

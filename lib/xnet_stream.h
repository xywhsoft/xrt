#ifndef XRT_XNET_STREAM_H
#define XRT_XNET_STREAM_H

/*
	XNet mainline stream and listener layer.

	This header owns:
	  - listener and stream lifecycle
	  - completion-driven accept / connect / recv / send flow
	  - send queue, backpressure, graceful vs abort close, and sync wait slots
	  - TLS attachment and handshake progression on top of transport-owned I/O

	Current focus:
	  - Windows native IOCP flow is active in mainline
	  - Linux backend finalization still continues under the xnet finalization spec
*/


/* ============================== Event tables ============================== */

#if !defined(XRT_BUILD_CORE)

typedef struct {
	bool (*OnAccept)(ptr pOwner, xnetlistener* pListener, xnetstream* pStream);
	void (*OnError)(ptr pOwner, xnetlistener* pListener, int iSysErr);
} xnetlistenerevents;

typedef struct {
	void (*OnOpen)(ptr pOwner, xnetstream* pStream);
	void (*OnRecv)(ptr pOwner, xnetstream* pStream, xnetchain* pChain);
	void (*OnDrain)(ptr pOwner, xnetstream* pStream);
	void (*OnClose)(ptr pOwner, xnetstream* pStream, xnet_result iReason);
	void (*OnError)(ptr pOwner, xnetstream* pStream, int iSysErr);
	void (*OnHighWater)(ptr pOwner, xnetstream* pStream, uint32 iQueuedBytes);
	void (*OnLowWater)(ptr pOwner, xnetstream* pStream, uint32 iQueuedBytes);
	void (*OnTimer)(ptr pOwner, xnetstream* pStream);
} xnetstreamevents;

#endif /* !XRT_BUILD_CORE */



/* ============================== Internal queue model ============================== */

typedef struct {
	xnetchain tQueue;
	uint32 iQueuedBytes;
	uint32 iHighWater;
	uint32 iLowWater;
	bool bHighWaterHit;
	bool bWritePosted;
} __xnet_sendq;

typedef struct __xnet_send_budget_segment {
	struct __xnet_send_budget_segment* pNext;
	uint64 iWireBytes;
	uint64 iBudgetBytes;
} __xnet_send_budget_segment;

#define __XNET_STREAM_STATE_INIT           0x00000001u
#define __XNET_STREAM_STATE_CLOSE_EMITTED  0x00000002u
#define __XNET_STREAM_STATE_OPEN_EMITTED   0x00000004u

#define __XNET_STREAM_CLOSE_REQUESTED      0x40000000L

#define __XNET_STREAM_ASYNC_SEND_COPY      1u
#define __XNET_STREAM_ASYNC_SEND_REF       2u
#define __XNET_STREAM_ASYNC_RECV_COPY      3u
#define __XNET_STREAM_ASYNC_RECV_REF       4u
#define __XNET_STREAM_ASYNC_DISPATCH_RECV  5u
#define __XNET_STREAM_ASYNC_TLS_HANDSHAKE  6u
#define __XNET_STREAM_ASYNC_CLOSE          7u
#define __XNET_STREAM_ASYNC_PAUSE_READ     8u
#define __XNET_STREAM_ASYNC_RESUME_READ    9u

typedef struct {
	xnetstream* pStream;
	uint32 iType;
	uint32 iLen;
	uint32 bSendBudgetReserved;
	xsem hDone;
	xnetbufref tRef;
	uint8 aData[1];
} __xnet_stream_async_op;

typedef struct {
	xnetstream* pStream;
	xnetchain* pChain;
} __xnet_stream_recv_chain_task;

typedef void (*__xnet_stream_sync_wait_fn)(xnetstream* pStream, xnet_result iStatus, ptr pCtx);

typedef struct __xnet_stream_wait_slot {
	struct __xnet_stream_wait_slot* pNext;
	__xnet_stream_sync_wait_fn pfnWait;
	ptr pCtx;
} __xnet_stream_wait_slot;

typedef void (*__xnet_listener_sync_wait_fn)(xnetlistener* pListener, xnet_result iStatus, xnetstream* pStream, ptr pCtx);
typedef bool (*__xnet_listener_sync_wait_ready_fn)(ptr pCtx);

typedef struct __xnet_listener_wait_slot {
	struct __xnet_listener_wait_slot* pNext;
	__xnet_listener_sync_wait_fn pfnWait;
	__xnet_listener_sync_wait_ready_fn pfnCanAccept;
	ptr pCtx;
} __xnet_listener_wait_slot;

typedef struct {
	xsocket hSocket;
	struct sockaddr_storage tStorage;
	socklen_t iAddrLen;
} __xnet_listener_accept_raw;

typedef struct __xnet_accepted_socket_node {
	struct __xnet_accepted_socket_node* pNext;
	xsocket hSocket;
} __xnet_accepted_socket_node;

#define __XNET_LISTENER_ACCEPT_SYS_CLOSED (-1)

#define __XNET_STREAM_WAIT_READABLE 0u
#define __XNET_STREAM_WAIT_WRITABLE 1u
#define __XNET_STREAM_WAIT_DRAIN    2u
#define __XNET_STREAM_WAIT_CLOSE    3u
#define __XNET_STREAM_WAIT_COUNT    4u

#if !defined(XRT_BUILD_CORE)
#define XNET_STREAM_WAIT_READABLE __XNET_STREAM_WAIT_READABLE
#define XNET_STREAM_WAIT_WRITABLE __XNET_STREAM_WAIT_WRITABLE
#define XNET_STREAM_WAIT_DRAIN    __XNET_STREAM_WAIT_DRAIN
#define XNET_STREAM_WAIT_CLOSE    __XNET_STREAM_WAIT_CLOSE
#endif



/* ============================== Public object layout ============================== */

struct xrt_net_listener {
	xnetengine* pEngine;
	xnetworker* pWorker;
	const xnetlistenerevents* pEvents;
	const xnetstreamevents* pStreamEvents;
	ptr pUserData;
	xsocket hSocket;
	__xnet_accepted_socket_node* pAcceptedHead;
	__xnet_accepted_socket_node* pAcceptedTail;
	__xnet_listener_wait_slot tAcceptWait;
	xnetlistenconfig tConfig;
	uint64 iAcceptOpId;
	uint32 iPortCount;
	uint32 iNextWorker;
	uint32 iAcceptedQueued;
	uint32 iAcceptConcurrency;
	volatile long iAsyncHoldCount;
	volatile long iAcceptArmedCount;
	bool bRunning;
	bool bDestroyPending;
};

struct xrt_net_stream {
	uint64 iId;
	xnetengine* pEngine;
	xnetworker* pWorker;
	xnetlistener* pListener;
	xtlssession* pTls;
	xnetproxy* pProxy;
	__xnet_proxy_state* pProxyState;
	const xnetstreamevents* pEvents;
	xsocket hSocket;
	xnetaddr tLocalAddr;
	xnetaddr tRemoteAddr;
	xnetaddrlist* pConnectAddrList;
	uint32 iConnectAddrIndex;
	uint32 iConnectFallbackDelayMs;
	uint64 iConnectOpId;
	xnetchain tRxChain;
	__xnet_sendq tSendQ;
	ptr pUserData;
	__xnet_stream_wait_slot arrSyncWait[__XNET_STREAM_WAIT_COUNT];
	uint32 iState;
	uint32 iFlags;
	uint32 iRecvLimit;
	uint32 iMaxQueuedBytes;
	uint32 iControlReserveBytes;
	uint32 iConnectTimeoutMs;
	uint32 iConnectFlags;
	xnet_result iCloseReason;
	volatile int64 iReservedSendBytes;
	volatile long iRxLock;
	__xnet_send_budget_segment* pSendBudgetHead;
	__xnet_send_budget_segment* pSendBudgetTail;
	volatile long iAsyncHoldCount;
	volatile long iOpenTimerState;
	volatile long iCloseRequestState;
	volatile long iSendSubmitterCount;
	volatile long bReadPaused;
	volatile long bRecvArmed;
	bool bSendArmed;
	bool bClosing;
	bool bWriteShutdown;
	bool bTlsCloseQueued;
	bool bDestroyPending;
};

XXAPI void xrtNetStreamDestroy(xnetstream* pStream);
XXAPI void xrtNetStreamClose(xnetstream* pStream, uint32 iFlags);
XXAPI xnet_result xrtNetStreamShutdownWrite(xnetstream* pStream);
XXAPI xnet_result xrtNetStreamAbort(xnetstream* pStream);
static void __xnetStreamOnPortEvents(xnetworker* pWorker, const xnetportevent* pEvents, uint32 iCount);
static bool __xnetStreamArmRecvWatch(xnetstream* pStream);
static bool UNUSED_ATTR __xnetStreamArmSendWatch(xnetstream* pStream);
static void __xnetStreamFinalizeSocketClose(xnetstream* pStream);
static void __xnetStreamFinishClose(xnetstream* pStream, xnet_result iReason);
static void __xnetStreamBeginGracefulCloseWait(xnetstream* pStream);
static bool __xnetStreamHasPreOpenGate(const xnetstream* pStream);
static void __xnetStreamDetachTls(xnetstream* pStream);
static void __xnetStreamNotifyDestroyWaiters(xnetstream* pStream);
static void __xnetStreamAbandonUnownedAccepted(xnetstream* pStream);
static void __xnetListenerCloseSocket(xnetlistener* pListener);
static void __xnetListenerClearAcceptedSockets(xnetlistener* pListener);
static void __xnetSocketCloseHandle(xsocket* phSocket);

static bool __xnetStreamRecvArmed(const xnetstream* pStream)
{
	return pStream && __xnetAtomicLoad32(&pStream->bRecvArmed) != 0;
}

static void __xnetStreamClearRecvArmed(xnetstream* pStream)
{
	if ( pStream ) {
		(void)__xnetAtomicExchange32(&pStream->bRecvArmed, 0);
	}
}
static void __xnetStreamKickWrite(xnetstream* pStream);
static bool __xnetStreamDrainTlsPlain(xnetstream* pStream);
static bool __xnetStreamDrainTlsCipherNow(xnetstream* pStream);
static bool __xnetStreamWaitReadableNow(xnetstream* pStream, uint32 iTimeoutMs);
static bool __xnetStreamDriveProxyState(xnetstream* pStream, const void* pData, size_t iLen);
static bool __xnetStreamDriveTlsHandshake(xnetstream* pStream);
static void __xnetStreamDetachProxy(xnetstream* pStream);
static void __xnetStreamEmitOpen(xnetstream* pStream);
static bool __xnetStreamDrainSocketNow(xnetstream* pStream);
static void __xnetStreamHandleRecvEvent(xnetstream* pStream, xnetchain* pChain);
static void __xnetStreamHandleOpenTimer(xnetstream* pStream);
static bool __xnetListenerRegisterSyncAcceptWait(xnetlistener* pListener, __xnet_listener_sync_wait_fn pfnWait, __xnet_listener_sync_wait_ready_fn pfnCanAccept, ptr pCtx);
static bool __xnetListenerCancelSyncAcceptWait(xnetlistener* pListener, ptr pCtx);



/* ============================== Internal helpers ============================== */

static xnetworker* __xnetStreamPickWorker(xnetengine* pEngine, xnetlistener* pListener)
{
	uint32 iIndex;
	uint64 iNextId;
	if ( !pEngine || pEngine->iWorkerCount == 0 ) { return NULL; }
	if ( pListener ) {
		iIndex = pListener->iNextWorker % pEngine->iWorkerCount;
		pListener->iNextWorker = (iIndex + 1u) % pEngine->iWorkerCount;
		return &pEngine->arrWorkers[iIndex];
	}

	iNextId = (uint64)__xnetAtomicLoad64((const volatile int64*)&pEngine->iNextStreamId);
	iIndex = (uint32)((iNextId > 0 ? iNextId - 1u : 0u) % pEngine->iWorkerCount);
	return &pEngine->arrWorkers[iIndex];
}


// 内部函数：__xnetListenerPickWorker
static xnetworker* __xnetListenerPickWorker(xnetengine* pEngine, const xnetlistenconfig* pCfg)
{
	uint32 iIndex = 0;
	if ( !pEngine || pEngine->iWorkerCount == 0 ) { return NULL; }
	if ( pCfg && pCfg->tBindAddr.iPort > 0 ) {
		iIndex = pCfg->tBindAddr.iPort % pEngine->iWorkerCount;
	}
	return &pEngine->arrWorkers[iIndex];
}


// 内部函数：__xnetStreamInitQueues
static void __xnetStreamInitQueues(xnetstream* pStream, xnetworker* pWorker)
{
	xnetmemctx* pMemCtx = pWorker ? &pWorker->tMemCtx : NULL;
	if ( !pStream ) { return; }
	xrtNetChainInitEx(&pStream->tRxChain, pMemCtx);
	xrtNetChainInitEx(&pStream->tSendQ.tQueue, pMemCtx);
}


static void __xnetStreamRxLock(xnetstream* pStream)
{
	if ( pStream ) { __xrtOwnerSpinLock(&pStream->iRxLock); }
}


static void __xnetStreamRxUnlock(xnetstream* pStream)
{
	if ( pStream ) { __xrtOwnerSpinUnlock(&pStream->iRxLock); }
}


static size_t __xnetStreamRxBytes(const xnetstream* pStream)
{
	size_t iBytes;
	if ( !pStream ) { return 0u; }
	__xnetStreamRxLock((xnetstream*)pStream);
	iBytes = xrtNetChainBytes(&pStream->tRxChain);
	__xnetStreamRxUnlock((xnetstream*)pStream);
	return iBytes;
}


static void __xnetStreamClearRx(xnetstream* pStream)
{
	if ( !pStream ) { return; }
	__xnetStreamRxLock(pStream);
	xrtNetChainClear(&pStream->tRxChain);
	__xnetStreamRxUnlock(pStream);
}


static bool __xnetStreamReadPaused(const xnetstream* pStream)
{
	return pStream && __xnetAtomicLoad32(&pStream->bReadPaused) != 0;
}


static bool __xnetStreamBeginSendSubmission(xnetstream* pStream)
{
	if ( !pStream ) { return false; }
	if ( pStream->pWorker && __xnetEngineIsCurrentWorker(pStream->pWorker) ) {
		return __xnetAtomicLoad32(&pStream->iCloseRequestState) == 0;
	}
	(void)__xnetAtomicAddFetch32(&pStream->iSendSubmitterCount, 1);
	if ( __xnetAtomicLoad32(&pStream->iCloseRequestState) != 0 ) {
		(void)__xnetAtomicAddFetch32(&pStream->iSendSubmitterCount, -1);
		return false;
	}
	return true;
}


static void __xnetStreamEndSendSubmission(xnetstream* pStream)
{
	if ( pStream && (!pStream->pWorker || !__xnetEngineIsCurrentWorker(pStream->pWorker)) ) {
		(void)__xnetAtomicAddFetch32(&pStream->iSendSubmitterCount, -1);
	}
}


static long __xnetStreamRequestClose(xnetstream* pStream, uint32 iFlags)
{
	long iOldState;
	long iNewState;
	if ( !pStream ) { return 0; }
	do {
		iOldState = __xnetAtomicLoad32(&pStream->iCloseRequestState);
		iNewState = iOldState | __XNET_STREAM_CLOSE_REQUESTED | (long)iFlags;
	} while ( __xnetAtomicCompareExchange32(&pStream->iCloseRequestState, iNewState, iOldState) != iOldState );
	while ( __xnetAtomicLoad32(&pStream->iSendSubmitterCount) != 0 ) { xrtThreadYield(); }
	return iNewState;
}


static uint64 __xnetStreamReservedSendBytes(const xnetstream* pStream)
{
	int64 iValue = pStream ? __xnetAtomicLoad64(&pStream->iReservedSendBytes) : 0;
	return iValue > 0 ? (uint64)iValue : 0u;
}


static bool __xnetStreamTryReserveSendEx(xnetstream* pStream, uint64 iBytes, bool bControl)
{
	int64 iPrev;
	int64 iNext;
	uint64 iMax;
	if ( !pStream || iBytes == 0u || iBytes > (uint64)INT64_MAX ) { return false; }
	iMax = pStream->iMaxQueuedBytes;
	if ( !bControl && iMax > 0u && pStream->iControlReserveBytes > 0u ) {
		iMax = pStream->iControlReserveBytes < iMax ? iMax - pStream->iControlReserveBytes : 0u;
	}
	for ( ;; ) {
		iPrev = __xnetAtomicLoad64(&pStream->iReservedSendBytes);
		if ( iPrev < 0 || (uint64)iPrev > UINT64_MAX - iBytes ) { return false; }
		if ( iMax > 0u && (uint64)iPrev + iBytes > iMax ) { return false; }
		iNext = iPrev + (int64)iBytes;
		if ( __xnetAtomicCompareExchange64(&pStream->iReservedSendBytes, iNext, iPrev) == iPrev ) {
			return true;
		}
	}
}


static bool __xnetStreamTryReserveSend(xnetstream* pStream, uint64 iBytes)
{
	return __xnetStreamTryReserveSendEx(pStream, iBytes, false);
}


static bool __xnetStreamTryReserveControlSend(xnetstream* pStream, uint64 iBytes)
{
	return __xnetStreamTryReserveSendEx(pStream, iBytes, true);
}


static void __xnetStreamReleaseSend(xnetstream* pStream, uint64 iBytes)
{
	int64 iPrev;
	int64 iNext;
	if ( !pStream || iBytes == 0u ) { return; }
	for ( ;; ) {
		iPrev = __xnetAtomicLoad64(&pStream->iReservedSendBytes);
		if ( iPrev <= 0 ) { return; }
		iNext = iBytes >= (uint64)iPrev ? 0 : iPrev - (int64)iBytes;
		if ( __xnetAtomicCompareExchange64(&pStream->iReservedSendBytes, iNext, iPrev) == iPrev ) {
			return;
		}
	}
}


static __xnet_send_budget_segment* __xnetStreamAllocSendBudget(uint64 iWireBytes, uint64 iBudgetBytes)
{
	__xnet_send_budget_segment* pSegment;
	if ( iWireBytes == 0u || iBudgetBytes == 0u ) { return NULL; }
	pSegment = (__xnet_send_budget_segment*)XNET_ALLOC(sizeof(*pSegment));
	if ( !pSegment ) { return NULL; }
	pSegment->pNext = NULL;
	pSegment->iWireBytes = iWireBytes;
	pSegment->iBudgetBytes = iBudgetBytes;
	return pSegment;
}


static void __xnetStreamTrackSendBudget(xnetstream* pStream, __xnet_send_budget_segment* pSegment)
{
	if ( !pStream || !pSegment ) { return; }
	if ( pStream->pSendBudgetTail ) {
		pStream->pSendBudgetTail->pNext = pSegment;
	} else {
		pStream->pSendBudgetHead = pSegment;
	}
	pStream->pSendBudgetTail = pSegment;
}


static void __xnetStreamConsumeSendBudget(xnetstream* pStream, uint64 iWireBytes)
{
	while ( pStream && iWireBytes > 0u && pStream->pSendBudgetHead ) {
		__xnet_send_budget_segment* pSegment = pStream->pSendBudgetHead;
		uint64 iConsume = iWireBytes < pSegment->iWireBytes ? iWireBytes : pSegment->iWireBytes;
		if ( pSegment->iBudgetBytes == pSegment->iWireBytes ) {
			pSegment->iBudgetBytes -= iConsume;
			__xnetStreamReleaseSend(pStream, iConsume);
		}
		pSegment->iWireBytes -= iConsume;
		iWireBytes -= iConsume;
		if ( pSegment->iWireBytes == 0u ) {
			if ( pSegment->iBudgetBytes > 0u ) {
				__xnetStreamReleaseSend(pStream, pSegment->iBudgetBytes);
			}
			pStream->pSendBudgetHead = pSegment->pNext;
			if ( !pStream->pSendBudgetHead ) { pStream->pSendBudgetTail = NULL; }
			XNET_FREE(pSegment);
		}
	}
}


static void __xnetStreamClearSendQueue(xnetstream* pStream)
{
	__xnet_send_budget_segment* pSegment;
	if ( !pStream ) { return; }
	xrtNetChainClear(&pStream->tSendQ.tQueue);
	pStream->tSendQ.iQueuedBytes = 0u;
	pStream->tSendQ.bHighWaterHit = false;
	pStream->tSendQ.bWritePosted = false;
	while ( (pSegment = pStream->pSendBudgetHead) != NULL ) {
		pStream->pSendBudgetHead = pSegment->pNext;
		__xnetStreamReleaseSend(pStream, pSegment->iBudgetBytes);
		XNET_FREE(pSegment);
	}
	pStream->pSendBudgetTail = NULL;
}


// 内部函数：绑定流引擎
static void __xnetStreamBindEngine(xnetengine* pEngine)
{
	if ( !pEngine ) { return; }
	if ( !pEngine->pfnOnPortEvent || pEngine->pfnOnPortEvent == __xnetStreamOnPortEvents ) {
		pEngine->pfnOnPortEvent = __xnetStreamOnPortEvents;
		return;
	}
	if ( !pEngine->pfnOnPortEvent2 || pEngine->pfnOnPortEvent2 == __xnetStreamOnPortEvents ) {
		pEngine->pfnOnPortEvent2 = __xnetStreamOnPortEvents;
	}
}


// 内部函数：__xnetSocketLastErr
static int __xnetSocketLastErr(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return WSAGetLastError();
	#else
		return errno;
	#endif
}


// 内部函数：__xnetSocketWouldBlock
static bool __xnetSocketWouldBlock(int iErr)
{
	#if defined(_WIN32) || defined(_WIN64)
		return iErr == WSAEWOULDBLOCK || iErr == WSAEINPROGRESS || iErr == WSAEALREADY;
	#else
		return iErr == EINPROGRESS || iErr == EWOULDBLOCK || iErr == EAGAIN || iErr == EALREADY;
	#endif
}


// 内部函数：__xnetSocketIsValid
static bool __xnetSocketIsValid(xsocket hSocket)
{
	return hSocket != XNET_SOCKET_INVALID;
}


// 内部函数：设置流错误
static void __xnetStreamSetError(const char* sError)
{
	__xnetErrorSetEx(XRT_NET_ERROR, XNET_OP_NONE, XNET_PHASE_COMPLETE, XNET_BACKEND_NONE, 0, sError);
}


// 内部函数：异步增加流 hold
static void __xnetStreamAddAsyncHold(xnetstream* pStream)
{
	if ( !pStream ) { return; }
	(void)__xnetAtomicAddFetch32(&pStream->iAsyncHoldCount, 1);
}


// 内部函数：异步释放流 hold
static void __xnetStreamReleaseAsyncHold(xnetstream* pStream)
{
	if ( !pStream ) { return; }
	if ( __xnetAtomicAddFetch32(&pStream->iAsyncHoldCount, -1) == 0 && pStream->bDestroyPending ) {
		__xnetStreamNotifyDestroyWaiters(pStream);
		__xnetStreamClearRx(pStream);
		__xnetStreamClearSendQueue(pStream);
		__xnetStreamFinalizeSocketClose(pStream);
		__xnetStreamDetachTls(pStream);
		__xnetStreamDetachProxy(pStream);
		__xnetEngineReleaseLiveObject(pStream->pEngine);
		XNET_FREE(pStream);
	}
}


// 内部函数：__xnetListenerFinalizeDestroy
static void __xnetListenerFinalizeDestroy(xnetlistener* pListener)
{
	if ( !pListener ) { return; }
	pListener->bRunning = false;
	(void)__xnetAtomicExchange32(&pListener->iAcceptArmedCount, 0);
	pListener->bDestroyPending = false;
	pListener->iAcceptOpId = 0;
	pListener->tAcceptWait.pfnWait = NULL;
	pListener->tAcceptWait.pfnCanAccept = NULL;
	pListener->tAcceptWait.pCtx = NULL;
	pListener->pEvents = NULL;
	pListener->pStreamEvents = NULL;
	pListener->pUserData = NULL;
	__xnetListenerClearAcceptedSockets(pListener);
	__xnetListenerCloseSocket(pListener);
	__xnetEngineReleaseLiveObject(pListener->pEngine);
	XNET_FREE(pListener);
}


// 内部函数：异步增加监听器 hold
static void __xnetListenerAddAsyncHold(xnetlistener* pListener)
{
	if ( !pListener ) { return; }
	(void)__xnetAtomicAddFetch32(&pListener->iAsyncHoldCount, 1);
}


// 内部函数：__xnetListenerPrepareDeferredDestroy
static void __xnetListenerPrepareDeferredDestroy(xnetlistener* pListener)
{
	if ( !pListener || pListener->bDestroyPending ) { return; }
	pListener->bDestroyPending = true;
	pListener->pEvents = NULL;
	pListener->pStreamEvents = NULL;
	pListener->pUserData = NULL;
}


// 内部函数：异步释放监听器 hold
static void __xnetListenerReleaseAsyncHold(xnetlistener* pListener)
{
	if ( !pListener ) { return; }
	if ( __xnetAtomicAddFetch32(&pListener->iAsyncHoldCount, -1) == 0 && pListener->bDestroyPending ) {
		__xnetListenerFinalizeDestroy(pListener);
	}
}


// 内部函数：__xnetListenerCanDispatchAccept
static bool __xnetListenerCanDispatchAccept(xnetlistener* pListener)
{
	if ( !pListener || !pListener->bRunning ) { return false; }
	if ( pListener->tAcceptWait.pfnWait != NULL ) {
		if ( pListener->tAcceptWait.pfnCanAccept && !pListener->tAcceptWait.pfnCanAccept(pListener->tAcceptWait.pCtx) ) {
			return false;
		}
		return true;
	}
	return (pListener->tConfig.iFlags & XNET_LISTEN_F_PULL_ACCEPT) == 0 &&
		pListener->pEvents && pListener->pEvents->OnAccept;
}


// 内部函数：__xnetListenerNextAcceptOpId
static uint64 __xnetListenerNextAcceptOpId(xnetlistener* pListener)
{
	uint64 iOpId = 0;
	if ( !pListener || !pListener->pEngine ) { return 0; }
	iOpId = __xnetEngineAllocStreamId(pListener->pEngine);
	return iOpId;
}


static bool __xnetListenerEnqueueWait(xnetlistener* pListener, __xnet_listener_sync_wait_fn pfnWait,
	__xnet_listener_sync_wait_ready_fn pfnCanAccept, ptr pCtx)
{
	__xnet_listener_wait_slot* pHead;
	__xnet_listener_wait_slot* pTail;
	__xnet_listener_wait_slot* pNode;
	if ( !pListener || !pfnWait ) { return false; }
	pHead = &pListener->tAcceptWait;
	if ( pHead->pfnWait == NULL ) {
		pHead->pNext = NULL;
		pHead->pfnWait = pfnWait;
		pHead->pfnCanAccept = pfnCanAccept;
		pHead->pCtx = pCtx;
		return true;
	}
	pNode = (__xnet_listener_wait_slot*)XNET_ALLOC(sizeof(*pNode));
	if ( !pNode ) { return false; }
	memset(pNode, 0, sizeof(*pNode));
	pNode->pfnWait = pfnWait;
	pNode->pfnCanAccept = pfnCanAccept;
	pNode->pCtx = pCtx;
	pTail = pHead;
	while ( pTail->pNext ) { pTail = pTail->pNext; }
	pTail->pNext = pNode;
	return true;
}


static bool __xnetListenerPopWait(xnetlistener* pListener, __xnet_listener_sync_wait_fn* ppfnWait, ptr* ppCtx)
{
	__xnet_listener_wait_slot* pHead;
	__xnet_listener_wait_slot* pNext;
	if ( !pListener || !ppfnWait || !ppCtx ) { return false; }
	pHead = &pListener->tAcceptWait;
	if ( pHead->pfnWait == NULL ) { return false; }
	*ppfnWait = pHead->pfnWait;
	*ppCtx = pHead->pCtx;
	pNext = pHead->pNext;
	if ( pNext ) {
		*pHead = *pNext;
		XNET_FREE(pNext);
	} else {
		memset(pHead, 0, sizeof(*pHead));
	}
	return true;
}


static bool __xnetListenerRemoveWait(xnetlistener* pListener, ptr pCtx)
{
	__xnet_listener_wait_slot* pHead;
	__xnet_listener_wait_slot* pPrev;
	__xnet_listener_wait_slot* pNode;
	if ( !pListener ) { return false; }
	pHead = &pListener->tAcceptWait;
	if ( pHead->pfnWait == NULL ) { return false; }
	if ( pHead->pCtx == pCtx ) {
		__xnet_listener_sync_wait_fn pfnDiscard;
		ptr pDiscardCtx;
		return __xnetListenerPopWait(pListener, &pfnDiscard, &pDiscardCtx);
	}
	pPrev = pHead;
	pNode = pHead->pNext;
	while ( pNode ) {
		if ( pNode->pCtx == pCtx ) {
			pPrev->pNext = pNode->pNext;
			XNET_FREE(pNode);
			return true;
		}
		pPrev = pNode;
		pNode = pNode->pNext;
	}
	return false;
}


// 内部函数：接受监听器 notify 同步
static void __xnetListenerNotifySyncAccept(xnetlistener* pListener, xnet_result iStatus, xnetstream* pStream)
{
	__xnet_listener_sync_wait_fn pfnWait = NULL;
	ptr pCtx = NULL;

	if ( !__xnetListenerPopWait(pListener, &pfnWait, &pCtx) ) { return; }
	pfnWait(pListener, iStatus, pStream, pCtx);
}


static void __xnetListenerNotifyAllWaiters(xnetlistener* pListener, xnet_result iStatus)
{
	uint32 iCount = 0;
	if ( !pListener ) { return; }
	for ( __xnet_listener_wait_slot* pNode = &pListener->tAcceptWait;
		pNode && pNode->pfnWait; pNode = pNode->pNext ) {
		iCount++;
	}
	while ( iCount-- > 0 && pListener->tAcceptWait.pfnWait ) {
		__xnetListenerNotifySyncAccept(pListener, iStatus, NULL);
	}
}


static bool __xnetListenerQueueAcceptedSocket(xnetlistener* pListener, xsocket hSocket)
{
	__xnet_accepted_socket_node* pNode;
	uint32 iLimit;
	if ( !pListener || !__xnetSocketIsValid(hSocket) ) { return false; }
	iLimit = pListener->tConfig.iAcceptQueueLimit > 0u ? pListener->tConfig.iAcceptQueueLimit : 256u;
	if ( pListener->iAcceptedQueued >= iLimit ) {
		__xnetSocketCloseHandle(&hSocket);
		return false;
	}
	pNode = (__xnet_accepted_socket_node*)XNET_ALLOC(sizeof(*pNode));
	if ( !pNode ) {
		__xnetSocketCloseHandle(&hSocket);
		return false;
	}
	pNode->pNext = NULL;
	pNode->hSocket = hSocket;
	if ( pListener->pAcceptedTail ) {
		pListener->pAcceptedTail->pNext = pNode;
	} else {
		pListener->pAcceptedHead = pNode;
	}
	pListener->pAcceptedTail = pNode;
	++pListener->iAcceptedQueued;
	return true;
}


static xsocket __xnetListenerPopAcceptedSocket(xnetlistener* pListener)
{
	__xnet_accepted_socket_node* pNode;
	xsocket hSocket;
	if ( !pListener || !pListener->pAcceptedHead ) { return XNET_SOCKET_INVALID; }
	pNode = pListener->pAcceptedHead;
	pListener->pAcceptedHead = pNode->pNext;
	if ( !pListener->pAcceptedHead ) { pListener->pAcceptedTail = NULL; }
	if ( pListener->iAcceptedQueued > 0u ) { --pListener->iAcceptedQueued; }
	hSocket = pNode->hSocket;
	XNET_FREE(pNode);
	return hSocket;
}


static void __xnetListenerClearAcceptedSockets(xnetlistener* pListener)
{
	xsocket hSocket;
	if ( !pListener ) { return; }
	while ( __xnetSocketIsValid((hSocket = __xnetListenerPopAcceptedSocket(pListener))) ) {
		__xnetSocketCloseHandle(&hSocket);
	}
}


// 内部函数：__xnetStreamGetSyncWaitSlot
static __xnet_stream_wait_slot* __xnetStreamGetSyncWaitSlot(xnetstream* pStream, uint32 iWaitKind)
{
	if ( !pStream || iWaitKind >= __XNET_STREAM_WAIT_COUNT ) { return NULL; }
	return &pStream->arrSyncWait[iWaitKind];
}


static bool __xnetStreamEnqueueSyncWait(__xnet_stream_wait_slot* pHead, __xnet_stream_sync_wait_fn pfnWait, ptr pCtx)
{
	__xnet_stream_wait_slot* pNode;
	__xnet_stream_wait_slot* pTail;
	if ( !pHead || !pfnWait ) { return false; }
	if ( pHead->pfnWait == NULL ) {
		pHead->pNext = NULL;
		pHead->pfnWait = pfnWait;
		pHead->pCtx = pCtx;
		return true;
	}
	pNode = (__xnet_stream_wait_slot*)XNET_ALLOC(sizeof(*pNode));
	if ( !pNode ) { return false; }
	memset(pNode, 0, sizeof(*pNode));
	pNode->pfnWait = pfnWait;
	pNode->pCtx = pCtx;
	pTail = pHead;
	while ( pTail->pNext ) { pTail = pTail->pNext; }
	pTail->pNext = pNode;
	return true;
}


static bool __xnetStreamPopSyncWait(__xnet_stream_wait_slot* pHead, __xnet_stream_sync_wait_fn* ppfnWait, ptr* ppCtx)
{
	__xnet_stream_wait_slot* pNext;
	if ( !pHead || pHead->pfnWait == NULL || !ppfnWait || !ppCtx ) { return false; }
	*ppfnWait = pHead->pfnWait;
	*ppCtx = pHead->pCtx;
	pNext = pHead->pNext;
	if ( pNext ) {
		*pHead = *pNext;
		XNET_FREE(pNext);
	} else {
		memset(pHead, 0, sizeof(*pHead));
	}
	return true;
}


static bool __xnetStreamRemoveSyncWait(__xnet_stream_wait_slot* pHead, ptr pCtx)
{
	__xnet_stream_wait_slot* pPrev;
	__xnet_stream_wait_slot* pNode;
	if ( !pHead || pHead->pfnWait == NULL ) { return false; }
	if ( pHead->pCtx == pCtx ) {
		__xnet_stream_sync_wait_fn pfnDiscard;
		ptr pDiscardCtx;
		return __xnetStreamPopSyncWait(pHead, &pfnDiscard, &pDiscardCtx);
	}
	pPrev = pHead;
	pNode = pHead->pNext;
	while ( pNode ) {
		if ( pNode->pCtx == pCtx ) {
			pPrev->pNext = pNode->pNext;
			XNET_FREE(pNode);
			return true;
		}
		pPrev = pNode;
		pNode = pNode->pNext;
	}
	return false;
}


// 内部函数：__xnetStreamResolveSyncWaitNow
static bool __xnetStreamResolveSyncWaitNow(xnetstream* pStream, uint32 iWaitKind, xnet_result* pStatus)
{
	if ( !pStream || !pStatus ) { return false; }

	// 根据等待类型判断是否可以立即完成
	switch ( iWaitKind ) {
		case __XNET_STREAM_WAIT_READABLE:
			// 流已关闭，返回关闭原因
			if ( (pStream->iState & __XNET_STREAM_STATE_CLOSE_EMITTED) != 0 ) {
				*pStatus = pStream->iCloseReason;
				return true;
			}
			// 接收链中有数据可读
			if ( __xnetStreamRxBytes(pStream) > 0 ) {
				*pStatus = XRT_NET_OK;
				return true;
			}
			return false;

		case __XNET_STREAM_WAIT_WRITABLE:
			// 流已关闭，返回关闭原因
			if ( (pStream->iState & __XNET_STREAM_STATE_CLOSE_EMITTED) != 0 ) {
				*pStatus = pStream->iCloseReason;
				return true;
			}
			// 流正在关闭中
			if ( pStream->bClosing ) {
				*pStatus = XRT_NET_CLOSED;
				return true;
			}
			// 尚未触发 OnOpen 事件，暂不可写
			if ( __xnetStreamHasPreOpenGate(pStream) ) {
				return false;
			}
			// 未触及高水位或已回落到低水位以下，可以写入
			if ( !pStream->tSendQ.bHighWaterHit || pStream->tSendQ.iQueuedBytes <= pStream->tSendQ.iLowWater ) {
				*pStatus = XRT_NET_OK;
				return true;
			}
			return false;

		case __XNET_STREAM_WAIT_DRAIN:
			// 流已关闭
			if ( (pStream->iState & __XNET_STREAM_STATE_CLOSE_EMITTED) != 0 ) {
				*pStatus = XRT_NET_CLOSED;
				return true;
			}
			// 发送队列已排空，根据是否正在关闭返回不同状态
			if ( pStream->tSendQ.iQueuedBytes == 0 ) {
				*pStatus = pStream->bClosing ? XRT_NET_CLOSED : XRT_NET_OK;
				return true;
			}
			return false;

		case __XNET_STREAM_WAIT_CLOSE:
			// 流已关闭，返回关闭原因
			if ( (pStream->iState & __XNET_STREAM_STATE_CLOSE_EMITTED) != 0 ) {
				*pStatus = pStream->iCloseReason;
				return true;
			}
			return false;
	}

	return false;
}


// 内部函数：等待流 notify 同步
static void __xnetStreamNotifySyncWait(xnetstream* pStream, uint32 iWaitKind, xnet_result iStatus)
{
	__xnet_stream_wait_slot* pSlot = NULL;
	__xnet_stream_sync_wait_fn pfnWait = NULL;
	ptr pCtx = NULL;
	uint32 iNotifyCount = 0;

	pSlot = __xnetStreamGetSyncWaitSlot(pStream, iWaitKind);
	if ( pSlot == NULL || pSlot->pfnWait == NULL ) { return; }

	if ( iWaitKind == __XNET_STREAM_WAIT_READABLE && iStatus == XRT_NET_OK ) {
		iNotifyCount = 1;
	} else {
		for ( __xnet_stream_wait_slot* pNode = pSlot; pNode && pNode->pfnWait; pNode = pNode->pNext ) {
			iNotifyCount++;
		}
	}
	while ( iNotifyCount-- > 0 && __xnetStreamPopSyncWait(pSlot, &pfnWait, &pCtx) ) {
		pfnWait(pStream, iStatus, pCtx);
	}
}


// 内部函数：__xnetStreamNotifySyncReadable
static void __xnetStreamNotifySyncReadable(xnetstream* pStream, xnet_result iStatus)
{
	__xnetStreamNotifySyncWait(pStream, __XNET_STREAM_WAIT_READABLE, iStatus);
}


// 内部函数：__xnetStreamNotifySyncWritable
static void __xnetStreamNotifySyncWritable(xnetstream* pStream, xnet_result iStatus)
{
	__xnetStreamNotifySyncWait(pStream, __XNET_STREAM_WAIT_WRITABLE, iStatus);
}


// 内部函数：关闭流 notify 同步
static void __xnetStreamNotifySyncClose(xnetstream* pStream, xnet_result iStatus)
{
	__xnetStreamNotifySyncWait(pStream, __XNET_STREAM_WAIT_CLOSE, iStatus);
}


// 内部函数：__xnetStreamNotifySyncDrain
static void __xnetStreamNotifySyncDrain(xnetstream* pStream, xnet_result iStatus)
{
	__xnetStreamNotifySyncWait(pStream, __XNET_STREAM_WAIT_DRAIN, iStatus);
}


// 内部函数：销毁流状态
static xnet_result __xnetStreamDestroyStatus(const xnetstream* pStream)
{
	if ( !pStream || pStream->iCloseReason == XRT_NET_AGAIN ) { return XRT_NET_CLOSED; }
	return pStream->iCloseReason;
}


// 内部函数：__xnetStreamNotifyDestroyWaiters
static void __xnetStreamNotifyDestroyWaiters(xnetstream* pStream)
{
	xnet_result iReason = __xnetStreamDestroyStatus(pStream);
	if ( !pStream ) { return; }
	__xnetStreamNotifySyncReadable(pStream, iReason);
	__xnetStreamNotifySyncWritable(pStream, iReason);
	__xnetStreamNotifySyncDrain(pStream, XRT_NET_CLOSED);
	__xnetStreamNotifySyncClose(pStream, iReason);
}


// 内部函数：__xnetStreamPrepareDeferredDestroy
static void __xnetStreamPrepareDeferredDestroy(xnetstream* pStream)
{
	if ( !pStream || pStream->bDestroyPending ) { return; }
	pStream->bDestroyPending = true;
	pStream->pEvents = NULL;
	pStream->pUserData = NULL;
	__xnetStreamNotifyDestroyWaiters(pStream);
}


// 内部函数：__xnetStreamAbandonUnownedAccepted
static void __xnetStreamAbandonUnownedAccepted(xnetstream* pStream)
{
	if ( !pStream ) { return; }
	__xnetStreamAddAsyncHold(pStream);
	__xnetStreamPrepareDeferredDestroy(pStream);
	xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
	__xnetStreamReleaseAsyncHold(pStream);
}


// 内部函数：等待流 register 同步
static bool __xnetStreamRegisterSyncWait(xnetstream* pStream, uint32 iWaitKind, __xnet_stream_sync_wait_fn pfnWait, ptr pCtx)
{
	__xnet_stream_wait_slot* pSlot = NULL;
	xnet_result iImmediateStatus = XRT_NET_ERROR;

	if ( !pStream || !pfnWait ) { return false; }
	if ( pStream->pWorker && !__xnetEngineIsCurrentWorker(pStream->pWorker) ) { return false; }
	if ( iWaitKind == __XNET_STREAM_WAIT_READABLE && !__xnetStreamReadPaused(pStream) ) { return false; }

	pSlot = __xnetStreamGetSyncWaitSlot(pStream, iWaitKind);
	if ( pSlot == NULL ) { return false; }

	if ( __xnetStreamResolveSyncWaitNow(pStream, iWaitKind, &iImmediateStatus) ) {
		pfnWait(pStream, iImmediateStatus, pCtx);
		return true;
	}

	return __xnetStreamEnqueueSyncWait(pSlot, pfnWait, pCtx);
}


// 内部函数：等待流 cancel 同步
static bool __xnetStreamCancelSyncWait(xnetstream* pStream, uint32 iWaitKind, ptr pCtx)
{
	__xnet_stream_wait_slot* pSlot = NULL;

	if ( !pStream ) { return false; }
	if ( pStream->pWorker && !__xnetEngineIsCurrentWorker(pStream->pWorker) ) { return false; }

	pSlot = __xnetStreamGetSyncWaitSlot(pStream, iWaitKind);
	return pSlot ? __xnetStreamRemoveSyncWait(pSlot, pCtx) : false;
}


// 内部函数：__xnetStreamRegisterSyncDrainWait
static bool UNUSED_ATTR __xnetStreamRegisterSyncDrainWait(xnetstream* pStream, __xnet_stream_sync_wait_fn pfnWait, ptr pCtx)
{
	return __xnetStreamRegisterSyncWait(pStream, __XNET_STREAM_WAIT_DRAIN, pfnWait, pCtx);
}


// 内部函数：__xnetStreamRegisterSyncReadableWait
static bool UNUSED_ATTR __xnetStreamRegisterSyncReadableWait(xnetstream* pStream, __xnet_stream_sync_wait_fn pfnWait, ptr pCtx)
{
	return __xnetStreamRegisterSyncWait(pStream, __XNET_STREAM_WAIT_READABLE, pfnWait, pCtx);
}


// 内部函数：__xnetStreamRegisterSyncWritableWait
static bool UNUSED_ATTR __xnetStreamRegisterSyncWritableWait(xnetstream* pStream, __xnet_stream_sync_wait_fn pfnWait, ptr pCtx)
{
	return __xnetStreamRegisterSyncWait(pStream, __XNET_STREAM_WAIT_WRITABLE, pfnWait, pCtx);
}


// 内部函数：__xnetStreamRegisterSyncCloseWait
static bool UNUSED_ATTR __xnetStreamRegisterSyncCloseWait(xnetstream* pStream, __xnet_stream_sync_wait_fn pfnWait, ptr pCtx)
{
	return __xnetStreamRegisterSyncWait(pStream, __XNET_STREAM_WAIT_CLOSE, pfnWait, pCtx);
}


// 内部函数：__xnetSocketBytesAvailable
static uint32 UNUSED_ATTR __xnetSocketBytesAvailable(xsocket hSocket)
{
	if ( !__xnetSocketIsValid(hSocket) ) { return 0; }
	#if defined(_WIN32) || defined(_WIN64)
		u_long iAvail = 0;
		if ( ioctlsocket(hSocket, FIONREAD, &iAvail) != 0 ) { return 0; }
		return (uint32)iAvail;
	#else
		int iAvail = 0;
		if ( ioctl(hSocket, FIONREAD, &iAvail) != 0 || iAvail <= 0 ) { return 0; }
		return (uint32)iAvail;
	#endif
}


// 内部函数：__xnetSocketCloseHandle
static void __xnetSocketCloseHandle(xsocket* phSocket)
{
	if ( !phSocket || !__xnetSocketIsValid(*phSocket) ) { return; }
	#if defined(_WIN32) || defined(_WIN64)
		closesocket(*phSocket);
	#else
		close(*phSocket);
	#endif
	*phSocket = XNET_SOCKET_INVALID;
}


// 内部函数：__xnetSocketShutdownWrite
static bool __xnetSocketShutdownWrite(xsocket hSocket)
{
	if ( !__xnetSocketIsValid(hSocket) ) { return false; }
	#if defined(_WIN32) || defined(_WIN64)
		if ( shutdown(hSocket, SD_SEND) == 0 ) { return true; }
		switch ( WSAGetLastError() ) {
			case WSAENOTSOCK:
			case WSAESHUTDOWN:
			case WSAECONNRESET:
			case WSAECONNABORTED:
			case WSAENOTCONN:
				return true;
			default:
				return false;
		}
	#else
		if ( shutdown(hSocket, SHUT_WR) == 0 ) { return true; }
		switch ( errno ) {
			case ENOTSOCK:
			case ENOTCONN:
			case EPIPE:
			case ESHUTDOWN:
			case ECONNRESET:
			case ECONNABORTED:
			case EBADF:
				return true;
			default:
				return false;
		}
	#endif
}


// 内部函数：__xnetSocketShutdownBoth
static bool __xnetSocketShutdownBoth(xsocket hSocket)
{
	if ( !__xnetSocketIsValid(hSocket) ) { return false; }
	#if defined(_WIN32) || defined(_WIN64)
		if ( shutdown(hSocket, SD_BOTH) == 0 ) { return true; }
		switch ( WSAGetLastError() ) {
			case WSAENOTSOCK:
			case WSAESHUTDOWN:
			case WSAECONNRESET:
			case WSAECONNABORTED:
			case WSAENOTCONN:
				return true;
			default:
				return false;
		}
	#else
		if ( shutdown(hSocket, SHUT_RDWR) == 0 ) { return true; }
		switch ( errno ) {
			case ENOTSOCK:
			case ENOTCONN:
			case EPIPE:
			case ESHUTDOWN:
			case ECONNRESET:
			case ECONNABORTED:
			case EBADF:
				return true;
			default:
				return false;
		}
	#endif
}


// 内部函数：设置套接字 non 块
static bool __xnetSocketSetNonBlock(xsocket hSocket, bool bEnable)
{
	if ( !__xnetSocketIsValid(hSocket) ) { return false; }
	#if defined(_WIN32) || defined(_WIN64)
		u_long iMode = bEnable ? 1u : 0u;
		return ioctlsocket(hSocket, FIONBIO, &iMode) == 0;
	#else
		int iFlags = fcntl(hSocket, F_GETFL, 0);
		if ( iFlags < 0 ) { return false; }
		if ( bEnable ) {
			iFlags |= O_NONBLOCK;
		} else {
			iFlags &= ~O_NONBLOCK;
		}
		return fcntl(hSocket, F_SETFL, iFlags) == 0;
	#endif
}


// 内部函数：创建套接字流
static xsocket __xnetSocketCreateStream(int iFamily)
{
	#if defined(_WIN32) || defined(_WIN64)
		return WSASocket(iFamily, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	#else
		return socket(iFamily, SOCK_STREAM, IPPROTO_TCP);
	#endif
}


// 内部函数：__xnetStreamUseNativePortIO
static bool __xnetStreamUseNativePortIO(xnetstream* pStream)
{
	#if defined(_WIN32) || defined(_WIN64)
		return pStream && pStream->pWorker &&
			pStream->pWorker->tPort.pOps == xrtNetPortIOCPOps() &&
			__xnetSocketIsValid(pStream->hSocket);
	#elif defined(__linux__)
		return pStream && pStream->pWorker &&
			((pStream->pWorker->tPort.pOps == xrtNetPortUringOps() &&
			__xnetPortUringHasNativeRing(&pStream->pWorker->tPort)) ||
			(pStream->pWorker->tPort.pOps == xrtNetPortEpollOps() &&
			__xnetPortEpollReady(&pStream->pWorker->tPort)) ||
			(pStream->pWorker->tPort.pOps == xrtNetPortSelectOps() &&
			__xnetPortSelectReady(&pStream->pWorker->tPort))) &&
			__xnetSocketIsValid(pStream->hSocket);
	#elif defined(__APPLE__) && defined(__MACH__)
		return pStream && pStream->pWorker &&
			((pStream->pWorker->tPort.pOps == xrtNetPortKqueueOps() &&
			__xnetPortKqueueReady(&pStream->pWorker->tPort)) ||
			(pStream->pWorker->tPort.pOps == xrtNetPortSelectOps() &&
			__xnetPortSelectReady(&pStream->pWorker->tPort))) &&
			__xnetSocketIsValid(pStream->hSocket);
	#else
		return pStream && pStream->pWorker &&
			pStream->pWorker->tPort.pOps == xrtNetPortSelectOps() &&
			__xnetPortSelectReady(&pStream->pWorker->tPort) &&
			__xnetSocketIsValid(pStream->hSocket);
	#endif
}


// 内部函数：__xnetStreamUseNativePortOps
static bool __xnetStreamUseNativePortOps(xnetstream* pStream)
{
	#if defined(_WIN32) || defined(_WIN64)
		return pStream && pStream->pWorker &&
			pStream->pWorker->tPort.pOps == xrtNetPortIOCPOps();
	#elif defined(__linux__)
		return pStream && pStream->pWorker &&
			((pStream->pWorker->tPort.pOps == xrtNetPortUringOps() &&
			__xnetPortUringHasNativeRing(&pStream->pWorker->tPort)) ||
			(pStream->pWorker->tPort.pOps == xrtNetPortEpollOps() &&
			__xnetPortEpollReady(&pStream->pWorker->tPort)) ||
			(pStream->pWorker->tPort.pOps == xrtNetPortSelectOps() &&
			__xnetPortSelectReady(&pStream->pWorker->tPort)));
	#elif defined(__APPLE__) && defined(__MACH__)
		return pStream && pStream->pWorker &&
			((pStream->pWorker->tPort.pOps == xrtNetPortKqueueOps() &&
			__xnetPortKqueueReady(&pStream->pWorker->tPort)) ||
			(pStream->pWorker->tPort.pOps == xrtNetPortSelectOps() &&
			__xnetPortSelectReady(&pStream->pWorker->tPort)));
	#else
		return pStream && pStream->pWorker &&
			pStream->pWorker->tPort.pOps == xrtNetPortSelectOps() &&
			__xnetPortSelectReady(&pStream->pWorker->tPort);
	#endif
}


// 内部函数：提交监听器套接字 notice
static bool __xnetListenerSubmitSocketNotice(xnetlistener* pListener, uint16 iOpType, xsocket hSocket)
{
	xnetportsubmit tSubmit;
	if ( !pListener || !pListener->pWorker || !__xnetSocketIsValid(hSocket) ) { return false; }
	memset(&tSubmit, 0, sizeof(tSubmit));
	tSubmit.iOpType = iOpType;
	tSubmit.hSocket = (intptr_t)hSocket;
	tSubmit.pUserData = pListener;
	tSubmit.iOpId = (iOpType == XNET_PORT_OP_CLOSE) ? 0u : pListener->iAcceptOpId;
	return xrtNetPortSubmit(&pListener->pWorker->tPort, &tSubmit, 1) == XRT_NET_OK;
}


// 内部函数：__xnetListenerArmAcceptWatch
static bool __xnetListenerArmAcceptWatch(xnetlistener* pListener)
{
	uint64 iOpId;
	if ( !pListener || !pListener->bRunning || !__xnetSocketIsValid(pListener->hSocket) ) { return false; }
	iOpId = __xnetListenerNextAcceptOpId(pListener);
	if ( iOpId == 0 ) { return false; }
	pListener->iAcceptOpId = iOpId;
	__xnetListenerAddAsyncHold(pListener);
	if ( !__xnetListenerSubmitSocketNotice(pListener, XNET_PORT_OP_ACCEPT, pListener->hSocket) ) {
		#if defined(XRT_INTERNAL_TEST_ENV)
			fprintf(stderr, "[XNET ACCEPT] submit failed, worker=%u, op=%llu\n",
				pListener->pWorker ? pListener->pWorker->iId : 0u,
				(unsigned long long)iOpId);
		#endif
		__xnetListenerReleaseAsyncHold(pListener);
		return false;
	}
	(void)__xnetAtomicAddFetch32(&pListener->iAcceptArmedCount, 1);
	if ( pListener->pWorker && !__xnetEngineIsCurrentWorker(pListener->pWorker) ) {
		(void)xrtNetPortWake(&pListener->pWorker->tPort);
	}
	return true;
}


static uint32 __xnetListenerWaitCount(const xnetlistener* pListener)
{
	const __xnet_listener_wait_slot* pNode;
	uint32 iCount = 0u;
	if ( !pListener ) { return 0u; }
	for ( pNode = &pListener->tAcceptWait; pNode && pNode->pfnWait; pNode = pNode->pNext ) {
		++iCount;
	}
	return iCount;
}


static uint32 __xnetListenerDesiredAcceptCount(const xnetlistener* pListener)
{
	uint32 iDesired;
	if ( !__xnetListenerCanDispatchAccept((xnetlistener*)pListener) ) { return 0u; }
	if ( pListener->tAcceptWait.pfnWait ) {
		iDesired = __xnetListenerWaitCount(pListener);
		return iDesired < pListener->iAcceptConcurrency ? iDesired : pListener->iAcceptConcurrency;
	}
	return pListener->iAcceptConcurrency;
}


static bool __xnetListenerMaintainAcceptWatches(xnetlistener* pListener)
{
	uint32 iDesired = __xnetListenerDesiredAcceptCount(pListener);
	while ( (uint32)__xnetAtomicLoad32(&pListener->iAcceptArmedCount) < iDesired ) {
		if ( !__xnetListenerArmAcceptWatch(pListener) ) { return false; }
	}
	return true;
}


// 内部函数：__xnetListenerCancelAcceptWatch
static bool __xnetListenerCancelAcceptWatch(xnetlistener* pListener)
{
	if ( !pListener ) { return false; }
	pListener->iAcceptOpId = 0;
	return true;
}


// 内部函数：__xnetSocketSetReuseAddr
static bool __xnetSocketSetReuseAddr(xsocket hSocket)
{
	int iOpt = 1;
	if ( !__xnetSocketIsValid(hSocket) ) { return false; }
	#if defined(_WIN32) || defined(_WIN64)
		return setsockopt(hSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&iOpt, sizeof(iOpt)) == 0;
	#else
		return setsockopt(hSocket, SOL_SOCKET, SO_REUSEADDR, &iOpt, sizeof(iOpt)) == 0;
	#endif
}


// 内部函数：设置 Windows 独占监听地址，避免活动端口被其他 listener 抢占
static bool __xnetSocketSetExclusiveAddr(xsocket hSocket)
{
	if ( !__xnetSocketIsValid(hSocket) ) { return false; }
	#if defined(_WIN32) || defined(_WIN64)
		int iOpt = 1;
		return setsockopt(hSocket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
			(const char*)&iOpt, sizeof(iOpt)) == 0;
	#else
		(void)hSocket;
		return true;
	#endif
}


// 内部函数：设置套接字 reuse 端口
static bool __xnetSocketSetReusePort(xsocket hSocket)
{
	#if defined(SO_REUSEPORT)
		int iOpt = 1;
		if ( !__xnetSocketIsValid(hSocket) ) { return false; }
		#if defined(_WIN32) || defined(_WIN64)
			return setsockopt(hSocket, SOL_SOCKET, SO_REUSEPORT, (const char*)&iOpt, sizeof(iOpt)) == 0;
		#else
			return setsockopt(hSocket, SOL_SOCKET, SO_REUSEPORT, &iOpt, sizeof(iOpt)) == 0;
		#endif
	#else
		(void)hSocket;
		return false;
	#endif
}


// 内部函数：__xnetSocketSetNoDelay
static bool __xnetSocketSetNoDelay(xsocket hSocket)
{
	int iOpt = 1;
	if ( !__xnetSocketIsValid(hSocket) ) { return false; }
	#if defined(_WIN32) || defined(_WIN64)
		return setsockopt(hSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&iOpt, sizeof(iOpt)) == 0;
	#else
		return setsockopt(hSocket, IPPROTO_TCP, TCP_NODELAY, &iOpt, sizeof(iOpt)) == 0;
	#endif
}


// 内部函数：__xnetSocketSetKeepAlive
static bool __xnetSocketSetKeepAlive(xsocket hSocket)
{
	int iOpt = 1;
	if ( !__xnetSocketIsValid(hSocket) ) { return false; }
	#if defined(_WIN32) || defined(_WIN64)
		return setsockopt(hSocket, SOL_SOCKET, SO_KEEPALIVE, (const char*)&iOpt, sizeof(iOpt)) == 0;
	#else
		return setsockopt(hSocket, SOL_SOCKET, SO_KEEPALIVE, &iOpt, sizeof(iOpt)) == 0;
	#endif
}


// 内部函数：__xnetSocketUpdateLocalAddr
static bool __xnetSocketUpdateLocalAddr(xsocket hSocket, xnetaddr* pAddr)
{
	struct sockaddr_storage tStorage;
	socklen_t iLen = (socklen_t)sizeof(tStorage);
	if ( !__xnetSocketIsValid(hSocket) || !pAddr ) { return false; }
	memset(&tStorage, 0, sizeof(tStorage));
	if ( getsockname(hSocket, (struct sockaddr*)&tStorage, &iLen) != 0 ) { return false; }
	return __xnetAddrFromSockAddr(pAddr, (const struct sockaddr*)&tStorage);
}


// 内部函数：__xnetSocketUpdateRemoteAddr
static bool __xnetSocketUpdateRemoteAddr(xsocket hSocket, xnetaddr* pAddr)
{
	struct sockaddr_storage tStorage;
	socklen_t iLen = (socklen_t)sizeof(tStorage);
	if ( !__xnetSocketIsValid(hSocket) || !pAddr ) { return false; }
	memset(&tStorage, 0, sizeof(tStorage));
	if ( getpeername(hSocket, (struct sockaddr*)&tStorage, &iLen) != 0 ) { return false; }
	return __xnetAddrFromSockAddr(pAddr, (const struct sockaddr*)&tStorage);
}


// 内部函数：__xnetSocketApplyConnectFlags
static bool __xnetSocketApplyConnectFlags(xsocket hSocket, uint32 iFlags)
{
	bool bOk = true;
	if ( (iFlags & XNET_CONNECT_F_NO_DELAY) != 0 ) {
		bOk = __xnetSocketSetNoDelay(hSocket) && bOk;
	}
	if ( (iFlags & XNET_CONNECT_F_KEEPALIVE) != 0 ) {
		bOk = __xnetSocketSetKeepAlive(hSocket) && bOk;
	}
	return bOk;
}


// 内部函数：__xnetSocketApplyListenFlags
static bool __xnetSocketApplyListenFlags(xsocket hSocket, uint32 iFlags)
{
	bool bOk = true;
	if ( (iFlags & XNET_LISTEN_F_EXCLUSIVE_ADDR) != 0 ) {
		bOk = __xnetSocketSetExclusiveAddr(hSocket) && bOk;
	}
	if ( (iFlags & XNET_LISTEN_F_REUSE_ADDR) != 0 ) {
		bOk = __xnetSocketSetReuseAddr(hSocket) && bOk;
	}
	if ( (iFlags & XNET_LISTEN_F_REUSE_PORT) != 0 ) {
		(void)__xnetSocketSetReusePort(hSocket);
	}
	return bOk;
}


// 内部函数：__xnetStreamApplyWatermark
static void __xnetStreamApplyWatermark(xnetstream* pStream, uint32 iHighWater, uint32 iLowWater)
{
	if ( !pStream ) { return; }
	if ( iHighWater == 0 ) { iHighWater = 1; }
	if ( iLowWater > iHighWater ) { iLowWater = iHighWater; }
	pStream->tSendQ.iHighWater = iHighWater;
	pStream->tSendQ.iLowWater = iLowWater;
}


// 内部函数：__xnetStreamApplyDefaults
static void __xnetStreamApplyDefaults(xnetstream* pStream, const xnetconnectconfig* pConnectCfg, const xnetlistenconfig* pListenCfg)
{
	uint32 iHighWater;
	uint32 iLowWater;
	uint32 iRecvLimit;
	uint32 iMaxQueuedBytes;

	if ( !pStream || !pStream->pEngine ) { return; }

	// 从引擎配置中读取默认的高水位、低水位和接收限制
	iHighWater = pStream->pEngine->tConfig.iDefaultHighWater;
	iLowWater = pStream->pEngine->tConfig.iDefaultLowWater;
	iRecvLimit = 1048576u;
	iMaxQueuedBytes = pStream->pEngine->tConfig.iDefaultMaxQueuedBytes;
	if ( iMaxQueuedBytes == 0u ) { iMaxQueuedBytes = 1048576u; }

	// 监听配置优先覆盖默认值
	if ( pListenCfg ) {
		if ( pListenCfg->iHighWater > 0 ) { iHighWater = pListenCfg->iHighWater; }
		if ( pListenCfg->iLowWater <= iHighWater ) { iLowWater = pListenCfg->iLowWater; }
		if ( pListenCfg->iRecvLimit > 0 ) { iRecvLimit = pListenCfg->iRecvLimit; }
		if ( pListenCfg->iMaxQueuedBytes > 0 ) { iMaxQueuedBytes = pListenCfg->iMaxQueuedBytes; }
	}
	// 连接配置再次覆盖（优先级最高）
	if ( pConnectCfg ) {
		if ( pConnectCfg->iHighWater > 0 ) { iHighWater = pConnectCfg->iHighWater; }
		if ( pConnectCfg->iLowWater <= iHighWater ) { iLowWater = pConnectCfg->iLowWater; }
		if ( pConnectCfg->iRecvLimit > 0 ) { iRecvLimit = pConnectCfg->iRecvLimit; }
		if ( pConnectCfg->iMaxQueuedBytes > 0 ) { iMaxQueuedBytes = pConnectCfg->iMaxQueuedBytes; }
	}

	if ( iHighWater > iMaxQueuedBytes ) { iHighWater = iMaxQueuedBytes; }
	if ( iLowWater > iHighWater ) { iLowWater = iHighWater; }
	// 应用水位线和接收限制到流
	__xnetStreamApplyWatermark(pStream, iHighWater, iLowWater);
	pStream->iRecvLimit = iRecvLimit;
	pStream->iMaxQueuedBytes = iMaxQueuedBytes;
	pStream->iConnectTimeoutMs = pConnectCfg ? pConnectCfg->iConnectTimeoutMs : 0u;
	pStream->iConnectFallbackDelayMs = 250u;
	if ( pConnectCfg && pConnectCfg->iSize >=
		(uint32)(offsetof(xnetconnectconfig, iFallbackDelayMs) + sizeof(pConnectCfg->iFallbackDelayMs)) ) {
		pStream->iConnectFallbackDelayMs = pConnectCfg->iFallbackDelayMs;
	}
}


// 内部函数：__xnetStreamHasPreOpenGate
static bool __xnetStreamHasPreOpenGate(const xnetstream* pStream)
{
	return pStream && (pStream->iState & __XNET_STREAM_STATE_OPEN_EMITTED) == 0;
}


// 内部函数：打开流定时器 id
static uint64 __xnetStreamOpenTimerId(const xnetstream* pStream)
{
	return pStream ? (uint64)(uintptr_t)pStream : 0u;
}


// 内部函数：打开流 arm 定时器
static bool __xnetStreamArmOpenTimer(xnetstream* pStream, uint32 iTimeoutMs)
{
	if ( !pStream || !pStream->pWorker || iTimeoutMs == 0u ) { return false; }
	if ( __xnetAtomicCompareExchange32(&pStream->iOpenTimerState, 1, 0) != 0 ) { return false; }
	__xnetStreamAddAsyncHold(pStream);
	if ( xrtNetPortArmTimer(&pStream->pWorker->tPort, __xnetStreamOpenTimerId(pStream), iTimeoutMs) != XRT_NET_OK ) {
		(void)__xnetAtomicExchange32(&pStream->iOpenTimerState, 0);
		__xnetStreamReleaseAsyncHold(pStream);
		return false;
	}
	return true;
}


// 内部函数：打开流 cancel 定时器
static void __xnetStreamCancelOpenTimer(xnetstream* pStream)
{
	if ( !pStream || !pStream->pWorker ) { return; }
	if ( __xnetAtomicCompareExchange32(&pStream->iOpenTimerState, 0, 1) != 1 ) { return; }
	if ( xrtNetPortCancelTimer(&pStream->pWorker->tPort, __xnetStreamOpenTimerId(pStream)) == XRT_NET_OK ) {
		__xnetStreamReleaseAsyncHold(pStream);
	}
}


// 内部函数：流所有权相关处理
static ptr __xnetStreamOwner(xnetstream* pStream)
{
	return pStream ? pStream->pUserData : NULL;
}


// 内部函数：__xnetStreamAttachTls
static bool __xnetStreamAttachTls(xnetstream* pStream, const xtlsconfig* pCfg, bool bIsServer)
{
	if ( !pStream ) { return false; }
	if ( !pCfg ) { return true; }
	#ifdef XRT_NO_NETTLS
		(void)bIsServer;
		xrtSetError((str)"NETTLS module is disabled.", FALSE);
		return false;
	#else
	pStream->pTls = xrtNetTlsSessionCreate(pCfg, bIsServer);
	return pStream->pTls != NULL;
	#endif
}


// 先从后端端口登记中移除监听 socket，再关闭句柄，避免句柄复用命中陈旧登记。
static void __xnetListenerCloseSocket(xnetlistener* pListener)
{
	xsocket hSocket;
	if ( !pListener || !__xnetSocketIsValid(pListener->hSocket) ) { return; }
	hSocket = pListener->hSocket;
	(void)__xnetListenerSubmitSocketNotice(pListener, XNET_PORT_OP_CLOSE, hSocket);
	__xnetSocketCloseHandle(&pListener->hSocket);
}


// 内部函数：__xnetStreamDetachTls
static void __xnetStreamDetachTls(xnetstream* pStream)
{
	if ( !pStream || !pStream->pTls ) { return; }
	#ifndef XRT_NO_NETTLS
	xrtNetTlsSessionDestroy(pStream->pTls);
	#endif
	pStream->pTls = NULL;
	pStream->bTlsCloseQueued = false;
}


// 内部函数：清除流代理状态
static void __xnetStreamClearProxyState(xnetstream* pStream)
{
	if ( !pStream || !pStream->pProxyState ) { return; }
	__xnetProxyStateDestroy(pStream->pProxyState);
	pStream->pProxyState = NULL;
}


// 内部函数：__xnetStreamDetachProxy
static void __xnetStreamDetachProxy(xnetstream* pStream)
{
	if ( !pStream ) { return; }
	__xnetStreamClearProxyState(pStream);
	if ( pStream->pProxy ) {
		xrtNetProxyRelease(pStream->pProxy);
		pStream->pProxy = NULL;
	}
}


// 内部函数：__xnetStreamTlsReady
static bool __xnetStreamTlsReady(const xnetstream* pStream)
{
	#ifdef XRT_NO_NETTLS
		(void)pStream;
		return false;
	#else
	return pStream && pStream->pTls ? xrtNetTlsSessionIsReady(pStream->pTls) : false;
	#endif
}


// 获取 TLS 客户端握手中的 SNI，非 TLS 连接返回 NULL
XXAPI const char* xrtNetStreamTlsSNI(const xnetstream* pStream)
{
	#ifdef XRT_NO_NETTLS
		(void)pStream;
		return NULL;
	#else
	return pStream && pStream->pTls ? xrtNetTlsSessionGetSNI(pStream->pTls) : NULL;
	#endif
}


// 内部函数：__xnetStreamTryMarkCloseEmitted
static bool __xnetStreamTryMarkCloseEmitted(xnetstream* pStream)
{
	long iOldState;
	long iNewState;
	if ( !pStream ) { return false; }
	do {
		iOldState = __xnetAtomicLoad32((volatile long*)&pStream->iState);
		if ( (iOldState & __XNET_STREAM_STATE_CLOSE_EMITTED) != 0 ) { return false; }
		iNewState = iOldState | __XNET_STREAM_STATE_CLOSE_EMITTED;
	} while ( __xnetAtomicCompareExchange32((volatile long*)&pStream->iState, iNewState, iOldState) != iOldState );
	return true;
}


// 内部函数：__xnetStreamEmitClose
static void __xnetStreamEmitClose(xnetstream* pStream, xnet_result iReason)
{
	(void)__xnetStreamRequestClose(pStream, pStream ? pStream->iFlags : 0u);
	if ( !__xnetStreamTryMarkCloseEmitted(pStream) ) { return; }
	pStream->iCloseReason = iReason;
	__xnetStreamNotifySyncReadable(pStream, iReason);
	__xnetStreamNotifySyncWritable(pStream, iReason);
	__xnetStreamNotifySyncClose(pStream, iReason);
	if ( pStream->pEvents && pStream->pEvents->OnClose ) {
		pStream->pEvents->OnClose(__xnetStreamOwner(pStream), pStream, iReason);
	}
}


// 内部函数：发送流 refresh 状态
static void __xnetStreamRefreshSendState(xnetstream* pStream, uint32 iPrevQueuedBytes, bool bPrevHighWater)
{
	size_t iQueuedBytes;
	if ( !pStream ) { return; }

	// 重新计算发送队列中的字节数
	iQueuedBytes = xrtNetChainBytes(&pStream->tSendQ.tQueue);
	pStream->tSendQ.iQueuedBytes = iQueuedBytes > UINT32_MAX ? UINT32_MAX : (uint32)iQueuedBytes;

	// 检测高水位触发：之前未触发但现在超过高水位阈值
	if ( !bPrevHighWater && pStream->tSendQ.iHighWater > 0 && pStream->tSendQ.iQueuedBytes >= pStream->tSendQ.iHighWater ) {
		pStream->tSendQ.bHighWaterHit = true;
		if ( pStream->pEvents && pStream->pEvents->OnHighWater ) {
			pStream->pEvents->OnHighWater(__xnetStreamOwner(pStream), pStream, pStream->tSendQ.iQueuedBytes);
		}
	} else if ( bPrevHighWater && pStream->tSendQ.iQueuedBytes <= pStream->tSendQ.iLowWater ) {
		// 高水位恢复：之前触发但现在已回落到低水位以下
		pStream->tSendQ.bHighWaterHit = false;
		__xnetStreamNotifySyncWritable(pStream, XRT_NET_OK);
		if ( pStream->pEvents && pStream->pEvents->OnLowWater ) {
			pStream->pEvents->OnLowWater(__xnetStreamOwner(pStream), pStream, pStream->tSendQ.iQueuedBytes);
		}
	}

	// 发送队列从有数据变为空时，触发排空通知
	if ( iPrevQueuedBytes > 0 && pStream->tSendQ.iQueuedBytes == 0 && pStream->pEvents && pStream->pEvents->OnDrain ) {
		__xnetStreamNotifySyncDrain(pStream, XRT_NET_OK);
		pStream->pEvents->OnDrain(__xnetStreamOwner(pStream), pStream);
	}
	else if ( iPrevQueuedBytes > 0 && pStream->tSendQ.iQueuedBytes == 0 ) {
		__xnetStreamNotifySyncDrain(pStream, XRT_NET_OK);
	}
}


// 内部函数：发送流 consume 队列
static size_t __xnetStreamConsumeSendQueue(xnetstream* pStream, size_t iLen)
{
	size_t iQueuedBytes;
	uint32 iPrevQueuedBytes;
	bool bPrevHighWater;

	if ( !pStream || iLen == 0 ) { return 0; }

	// 检查发送队列是否有数据
	iQueuedBytes = xrtNetChainBytes(&pStream->tSendQ.tQueue);
	if ( iQueuedBytes == 0 ) {
		// 队列已空且流正在关闭，触发关闭事件
		if ( pStream->bClosing ) {
			__xnetStreamEmitClose(pStream, XRT_NET_CLOSED);
		}
		return 0;
	}
	if ( iLen > iQueuedBytes ) { iLen = iQueuedBytes; }

	// 保存当前水位状态用于后续刷新判断
	iPrevQueuedBytes = pStream->tSendQ.iQueuedBytes;
	bPrevHighWater = pStream->tSendQ.bHighWaterHit;
	// 从发送队列中消费指定长度的数据
	xrtNetChainConsume(&pStream->tSendQ.tQueue, iLen);
	__xnetStreamConsumeSendBudget(pStream, iLen);
	// 刷新发送状态（水位、排空通知等）
	__xnetStreamRefreshSendState(pStream, iPrevQueuedBytes, bPrevHighWater);
	// 消费后队列为空且流正在关闭，执行关闭收尾
	if ( pStream->bClosing && pStream->tSendQ.iQueuedBytes == 0 ) {
		if ( (pStream->iFlags & XNET_CLOSE_F_WAIT_PEER) != 0 ) {
			__xnetStreamBeginGracefulCloseWait(pStream);
		} else {
			__xnetStreamFinishClose(pStream, XRT_NET_CLOSED);
		}
	}
	return iLen;
}


// 内部函数：复制流队列
static bool __xnetStreamQueueCopy(xnetstream* pStream, const void* pData, size_t iLen)
{
	return pStream ? xrtNetChainAppendCopy(&pStream->tSendQ.tQueue, pData, iLen) : false;
}


// 内部函数：__xnetStreamQueueRef
static bool __xnetStreamQueueRef(xnetstream* pStream, const xnetbufref* pRef)
{
	return pStream ? xrtNetChainAppendRef(&pStream->tSendQ.tQueue, pRef) : false;
}


// 内部函数：分配流临时 chain
static xnetchain* __xnetStreamAllocTempChain(xnetstream* pStream)
{
	xnetchain* pChain;
	xnetmemctx* pMemCtx;
	if ( !pStream || !pStream->pWorker ) { return NULL; }
	pChain = (xnetchain*)XNET_ALLOC(sizeof(xnetchain));
	if ( !pChain ) { return NULL; }
	pMemCtx = &pStream->pWorker->tMemCtx;
	xrtNetChainInitEx(pChain, pMemCtx);
	return pChain;
}


// 内部函数：释放流临时 chain
static void __xnetStreamFreeTempChain(xnetchain* pChain)
{
	if ( !pChain ) { return; }
	xrtNetChainClear(pChain);
	XNET_FREE(pChain);
}


static bool __xnetStreamAppendSendCopyEx(xnetstream* pStream, const void* pData, size_t iLen,
	bool bBudgetReserved)
{
	__xnet_send_budget_segment* pSegment;
	uint32 iPrevQueuedBytes;
	bool bPrevHighWater;
	if ( !pStream || !pData || iLen == 0 ) {
		if ( bBudgetReserved ) { __xnetStreamReleaseSend(pStream, iLen); }
		return false;
	}
	if ( !bBudgetReserved && !__xnetStreamTryReserveSend(pStream, iLen) ) { return false; }
	pSegment = __xnetStreamAllocSendBudget(iLen, iLen);
	if ( !pSegment ) {
		__xnetStreamReleaseSend(pStream, iLen);
		return false;
	}
	iPrevQueuedBytes = pStream->tSendQ.iQueuedBytes;
	bPrevHighWater = pStream->tSendQ.bHighWaterHit;
	if ( !__xnetStreamQueueCopy(pStream, pData, iLen) ) {
		__xnetStreamReleaseSend(pStream, iLen);
		XNET_FREE(pSegment);
		return false;
	}
	__xnetStreamTrackSendBudget(pStream, pSegment);
	__xnetStreamRefreshSendState(pStream, iPrevQueuedBytes, bPrevHighWater);
	return true;
}


// 内部函数：__xnetStreamAppendSendCopy
static bool __xnetStreamAppendSendCopy(xnetstream* pStream, const void* pData, size_t iLen)
{
	return __xnetStreamAppendSendCopyEx(pStream, pData, iLen, false);
}


static bool __xnetStreamAppendControlSendCopy(xnetstream* pStream, const void* pData, size_t iLen)
{
	if ( !__xnetStreamTryReserveControlSend(pStream, iLen) ) { return false; }
	return __xnetStreamAppendSendCopyEx(pStream, pData, iLen, true);
}


// 内部函数：__xnetStreamAppendSendVec
static bool __xnetStreamAppendSendVecEx(xnetstream* pStream, const xnetspan* pVec, uint32 iCount,
	bool bBudgetReserved)
{
	xnetchain tTemp;
	__xnet_send_budget_segment* pSegment;
	uint64 iTotal = 0u;
	uint32 iPrevQueuedBytes;
	bool bPrevHighWater;
	if ( !pStream || !pVec || iCount == 0 ) { return false; }
	for ( uint32 i = 0; i < iCount; ++i ) {
		if ( !pVec[i].pData || pVec[i].iLen == 0 || iTotal > UINT64_MAX - pVec[i].iLen ) {
			if ( bBudgetReserved ) { __xnetStreamReleaseSend(pStream, iTotal); }
			return false;
		}
		iTotal += pVec[i].iLen;
	}
	if ( !bBudgetReserved && !__xnetStreamTryReserveSend(pStream, iTotal) ) { return false; }
	pSegment = __xnetStreamAllocSendBudget(iTotal, iTotal);
	if ( !pSegment ) {
		__xnetStreamReleaseSend(pStream, iTotal);
		return false;
	}

	xrtNetChainInitEx(&tTemp, (xnetmemctx*)pStream->tSendQ.tQueue.pMemCtx);
	for ( uint32 i = 0; i < iCount; ++i ) {
		if ( !xrtNetChainAppendCopy(&tTemp, pVec[i].pData, pVec[i].iLen) ) {
			xrtNetChainClear(&tTemp);
			__xnetStreamReleaseSend(pStream, iTotal);
			XNET_FREE(pSegment);
			return false;
		}
	}

	iPrevQueuedBytes = pStream->tSendQ.iQueuedBytes;
	bPrevHighWater = pStream->tSendQ.bHighWaterHit;
	__xnetChainSplice(&pStream->tSendQ.tQueue, &tTemp);
	__xnetStreamTrackSendBudget(pStream, pSegment);
	__xnetStreamRefreshSendState(pStream, iPrevQueuedBytes, bPrevHighWater);
	return true;
}


static bool UNUSED_ATTR __xnetStreamAppendSendVec(xnetstream* pStream, const xnetspan* pVec, uint32 iCount)
{
	return __xnetStreamAppendSendVecEx(pStream, pVec, iCount, false);
}


// 内部函数：__xnetStreamAppendSendRef
static bool __xnetStreamAppendSendRefEx(xnetstream* pStream, const xnetbufref* pRef,
	bool bBudgetReserved)
{
	__xnet_send_budget_segment* pSegment;
	uint32 iPrevQueuedBytes;
	bool bPrevHighWater;
	if ( !pStream || !pRef || !pRef->pData || pRef->iLen == 0 ) {
		if ( bBudgetReserved && pRef ) { __xnetStreamReleaseSend(pStream, pRef->iLen); }
		return false;
	}
	if ( !bBudgetReserved && !__xnetStreamTryReserveSend(pStream, pRef->iLen) ) { return false; }
	pSegment = __xnetStreamAllocSendBudget(pRef->iLen, pRef->iLen);
	if ( !pSegment ) {
		__xnetStreamReleaseSend(pStream, pRef->iLen);
		return false;
	}
	iPrevQueuedBytes = pStream->tSendQ.iQueuedBytes;
	bPrevHighWater = pStream->tSendQ.bHighWaterHit;
	if ( !__xnetStreamQueueRef(pStream, pRef) ) {
		__xnetStreamReleaseSend(pStream, pRef->iLen);
		XNET_FREE(pSegment);
		return false;
	}
	__xnetStreamTrackSendBudget(pStream, pSegment);
	__xnetStreamRefreshSendState(pStream, iPrevQueuedBytes, bPrevHighWater);
	return true;
}


static bool UNUSED_ATTR __xnetStreamAppendSendRef(xnetstream* pStream, const xnetbufref* pRef)
{
	return __xnetStreamAppendSendRefEx(pStream, pRef, false);
}


static bool __xnetStreamAppendControlSendRef(xnetstream* pStream, const xnetbufref* pRef)
{
	if ( !pRef || !__xnetStreamTryReserveControlSend(pStream, pRef->iLen) ) { return false; }
	return __xnetStreamAppendSendRefEx(pStream, pRef, true);
}


// 内部函数：__xnetStreamAppendRecvCopy
static bool __xnetStreamAppendRecvCopy(xnetstream* pStream, const void* pData, size_t iLen)
{
	bool bOk;
	bool bOverflow;
	if ( !pStream || !pData || iLen == 0 ) { return false; }
	__xnetStreamRxLock(pStream);
	bOverflow = pStream->iRecvLimit > 0 && xrtNetChainBytes(&pStream->tRxChain) + iLen > pStream->iRecvLimit;
	bOk = !bOverflow && xrtNetChainAppendCopy(&pStream->tRxChain, pData, iLen);
	__xnetStreamRxUnlock(pStream);
	if ( bOverflow ) {
		if ( pStream->pEvents && pStream->pEvents->OnError ) {
			pStream->pEvents->OnError(__xnetStreamOwner(pStream), pStream, -1);
		}
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return false;
	}
	if ( bOk ) {
		__xnetStreamNotifySyncReadable(pStream, XRT_NET_OK);
	}
	return bOk;
}


// 内部函数：__xnetStreamAppendRecvRef
static bool UNUSED_ATTR __xnetStreamAppendRecvRef(xnetstream* pStream, const xnetbufref* pRef)
{
	bool bOk;
	bool bOverflow;
	if ( !pStream || !pRef ) { return false; }
	__xnetStreamRxLock(pStream);
	bOverflow = pStream->iRecvLimit > 0 && xrtNetChainBytes(&pStream->tRxChain) + pRef->iLen > pStream->iRecvLimit;
	bOk = !bOverflow && xrtNetChainAppendRef(&pStream->tRxChain, pRef);
	__xnetStreamRxUnlock(pStream);
	if ( bOverflow ) {
		if ( pStream->pEvents && pStream->pEvents->OnError ) {
			pStream->pEvents->OnError(__xnetStreamOwner(pStream), pStream, -1);
		}
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return false;
	}
	if ( bOk ) {
		__xnetStreamNotifySyncReadable(pStream, XRT_NET_OK);
	}
	return bOk;
}


// 内部函数：__xnetStreamDispatchRecv
static void __xnetStreamDispatchRecv(xnetstream* pStream)
{
	if ( !pStream || __xnetStreamReadPaused(pStream) ) { return; }
	if ( __xnetStreamRxBytes(pStream) == 0 ) { return; }
	if ( pStream->pEvents && pStream->pEvents->OnRecv ) {
		pStream->pEvents->OnRecv(__xnetStreamOwner(pStream), pStream, &pStream->tRxChain);
	}
}


// 内部函数：获取流 drive 代理状态
static bool __xnetStreamDriveProxyState(xnetstream* pStream, const void* pData, size_t iLen)
{
	char aSend[2048];
	size_t iSendLen = 0;
	uint32 iAction;
	xnetchain* pCarry = NULL;
	if ( !pStream || !pStream->pProxy || !pStream->pProxyState ) { return true; }

	// 根据代理状态阶段，发起握手或喂入接收数据
	if ( pStream->pProxyState->iStage == __XNET_PROXY_STAGE_INIT ) {
		iAction = __xnetProxyStateBegin(pStream->pProxyState, pStream->pProxy, aSend, sizeof(aSend), &iSendLen);
	} else {
		iAction = __xnetProxyStateFeed(pStream->pProxyState, pStream->pProxy, pData, iLen, aSend, sizeof(aSend), &iSendLen);
	}

	// 代理协议等待更多数据
	if ( iAction == __XNET_PROXY_ACTION_WAIT ) {
		return true;
	}
	// 代理协议需要发送握手数据
	if ( iAction == __XNET_PROXY_ACTION_SEND ) {
		if ( iSendLen == 0 || !__xnetStreamAppendSendCopy(pStream, aSend, iSendLen) ) {
			iAction = __XNET_PROXY_ACTION_ERROR;
		} else {
			__xnetStreamKickWrite(pStream);
			return true;
		}
	}
	// 代理握手完成，进入正常数据传输
	if ( iAction == __XNET_PROXY_ACTION_READY ) {
		// 保存代理阶段接收到的残余数据
		if ( pStream->pProxyState->iRecvLen > 0 ) {
			pCarry = __xnetStreamAllocTempChain(pStream);
			if ( !pCarry || !xrtNetChainAppendCopy(pCarry, pStream->pProxyState->pRecv, pStream->pProxyState->iRecvLen) ) {
				__xnetStreamFreeTempChain(pCarry);
				iAction = __XNET_PROXY_ACTION_ERROR;
			}
		}
		if ( iAction == __XNET_PROXY_ACTION_READY ) {
			// 清除代理状态，进入正常流模式
			__xnetStreamClearProxyState(pStream);
			if ( pStream->pTls ) {
				// 有 TLS 配置，继续驱动 TLS 握手
				if ( !__xnetStreamDriveTlsHandshake(pStream) ) {
					__xnetStreamFreeTempChain(pCarry);
					return false;
				}
				if ( pCarry ) {
					__xnetStreamHandleRecvEvent(pStream, pCarry);
				}
			} else {
				// 无 TLS，触发 OnOpen 事件
				__xnetStreamEmitOpen(pStream);
				if ( pCarry ) {
					// 处理代理阶段残留的接收数据
					__xnetStreamHandleRecvEvent(pStream, pCarry);
				} else if ( !pStream->bClosing && __xnetSocketIsValid(pStream->hSocket) && !__xnetStreamRecvArmed(pStream) ) {
					(void)__xnetStreamArmRecvWatch(pStream);
				}
			}
			return true;
		}
	}
	// 代理握手失败，触发错误并关闭流
	if ( iAction == __XNET_PROXY_ACTION_ERROR ) {
		__xnetStreamFreeTempChain(pCarry);
		if ( pStream->pEvents && pStream->pEvents->OnError ) {
			pStream->pEvents->OnError(__xnetStreamOwner(pStream), pStream, -1);
		}
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return false;
	}
	return true;
}


// 内部函数：__xnetStreamBuildSendSpans
static uint32 __xnetStreamBuildSendSpans(const xnetstream* pStream, xnetspan* pOut, uint32 iMaxCount)
{
	if ( !pStream || !pOut || iMaxCount == 0 ) { return 0; }
	return xrtNetChainGetSpans(&pStream->tSendQ.tQueue, pOut, iMaxCount);
}


// 内部函数：__xnetStreamTryBeginWrite
static bool __xnetStreamTryBeginWrite(xnetstream* pStream, xnetspan* pOut, uint32 iMaxCount, uint32* pSpanCount)
{
	uint32 iCount;
	if ( pSpanCount ) { *pSpanCount = 0; }
	if ( !pStream || !pOut || iMaxCount == 0 ) { return false; }
	if ( pStream->tSendQ.bWritePosted || pStream->tSendQ.iQueuedBytes == 0 ) { return false; }
	iCount = __xnetStreamBuildSendSpans(pStream, pOut, iMaxCount);
	if ( iCount == 0 ) { return false; }
	pStream->tSendQ.bWritePosted = true;
	if ( pSpanCount ) { *pSpanCount = iCount; }
	return true;
}


// 内部函数：__xnetStreamSubmitWrite
static bool __xnetStreamSubmitWrite(xnetstream* pStream)
{
	xnetspan arrVec[16];
	xnetportsubmit tSubmit;
	uint32 iSpanCount = 0;
	if ( !pStream || !pStream->pWorker ) { return false; }
	if ( !__xnetStreamTryBeginWrite(pStream, arrVec, 16, &iSpanCount) ) { return false; }
	memset(&tSubmit, 0, sizeof(tSubmit));
	tSubmit.iOpType = XNET_PORT_OP_SEND;
	tSubmit.iOpId = pStream->iId;
	tSubmit.hSocket = (intptr_t)pStream->hSocket;
	tSubmit.pUserData = pStream;
	tSubmit.pVec = arrVec;
	tSubmit.iVecCount = iSpanCount;
	tSubmit.tAddr = pStream->tRemoteAddr;
	__xnetStreamAddAsyncHold(pStream);
	if ( xrtNetPortSubmit(&pStream->pWorker->tPort, &tSubmit, 1) != XRT_NET_OK ) {
		__xnetStreamReleaseAsyncHold(pStream);
		pStream->tSendQ.bWritePosted = false;
		return false;
	}
	return true;
}


// 内部函数：__xnetStreamKickWrite
static void __xnetStreamKickWrite(xnetstream* pStream)
{
	if ( !pStream ) { return; }
	if ( !__xnetStreamSubmitWrite(pStream) ) {
		#if defined(XNET_DEBUG_IOCP_NATIVE)
			fprintf(stderr, "[STREAM] kick write submit fail stream=%llu socket=%p queued=%u native=%d\n",
				(unsigned long long)pStream->iId,
				(void*)(uintptr_t)pStream->hSocket,
				pStream->tSendQ.iQueuedBytes,
				__xnetStreamUseNativePortIO(pStream) ? 1 : 0);
		#endif
	}
}


static bool __xnetStreamQueueTlsCipherEx(xnetstream* pStream, uint64 iBudgetBytes,
	bool bBudgetReserved)
{
	#ifdef XRT_NO_NETTLS
		(void)pStream;
		(void)iBudgetBytes;
		(void)bBudgetReserved;
		return false;
	#else
	char aBuf[4096];
	xnetchain tTemp;
	__xnet_send_budget_segment* pSegment;
	size_t iWireBytes;
	uint32 iPrevQueuedBytes;
	bool bPrevHighWater;
	if ( !pStream || !pStream->pTls ) {
		if ( bBudgetReserved ) { __xnetStreamReleaseSend(pStream, iBudgetBytes); }
		return false;
	}
	iWireBytes = xrtNetTlsSessionPendingCipher(pStream->pTls);
	if ( iWireBytes == 0u ) {
		if ( bBudgetReserved ) {
			__xnetStreamReleaseSend(pStream, iBudgetBytes);
			return false;
		}
		return true;
	}
	if ( !bBudgetReserved ) { iBudgetBytes = iWireBytes; }
	if ( iBudgetBytes == 0u || (!bBudgetReserved && !__xnetStreamTryReserveSend(pStream, iBudgetBytes)) ) {
		return false;
	}
	pSegment = __xnetStreamAllocSendBudget(iWireBytes, iBudgetBytes);
	if ( !pSegment ) {
		__xnetStreamReleaseSend(pStream, iBudgetBytes);
		return false;
	}
	xrtNetChainInitEx(&tTemp, (xnetmemctx*)pStream->tSendQ.tQueue.pMemCtx);
	while ( xrtNetTlsSessionPendingCipher(pStream->pTls) > 0 ) {
		size_t iRead = 0;
		if ( xrtNetTlsSessionPeekCipher(pStream->pTls, aBuf, sizeof(aBuf), &iRead) != XRT_NET_OK || iRead == 0 ) {
			xrtNetChainClear(&tTemp);
			__xnetStreamReleaseSend(pStream, iBudgetBytes);
			XNET_FREE(pSegment);
			return false;
		}
		if ( !xrtNetChainAppendCopy(&tTemp, aBuf, iRead) ) {
			xrtNetChainClear(&tTemp);
			__xnetStreamReleaseSend(pStream, iBudgetBytes);
			XNET_FREE(pSegment);
			return false;
		}
		xrtNetTlsSessionConsumeCipher(pStream->pTls, iRead);
	}
	iPrevQueuedBytes = pStream->tSendQ.iQueuedBytes;
	bPrevHighWater = pStream->tSendQ.bHighWaterHit;
	__xnetChainSplice(&pStream->tSendQ.tQueue, &tTemp);
	__xnetStreamTrackSendBudget(pStream, pSegment);
	__xnetStreamRefreshSendState(pStream, iPrevQueuedBytes, bPrevHighWater);
	return true;
	#endif
}


// 内部函数：流队列 TLS cipher相关处理
static bool __xnetStreamQueueTlsCipher(xnetstream* pStream)
{
	return __xnetStreamQueueTlsCipherEx(pStream, 0u, false);
}


// 内部函数：__xnetStreamDrainTlsPlain
static bool __xnetStreamDrainTlsPlain(xnetstream* pStream)
{
	#ifdef XRT_NO_NETTLS
		(void)pStream;
		return false;
	#else
	char aBuf[4096];
	bool bReadAny = false;
	if ( !pStream || !pStream->pTls ) { return false; }
	// 循环读取 TLS 解密后的明文数据
	for ( ;; ) {
		size_t iRead = 0;
		xnet_result iRes = xrtNetTlsSessionReadPlain(pStream->pTls, aBuf, sizeof(aBuf), &iRead);
		if ( iRes == XRT_NET_OK && iRead > 0 ) {
			// 将明文数据追加到接收链
			if ( !__xnetStreamAppendRecvCopy(pStream, aBuf, iRead) ) {
				return false;
			}
			bReadAny = true;
			continue;
		}
		// TLS 需要更多密文数据才能继续解密
		if ( iRes == XRT_NET_AGAIN ) { break; }
		// 对端关闭了 TLS 连接
		if ( iRes == XRT_NET_CLOSED ) {
			if ( bReadAny && !__xnetStreamReadPaused(pStream) ) {
				__xnetStreamDispatchRecv(pStream);
			}
			__xnetStreamFinishClose(pStream, XRT_NET_CLOSED);
			return false;
		}
		// TLS 解密遇到错误
		if ( iRes != XRT_NET_OK ) {
			if ( pStream->pEvents && pStream->pEvents->OnError ) {
				pStream->pEvents->OnError(__xnetStreamOwner(pStream), pStream, -1);
			}
			xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
			return false;
		}
		break;
	}
	// 读取到了数据且读取未暂停，分发接收事件
	if ( bReadAny ) {
		if ( !__xnetStreamReadPaused(pStream) ) {
			__xnetStreamDispatchRecv(pStream);
		}
	}
	return true;
	#endif
}


// 内部函数：立即排空当前 socket 中已经到达的 TLS 密文
static bool __xnetStreamDrainTlsCipherNow(xnetstream* pStream)
{
	#ifdef XRT_NO_NETTLS
		(void)pStream;
		return false;
	#else
	char aBuf[4096];
	bool bReadAny = false;
	uint32 iSpin = 0;
	if ( !pStream || !pStream->pTls || pStream->bClosing || !__xnetSocketIsValid(pStream->hSocket) ) { return false; }
	if ( __xnetStreamUseNativePortIO(pStream) ) { return false; }
	for ( ;; ) {
		int iRet;
		#if defined(_WIN32) || defined(_WIN64)
			iRet = recv(pStream->hSocket, aBuf, (int)sizeof(aBuf), 0);
		#else
			iRet = (int)recv(pStream->hSocket, aBuf, sizeof(aBuf), 0);
		#endif
		if ( iRet > 0 ) {
			if ( xrtNetTlsSessionFeedCipher(pStream->pTls, aBuf, (size_t)iRet) != XRT_NET_OK ) {
				if ( pStream->pEvents && pStream->pEvents->OnError ) {
					pStream->pEvents->OnError(__xnetStreamOwner(pStream), pStream, -1);
				}
				xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
				return bReadAny;
			}
			bReadAny = true;
			if ( ++iSpin >= 16u ) { break; }
			continue;
		}
		if ( iRet == 0 ) {
			xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
			return bReadAny;
		}
		if ( __xnetSocketWouldBlock(__xnetSocketLastErr()) ) { break; }
		if ( pStream->pEvents && pStream->pEvents->OnError ) {
			pStream->pEvents->OnError(__xnetStreamOwner(pStream), pStream, -1);
		}
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return bReadAny;
	}
	return bReadAny;
	#endif
}


// 内部函数：短时间等待 socket 可读，用于 TLS 握手避免错过 accept 后紧随到达的数据
static bool __xnetStreamWaitReadableNow(xnetstream* pStream, uint32 iTimeoutMs)
{
	fd_set tReadSet;
	struct timeval tTimeout;
	int iRet;
	if ( !pStream || pStream->bClosing || !__xnetSocketIsValid(pStream->hSocket) ) { return false; }
	FD_ZERO(&tReadSet);
	FD_SET(pStream->hSocket, &tReadSet);
	tTimeout.tv_sec = (long)(iTimeoutMs / 1000u);
	tTimeout.tv_usec = (long)((iTimeoutMs % 1000u) * 1000u);
	#if defined(_WIN32) || defined(_WIN64)
		iRet = select(0, &tReadSet, NULL, NULL, &tTimeout);
	#else
		iRet = select((int)pStream->hSocket + 1, &tReadSet, NULL, NULL, &tTimeout);
	#endif
	return iRet > 0 && FD_ISSET(pStream->hSocket, &tReadSet);
}


// 内部函数：立即排空当前 socket 中已经到达的数据
static bool __xnetStreamDrainSocketNow(xnetstream* pStream)
{
	char aBuf[4096];
	bool bReadAny = false;
	uint32 iSpin = 0;
	if ( !pStream || pStream->pTls || pStream->bClosing || !__xnetSocketIsValid(pStream->hSocket) ) { return true; }
	for ( ;; ) {
		int iRet;
		#if defined(_WIN32) || defined(_WIN64)
			iRet = recv(pStream->hSocket, aBuf, (int)sizeof(aBuf), 0);
		#else
			iRet = (int)recv(pStream->hSocket, aBuf, sizeof(aBuf), 0);
		#endif
		if ( iRet > 0 ) {
			if ( !__xnetStreamAppendRecvCopy(pStream, aBuf, (size_t)iRet) ) {
				return false;
			}
			bReadAny = true;
			if ( ++iSpin >= 16u ) { break; }
			continue;
		}
		if ( iRet == 0 ) {
			xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
			return false;
		}
		if ( __xnetSocketWouldBlock(__xnetSocketLastErr()) ) { break; }
		if ( pStream->pEvents && pStream->pEvents->OnError ) {
			pStream->pEvents->OnError(__xnetStreamOwner(pStream), pStream, -1);
		}
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return false;
	}
	if ( bReadAny && !__xnetStreamReadPaused(pStream) ) {
		__xnetStreamDispatchRecv(pStream);
	}
	return true;
}


// 内部函数：__xnetStreamDriveTlsHandshake
static bool __xnetStreamDriveTlsHandshake(xnetstream* pStream)
{
	#ifdef XRT_NO_NETTLS
		(void)pStream;
		return false;
	#else
	xnet_result iRes = XRT_NET_AGAIN;
	uint32 iSpin = 0;
	if ( !pStream || !pStream->pTls || !__xnetSocketIsValid(pStream->hSocket) ) { return false; }
	// 代理尚未完成，不能开始 TLS 握手
	if ( pStream->pProxyState ) { return true; }
	#ifdef DEBUG_TRACE
		printf("    [XNET_TLS] drive stream=%llu server=%d queued=%u ready=%d\n",
			(unsigned long long)pStream->iId,
			pStream->pTls->bIsServer ? 1 : 0,
			pStream->tSendQ.iQueuedBytes,
			__xnetStreamTlsReady(pStream) ? 1 : 0);
	#endif
	// 发送队列中还有数据未发出，优先完成发送后再继续握手
	if ( pStream->tSendQ.iQueuedBytes > 0 ) {
		__xnetStreamKickWrite(pStream);
		return true;
	}

	// 最多循环 8 轮推进 TLS 握手状态机
	for ( iSpin = 0; iSpin < 8; ++iSpin ) {
		(void)__xnetStreamDrainTlsCipherNow(pStream);
		if ( pStream->bClosing ) { return false; }
		iRes = xrtNetTlsSessionDriveHandshake(pStream->pTls);
		#ifdef DEBUG_TRACE
			printf("    [XNET_TLS] step stream=%llu res=%d pendingCipher=%u pendingRecv=%u readable=%u\n",
				(unsigned long long)pStream->iId,
				(int)iRes,
				(uint32)xrtNetTlsSessionPendingCipher(pStream->pTls),
				(uint32)xrtNetTlsSessionPendingRecv(pStream->pTls),
				__xnetSocketBytesAvailable(pStream->hSocket));
		#endif
		// 将 TLS 产生的密文数据加入发送队列
		if ( !__xnetStreamQueueTlsCipher(pStream) ) {
			if ( pStream->pEvents && pStream->pEvents->OnError ) {
				pStream->pEvents->OnError(__xnetStreamOwner(pStream), pStream, -1);
			}
			xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
			return false;
		}
		// 密文数据已入队，提交发送后等待完成再继续握手
		if ( pStream->tSendQ.iQueuedBytes > 0 ) {
			__xnetStreamKickWrite(pStream);
			break;
		}
		// TLS 握手已完成
		if ( __xnetStreamTlsReady(pStream) ) { break; }
		// 握手返回非 AGAIN 的结果（成功或失败），停止循环
		if ( iRes != XRT_NET_AGAIN ) { break; }
		// TLS 没有更多待处理的接收数据，等待网络数据到达
		if ( xrtNetTlsSessionPendingRecv(pStream->pTls) == 0 ) {
			if ( __xnetStreamDrainTlsCipherNow(pStream) ) { continue; }
			if ( __xnetStreamWaitReadableNow(pStream, 50u) && __xnetStreamDrainTlsCipherNow(pStream) ) { continue; }
			break;
		}
	}
	// TLS 握手成功，触发 OnOpen 并排空已解密的明文数据
	if ( __xnetStreamTlsReady(pStream) ) {
		(void)__xnetStreamDrainTlsCipherNow(pStream);
		__xnetStreamEmitOpen(pStream);
		(void)__xnetStreamDrainTlsPlain(pStream);
	}
	// 重新挂起接收监视以继续接收后续数据
	if ( !pStream->bClosing && __xnetSocketIsValid(pStream->hSocket) ) {
		(void)__xnetStreamArmRecvWatch(pStream);
	}
	if ( iRes == XRT_NET_OK || iRes == XRT_NET_AGAIN ) { return true; }
	// TLS 握手失败
	if ( pStream->pEvents && pStream->pEvents->OnError ) {
		pStream->pEvents->OnError(__xnetStreamOwner(pStream), pStream, -1);
	}
	xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
	return false;
	#endif
}


static bool __xnetStreamAppendTlsPlainCopyEx(xnetstream* pStream, const void* pData, size_t iLen,
	bool bBudgetReserved)
{
	#ifdef XRT_NO_NETTLS
		(void)pStream;
		(void)pData;
		(void)iLen;
		if ( bBudgetReserved ) { __xnetStreamReleaseSend(pStream, iLen); }
		return false;
	#else
	size_t iWritten = 0;
	if ( !pStream || !pStream->pTls || !__xnetStreamTlsReady(pStream) || !pData || iLen == 0 ) {
		if ( bBudgetReserved ) { __xnetStreamReleaseSend(pStream, iLen); }
		return false;
	}
	if ( !bBudgetReserved && !__xnetStreamTryReserveSend(pStream, iLen) ) { return false; }
	if ( xrtNetTlsSessionWritePlain(pStream->pTls, pData, iLen, &iWritten) != XRT_NET_OK || iWritten != iLen ) {
		__xnetStreamReleaseSend(pStream, iLen);
		return false;
	}
	if ( !__xnetStreamQueueTlsCipherEx(pStream, iLen, true) ) { return false; }
	__xnetStreamKickWrite(pStream);
	return true;
	#endif
}


// 内部函数：__xnetStreamAppendTlsPlainCopy
static bool __xnetStreamAppendTlsPlainCopy(xnetstream* pStream, const void* pData, size_t iLen)
{
	return __xnetStreamAppendTlsPlainCopyEx(pStream, pData, iLen, false);
}


static bool __xnetStreamAppendTlsControlPlainCopy(xnetstream* pStream, const void* pData, size_t iLen)
{
	if ( !__xnetStreamTryReserveControlSend(pStream, iLen) ) { return false; }
	return __xnetStreamAppendTlsPlainCopyEx(pStream, pData, iLen, true);
}


// 内部函数：__xnetStreamAppendTlsPlainVec
static bool __xnetStreamAppendTlsPlainVecEx(xnetstream* pStream, const xnetspan* pVec, uint32 iCount,
	bool bBudgetReserved)
{
	#ifdef XRT_NO_NETTLS
	uint64 iDisabledTotal = 0u;
	(void)pStream;
	if ( pVec ) {
		for ( uint32 i = 0; i < iCount; ++i ) { iDisabledTotal += pVec[i].iLen; }
	}
	if ( bBudgetReserved ) { __xnetStreamReleaseSend(pStream, iDisabledTotal); }
	return false;
	#else
	uint64 iTotal = 0u;
	size_t iWritten;
	if ( !pStream || !pStream->pTls || !__xnetStreamTlsReady(pStream) || !pVec || iCount == 0 ) { return false; }
	for ( uint32 i = 0; i < iCount; ++i ) {
		if ( !pVec[i].pData || pVec[i].iLen == 0 || iTotal > UINT64_MAX - pVec[i].iLen ) {
			if ( bBudgetReserved ) { __xnetStreamReleaseSend(pStream, iTotal); }
			return false;
		}
		iTotal += pVec[i].iLen;
	}
	if ( !bBudgetReserved && !__xnetStreamTryReserveSend(pStream, iTotal) ) { return false; }
	for ( uint32 i = 0; i < iCount; ++i ) {
		iWritten = 0u;
		if ( xrtNetTlsSessionWritePlain(pStream->pTls, pVec[i].pData, pVec[i].iLen, &iWritten) != XRT_NET_OK ||
			iWritten != pVec[i].iLen ) {
			__xnetStreamReleaseSend(pStream, iTotal);
			return false;
		}
	}
	if ( !__xnetStreamQueueTlsCipherEx(pStream, iTotal, true) ) { return false; }
	__xnetStreamKickWrite(pStream);
	return true;
	#endif
}


static bool UNUSED_ATTR __xnetStreamAppendTlsPlainVec(xnetstream* pStream, const xnetspan* pVec, uint32 iCount)
{
	return __xnetStreamAppendTlsPlainVecEx(pStream, pVec, iCount, false);
}


// 内部函数：__xnetStreamAppendTlsPlainRef
static bool __xnetStreamAppendTlsPlainRefEx(xnetstream* pStream, const xnetbufref* pRef,
	bool bBudgetReserved)
{
	if ( !pStream || !pRef || !pRef->pData || pRef->iLen == 0 ) {
		if ( bBudgetReserved && pRef ) { __xnetStreamReleaseSend(pStream, pRef->iLen); }
		return false;
	}
	return __xnetStreamAppendTlsPlainCopyEx(pStream, pRef->pData, pRef->iLen, bBudgetReserved);
}


static bool UNUSED_ATTR __xnetStreamAppendTlsPlainRef(xnetstream* pStream, const xnetbufref* pRef)
{
	return __xnetStreamAppendTlsPlainRefEx(pStream, pRef, false);
}


// 内部函数：__xnetStreamEmitOpen
static void __xnetStreamEmitOpen(xnetstream* pStream)
{
	if ( !pStream || (pStream->iState & __XNET_STREAM_STATE_OPEN_EMITTED) != 0 ) { return; }
	__xnetStreamCancelOpenTimer(pStream);
	pStream->iState |= __XNET_STREAM_STATE_OPEN_EMITTED;
	__xnetStreamNotifySyncWritable(pStream, XRT_NET_OK);
	if ( pStream->pEvents && pStream->pEvents->OnOpen ) {
		pStream->pEvents->OnOpen(__xnetStreamOwner(pStream), pStream);
	}
}


// 内部函数：打开流 submit 事件
static bool __xnetStreamSubmitOpenEvent(xnetstream* pStream, uint16 iOpType)
{
	xnetportsubmit tSubmit;
	if ( !pStream || !pStream->pWorker ) { return false; }
	memset(&tSubmit, 0, sizeof(tSubmit));
	tSubmit.iOpType = iOpType;
	if ( iOpType == XNET_PORT_OP_ACCEPT ) {
		tSubmit.iFlags = XNET_PORT_EVENT_F_ACCEPTED_OPEN;
	}
	tSubmit.iOpId = iOpType == XNET_PORT_OP_CONNECT ? pStream->iConnectOpId : pStream->iId;
	tSubmit.pUserData = pStream;
	tSubmit.hSocket = (intptr_t)pStream->hSocket;
	tSubmit.tAddr = pStream->tRemoteAddr;
	__xnetStreamAddAsyncHold(pStream);
	if ( xrtNetPortSubmit(&pStream->pWorker->tPort, &tSubmit, 1) != XRT_NET_OK ) {
		__xnetStreamReleaseAsyncHold(pStream);
		return false;
	}
	return true;
}


// 内部函数：提交流套接字 notice
static bool __xnetStreamSubmitSocketNotice(xnetstream* pStream, uint16 iOpType, xsocket hSocket)
{
	xnetportsubmit tSubmit;
	bool bTrackHold;
	if ( !pStream || !pStream->pWorker || !__xnetSocketIsValid(hSocket) ) { return false; }
	memset(&tSubmit, 0, sizeof(tSubmit));
	tSubmit.iOpType = iOpType;
	tSubmit.hSocket = (intptr_t)hSocket;
	tSubmit.pUserData = pStream;
	tSubmit.iOpId = pStream->iId;
	bTrackHold = (iOpType != XNET_PORT_OP_CLOSE);
	if ( bTrackHold ) {
		__xnetStreamAddAsyncHold(pStream);
	}
	if ( xrtNetPortSubmit(&pStream->pWorker->tPort, &tSubmit, 1) != XRT_NET_OK ) {
		if ( bTrackHold ) {
			__xnetStreamReleaseAsyncHold(pStream);
		}
		return false;
	}
	return true;
}


// 内部函数：__xnetStreamArmRecvWatch
static bool __xnetStreamArmRecvWatch(xnetstream* pStream)
{
	if ( !pStream || pStream->bClosing || !__xnetSocketIsValid(pStream->hSocket) ) { return false; }
	if ( __xnetAtomicCompareExchange32(&pStream->bRecvArmed, 1, 0) != 0 ) { return false; }
	if ( !__xnetStreamSubmitSocketNotice(pStream, XNET_PORT_OP_RECV, pStream->hSocket) ) {
		(void)__xnetAtomicExchange32(&pStream->bRecvArmed, 0);
		#if defined(XNET_DEBUG_IOCP_NATIVE)
			fprintf(stderr, "[STREAM] arm recv fail stream=%llu socket=%p native=%d\n",
				(unsigned long long)pStream->iId,
				(void*)(uintptr_t)pStream->hSocket,
				__xnetStreamUseNativePortIO(pStream) ? 1 : 0);
		#endif
		return false;
	}
	if ( pStream->pWorker && !__xnetEngineIsCurrentWorker(pStream->pWorker) ) {
		(void)xrtNetPortWake(&pStream->pWorker->tPort);
	}
	return true;
}


// 内部函数：__xnetStreamArmSendWatch
static bool UNUSED_ATTR __xnetStreamArmSendWatch(xnetstream* pStream)
{
	if ( !pStream || pStream->bSendArmed || pStream->tSendQ.iQueuedBytes == 0 || !__xnetSocketIsValid(pStream->hSocket) ) { return false; }
	if ( !__xnetStreamSubmitSocketNotice(pStream, XNET_PORT_OP_SEND, pStream->hSocket) ) { return false; }
	pStream->bSendArmed = true;
	if ( pStream->pWorker && !__xnetEngineIsCurrentWorker(pStream->pWorker) ) {
		(void)xrtNetPortWake(&pStream->pWorker->tPort);
	}
	return true;
}


// 内部函数：关闭流 finalize 套接字
static void __xnetStreamFinalizeSocketClose(xnetstream* pStream)
{
	xsocket hSocket;
	if ( !pStream ) { return; }
	hSocket = pStream->hSocket;
	__xnetStreamClearRecvArmed(pStream);
	pStream->bSendArmed = false;
	pStream->bWriteShutdown = false;
	if ( __xnetSocketIsValid(hSocket) ) {
		(void)__xnetSocketShutdownBoth(hSocket);
		(void)__xnetStreamSubmitSocketNotice(pStream, XNET_PORT_OP_CLOSE, hSocket);
		__xnetSocketCloseHandle(&pStream->hSocket);
	}
}


// 内部函数：__xnetStreamFinishClose
static void __xnetStreamFinishClose(xnetstream* pStream, xnet_result iReason)
{
	(void)__xnetStreamRequestClose(pStream, pStream ? pStream->iFlags : 0u);
	if ( !__xnetStreamTryMarkCloseEmitted(pStream) ) { return; }
	pStream->iCloseReason = iReason;
	if ( pStream->pConnectAddrList ) {
		xrtNetAddrListDestroy(pStream->pConnectAddrList);
		pStream->pConnectAddrList = NULL;
		pStream->iConnectAddrIndex = 0u;
	}
	pStream->iConnectOpId = 0u;
	__xnetStreamCancelOpenTimer(pStream);
	__xnetStreamClearProxyState(pStream);
	__xnetStreamFinalizeSocketClose(pStream);
	__xnetStreamNotifySyncReadable(pStream, iReason);
	__xnetStreamNotifySyncWritable(pStream, iReason);
	__xnetStreamNotifySyncDrain(pStream, iReason);
	__xnetStreamNotifySyncClose(pStream, iReason);
	__xnetStreamDetachTls(pStream);
	if ( pStream->pEvents && pStream->pEvents->OnClose ) {
		pStream->pEvents->OnClose(__xnetStreamOwner(pStream), pStream, iReason);
	}
}


// 内部函数：打开流 handle 定时器
static void __xnetStreamHandleOpenTimer(xnetstream* pStream)
{
	if ( !pStream ) { return; }
	if ( __xnetAtomicCompareExchange32(&pStream->iOpenTimerState, 0, 1) != 1 ) { return; }
	if ( (pStream->iState & (__XNET_STREAM_STATE_OPEN_EMITTED | __XNET_STREAM_STATE_CLOSE_EMITTED)) != 0 ) { return; }
	pStream->bClosing = true;
	pStream->iFlags = XNET_CLOSE_F_ABORT;
	__xnetStreamClearRx(pStream);
	__xnetStreamClearSendQueue(pStream);
	__xnetStreamFinishClose(pStream, XRT_NET_TIMEOUT);
}


// 内部函数：__xnetStreamBeginGracefulCloseWait
static void __xnetStreamBeginGracefulCloseWait(xnetstream* pStream)
{
	if ( !pStream ) { return; }
	if ( !__xnetSocketIsValid(pStream->hSocket) ) {
		__xnetStreamFinishClose(pStream, XRT_NET_CLOSED);
		return;
	}
	if ( !pStream->bWriteShutdown ) {
		if ( !__xnetSocketShutdownWrite(pStream->hSocket) ) {
			__xnetStreamFinishClose(pStream, XRT_NET_CLOSED);
			return;
		}
		pStream->bWriteShutdown = true;
	}
	if ( !__xnetStreamRecvArmed(pStream) ) {
		(void)__xnetStreamArmRecvWatch(pStream);
	}
}


// 内部函数：__xnetStreamCompleteWrite
static size_t __xnetStreamCompleteWrite(xnetstream* pStream, size_t iBytes)
{
	bool bNeedResubmit;
	if ( !pStream ) { return 0; }
	__xnetStreamAddAsyncHold(pStream);
	pStream->tSendQ.bWritePosted = false;
	iBytes = __xnetStreamConsumeSendQueue(pStream, iBytes);
	if ( pStream->bClosing &&
		(pStream->iFlags & XNET_CLOSE_F_ABORT) == 0 &&
		pStream->tSendQ.iQueuedBytes == 0 ) {
		if ( (pStream->iFlags & XNET_CLOSE_F_WAIT_PEER) != 0 ) {
			__xnetStreamBeginGracefulCloseWait(pStream);
		} else {
			__xnetStreamFinishClose(pStream, XRT_NET_CLOSED);
		}
		__xnetStreamReleaseAsyncHold(pStream);
		return iBytes;
	}
	bNeedResubmit = pStream->tSendQ.iQueuedBytes > 0 && !pStream->tSendQ.bWritePosted &&
		__xnetSocketIsValid(pStream->hSocket);
	if ( bNeedResubmit ) {
		__xnetStreamKickWrite(pStream);
	}
	__xnetStreamReleaseAsyncHold(pStream);
	return iBytes;
}


// 内部函数：__xnetStreamSubmitRecvChain
static bool __xnetStreamSubmitRecvChain(xnetstream* pStream, xnetchain* pChain)
{
	xnetportsubmit tSubmit;
	if ( !pStream || !pStream->pWorker || !pChain ) { return false; }
	memset(&tSubmit, 0, sizeof(tSubmit));
	tSubmit.iOpType = XNET_PORT_OP_RECV;
	tSubmit.iFlags = XNET_PORT_EVENT_F_DEFERRED_RECV;
	tSubmit.hSocket = (intptr_t)XNET_SOCKET_INVALID;
	tSubmit.pUserData = pStream;
	tSubmit.pChain = pChain;
	tSubmit.tAddr = pStream->tRemoteAddr;
	__xnetStreamAddAsyncHold(pStream);
	if ( xrtNetPortSubmit(&pStream->pWorker->tPort, &tSubmit, 1) != XRT_NET_OK ) {
		__xnetStreamReleaseAsyncHold(pStream);
		return false;
	}
	return true;
}


// Deferred native recv payload dispatch runs through the engine command queue so
// worker stop drains it before port resources are destroyed.
static void __xnetStreamDeferredRecvChainTask(xnetworker* pWorker, ptr pArg)
{
	__xnet_stream_recv_chain_task* pTask = (__xnet_stream_recv_chain_task*)pArg;
	xnetstream* pStream = pTask ? pTask->pStream : NULL;
	xnetchain* pChain = pTask ? pTask->pChain : NULL;
	(void)pWorker;
	if ( !pTask ) { return; }
	if ( pChain ) {
		__xnetStreamHandleRecvEvent(pStream, pChain);
	}
	XNET_FREE(pTask);
	__xnetStreamReleaseAsyncHold(pStream);
}

static bool __xnetStreamPostDeferredRecvChain(xnetstream* pStream, xnetchain* pChain)
{
	__xnet_stream_recv_chain_task* pTask;
	if ( !pStream || !pStream->pEngine || !pStream->pWorker || !pChain ) { return false; }
	pTask = (__xnet_stream_recv_chain_task*)XNET_ALLOC(sizeof(__xnet_stream_recv_chain_task));
	if ( !pTask ) { return false; }
	pTask->pStream = pStream;
	pTask->pChain = pChain;
	__xnetStreamAddAsyncHold(pStream);
	if ( xrtNetEnginePost(pStream->pEngine, pStream->pWorker->iId, __xnetStreamDeferredRecvChainTask, pTask) != XRT_NET_OK ) {
		__xnetStreamReleaseAsyncHold(pStream);
		XNET_FREE(pTask);
		return false;
	}
	return true;
}

// 内部函数：__xnetStreamFeedTlsChain
static bool __xnetStreamFeedTlsChain(xnetstream* pStream, xnetchain* pChain)
{
	#ifdef XRT_NO_NETTLS
		(void)pStream;
		(void)pChain;
		return false;
	#else
	xnetspan arrSpan[16];
	uint32 iSpanCount;

	if ( !pStream || !pStream->pTls || !pChain ) { return false; }
	iSpanCount = xrtNetChainGetSpans(pChain, arrSpan, 16);
	for ( uint32 i = 0; i < iSpanCount; ++i ) {
		if ( arrSpan[i].iLen == 0 ) { continue; }
		if ( xrtNetTlsSessionFeedCipher(pStream->pTls, arrSpan[i].pData, arrSpan[i].iLen) != XRT_NET_OK ) {
			return false;
		}
	}
	return true;
	#endif
}


// 内部函数：接收流 handle 事件
static void __xnetStreamHandleRecvEvent(xnetstream* pStream, xnetchain* pChain)
{
	if ( !pStream || !pChain ) {
		__xnetStreamFreeTempChain(pChain);
		return;
	}
	// 代理握手阶段：将接收到的数据喂给代理状态机
	if ( pStream->pProxyState ) {
		xnetspan arrSpan[16];
		uint32 iSpanCount = xrtNetChainGetSpans(pChain, arrSpan, 16);
		for ( uint32 i = 0; i < iSpanCount; ++i ) {
			xnetchain* pRemainder = NULL;
			if ( arrSpan[i].iLen == 0 ) { continue; }
			// 代理状态在处理过程中可能被清除，需要检查
			if ( !pStream->pProxyState ) {
				// 代理已完成，收集剩余数据用于后续处理
				pRemainder = __xnetStreamAllocTempChain(pStream);
				if ( !pRemainder || !xrtNetChainAppendCopy(pRemainder, arrSpan[i].pData, arrSpan[i].iLen) ) {
					__xnetStreamFreeTempChain(pRemainder);
					if ( pStream->pEvents && pStream->pEvents->OnError ) {
						pStream->pEvents->OnError(__xnetStreamOwner(pStream), pStream, -1);
					}
					__xnetStreamFreeTempChain(pChain);
					xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
					return;
				}
				// 将后续 span 也追加到残余数据中
				for ( uint32 j = i + 1u; j < iSpanCount; ++j ) {
					if ( arrSpan[j].iLen == 0 ) { continue; }
					if ( !xrtNetChainAppendCopy(pRemainder, arrSpan[j].pData, arrSpan[j].iLen) ) {
						__xnetStreamFreeTempChain(pRemainder);
						if ( pStream->pEvents && pStream->pEvents->OnError ) {
							pStream->pEvents->OnError(__xnetStreamOwner(pStream), pStream, -1);
						}
						__xnetStreamFreeTempChain(pChain);
						xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
						return;
					}
				}
				__xnetStreamFreeTempChain(pChain);
				// 递归处理残余数据（此时已进入正常流模式）
				__xnetStreamHandleRecvEvent(pStream, pRemainder);
				return;
			}
			// 驱动代理状态机处理当前 span
			if ( !__xnetStreamDriveProxyState(pStream, arrSpan[i].pData, arrSpan[i].iLen) ) {
				__xnetStreamFreeTempChain(pChain);
				return;
			}
		}
		__xnetStreamFreeTempChain(pChain);
		// 代理尚未完成，继续挂起接收监视
		if ( pStream->pProxyState && !pStream->bClosing && __xnetSocketIsValid(pStream->hSocket) && !__xnetStreamRecvArmed(pStream) ) {
			(void)__xnetStreamArmRecvWatch(pStream);
		}
		return;
	}
	// 接收数据超过接收限制，触发错误并关闭流
	if ( pStream->iRecvLimit > 0 && __xnetStreamRxBytes(pStream) + xrtNetChainBytes(pChain) > pStream->iRecvLimit ) {
		if ( pStream->pEvents && pStream->pEvents->OnError ) {
			pStream->pEvents->OnError(__xnetStreamOwner(pStream), pStream, -1);
		}
		__xnetStreamFreeTempChain(pChain);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return;
	}
	// TLS 模式：将密文数据喂给 TLS 会话
	if ( pStream->pTls ) {
		bool bHandshakeReady = __xnetStreamTlsReady(pStream);
		if ( !__xnetStreamFeedTlsChain(pStream, pChain) ) {
			__xnetStreamFreeTempChain(pChain);
			if ( pStream->pEvents && pStream->pEvents->OnError ) {
				pStream->pEvents->OnError(__xnetStreamOwner(pStream), pStream, -1);
			}
			xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
			return;
		}
		__xnetStreamFreeTempChain(pChain);
		if ( !bHandshakeReady ) {
			// TLS 握手尚未完成，继续驱动握手
			(void)__xnetStreamDriveTlsHandshake(pStream);
		} else {
			// TLS 已就绪，排空明文数据并继续接收
			(void)__xnetStreamDrainTlsPlain(pStream);
			if ( !pStream->bClosing && !__xnetStreamReadPaused(pStream) &&
				__xnetSocketIsValid(pStream->hSocket) && !__xnetStreamRecvArmed(pStream) ) {
				(void)__xnetStreamArmRecvWatch(pStream);
			}
		}
		return;
	}
	// 普通模式：将数据拼接到接收链
	__xnetStreamRxLock(pStream);
	__xnetChainSplice(&pStream->tRxChain, pChain);
	__xnetStreamRxUnlock(pStream);
	__xnetStreamFreeTempChain(pChain);
	__xnetStreamNotifySyncReadable(pStream, XRT_NET_OK);
	if ( __xnetStreamReadPaused(pStream) ) {
		return;
	}
	// 分发接收事件给上层
	__xnetStreamDispatchRecv(pStream);
	// 原生 IO 模式下需要重新挂起接收监视
	if ( !pStream->bClosing && __xnetStreamUseNativePortIO(pStream) && !__xnetStreamRecvArmed(pStream) ) {
		(void)__xnetStreamArmRecvWatch(pStream);
	}
}


// 内部函数：发送流 handle 事件
static void __xnetStreamHandleSendEvent(xnetstream* pStream, const xnetportevent* pEvent)
{
	if ( !pStream || !pEvent ) { return; }
	pStream->bSendArmed = false;
	#if defined(XNET_DEBUG_IOCP_NATIVE)
		if ( __xnetStreamUseNativePortIO(pStream) ) {
			fprintf(stderr, "[STREAM] send event stream=%llu bytes=%u queued=%u status=%d\n",
				(unsigned long long)pStream->iId, pEvent->iBytes, pStream->tSendQ.iQueuedBytes, (int)pEvent->iStatus);
		}
	#endif
	// 代理握手阶段：完成发送后继续驱动代理或挂起接收
	if ( pStream->pProxyState ) {
		if ( pEvent->iBytes > 0 ) {
			(void)__xnetStreamCompleteWrite(pStream, pEvent->iBytes);
		}
		if ( !pStream->bClosing && pStream->tSendQ.iQueuedBytes > 0 ) {
			__xnetStreamKickWrite(pStream);
		} else if ( !pStream->bClosing && __xnetSocketIsValid(pStream->hSocket) && !__xnetStreamRecvArmed(pStream) ) {
			(void)__xnetStreamArmRecvWatch(pStream);
		}
		return;
	}
	// TLS 握手尚未完成：完成发送后继续驱动 TLS 握手
	if ( pStream->pTls && !__xnetStreamTlsReady(pStream) ) {
		if ( pEvent->iBytes > 0 ) {
			(void)__xnetStreamCompleteWrite(pStream, pEvent->iBytes);
			if ( !pStream->bClosing && pStream->tSendQ.iQueuedBytes == 0 ) {
				(void)__xnetStreamDriveTlsHandshake(pStream);
			}
		} else {
			if ( !pStream->bClosing && pStream->tSendQ.iQueuedBytes > 0 ) {
				__xnetStreamKickWrite(pStream);
			} else if ( !pStream->bClosing ) {
				(void)__xnetStreamDriveTlsHandshake(pStream);
			}
		}
		return;
	}
	// 零字节发送事件但套接字仍然有效，重新提交写入
	if ( pEvent->iBytes == 0 && __xnetSocketIsValid(pStream->hSocket) ) {
		__xnetStreamKickWrite(pStream);
		return;
	}
	// 正常模式：完成写入并消费发送队列
	(void)__xnetStreamCompleteWrite(pStream, pEvent->iBytes);
	if ( pStream->pTls && __xnetStreamTlsReady(pStream) && !pStream->bClosing ) {
		(void)__xnetStreamDrainTlsCipherNow(pStream);
		(void)__xnetStreamDrainTlsPlain(pStream);
		if ( !__xnetStreamReadPaused(pStream) && __xnetSocketIsValid(pStream->hSocket) && !__xnetStreamRecvArmed(pStream) ) {
			(void)__xnetStreamArmRecvWatch(pStream);
		}
	}
}


static void __xnetStreamApplyPauseRead(xnetstream* pStream)
{
	if ( pStream ) { (void)__xnetAtomicExchange32(&pStream->bReadPaused, 1); }
}


static void __xnetStreamApplyResumeRead(xnetstream* pStream)
{
	if ( !pStream ) { return; }
	(void)__xnetAtomicExchange32(&pStream->bReadPaused, 0);
	if ( __xnetStreamRxBytes(pStream) > 0 ) {
		__xnetStreamDispatchRecv(pStream);
		if ( __xnetStreamRxBytes(pStream) > 0 ) { return; }
	}
	if ( !pStream->bClosing && __xnetSocketIsValid(pStream->hSocket) && !__xnetStreamRecvArmed(pStream) ) {
		if ( pStream->pProxyState ) {
			(void)__xnetStreamArmRecvWatch(pStream);
		} else if ( pStream->pTls && !__xnetStreamTlsReady(pStream) ) {
			(void)__xnetStreamDriveTlsHandshake(pStream);
		} else {
			(void)__xnetStreamArmRecvWatch(pStream);
		}
	}
}


static void __xnetStreamReleaseAcceptedRef(xnetbufref* pRef)
{
	void (*pfnRelease)(ptr, const void*, size_t);
	ptr pReleaseCtx;
	const void* pData;
	size_t iLen;
	if ( !pRef || !pRef->pData ) { return; }
	pfnRelease = pRef->pfnRelease;
	pReleaseCtx = pRef->pReleaseCtx;
	pData = pRef->pData;
	iLen = pRef->iLen;
	memset(pRef, 0, sizeof(*pRef));
	if ( pfnRelease ) { pfnRelease(pReleaseCtx, pData, iLen); }
}


// 内部函数：流异步任务相关处理
static void __xnetStreamAsyncTask(xnetworker* pWorker, ptr pArg)
{
	__xnet_stream_async_op* pOp = (__xnet_stream_async_op*)pArg;
	xnetstream* pStream = pOp ? pOp->pStream : NULL;
	(void)pWorker;
	if ( !pOp || !pStream ) {
		if ( pOp && pOp->hDone ) { (void)xrtSemPost(pOp->hDone); }
		if ( pOp && (pOp->iType == __XNET_STREAM_ASYNC_SEND_REF ||
			pOp->iType == __XNET_STREAM_ASYNC_RECV_REF) ) {
			__xnetStreamReleaseAcceptedRef(&pOp->tRef);
		}
		if ( pOp ) { XNET_FREE(pOp); }
		return;
	}
	// 流正在关闭且不是分发接收操作，直接丢弃
	if ( pStream->bClosing &&
		 pOp->iType != __XNET_STREAM_ASYNC_DISPATCH_RECV &&
		 pOp->iType != __XNET_STREAM_ASYNC_CLOSE ) {
		if ( pOp->bSendBudgetReserved ) {
			__xnetStreamReleaseSend(pStream, pOp->iLen);
		}
		if ( pOp->iType == __XNET_STREAM_ASYNC_SEND_REF ||
			pOp->iType == __XNET_STREAM_ASYNC_RECV_REF ) {
			__xnetStreamReleaseAcceptedRef(&pOp->tRef);
		}
		if ( pOp->hDone ) { (void)xrtSemPost(pOp->hDone); }
		XNET_FREE(pOp);
		__xnetStreamReleaseAsyncHold(pStream);
		return;
	}

	// 根据异步操作类型分发处理
	switch ( pOp->iType ) {
		case __XNET_STREAM_ASYNC_SEND_COPY:
			// TLS 模式下通过 TLS 加密后发送，否则直接加入发送队列
			if ( pStream->pTls ) {
				(void)__xnetStreamAppendTlsPlainCopyEx(pStream, pOp->aData, pOp->iLen, true);
			} else if ( __xnetStreamAppendSendCopyEx(pStream, pOp->aData, pOp->iLen, true) ) {
				__xnetStreamKickWrite(pStream);
			}
			break;
		case __XNET_STREAM_ASYNC_SEND_REF:
			// TLS 模式下通过 TLS 加密后发送引用数据
			if ( pStream->pTls ) {
				(void)__xnetStreamAppendTlsPlainRefEx(pStream, &pOp->tRef, true);
				__xnetStreamReleaseAcceptedRef(&pOp->tRef);
			} else if ( __xnetStreamAppendSendRefEx(pStream, &pOp->tRef, true) ) {
				memset(&pOp->tRef, 0, sizeof(pOp->tRef));
				__xnetStreamKickWrite(pStream);
			}
			break;
		case __XNET_STREAM_ASYNC_RECV_COPY:
			{
				// 将复制的接收数据构建为临时 chain 并提交到接收流程
				xnetchain* pChain = __xnetStreamAllocTempChain(pStream);
				if ( pChain && xrtNetChainAppendCopy(pChain, pOp->aData, pOp->iLen) && __xnetStreamSubmitRecvChain(pStream, pChain) ) {
					pChain = NULL;
				}
				__xnetStreamFreeTempChain(pChain);
			}
			break;
		case __XNET_STREAM_ASYNC_RECV_REF:
			{
				// 将引用的接收数据构建为临时 chain 并提交到接收流程
				xnetchain* pChain = __xnetStreamAllocTempChain(pStream);
				if ( pChain && xrtNetChainAppendRef(pChain, &pOp->tRef) ) {
					memset(&pOp->tRef, 0, sizeof(pOp->tRef));
					if ( __xnetStreamSubmitRecvChain(pStream, pChain) ) { pChain = NULL; }
				}
				__xnetStreamFreeTempChain(pChain);
			}
			break;
		case __XNET_STREAM_ASYNC_DISPATCH_RECV:
			// 分发已缓存的接收数据给上层
			__xnetStreamDispatchRecv(pStream);
			// 分发后如果接收链已空，根据状态继续挂起接收或驱动 TLS
			if ( pStream && !__xnetStreamReadPaused(pStream) && !pStream->bClosing &&
				__xnetStreamRxBytes(pStream) == 0 &&
				__xnetSocketIsValid(pStream->hSocket) && !__xnetStreamRecvArmed(pStream) ) {
				if ( pStream->pProxyState ) {
					(void)__xnetStreamArmRecvWatch(pStream);
				} else if ( pStream->pTls && !__xnetStreamTlsReady(pStream) ) {
					(void)__xnetStreamDriveTlsHandshake(pStream);
				} else {
					(void)__xnetStreamArmRecvWatch(pStream);
				}
			}
			break;
		case __XNET_STREAM_ASYNC_TLS_HANDSHAKE:
			// 驱动 TLS 握手继续进行
			(void)__xnetStreamDriveTlsHandshake(pStream);
			break;
		case __XNET_STREAM_ASYNC_CLOSE:
			xrtNetStreamClose(pStream, pOp->iLen);
			break;
		case __XNET_STREAM_ASYNC_PAUSE_READ:
			__xnetStreamApplyPauseRead(pStream);
			break;
		case __XNET_STREAM_ASYNC_RESUME_READ:
			__xnetStreamApplyResumeRead(pStream);
			break;
		default:
			break;
	}
	// 释放异步操作结构并释放异步持有计数
	if ( pOp->iType == __XNET_STREAM_ASYNC_SEND_REF ||
		pOp->iType == __XNET_STREAM_ASYNC_RECV_REF ) {
		__xnetStreamReleaseAcceptedRef(&pOp->tRef);
	}
	if ( pOp->hDone ) { (void)xrtSemPost(pOp->hDone); }
	XNET_FREE(pOp);
	__xnetStreamReleaseAsyncHold(pStream);
}


// 内部函数：异步复制流 alloc
static __xnet_stream_async_op* __xnetStreamAllocAsyncCopy(xnetstream* pStream, uint32 iType, const void* pData, size_t iLen)
{
	__xnet_stream_async_op* pOp;
	size_t iSize;
	if ( !pStream || !pData || iLen == 0 || iLen > UINT32_MAX ) { return NULL; }
	iSize = sizeof(__xnet_stream_async_op) + iLen;
	pOp = (__xnet_stream_async_op*)XNET_ALLOC(iSize);
	if ( !pOp ) { return NULL; }
	memset(pOp, 0, iSize);
	pOp->pStream = pStream;
	pOp->iType = iType;
	pOp->iLen = (uint32)iLen;
	memcpy(pOp->aData, pData, iLen);
	return pOp;
}


// 内部函数：异步分配流 ref
static __xnet_stream_async_op* __xnetStreamAllocAsyncRef(xnetstream* pStream, uint32 iType, const xnetbufref* pRef)
{
	__xnet_stream_async_op* pOp;
	if ( !pStream || !pRef || !pRef->pData || pRef->iLen == 0 ) { return NULL; }
	pOp = (__xnet_stream_async_op*)XNET_ALLOC(sizeof(__xnet_stream_async_op));
	if ( !pOp ) { return NULL; }
	memset(pOp, 0, sizeof(__xnet_stream_async_op));
	pOp->pStream = pStream;
	pOp->iType = iType;
	pOp->iLen = pRef->iLen;
	pOp->tRef = *pRef;
	return pOp;
}


// 内部函数：异步复制流 alloc vec
static __xnet_stream_async_op* __xnetStreamAllocAsyncVecCopy(xnetstream* pStream, uint32 iType, const xnetspan* pVec, uint32 iCount)
{
	__xnet_stream_async_op* pOp;
	size_t iTotal = 0;
	size_t iOffset = 0;
	size_t iSize;
	if ( !pStream || !pVec || iCount == 0 ) { return NULL; }
	for ( uint32 i = 0; i < iCount; ++i ) {
		if ( !pVec[i].pData || pVec[i].iLen == 0 ) { return NULL; }
		iTotal += pVec[i].iLen;
		if ( iTotal > UINT32_MAX ) { return NULL; }
	}
	iSize = sizeof(__xnet_stream_async_op) + iTotal;
	pOp = (__xnet_stream_async_op*)XNET_ALLOC(iSize);
	if ( !pOp ) { return NULL; }
	memset(pOp, 0, iSize);
	pOp->pStream = pStream;
	pOp->iType = iType;
	pOp->iLen = (uint32)iTotal;
	for ( uint32 i = 0; i < iCount; ++i ) {
		memcpy(pOp->aData + iOffset, pVec[i].pData, pVec[i].iLen);
		iOffset += pVec[i].iLen;
	}
	return pOp;
}


// 内部函数：异步分配流 simple
static __xnet_stream_async_op* __xnetStreamAllocAsyncSimple(xnetstream* pStream, uint32 iType)
{
	__xnet_stream_async_op* pOp;
	if ( !pStream ) { return NULL; }
	pOp = (__xnet_stream_async_op*)XNET_ALLOC(sizeof(__xnet_stream_async_op));
	if ( !pOp ) { return NULL; }
	memset(pOp, 0, sizeof(__xnet_stream_async_op));
	pOp->pStream = pStream;
	pOp->iType = iType;
	return pOp;
}


// 内部函数：__xnetStreamPostAsync
static xnet_result __xnetStreamPostAsync(xnetstream* pStream, __xnet_stream_async_op* pOp)
{
	if ( !pStream || !pOp || !pStream->pEngine || !pStream->pWorker ) {
		if ( pOp && pOp->bSendBudgetReserved ) { __xnetStreamReleaseSend(pStream, pOp->iLen); }
		if ( pOp ) { XNET_FREE(pOp); }
		return XRT_NET_ERROR;
	}
	__xnetStreamAddAsyncHold(pStream);
	if ( xrtNetEnginePost(pStream->pEngine, pStream->pWorker->iId, __xnetStreamAsyncTask, pOp) != XRT_NET_OK ) {
		__xnetStreamReleaseAsyncHold(pStream);
		if ( pOp->bSendBudgetReserved ) { __xnetStreamReleaseSend(pStream, pOp->iLen); }
		XNET_FREE(pOp);
		return XRT_NET_ERROR;
	}
	return XRT_NET_OK;
}


// 内部函数：__xnetStreamPostSendCopy
static xnet_result __xnetStreamPostSendCopyEx(xnetstream* pStream, const void* pData, size_t iLen,
	bool bBudgetReserved)
{
	if ( !bBudgetReserved && !__xnetStreamTryReserveSend(pStream, iLen) ) { return XRT_NET_AGAIN; }
	__xnet_stream_async_op* pOp = __xnetStreamAllocAsyncCopy(pStream, __XNET_STREAM_ASYNC_SEND_COPY, pData, iLen);
	if ( !pOp ) {
		__xnetStreamReleaseSend(pStream, iLen);
		return XRT_NET_ERROR;
	}
	pOp->bSendBudgetReserved = 1u;
	return __xnetStreamPostAsync(pStream, pOp);
}


static xnet_result UNUSED_ATTR __xnetStreamPostSendCopy(xnetstream* pStream, const void* pData, size_t iLen)
{
	return __xnetStreamPostSendCopyEx(pStream, pData, iLen, false);
}


// 内部函数：__xnetStreamPostSendVec
static xnet_result __xnetStreamPostSendVecEx(xnetstream* pStream, const xnetspan* pVec, uint32 iCount,
	bool bBudgetReserved)
{
	uint64 iTotal = 0u;
	if ( !pVec || iCount == 0u ) { return XRT_NET_ERROR; }
	for ( uint32 i = 0; i < iCount; ++i ) { iTotal += pVec[i].iLen; }
	if ( !bBudgetReserved && !__xnetStreamTryReserveSend(pStream, iTotal) ) { return XRT_NET_AGAIN; }
	__xnet_stream_async_op* pOp = __xnetStreamAllocAsyncVecCopy(pStream, __XNET_STREAM_ASYNC_SEND_COPY, pVec, iCount);
	if ( !pOp ) {
		__xnetStreamReleaseSend(pStream, iTotal);
		return XRT_NET_ERROR;
	}
	pOp->bSendBudgetReserved = 1u;
	return __xnetStreamPostAsync(pStream, pOp);
}


static xnet_result UNUSED_ATTR __xnetStreamPostSendVec(xnetstream* pStream, const xnetspan* pVec, uint32 iCount)
{
	return __xnetStreamPostSendVecEx(pStream, pVec, iCount, false);
}


// 内部函数：__xnetStreamPostSendRef
static xnet_result __xnetStreamPostSendRefEx(xnetstream* pStream, const xnetbufref* pRef,
	bool bBudgetReserved)
{
	if ( !pRef || !pRef->pData || pRef->iLen == 0u ) { return XRT_NET_ERROR; }
	if ( !bBudgetReserved && !__xnetStreamTryReserveSend(pStream, pRef->iLen) ) { return XRT_NET_AGAIN; }
	__xnet_stream_async_op* pOp = __xnetStreamAllocAsyncRef(pStream, __XNET_STREAM_ASYNC_SEND_REF, pRef);
	if ( !pOp ) {
		if ( pRef ) { __xnetStreamReleaseSend(pStream, pRef->iLen); }
		return XRT_NET_ERROR;
	}
	pOp->bSendBudgetReserved = 1u;
	return __xnetStreamPostAsync(pStream, pOp);
}


static xnet_result UNUSED_ATTR __xnetStreamPostSendRef(xnetstream* pStream, const xnetbufref* pRef)
{
	return __xnetStreamPostSendRefEx(pStream, pRef, false);
}


// 内部函数：__xnetStreamPostRecvCopy
static xnet_result UNUSED_ATTR __xnetStreamPostRecvCopy(xnetstream* pStream, const void* pData, size_t iLen)
{
	return __xnetStreamPostAsync(pStream, __xnetStreamAllocAsyncCopy(pStream, __XNET_STREAM_ASYNC_RECV_COPY, pData, iLen));
}


// 内部函数：__xnetStreamPostRecvRef
static xnet_result UNUSED_ATTR __xnetStreamPostRecvRef(xnetstream* pStream, const xnetbufref* pRef)
{
	return __xnetStreamPostAsync(pStream, __xnetStreamAllocAsyncRef(pStream, __XNET_STREAM_ASYNC_RECV_REF, pRef));
}


// 内部函数：__xnetStreamPostRecvDispatch
static xnet_result UNUSED_ATTR __xnetStreamPostRecvDispatch(xnetstream* pStream)
{
	return __xnetStreamPostAsync(pStream, __xnetStreamAllocAsyncSimple(pStream, __XNET_STREAM_ASYNC_DISPATCH_RECV));
}


// 内部函数：__xnetStreamPostTlsHandshake
static xnet_result __xnetStreamPostTlsHandshake(xnetstream* pStream)
{
	#ifdef XRT_NO_NETTLS
		(void)pStream;
		return XRT_NET_ERROR;
	#else
	if ( !pStream || !pStream->pTls || !__xnetSocketIsValid(pStream->hSocket) ) { return XRT_NET_ERROR; }
	return __xnetStreamPostAsync(pStream, __xnetStreamAllocAsyncSimple(pStream, __XNET_STREAM_ASYNC_TLS_HANDSHAKE));
	#endif
}


// 将关闭操作排到同一 worker 队列，确保先前投递的 send 一定先执行。
static xnet_result __xnetStreamPostClose(xnetstream* pStream, uint32 iFlags)
{
	__xnet_stream_async_op* pOp;

	pOp = __xnetStreamAllocAsyncSimple(pStream, __XNET_STREAM_ASYNC_CLOSE);
	if ( pOp == NULL ) { return XRT_NET_ERROR; }
	pOp->iLen = iFlags;
	return __xnetStreamPostAsync(pStream, pOp);
}



/* ============================== Listener helpers ============================== */

static uint32 __xnetListenerResolveAcceptConcurrency(xnetlistener* pListener)
{
	uint32 iConcurrency;
	if ( !pListener || !pListener->pEngine || !pListener->pWorker ) { return 1u; }
	iConcurrency = pListener->tConfig.iAcceptConcurrency;
	if ( iConcurrency == 0u ) {
		iConcurrency = pListener->pEngine->iWorkerCount * 2u;
		if ( iConcurrency < 4u ) { iConcurrency = 4u; }
		if ( pListener->pEngine->tConfig.iAcceptBatch > 0u &&
			iConcurrency > pListener->pEngine->tConfig.iAcceptBatch ) {
			iConcurrency = pListener->pEngine->tConfig.iAcceptBatch;
		}
	}
	if ( iConcurrency == 0u ) { iConcurrency = 1u; }
	if ( iConcurrency > 1024u ) { iConcurrency = 1024u; }
	#if defined(_WIN32) || defined(_WIN64)
		if ( pListener->pWorker->tPort.pOps != xrtNetPortIOCPOps() ) { iConcurrency = 1u; }
	#elif defined(__linux__)
		if ( pListener->pWorker->tPort.pOps != xrtNetPortUringOps() ||
			!__xnetPortUringHasNativeRing(&pListener->pWorker->tPort) ) { iConcurrency = 1u; }
	#else
		iConcurrency = 1u;
	#endif
	return iConcurrency;
}

XXAPI xnetlistener* xrtNetListenerCreate(xnetengine* pEngine, const xnetlistenconfig* pCfg,
	const xnetlistenerevents* pEvents, const xnetstreamevents* pStreamEvents, ptr pUserData)
{
	xnetlistener* pListener;
	if ( !pEngine ) { return NULL; }
	if ( pCfg && !__xnetConfigHeaderIsValid(pCfg->iSize, pCfg->iVersion, XNET_LISTEN_CONFIG_V1_SIZE) ) {
		__xnetErrorSetEx(XRT_NET_ERROR, XNET_OP_ACCEPT, XNET_PHASE_VALIDATE, XNET_BACKEND_NONE, 0,
			"invalid listener config version or size.");
		return NULL;
	}

	// 分配并初始化监听器结构
	pListener = (xnetlistener*)XNET_ALLOC(sizeof(xnetlistener));
	if ( !pListener ) { return NULL; }
	memset(pListener, 0, sizeof(xnetlistener));
	__xnetStreamBindEngine(pEngine);
	pListener->hSocket = XNET_SOCKET_INVALID;
	pListener->pEngine = pEngine;
	__xnetEngineAddLiveObject(pEngine);
	pListener->pEvents = pEvents;
	pListener->pStreamEvents = pStreamEvents;
	pListener->pUserData = pUserData;
	// 复制监听配置或使用默认配置
	xrtNetListenConfigInit(&pListener->tConfig);
	if ( pCfg ) {
		(void)__xnetConfigCopyKnown(&pListener->tConfig, sizeof(pListener->tConfig),
			pCfg, pCfg->iSize, pCfg->iVersion, XNET_LISTEN_CONFIG_V1_SIZE);
		pListener->tConfig.iSize = (uint32)sizeof(pListener->tConfig);
	}
	// 根据 REUSE_PORT 标志决定绑定到多少个 worker
	pListener->iPortCount = ((pListener->tConfig.iFlags & XNET_LISTEN_F_REUSE_PORT) != 0 && pEngine->iWorkerCount > 0)
		? pEngine->iWorkerCount
		: 1u;
	pListener->pWorker = __xnetListenerPickWorker(pEngine, &pListener->tConfig);
	pListener->iAcceptConcurrency = __xnetListenerResolveAcceptConcurrency(pListener);
	pListener->iNextWorker = 0;
	return pListener;
}


// 销毁网络监听器
XXAPI void xrtNetListenerDestroy(xnetlistener* pListener)
{
	if ( !pListener ) { return; }
	if ( pListener->tAcceptWait.pfnWait != NULL ) {
		__xnetStreamSetError("cannot destroy listener while an async waiter or task still holds it.");
		return;
	}
	pListener->bRunning = false;
	(void)__xnetListenerCancelAcceptWatch(pListener);
	__xnetListenerClearAcceptedSockets(pListener);
	__xnetListenerCloseSocket(pListener);
	__xnetListenerNotifyAllWaiters(pListener, XRT_NET_CLOSED);
	if ( __xnetAtomicLoad32(&pListener->iAsyncHoldCount) != 0 ) {
		__xnetListenerPrepareDeferredDestroy(pListener);
		return;
	}
	__xnetListenerFinalizeDestroy(pListener);
}


// 启动网络监听器
XXAPI xnet_result xrtNetListenerStart(xnetlistener* pListener)
{
	struct sockaddr_storage tStorage;
	socklen_t iAddrLen = 0;
	if ( !pListener || !pListener->pEngine || __xnetAtomicLoad32(&pListener->pEngine->bRunning) == 0 ) { return XRT_NET_ERROR; }
	if ( pListener->bRunning ) { return XRT_NET_OK; }
	if ( __xnetAtomicLoad32(&pListener->iAsyncHoldCount) != 0 ) { return XRT_NET_AGAIN; }
	// 将绑定地址转换为 sockaddr 结构
	if ( !__xnetAddrToSockAddr(&pListener->tConfig.tBindAddr, &tStorage, &iAddrLen) ) { return XRT_NET_ERROR; }
	// 创建 TCP 流套接字
	pListener->hSocket = __xnetSocketCreateStream(pListener->tConfig.tBindAddr.iFamily);
	if ( !__xnetSocketIsValid(pListener->hSocket) ) { return XRT_NET_ERROR; }
	if ( !__xnetSocketApplyListenFlags(pListener->hSocket, pListener->tConfig.iFlags) ) {
		__xnetListenerCloseSocket(pListener);
		return XRT_NET_ERROR;
	}
	(void)__xnetSocketSetNonBlock(pListener->hSocket, true);
	// 绑定到指定地址和端口
	if ( bind(pListener->hSocket, (struct sockaddr*)&tStorage, iAddrLen) != 0 ) {
		if ( pListener->pEvents && pListener->pEvents->OnError ) {
			pListener->pEvents->OnError(pListener->pUserData, pListener, __xnetSocketLastErr());
		}
		__xnetListenerCloseSocket(pListener);
		return XRT_NET_ERROR;
	}
	// 开始监听，默认 backlog 为 128
	if ( listen(pListener->hSocket, (int)(pListener->tConfig.iBacklog > 0 ? pListener->tConfig.iBacklog : 128u)) != 0 ) {
		if ( pListener->pEvents && pListener->pEvents->OnError ) {
			pListener->pEvents->OnError(pListener->pUserData, pListener, __xnetSocketLastErr());
		}
		__xnetListenerCloseSocket(pListener);
		return XRT_NET_ERROR;
	}
	// 更新本地地址信息（bind 后系统可能分配了实际端口）
	(void)__xnetSocketUpdateLocalAddr(pListener->hSocket, &pListener->tConfig.tBindAddr);
	pListener->pWorker = __xnetListenerPickWorker(pListener->pEngine, &pListener->tConfig);
	pListener->iAcceptConcurrency = __xnetListenerResolveAcceptConcurrency(pListener);
	pListener->iAcceptOpId = 0;
	(void)__xnetAtomicExchange32(&pListener->iAcceptArmedCount, 0);
	pListener->bRunning = true;
	if ( (pListener->tConfig.iFlags & XNET_LISTEN_F_PULL_ACCEPT) == 0 &&
		pListener->pEvents && pListener->pEvents->OnAccept && !__xnetListenerMaintainAcceptWatches(pListener) ) {
		pListener->bRunning = false;
		__xnetListenerCloseSocket(pListener);
		return XRT_NET_ERROR;
	}
	return XRT_NET_OK;
}


// 停止网络监听器
XXAPI void xrtNetListenerStop(xnetlistener* pListener)
{
	if ( !pListener ) { return; }
	pListener->bRunning = false;
	(void)__xnetListenerCancelAcceptWatch(pListener);
	__xnetListenerClearAcceptedSockets(pListener);
	__xnetListenerCloseSocket(pListener);
	__xnetListenerNotifyAllWaiters(pListener, XRT_NET_CLOSED);
}


// 内部函数：创建监听器 accepted 流
static xnetstream* __xnetListenerCreateAcceptedStream(xnetlistener* pListener, ptr pUserData)
{
	xnetstream* pStream;
	xnetworker* pWorker;
	bool bAccepted = true;

	if ( !pListener || !pListener->pEngine ) { return NULL; }

	// 分配并初始化流结构
	pStream = (xnetstream*)XNET_ALLOC(sizeof(xnetstream));
	if ( !pStream ) { return NULL; }
	memset(pStream, 0, sizeof(xnetstream));
	__xnetStreamBindEngine(pListener->pEngine);
	pStream->hSocket = XNET_SOCKET_INVALID;
	pWorker = __xnetStreamPickWorker(pListener->pEngine, pListener);

	// 设置流的基本属性
	pStream->iId = __xnetEngineAllocStreamId(pListener->pEngine);
	pStream->pEngine = pListener->pEngine;
	__xnetEngineAddLiveObject(pListener->pEngine);
	pStream->pWorker = pWorker;
	pStream->pListener = pListener;
	pStream->pEvents = pListener->pStreamEvents;
	pStream->pUserData = pUserData;
	pStream->iState = __XNET_STREAM_STATE_INIT;
	pStream->iCloseReason = XRT_NET_AGAIN;
	__xnetStreamInitQueues(pStream, pWorker);
	__xnetStreamApplyDefaults(pStream, NULL, &pListener->tConfig);
	// 尝试附加 TLS 会话（服务端角色）
	if ( !__xnetStreamAttachTls(pStream, pListener->tConfig.pTlsConfig, true) ) {
		xrtNetStreamDestroy(pStream);
		return NULL;
	}

	// 回调 OnAccept 事件，让上层决定是否接受此连接
	if ( pListener->pEvents && pListener->pEvents->OnAccept ) {
		bAccepted = pListener->pEvents->OnAccept(pListener->pUserData, pListener, pStream);
	}
	if ( !bAccepted ) {
		xrtNetStreamDestroy(pStream);
		return NULL;
	}
	return pStream;
}


// 内部函数：接受监听器套接字
static bool __xnetListenerAcceptSocket(xnetlistener* pListener, __xnet_listener_accept_raw* pRaw, int* pSysErr)
{
	if ( pSysErr ) { *pSysErr = 0; }
	// 初始化原始接受结果结构
	if ( pRaw ) {
		memset(pRaw, 0, sizeof(*pRaw));
		pRaw->hSocket = XNET_SOCKET_INVALID;
		pRaw->iAddrLen = (socklen_t)sizeof(pRaw->tStorage);
	}
	if ( !pListener || !pRaw || !pListener->bRunning || !__xnetSocketIsValid(pListener->hSocket) ) {
		if ( pSysErr ) { *pSysErr = __XNET_LISTENER_ACCEPT_SYS_CLOSED; }
		return false;
	}

	// 尝试接受一个新连接
	pRaw->hSocket = accept(pListener->hSocket, (struct sockaddr*)&pRaw->tStorage, &pRaw->iAddrLen);
	if ( !__xnetSocketIsValid(pRaw->hSocket) ) {
		int iErr = __xnetSocketLastErr();
		// 非阻塞模式下无连接可用不算错误
		if ( pSysErr ) {
			*pSysErr = __xnetSocketWouldBlock(iErr) ? 0 : iErr;
		}
		// 真正的错误回调 OnError
		if ( !__xnetSocketWouldBlock(iErr) && pListener->pEvents && pListener->pEvents->OnError ) {
			pListener->pEvents->OnError(pListener->pUserData, pListener, iErr);
		}
		return false;
	}
	return true;
}


// 内部函数：__xnetListenerWrapAcceptedSocket
static xnetstream* __xnetListenerWrapAcceptedSocket(xnetlistener* pListener, const __xnet_listener_accept_raw* pRaw, ptr pUserData)
{
	xnetstream* pStream;
	if ( !pListener || !pRaw || !__xnetSocketIsValid(pRaw->hSocket) ) { return NULL; }

	// 创建已接受的流对象
	pStream = __xnetListenerCreateAcceptedStream(pListener, pUserData);
	if ( !pStream ) {
		return NULL;
	}
	pStream->hSocket = pRaw->hSocket;
	(void)__xnetSocketSetNonBlock(pStream->hSocket, true);
	// 应用套接字选项：无延迟和保活
	if ( (pListener->tConfig.iFlags & XNET_LISTEN_F_NO_DELAY) != 0 ) {
		(void)__xnetSocketSetNoDelay(pStream->hSocket);
	}
	if ( (pListener->tConfig.iFlags & XNET_LISTEN_F_KEEPALIVE) != 0 ) {
		(void)__xnetSocketSetKeepAlive(pStream->hSocket);
	}
	// 解析对端地址：优先从 accept 返回的地址中获取
	if ( pRaw->iAddrLen > 0 && ((const struct sockaddr*)&pRaw->tStorage)->sa_family != 0 ) {
		(void)__xnetAddrFromSockAddr(&pStream->tRemoteAddr, (const struct sockaddr*)&pRaw->tStorage);
	} else {
		struct sockaddr_storage tPeer;
		socklen_t iPeerLen = (socklen_t)sizeof(tPeer);
		memset(&tPeer, 0, sizeof(tPeer));
		if ( getpeername(pStream->hSocket, (struct sockaddr*)&tPeer, &iPeerLen) == 0 ) {
			(void)__xnetAddrFromSockAddr(&pStream->tRemoteAddr, (const struct sockaddr*)&tPeer);
		}
	}
	// 更新本地地址
	(void)__xnetSocketUpdateLocalAddr(pStream->hSocket, &pStream->tLocalAddr);
	// TLS 模式：提交异步 TLS 握手任务
	if ( pStream->pTls ) {
		if ( __xnetStreamPostTlsHandshake(pStream) != XRT_NET_OK ) {
			(void)__xnetStreamDriveTlsHandshake(pStream);
		}
	} else {
		// 将 accepted-stream open 投递回所属 worker，避免 accept 线程直接进入流回调。
		if ( !__xnetStreamSubmitOpenEvent(pStream, XNET_PORT_OP_ACCEPT) ) {
			__xnetStreamEmitOpen(pStream);
			if ( __xnetStreamDrainSocketNow(pStream) && !pStream->bClosing &&
				__xnetSocketIsValid(pStream->hSocket) && !__xnetStreamRecvArmed(pStream) ) {
				(void)__xnetStreamArmRecvWatch(pStream);
			}
		}
	}
	return pStream;
}


// 内部函数：等待监听器 register 同步接受
static bool __xnetListenerRegisterSyncAcceptWait(xnetlistener* pListener, __xnet_listener_sync_wait_fn pfnWait, __xnet_listener_sync_wait_ready_fn pfnCanAccept, ptr pCtx)
{
	__xnet_listener_accept_raw tRaw;
	int iSysErr = 0;
	xnetstream* pStream = NULL;

	if ( !pListener || !pfnWait ) { return false; }
	// 必须在监听器所属的 worker 线程上调用
	if ( pListener->pWorker && !__xnetEngineIsCurrentWorker(pListener->pWorker) ) { return false; }
	if ( !__xnetListenerEnqueueWait(pListener, pfnWait, pfnCanAccept, pCtx) ) { return false; }

	// 注册同步接受等待的回调

	// 监听器未运行或套接字无效，立即通知关闭
	if ( !pListener->bRunning || !__xnetSocketIsValid(pListener->hSocket) ) {
		__xnetListenerNotifyAllWaiters(pListener, XRT_NET_CLOSED);
		return true;
	}

	/*
		AcceptEx 可能在限时 waiter 取消后才完成。该连接已经从系统 backlog
		取出，不能丢弃；下一位 waiter 应优先接管它。
	*/
	memset(&tRaw, 0, sizeof(tRaw));
	tRaw.hSocket = __xnetListenerPopAcceptedSocket(pListener);
	if ( __xnetSocketIsValid(tRaw.hSocket) ) {
		pStream = __xnetListenerWrapAcceptedSocket(pListener, &tRaw, NULL);
		if ( pStream ) {
			__xnetListenerNotifySyncAccept(pListener, XRT_NET_OK, pStream);
			if ( __xnetListenerCanDispatchAccept(pListener) ) { (void)__xnetListenerMaintainAcceptWatches(pListener); }
			return true;
		}
		__xnetSocketCloseHandle(&tRaw.hSocket);
	}

	// 尝试立即接受一个连接
	memset(&tRaw, 0, sizeof(tRaw));
	tRaw.hSocket = XNET_SOCKET_INVALID;
	if ( __xnetListenerAcceptSocket(pListener, &tRaw, &iSysErr) ) {
		// 检查是否还能分发接受事件
		if ( !__xnetListenerCanDispatchAccept(pListener) ) {
			__xnetSocketCloseHandle(&tRaw.hSocket);
			return true;
		}
		pStream = __xnetListenerWrapAcceptedSocket(pListener, &tRaw, NULL);
		if ( pStream ) {
			__xnetListenerNotifySyncAccept(pListener, XRT_NET_OK, pStream);
			if ( __xnetListenerCanDispatchAccept(pListener) ) { (void)__xnetListenerMaintainAcceptWatches(pListener); }
			return true;
		}
		__xnetSocketCloseHandle(&tRaw.hSocket);
	}

	// 监听器已关闭
	if ( iSysErr == __XNET_LISTENER_ACCEPT_SYS_CLOSED ) {
		__xnetListenerNotifyAllWaiters(pListener, XRT_NET_CLOSED);
		return true;
	}
	// accept 遇到系统错误
	if ( iSysErr != 0 ) {
		__xnetListenerNotifySyncAccept(pListener, XRT_NET_ERROR, NULL);
		return true;
	}
	// 无法分发（上层取消了等待）
	if ( !__xnetListenerCanDispatchAccept(pListener) ) {
		return true;
	}
	// 挂起异步接受监视以等待下一个连接
	if ( !__xnetListenerMaintainAcceptWatches(pListener) ) {
		__xnetListenerNotifySyncAccept(pListener, XRT_NET_ERROR, NULL);
	}
	return true;
}


// 内部函数：等待监听器 cancel 同步接受
static bool __xnetListenerCancelSyncAcceptWait(xnetlistener* pListener, ptr pCtx)
{
	if ( !pListener ) { return false; }
	if ( pListener->pWorker && !__xnetEngineIsCurrentWorker(pListener->pWorker) ) { return false; }
	return __xnetListenerRemoveWait(pListener, pCtx);
}


// 内部函数：接受监听器 handle 事件
static void __xnetListenerHandleAcceptEvent(xnetlistener* pListener)
{
	__xnet_listener_accept_raw tRaw;
	int iSysErr = 0;
	xnetstream* pStream = NULL;

	if ( !pListener ) { return; }
	// 检查是否可以分发接受事件
	if ( !__xnetListenerCanDispatchAccept(pListener) ) { return; }

	// 尝试接受一个新连接
	memset(&tRaw, 0, sizeof(tRaw));
	tRaw.hSocket = XNET_SOCKET_INVALID;
	if ( __xnetListenerAcceptSocket(pListener, &tRaw, &iSysErr) ) {
		// 再次检查分发条件（可能在 accept 过程中状态变化）
		if ( !__xnetListenerCanDispatchAccept(pListener) ) {
			__xnetSocketCloseHandle(&tRaw.hSocket);
			return;
		}
		pStream = __xnetListenerWrapAcceptedSocket(pListener, &tRaw, NULL);
		if ( pStream ) {
			if ( pListener->tAcceptWait.pfnWait ) {
				__xnetListenerNotifySyncAccept(pListener, XRT_NET_OK, pStream);
				if ( __xnetListenerCanDispatchAccept(pListener) ) { (void)__xnetListenerMaintainAcceptWatches(pListener); }
			} else if ( __xnetListenerCanDispatchAccept(pListener) ) {
				(void)__xnetListenerMaintainAcceptWatches(pListener);
			}
			return;
		}
		#if defined(XRT_INTERNAL_TEST_ENV)
			fprintf(stderr, "[XNET ACCEPT] accepted socket wrap failed\n");
		#endif
		// 包装失败，关闭已接受的套接字并重新挂起接受监视
		__xnetSocketCloseHandle(&tRaw.hSocket);
		if ( __xnetListenerCanDispatchAccept(pListener) && pListener->bRunning && __xnetSocketIsValid(pListener->hSocket) ) {
			(void)__xnetListenerMaintainAcceptWatches(pListener);
		}
		return;
	}

	// 非阻塞模式下无连接可用，重新挂起接受监视
	if ( iSysErr == 0 ) {
		if ( __xnetListenerCanDispatchAccept(pListener) && pListener->bRunning && __xnetSocketIsValid(pListener->hSocket) ) {
			(void)__xnetListenerMaintainAcceptWatches(pListener);
		}
		return;
	}
	// 监听器已关闭
	if ( iSysErr == __XNET_LISTENER_ACCEPT_SYS_CLOSED ) {
		__xnetListenerNotifyAllWaiters(pListener, XRT_NET_CLOSED);
		return;
	}
	// accept 遇到系统错误
	#if defined(XRT_INTERNAL_TEST_ENV)
		fprintf(stderr, "[XNET ACCEPT] accept event failed, system error=%d\n", iSysErr);
	#endif
	__xnetListenerNotifySyncAccept(pListener, XRT_NET_ERROR, NULL);
}


// 内部函数：处理监听器 accepted 套接字事件
static void __xnetListenerHandleAcceptedSocketEvent(xnetlistener* pListener, xsocket hSocket)
{
	__xnet_listener_accept_raw tRaw;
	xnetstream* pStream = NULL;

	if ( !pListener ) { return; }
	// 无效套接字，回退到普通 accept 路径
	if ( !__xnetSocketIsValid(hSocket) ) {
		__xnetListenerHandleAcceptEvent(pListener);
		return;
	}
	// 检查是否可以分发接受事件
	if ( !__xnetListenerCanDispatchAccept(pListener) ) {
		/*
			限时 waiter 可能刚刚取消，但 AcceptEx 已经成功。保留这一个
			内核已接受连接，避免客户端连接成功后被静默丢弃。
		*/
		if ( pListener->bRunning ) {
			(void)__xnetListenerQueueAcceptedSocket(pListener, hSocket);
			return;
		}
		__xnetSocketCloseHandle(&hSocket);
		return;
	}

	// 使用 IOCP 返回的已接受套接字包装为流对象
	memset(&tRaw, 0, sizeof(tRaw));
	tRaw.hSocket = hSocket;
	pStream = __xnetListenerWrapAcceptedSocket(pListener, &tRaw, NULL);
	if ( pStream ) {
		if ( pListener->tAcceptWait.pfnWait ) {
			__xnetListenerNotifySyncAccept(pListener, XRT_NET_OK, pStream);
			if ( __xnetListenerCanDispatchAccept(pListener) ) { (void)__xnetListenerMaintainAcceptWatches(pListener); }
		} else if ( __xnetListenerCanDispatchAccept(pListener) ) {
			(void)__xnetListenerMaintainAcceptWatches(pListener);
		}
		return;
	}
	#if defined(XRT_INTERNAL_TEST_ENV)
		fprintf(stderr, "[XNET ACCEPT] native accepted socket wrap failed\n");
	#endif
	// 包装失败，关闭套接字并重新挂起接受监视
	__xnetSocketCloseHandle(&hSocket);
	if ( __xnetListenerCanDispatchAccept(pListener) &&
		pListener->bRunning && __xnetSocketIsValid(pListener->hSocket) ) {
		(void)__xnetListenerMaintainAcceptWatches(pListener);
	}
}

#if defined(XRT_INTERNAL_TEST_ENV)
// 内部函数：__xnetListenerTryAcceptOneEx
static xnetstream* __xnetListenerTryAcceptOneEx(xnetlistener* pListener, ptr pUserData, int* pSysErr)
{
	__xnet_listener_accept_raw tRaw;
	xnetstream* pStream = NULL;

	memset(&tRaw, 0, sizeof(tRaw));
	tRaw.hSocket = XNET_SOCKET_INVALID;
	if ( !__xnetListenerAcceptSocket(pListener, &tRaw, pSysErr) ) {
		return NULL;
	}

	pStream = __xnetListenerWrapAcceptedSocket(pListener, &tRaw, pUserData);
	if ( !pStream ) {
		__xnetSocketCloseHandle(&tRaw.hSocket);
		if ( pSysErr ) { *pSysErr = 0; }
	}
	return pStream;
}


// 内部函数：__xnetListenerTryAcceptOne
static xnetstream* __xnetListenerTryAcceptOne(xnetlistener* pListener, ptr pUserData)
{
	return __xnetListenerTryAcceptOneEx(pListener, pUserData, NULL);
}
#endif

/* ============================== Stream helpers ============================== */

XXAPI xnetstream* xrtNetStreamCreate(xnetengine* pEngine, const xnetstreamevents* pEvents, ptr pUserData)
{
	xnetstream* pStream;
	xnetworker* pWorker;
	if ( !pEngine ) { return NULL; }

	pStream = (xnetstream*)XNET_ALLOC(sizeof(xnetstream));
	if ( !pStream ) { return NULL; }
	memset(pStream, 0, sizeof(xnetstream));
	__xnetStreamBindEngine(pEngine);
	pStream->hSocket = XNET_SOCKET_INVALID;
	pWorker = __xnetStreamPickWorker(pEngine, NULL);

	pStream->iId = __xnetEngineAllocStreamId(pEngine);
	pStream->pEngine = pEngine;
	__xnetEngineAddLiveObject(pEngine);
	pStream->pWorker = pWorker;
	pStream->pEvents = pEvents;
	pStream->pUserData = pUserData;
	pStream->iState = __XNET_STREAM_STATE_INIT;
	pStream->iCloseReason = XRT_NET_AGAIN;
	__xnetStreamInitQueues(pStream, pWorker);
	__xnetStreamApplyDefaults(pStream, NULL, NULL);
	return pStream;
}


// 销毁网络流
XXAPI void xrtNetStreamDestroy(xnetstream* pStream)
{
	if ( !pStream ) { return; }
	if ( __xnetAtomicLoad32(&pStream->iAsyncHoldCount) != 0 ) {
		__xnetStreamPrepareDeferredDestroy(pStream);
		return;
	}
	__xnetStreamNotifyDestroyWaiters(pStream);
	__xnetStreamClearRx(pStream);
	__xnetStreamClearSendQueue(pStream);
	__xnetStreamFinalizeSocketClose(pStream);
	__xnetStreamDetachTls(pStream);
	__xnetStreamDetachProxy(pStream);
	__xnetEngineReleaseLiveObject(pStream->pEngine);
	XNET_FREE(pStream);
}


typedef struct {
	xnetstream* pStream;
	uint64 iConnectOpId;
} __xnet_connect_fallback_task;

static xnet_result __xnetStreamBeginNextResolvedConnect(xnetstream* pStream);


static void __xnetStreamConnectFallbackTask(xnetworker* pWorker, ptr pArg)
{
	__xnet_connect_fallback_task* pTask = (__xnet_connect_fallback_task*)pArg;
	xnetstream* pStream = pTask ? pTask->pStream : NULL;
	(void)pWorker;
	if ( pStream && !pStream->bClosing &&
		(pStream->iState & __XNET_STREAM_STATE_CLOSE_EMITTED) == 0 &&
		pStream->iConnectOpId == pTask->iConnectOpId &&
		pStream->pConnectAddrList &&
		pStream->iConnectAddrIndex < xrtNetAddrListCount(pStream->pConnectAddrList) ) {
		xsocket hPending = pStream->hSocket;
		if ( __xnetSocketIsValid(hPending) ) {
			(void)__xnetStreamSubmitSocketNotice(pStream, XNET_PORT_OP_CLOSE, hPending);
			__xnetSocketCloseHandle(&pStream->hSocket);
		}
		if ( __xnetStreamBeginNextResolvedConnect(pStream) != XRT_NET_OK ) {
			xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		}
	}
	if ( pStream ) { __xnetStreamReleaseAsyncHold(pStream); }
	XNET_FREE(pTask);
}


static void __xnetStreamScheduleConnectFallback(xnetstream* pStream)
{
	__xnet_connect_fallback_task* pTask;
	if ( !pStream || pStream->iConnectFallbackDelayMs == 0u ||
		!pStream->pConnectAddrList ||
		pStream->iConnectAddrIndex >= xrtNetAddrListCount(pStream->pConnectAddrList) ) { return; }
	pTask = (__xnet_connect_fallback_task*)XNET_ALLOC(sizeof(*pTask));
	if ( !pTask ) { return; }
	pTask->pStream = pStream;
	pTask->iConnectOpId = pStream->iConnectOpId;
	__xnetStreamAddAsyncHold(pStream);
	if ( xrtNetEnginePostDelayed(pStream->pEngine, pStream->pWorker->iId,
		pStream->iConnectFallbackDelayMs, __xnetStreamConnectFallbackTask, pTask) != XRT_NET_OK ) {
		__xnetStreamReleaseAsyncHold(pStream);
		XNET_FREE(pTask);
	}
}


static xnet_result __xnetStreamBeginResolvedConnect(xnetstream* pStream, const xnetaddr* pAddr)
{
	struct sockaddr_storage tStorage;
	socklen_t iAddrLen = 0;
	xsocket hSocket;
	if ( !pStream || !pAddr || pStream->bClosing ||
		(pStream->iState & __XNET_STREAM_STATE_CLOSE_EMITTED) != 0 ) { return XRT_NET_CLOSED; }
	pStream->tRemoteAddr = *pAddr;
	if ( !__xnetAddrToSockAddr(&pStream->tRemoteAddr, &tStorage, &iAddrLen) ) { return XRT_NET_ERROR; }
	hSocket = __xnetSocketCreateStream(pStream->tRemoteAddr.iFamily);
	if ( !__xnetSocketIsValid(hSocket) ) { return XRT_NET_ERROR; }
	(void)__xnetSocketApplyConnectFlags(hSocket, pStream->iConnectFlags);
	if ( !__xnetStreamUseNativePortOps(pStream) ) {
		__xnetStreamSetError("mainline stream connect requires a native xnet backend.");
		__xnetSocketCloseHandle(&hSocket);
		return XRT_NET_ERROR;
	}
	pStream->hSocket = hSocket;
	pStream->iConnectOpId = __xnetEngineAllocStreamId(pStream->pEngine);
	if ( pStream->iConnectOpId == 0u ) {
		__xnetSocketCloseHandle(&pStream->hSocket);
		return XRT_NET_ERROR;
	}
	/* The IOCP readiness completion is followed by recv(); it must never block the worker. */
	(void)__xnetSocketSetNonBlock(pStream->hSocket, true);
	if ( !__xnetStreamSubmitOpenEvent(pStream, XNET_PORT_OP_CONNECT) ) {
		__xnetSocketCloseHandle(&pStream->hSocket);
		pStream->iConnectOpId = 0u;
		return XRT_NET_ERROR;
	}
	__xnetStreamScheduleConnectFallback(pStream);
	return XRT_NET_OK;
}


static xnet_result __xnetStreamBeginNextResolvedConnect(xnetstream* pStream)
{
	uint32 iCount;
	if ( !pStream || !pStream->pConnectAddrList ) { return XRT_NET_ERROR; }
	iCount = xrtNetAddrListCount(pStream->pConnectAddrList);
	while ( pStream->iConnectAddrIndex < iCount ) {
		const xnetaddr* pAddr = xrtNetAddrListGet(
			pStream->pConnectAddrList, pStream->iConnectAddrIndex++);
		if ( pAddr && __xnetStreamBeginResolvedConnect(pStream, pAddr) == XRT_NET_OK ) {
			return XRT_NET_OK;
		}
	}
	return XRT_NET_ERROR;
}


static void __xnetStreamOnResolvedAll(xnetworker* pWorker, ptr pArg, xnet_result iStatus,
	xnetaddrlist* pList, int iSysErr)
{
	xnetstream* pStream = (xnetstream*)pArg;
	xnet_result iConnectStatus = iStatus;
	(void)pWorker;
	if ( !pStream ) {
		if ( pList ) { xrtNetAddrListDestroy(pList); }
		return;
	}
	if ( !pStream->bClosing && (pStream->iState & __XNET_STREAM_STATE_CLOSE_EMITTED) == 0 ) {
		if ( iStatus == XRT_NET_OK && pList ) {
			if ( pStream->pConnectAddrList ) { xrtNetAddrListDestroy(pStream->pConnectAddrList); }
			pStream->pConnectAddrList = pList;
			pStream->iConnectAddrIndex = 0u;
			pList = NULL;
			iConnectStatus = __xnetStreamBeginNextResolvedConnect(pStream);
		}
		if ( iConnectStatus != XRT_NET_OK ) {
			__xnetErrorSetEx(iConnectStatus, XNET_OP_RESOLVE, XNET_PHASE_COMPLETE,
				XNET_BACKEND_NONE, iSysErr, "asynchronous host resolution failed.");
			if ( pStream->pEvents && pStream->pEvents->OnError ) {
				pStream->pEvents->OnError(__xnetStreamOwner(pStream), pStream, iSysErr);
			}
			pStream->bClosing = true;
			pStream->iFlags = XNET_CLOSE_F_ABORT;
			__xnetStreamFinishClose(pStream, iConnectStatus == XRT_NET_CLOSED ? XRT_NET_CLOSED : XRT_NET_ERROR);
		}
	}
	if ( pList ) { xrtNetAddrListDestroy(pList); }
	__xnetStreamReleaseAsyncHold(pStream);
}


// 连接网络流
XXAPI xnet_result xrtNetStreamConnect(xnetstream* pStream, const xnetconnectconfig* pCfg)
{
	const char* sConnectHost;
	uint16 iConnectPort;
	xnetaddr tAddr;
	xnet_result iResolveStatus;
	if ( !pStream || !pStream->pEngine || !pStream->pEngine->bRunning || !pCfg ) { return XRT_NET_ERROR; }
	if ( !__xnetConfigHeaderIsValid(pCfg->iSize, pCfg->iVersion, XNET_CONNECT_CONFIG_V1_SIZE) ) {
		__xnetErrorSetEx(XRT_NET_ERROR, XNET_OP_CONNECT, XNET_PHASE_VALIDATE, XNET_BACKEND_NONE, 0,
			"invalid connect config version or size.");
		return XRT_NET_ERROR;
	}
	if ( pCfg->iAddressFamily != AF_UNSPEC && pCfg->iAddressFamily != AF_INET &&
		pCfg->iAddressFamily != AF_INET6 ) {
		__xnetErrorSetEx(XRT_NET_ERROR, XNET_OP_CONNECT, XNET_PHASE_VALIDATE,
			XNET_BACKEND_NONE, 0, "invalid connect address family.");
		return XRT_NET_ERROR;
	}
	__xnetStreamApplyDefaults(pStream, pCfg, NULL);
	if ( __xnetSocketIsValid(pStream->hSocket) ) { return XRT_NET_ERROR; }
	pStream->iConnectFlags = pCfg->iFlags;

	// 处理代理连接配置
	if ( pCfg && pCfg->pProxy ) {
		if ( !pCfg->sHost || !pCfg->sHost[0] || pCfg->iPort == 0 ) { return XRT_NET_ERROR; }
		pStream->pProxy = xrtNetProxyAddRef(pCfg->pProxy);
		if ( !pStream->pProxy ) { return XRT_NET_ERROR; }
		// 创建代理状态机，用于完成代理握手
		pStream->pProxyState = __xnetProxyStateCreate(pStream->pProxy, pCfg->sHost, pCfg->iPort);
		if ( !pStream->pProxyState ) {
			__xnetStreamDetachProxy(pStream);
			return XRT_NET_ERROR;
		}
		// 实际连接目标是代理服务器
		sConnectHost = pStream->pProxy->tConfig.sHost;
		iConnectPort = pStream->pProxy->tConfig.iPort;
	} else {
		sConnectHost = pCfg ? pCfg->sHost : NULL;
		iConnectPort = pCfg ? pCfg->iPort : 0;
	}

	// 附加 TLS 会话（客户端角色）
	if ( !__xnetStreamAttachTls(pStream, pCfg ? pCfg->pTlsConfig : NULL, false) ) {
		__xnetStreamDetachProxy(pStream);
		return XRT_NET_ERROR;
	}
	// 连接超时从提交时开始，覆盖 DNS、代理和 TLS 阶段。
	if ( pStream->iConnectTimeoutMs > 0 && !__xnetStreamArmOpenTimer(pStream, pStream->iConnectTimeoutMs) ) {
		__xnetStreamDetachTls(pStream);
		__xnetStreamDetachProxy(pStream);
		return XRT_NET_ERROR;
	}

	memset(&tAddr, 0, sizeof(tAddr));
	tAddr.iPort = iConnectPort;
	if ( sConnectHost && sConnectHost[0] &&
		xrtNetAddrParse(&tAddr, sConnectHost, iConnectPort) != XRT_NET_OK ) {
		__xnetStreamAddAsyncHold(pStream);
		iResolveStatus = __xnetEngineResolveAllAsync(pStream->pEngine, pStream->pWorker->iId,
			sConnectHost, iConnectPort, pCfg->iAddressFamily, __xnetStreamOnResolvedAll, pStream);
		if ( iResolveStatus == XRT_NET_OK ) { return XRT_NET_OK; }
		__xnetStreamReleaseAsyncHold(pStream);
		__xnetStreamCancelOpenTimer(pStream);
		__xnetStreamDetachTls(pStream);
		__xnetStreamDetachProxy(pStream);
		__xnetErrorSetEx(iResolveStatus, XNET_OP_RESOLVE, XNET_PHASE_SUBMIT,
			XNET_BACKEND_NONE, 0, "unable to queue asynchronous host resolution.");
		return iResolveStatus;
	}
	if ( sConnectHost && sConnectHost[0] && pCfg->iAddressFamily != AF_UNSPEC &&
		tAddr.iFamily != pCfg->iAddressFamily ) {
		__xnetStreamCancelOpenTimer(pStream);
		__xnetStreamDetachTls(pStream);
		__xnetStreamDetachProxy(pStream);
		__xnetErrorSetEx(XRT_NET_ERROR, XNET_OP_CONNECT, XNET_PHASE_VALIDATE,
			XNET_BACKEND_NONE, 0, "numeric connect address does not match requested family.");
		return XRT_NET_ERROR;
	}
	if ( !sConnectHost || !sConnectHost[0] ) { tAddr = pStream->tRemoteAddr; }
	iResolveStatus = __xnetStreamBeginResolvedConnect(pStream, &tAddr);
	if ( iResolveStatus != XRT_NET_OK ) {
		__xnetStreamCancelOpenTimer(pStream);
		__xnetStreamDetachTls(pStream);
		__xnetStreamDetachProxy(pStream);
	}
	return iResolveStatus;
}


// 关闭网络流
XXAPI void xrtNetStreamClose(xnetstream* pStream, uint32 iFlags)
{
	if ( !pStream ) { return; }
	(void)__xnetStreamRequestClose(pStream, iFlags);
	// 已经触发过关闭事件，不再重复处理
	if ( (pStream->iState & __XNET_STREAM_STATE_CLOSE_EMITTED) != 0 ) { return; }
	/*
		非 worker 线程的 send 也通过 worker 队列串行化。
		close 必须进入同一队列，避免先置位 bClosing 后丢弃尚未执行的 send。
	*/
	if ( pStream->pEngine != NULL && pStream->pWorker != NULL &&
		 !__xnetEngineIsCurrentWorker(pStream->pWorker) ) {
		if ( __xnetStreamPostClose(pStream, iFlags) == XRT_NET_OK ) { return; }
	}
	if ( pStream->bClosing ) {
		// A graceful close without WAIT_PEER completes an existing peer-wait close.
		if ( (iFlags & XNET_CLOSE_F_ABORT) == 0 ) {
			if ( (pStream->iFlags & XNET_CLOSE_F_WAIT_PEER) != 0 &&
				(iFlags & XNET_CLOSE_F_WAIT_PEER) == 0 ) {
				pStream->iFlags &= ~XNET_CLOSE_F_WAIT_PEER;
				if ( pStream->tSendQ.iQueuedBytes == 0u ) {
					__xnetStreamFinishClose(pStream, XRT_NET_CLOSED);
				} else {
					__xnetStreamKickWrite(pStream);
				}
			}
			return;
		}
		// Abort can upgrade an in-progress graceful close.
		if ( (pStream->iFlags & XNET_CLOSE_F_ABORT) == 0 ) {
			pStream->iFlags |= XNET_CLOSE_F_ABORT;
			__xnetStreamClearRx(pStream);
			__xnetStreamClearSendQueue(pStream);
			__xnetStreamFinishClose(pStream, XRT_NET_CLOSED);
		}
		return;
	}
	// 标记流正在关闭
	pStream->bClosing = true;
	pStream->iFlags = iFlags;
	// 中止模式：立即清空队列并完成关闭
	if ( (iFlags & XNET_CLOSE_F_ABORT) != 0 ) {
		__xnetStreamClearRx(pStream);
		__xnetStreamClearSendQueue(pStream);
		__xnetStreamFinishClose(pStream, XRT_NET_CLOSED);
		return;
	}
	#ifndef XRT_NO_NETTLS
		// 优雅关闭：如果 TLS 已就绪，先发送 TLS 关闭通知
		if ( pStream->pTls && __xnetStreamTlsReady(pStream) && !pStream->bTlsCloseQueued ) {
			pStream->bTlsCloseQueued = true;
			if ( xrtNetTlsSessionQueueClose(pStream->pTls) == XRT_NET_OK ) {
				if ( !__xnetStreamQueueTlsCipher(pStream) ) {
					__xnetStreamFinishClose(pStream, XRT_NET_CLOSED);
					return;
				}
			}
		}
	#endif
	// 发送队列已空，直接进入关闭收尾
	if ( pStream->tSendQ.iQueuedBytes == 0 ) {
		if ( (pStream->iFlags & XNET_CLOSE_F_WAIT_PEER) != 0 ) {
			// 等待对端关闭的优雅关闭模式
			__xnetStreamBeginGracefulCloseWait(pStream);
		} else {
			__xnetStreamFinishClose(pStream, XRT_NET_CLOSED);
		}
		return;
	}
	// 发送队列还有数据，先完成发送
	__xnetStreamKickWrite(pStream);
}


XXAPI xnet_result xrtNetStreamShutdownWrite(xnetstream* pStream)
{
	long iCloseState;
	if ( !pStream ) { return XRT_NET_ERROR; }
	if ( (pStream->iState & __XNET_STREAM_STATE_CLOSE_EMITTED) != 0 ) { return XRT_NET_CLOSED; }
	iCloseState = __xnetAtomicLoad32(&pStream->iCloseRequestState);
	if ( iCloseState != 0 ) {
		return (iCloseState & XNET_CLOSE_F_ABORT) != 0 ? XRT_NET_CLOSED : XRT_NET_OK;
	}
	if ( pStream->bClosing ) {
		return (pStream->iFlags & XNET_CLOSE_F_ABORT) != 0 ? XRT_NET_CLOSED : XRT_NET_OK;
	}
	xrtNetStreamClose(pStream, XNET_CLOSE_F_GRACEFUL | XNET_CLOSE_F_WAIT_PEER);
	return XRT_NET_OK;
}


XXAPI xnet_result xrtNetStreamAbort(xnetstream* pStream)
{
	if ( !pStream ) { return XRT_NET_ERROR; }
	if ( (pStream->iState & __XNET_STREAM_STATE_CLOSE_EMITTED) != 0 ) { return XRT_NET_CLOSED; }
	xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
	return XRT_NET_OK;
}


// 发送网络流
XXAPI xnet_result xrtNetStreamSend(xnetstream* pStream, const void* pData, size_t iLen)
{
	xnet_result iResult;
	if ( !pStream || !pStream->pEngine || __xnetAtomicLoad32(&pStream->pEngine->bRunning) == 0 || !pData || iLen == 0 ) { return XRT_NET_ERROR; }
	if ( !__xnetStreamBeginSendSubmission(pStream) ) { return XRT_NET_CLOSED; }
	if ( pStream->bClosing || (pStream->iState & __XNET_STREAM_STATE_CLOSE_EMITTED) != 0 ) { iResult = XRT_NET_CLOSED; goto done; }
	if ( __xnetStreamHasPreOpenGate(pStream) ) { iResult = XRT_NET_AGAIN; goto done; }
	if ( pStream->pTls && !__xnetStreamTlsReady(pStream) ) { iResult = XRT_NET_AGAIN; goto done; }
	if ( !__xnetStreamTryReserveSend(pStream, iLen) ) { iResult = XRT_NET_AGAIN; goto done; }
	if ( __xnetSocketIsValid(pStream->hSocket) ) {
		if ( __xnetEngineIsCurrentWorker(pStream->pWorker) ) {
			if ( pStream->pTls ) {
				iResult = __xnetStreamAppendTlsPlainCopyEx(pStream, pData, iLen, true) ? XRT_NET_OK : XRT_NET_ERROR;
				goto done;
			}
			if ( !__xnetStreamAppendSendCopyEx(pStream, pData, iLen, true) ) { iResult = XRT_NET_ERROR; goto done; }
			__xnetStreamKickWrite(pStream);
			iResult = XRT_NET_OK;
			goto done;
		}
		iResult = __xnetStreamPostSendCopyEx(pStream, pData, iLen, true);
		goto done;
	}
	if ( pStream->pTls ) {
		iResult = __xnetStreamAppendTlsPlainCopyEx(pStream, pData, iLen, true) ? XRT_NET_OK : XRT_NET_ERROR;
		goto done;
	}
	iResult = __xnetStreamAppendSendCopyEx(pStream, pData, iLen, true) ? XRT_NET_OK : XRT_NET_ERROR;
done:
	__xnetStreamEndSendSubmission(pStream);
	return iResult;
}


// 发送网络流 vec
XXAPI xnet_result xrtNetStreamSendVec(xnetstream* pStream, const xnetspan* pVec, uint32 iCount)
{
	uint64 iTotal = 0u;
	xnet_result iResult;
	if ( !pStream || !pStream->pEngine || __xnetAtomicLoad32(&pStream->pEngine->bRunning) == 0 || !pVec || iCount == 0 ) { return XRT_NET_ERROR; }
	for ( uint32 i = 0; i < iCount; ++i ) {
		if ( !pVec[i].pData || pVec[i].iLen == 0 || iTotal > UINT64_MAX - pVec[i].iLen ) { return XRT_NET_ERROR; }
		iTotal += pVec[i].iLen;
	}
	if ( !__xnetStreamBeginSendSubmission(pStream) ) { return XRT_NET_CLOSED; }
	if ( pStream->bClosing || (pStream->iState & __XNET_STREAM_STATE_CLOSE_EMITTED) != 0 ) { iResult = XRT_NET_CLOSED; goto done; }
	if ( __xnetStreamHasPreOpenGate(pStream) ) { iResult = XRT_NET_AGAIN; goto done; }
	if ( pStream->pTls && !__xnetStreamTlsReady(pStream) ) { iResult = XRT_NET_AGAIN; goto done; }
	if ( !__xnetStreamTryReserveSend(pStream, iTotal) ) { iResult = XRT_NET_AGAIN; goto done; }
	if ( __xnetSocketIsValid(pStream->hSocket) ) {
		if ( __xnetEngineIsCurrentWorker(pStream->pWorker) ) {
			if ( pStream->pTls ) {
				iResult = __xnetStreamAppendTlsPlainVecEx(pStream, pVec, iCount, true) ? XRT_NET_OK : XRT_NET_ERROR;
				goto done;
			}
			if ( !__xnetStreamAppendSendVecEx(pStream, pVec, iCount, true) ) { iResult = XRT_NET_ERROR; goto done; }
			__xnetStreamKickWrite(pStream);
			iResult = XRT_NET_OK;
			goto done;
		}
		iResult = __xnetStreamPostSendVecEx(pStream, pVec, iCount, true);
		goto done;
	}
	if ( pStream->pTls ) {
		iResult = __xnetStreamAppendTlsPlainVecEx(pStream, pVec, iCount, true) ? XRT_NET_OK : XRT_NET_ERROR;
		goto done;
	}
	iResult = __xnetStreamAppendSendVecEx(pStream, pVec, iCount, true) ? XRT_NET_OK : XRT_NET_ERROR;
done:
	__xnetStreamEndSendSubmission(pStream);
	return iResult;
}


static xnet_result __xnetStreamSendRefEx(xnetstream* pStream, const xnetbufref* pRef,
	bool bControl)
{
	xnet_result iResult;
	if ( !pStream || !pStream->pEngine || __xnetAtomicLoad32(&pStream->pEngine->bRunning) == 0 || !pRef || !pRef->pData || pRef->iLen == 0 ) { return XRT_NET_ERROR; }
	if ( !__xnetStreamBeginSendSubmission(pStream) ) { return XRT_NET_CLOSED; }
	if ( pStream->bClosing || (pStream->iState & __XNET_STREAM_STATE_CLOSE_EMITTED) != 0 ) { iResult = XRT_NET_CLOSED; goto done; }
	if ( __xnetStreamHasPreOpenGate(pStream) ) { iResult = XRT_NET_AGAIN; goto done; }
	if ( pStream->pTls && !__xnetStreamTlsReady(pStream) ) { iResult = XRT_NET_AGAIN; goto done; }
	if ( !__xnetStreamTryReserveSendEx(pStream, pRef->iLen, bControl) ) { iResult = XRT_NET_AGAIN; goto done; }
	if ( __xnetSocketIsValid(pStream->hSocket) ) {
		if ( __xnetEngineIsCurrentWorker(pStream->pWorker) ) {
			if ( pStream->pTls ) {
				iResult = __xnetStreamAppendTlsPlainRefEx(pStream, pRef, true) ? XRT_NET_OK : XRT_NET_ERROR;
				if ( iResult == XRT_NET_OK && pRef->pfnRelease ) {
					pRef->pfnRelease(pRef->pReleaseCtx, pRef->pData, pRef->iLen);
				}
				goto done;
			}
			if ( !__xnetStreamAppendSendRefEx(pStream, pRef, true) ) { iResult = XRT_NET_ERROR; goto done; }
			__xnetStreamKickWrite(pStream);
			iResult = XRT_NET_OK;
			goto done;
		}
		iResult = __xnetStreamPostSendRefEx(pStream, pRef, true);
		goto done;
	}
	if ( pStream->pTls ) {
		iResult = __xnetStreamAppendTlsPlainRefEx(pStream, pRef, true) ? XRT_NET_OK : XRT_NET_ERROR;
		if ( iResult == XRT_NET_OK && pRef->pfnRelease ) {
			pRef->pfnRelease(pRef->pReleaseCtx, pRef->pData, pRef->iLen);
		}
		goto done;
	}
	iResult = __xnetStreamAppendSendRefEx(pStream, pRef, true) ? XRT_NET_OK : XRT_NET_ERROR;
done:
	__xnetStreamEndSendSubmission(pStream);
	return iResult;
}


// 发送网络流 ref
XXAPI xnet_result xrtNetStreamSendRef(xnetstream* pStream, const xnetbufref* pRef)
{
	return __xnetStreamSendRefEx(pStream, pRef, false);
}


static bool __xnetStreamCurrentThreadIsEngineWorker(xnetstream* pStream)
{
	if ( !pStream || !pStream->pEngine ) { return false; }
	for ( uint32 i = 0; i < pStream->pEngine->iWorkerCount; ++i ) {
		if ( __xnetEngineIsCurrentWorker(&pStream->pEngine->arrWorkers[i]) ) { return true; }
	}
	return false;
}


static bool __xnetStreamPostReadControl(xnetstream* pStream, uint32 iType)
{
	__xnet_stream_async_op* pOp;
	xsem hDone = NULL;
	xnet_result iPostResult;
	bool bWait;
	if ( !pStream || !pStream->pEngine || !pStream->pWorker ) { return false; }
	if ( __xnetEngineIsCurrentWorker(pStream->pWorker) ) {
		if ( iType == __XNET_STREAM_ASYNC_PAUSE_READ ) { __xnetStreamApplyPauseRead(pStream); }
		else { __xnetStreamApplyResumeRead(pStream); }
		return true;
	}
	bWait = !__xnetStreamCurrentThreadIsEngineWorker(pStream);
	if ( bWait ) {
		hDone = xrtSemCreate(0u, 1u);
		if ( !hDone ) { return false; }
	}
	pOp = __xnetStreamAllocAsyncSimple(pStream, iType);
	if ( !pOp ) {
		if ( hDone ) { xrtSemDestroy(hDone); }
		return false;
	}
	pOp->hDone = hDone;
	iPostResult = __xnetStreamPostAsync(pStream, pOp);
	if ( iPostResult != XRT_NET_OK ) {
		if ( hDone ) { xrtSemDestroy(hDone); }
		return false;
	}
	if ( hDone ) {
		xrtSemWait(hDone);
		xrtSemDestroy(hDone);
	}
	return true;
}


// 读取网络流 pause
XXAPI void xrtNetStreamPauseRead(xnetstream* pStream)
{
	if ( !pStream ) { return; }
	(void)__xnetStreamPostReadControl(pStream, __XNET_STREAM_ASYNC_PAUSE_READ);
}


// 读取网络流 resume
XXAPI void xrtNetStreamResumeRead(xnetstream* pStream)
{
	if ( !pStream ) { return; }
	(void)__xnetStreamPostReadControl(pStream, __XNET_STREAM_ASYNC_RESUME_READ);
}


// 发送网络流 pending
XXAPI size_t xrtNetStreamPendingSend(const xnetstream* pStream)
{
	return pStream ? (size_t)__xnetStreamReservedSendBytes(pStream) : 0;
}


// xrtNetStreamLocalAddr 相关处理
XXAPI const xnetaddr* xrtNetStreamLocalAddr(const xnetstream* pStream)
{
	return pStream ? &pStream->tLocalAddr : NULL;
}


// xrtNetStreamRemoteAddr 相关处理
XXAPI const xnetaddr* xrtNetStreamRemoteAddr(const xnetstream* pStream)
{
	return pStream ? &pStream->tRemoteAddr : NULL;
}


// 设置网络流 user 数据
XXAPI void xrtNetStreamSetUserData(xnetstream* pStream, ptr pData)
{
	if ( !pStream ) { return; }
	pStream->pUserData = pData;
}


// 获取网络流 user 数据
XXAPI ptr xrtNetStreamGetUserData(xnetstream* pStream)
{
	return pStream ? pStream->pUserData : NULL;
}


// 内部函数：__xnetStreamOnPortEvents
static void __xnetStreamOnPortEvents(xnetworker* pWorker, const xnetportevent* pEvents, uint32 iCount)
{
	(void)pWorker;
	if ( !pEvents || iCount == 0 ) { return; }
	for ( uint32 i = 0; i < iCount; ++i ) {
		const xnetportevent* pEvent = &pEvents[i];
		// IOCP 原生接受事件（未标记为 ACCEPTED_OPEN 的非 connect 事件）
		if ( pEvent->iType == XNET_PORT_EVENT_ACCEPT &&
			(pEvent->iFlags & XNET_PORT_EVENT_F_ACCEPTED_OPEN) == 0 &&
			pEvent->iOpId != 0 ) {
			xnetlistener* pListener = (xnetlistener*)pEvent->pUserData;
			if ( pListener ) {
				if ( __xnetAtomicLoad32(&pListener->iAcceptArmedCount) > 0 ) {
					(void)__xnetAtomicAddFetch32(&pListener->iAcceptArmedCount, -1);
				}
				if ( pEvent->iStatus == XRT_NET_OK && __xnetSocketIsValid((xsocket)pEvent->hSocket) ) {
					// IOCP 返回了已接受的套接字
					__xnetListenerHandleAcceptedSocketEvent(pListener, (xsocket)pEvent->hSocket);
				} else {
					// 接受失败或无套接字，走普通 accept 路径
					__xnetListenerHandleAcceptEvent(pListener);
				}
			} else if ( pEvent->iStatus == XRT_NET_OK && __xnetSocketIsValid((xsocket)pEvent->hSocket) ) {
				#if defined(XRT_INTERNAL_TEST_ENV)
					fprintf(stderr, "[XNET ACCEPT] stale completion, expected=%llu, actual=%llu\n",
						0ull,
						(unsigned long long)pEvent->iOpId);
				#endif
				// 操作 ID 不匹配（过期的监听器），关闭陈旧套接字
				xsocket hStaleSocket = (xsocket)pEvent->hSocket;
				__xnetSocketCloseHandle(&hStaleSocket);
			}
			__xnetListenerReleaseAsyncHold(pListener);
		} else if ( pEvent->iType == XNET_PORT_EVENT_CONNECT || pEvent->iType == XNET_PORT_EVENT_ACCEPT ) {
			// 连接完成或原生接受的 open 事件
			xnetstream* pStream = (xnetstream*)pEvent->pUserData;
			if ( pStream ) {
				if ( pEvent->iType == XNET_PORT_EVENT_CONNECT &&
					pEvent->iOpId != pStream->iConnectOpId ) {
					__xnetStreamReleaseAsyncHold(pStream);
					continue;
				}
				if ( pEvent->iStatus != XRT_NET_OK ) {
					if ( pEvent->iType == XNET_PORT_EVENT_CONNECT &&
						pStream->pConnectAddrList &&
						pStream->iConnectAddrIndex < xrtNetAddrListCount(pStream->pConnectAddrList) ) {
						xsocket hFailed = pStream->hSocket;
						if ( __xnetSocketIsValid(hFailed) ) {
							(void)__xnetStreamSubmitSocketNotice(pStream, XNET_PORT_OP_CLOSE, hFailed);
							__xnetSocketCloseHandle(&pStream->hSocket);
						}
						if ( __xnetStreamBeginNextResolvedConnect(pStream) == XRT_NET_OK ) {
							__xnetStreamReleaseAsyncHold(pStream);
							continue;
						}
					}
					// 连接失败
					if ( pStream->pEvents && pStream->pEvents->OnError ) {
						pStream->pEvents->OnError(__xnetStreamOwner(pStream), pStream, -1);
					}
					xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
				} else {
					pStream->iConnectOpId = 0u;
					if ( pStream->pConnectAddrList ) {
						xrtNetAddrListDestroy(pStream->pConnectAddrList);
						pStream->pConnectAddrList = NULL;
						pStream->iConnectAddrIndex = 0u;
					}
					// 连接成功，更新地址信息
					if ( __xnetSocketIsValid(pStream->hSocket) ) {
						(void)__xnetSocketUpdateLocalAddr(pStream->hSocket, &pStream->tLocalAddr);
						(void)__xnetSocketUpdateRemoteAddr(pStream->hSocket, &pStream->tRemoteAddr);
					}
					// 根据配置进入不同流程：代理 -> TLS -> 直接打开
					if ( pStream->pProxyState ) {
						(void)__xnetStreamDriveProxyState(pStream, NULL, 0u);
					} else if ( pStream->pTls ) {
						(void)__xnetStreamDriveTlsHandshake(pStream);
					} else {
						if ( !pStream->bClosing &&
							__xnetSocketIsValid(pStream->hSocket) && !__xnetStreamRecvArmed(pStream) ) {
							(void)__xnetStreamArmRecvWatch(pStream);
						}
						__xnetStreamEmitOpen(pStream);
						if ( (__xnetStreamRecvArmed(pStream) || __xnetStreamDrainSocketNow(pStream)) && !pStream->bClosing &&
							__xnetSocketIsValid(pStream->hSocket) && !__xnetStreamRecvArmed(pStream) ) {
							(void)__xnetStreamArmRecvWatch(pStream);
						}
					}
				}
			}
			__xnetStreamReleaseAsyncHold(pStream);
		} else if ( pEvent->iType == XNET_PORT_EVENT_SEND ) {
			// 发送完成事件
			xnetstream* pStream = (xnetstream*)pEvent->pUserData;
			__xnetStreamHandleSendEvent(pStream, pEvent);
			__xnetStreamReleaseAsyncHold(pStream);
		} else if ( pEvent->iType == XNET_PORT_EVENT_RECV ) {
			// 接收完成事件
			xnetstream* pStream = (xnetstream*)pEvent->pUserData;
			if ( pStream ) {
				__xnetStreamClearRecvArmed(pStream);
			}
			#if defined(XNET_DEBUG_IOCP_NATIVE)
				if ( pStream && __xnetStreamUseNativePortIO(pStream) ) {
					fprintf(stderr, "[STREAM] recv event stream=%llu bytes=%u chain=%p status=%d flags=0x%x\n",
						(unsigned long long)pStream->iId, pEvent->iBytes, (void*)pEvent->pChain, (int)pEvent->iStatus, pEvent->iFlags);
				}
			#endif
			if ( pEvent->pChain ) {
				// 有接收数据，交给接收事件处理器
				if ( (pEvent->iFlags & XNET_PORT_EVENT_F_DEFERRED_RECV) == 0 &&
					pStream && __xnetStreamPostDeferredRecvChain(pStream, pEvent->pChain) ) {
					/* Ownership of pChain moved to the deferred recv task. */
				} else {
					__xnetStreamHandleRecvEvent(pStream, pEvent->pChain);
				}
			} else if ( pStream ) {
				// 无接收数据，根据事件状态处理
				#if defined(XNET_DEBUG_CLOSE_DIAG)
					if ( pEvent->iStatus == XRT_NET_CLOSED || (pEvent->iFlags & XNET_PORT_EVENT_F_EOF) != 0 ) {
						fprintf(stderr, "[CLOSE_DIAG][STREAM] recv eof stream=%llu tls=%d closing=%d flags=0x%x socket=%p\n",
							(unsigned long long)pStream->iId,
							pStream->pTls ? 1 : 0,
							pStream->bClosing ? 1 : 0,
							(unsigned)pStream->iFlags,
							(void*)(uintptr_t)pStream->hSocket);
					}
				#endif
				// 对端关闭连接或 EOF
				if ( pEvent->iStatus == XRT_NET_CLOSED || (pEvent->iFlags & XNET_PORT_EVENT_F_EOF) != 0 ) {
					__xnetStreamFinishClose(pStream, XRT_NET_CLOSED);
				} else if ( pEvent->iStatus != XRT_NET_OK ) {
					// 接收错误
					if ( pStream->pEvents && pStream->pEvents->OnError ) {
						pStream->pEvents->OnError(__xnetStreamOwner(pStream), pStream, -1);
					}
					xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
				} else if ( pStream->pProxyState ) {
					// 代理模式下零字节接收，重新挂起接收
					(void)__xnetStreamArmRecvWatch(pStream);
				} else if ( pStream->pTls && !__xnetStreamTlsReady(pStream) ) {
					// TLS 握手中，继续驱动
					(void)__xnetStreamDriveTlsHandshake(pStream);
				} else if ( !__xnetStreamReadPaused(pStream) ) {
					// 正常模式且未暂停读取，重新挂起接收
					(void)__xnetStreamArmRecvWatch(pStream);
				}
			}
			__xnetStreamReleaseAsyncHold(pStream);
		} else if ( pEvent->iType == XNET_PORT_EVENT_TIMER ) {
			// 定时器事件（跳过引擎心跳定时器）
			if ( pEvent->iOpId != __XNET_ENGINE_TIMER_PULSE_ID ) {
				xnetstream* pStream = (xnetstream*)(uintptr_t)pEvent->iOpId;
				if ( pStream && __xnetAtomicLoad32(&pStream->iOpenTimerState) != 0 ) {
					__xnetStreamHandleOpenTimer(pStream);
				}
				else if ( pStream && pStream->pEvents && pStream->pEvents->OnTimer ) {
					pStream->pEvents->OnTimer(__xnetStreamOwner(pStream), pStream);
				}
				__xnetStreamReleaseAsyncHold(pStream);
			}
		} else if ( pEvent->iType == XNET_PORT_EVENT_ERROR ) {
			// 端口级错误事件
			xnetstream* pStream = (xnetstream*)pEvent->pUserData;
			if ( pStream && pStream->pEvents && pStream->pEvents->OnError ) {
				pStream->pEvents->OnError(__xnetStreamOwner(pStream), pStream, -1);
			}
		}
	}
}


#endif

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

#define __XNET_STREAM_STATE_INIT           0x00000001u
#define __XNET_STREAM_STATE_CLOSE_EMITTED  0x00000002u
#define __XNET_STREAM_STATE_OPEN_EMITTED   0x00000004u

#define __XNET_STREAM_ASYNC_SEND_COPY      1u
#define __XNET_STREAM_ASYNC_SEND_REF       2u
#define __XNET_STREAM_ASYNC_RECV_COPY      3u
#define __XNET_STREAM_ASYNC_RECV_REF       4u
#define __XNET_STREAM_ASYNC_DISPATCH_RECV  5u
#define __XNET_STREAM_ASYNC_TLS_HANDSHAKE  6u

typedef struct {
	xnetstream* pStream;
	uint32 iType;
	uint32 iLen;
	xnetbufref tRef;
	uint8 aData[1];
} __xnet_stream_async_op;

typedef void (*__xnet_stream_sync_wait_fn)(xnetstream* pStream, xnet_result iStatus, ptr pCtx);

typedef struct {
	__xnet_stream_sync_wait_fn pfnWait;
	ptr pCtx;
} __xnet_stream_wait_slot;

typedef void (*__xnet_listener_sync_wait_fn)(xnetlistener* pListener, xnet_result iStatus, xnetstream* pStream, ptr pCtx);
typedef bool (*__xnet_listener_sync_wait_ready_fn)(ptr pCtx);

typedef struct {
	__xnet_listener_sync_wait_fn pfnWait;
	__xnet_listener_sync_wait_ready_fn pfnCanAccept;
	ptr pCtx;
} __xnet_listener_wait_slot;

typedef struct {
	xsocket hSocket;
	struct sockaddr_storage tStorage;
	socklen_t iAddrLen;
} __xnet_listener_accept_raw;

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
	__xnet_listener_wait_slot tAcceptWait;
	xnetlistenconfig tConfig;
	uint64 iAcceptOpId;
	uint32 iPortCount;
	uint32 iNextWorker;
	volatile long iAsyncHoldCount;
	bool bAcceptArmed;
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
	xnetchain tRxChain;
	__xnet_sendq tSendQ;
	ptr pUserData;
	__xnet_stream_wait_slot arrSyncWait[__XNET_STREAM_WAIT_COUNT];
	uint32 iState;
	uint32 iFlags;
	uint32 iRecvLimit;
	uint32 iConnectTimeoutMs;
	xnet_result iCloseReason;
	volatile long iAsyncHoldCount;
	volatile long iOpenTimerState;
	bool bReadPaused;
	volatile long bRecvArmed;
	bool bSendArmed;
	bool bClosing;
	bool bWriteShutdown;
	bool bTlsCloseQueued;
	bool bDestroyPending;
};

XXAPI void xrtNetStreamDestroy(xnetstream* pStream);
XXAPI void xrtNetStreamClose(xnetstream* pStream, uint32 iFlags);
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
static bool __xnetStreamDriveProxyState(xnetstream* pStream, const void* pData, size_t iLen);
static bool __xnetStreamDriveTlsHandshake(xnetstream* pStream);
static void __xnetStreamDetachProxy(xnetstream* pStream);
static void __xnetStreamEmitOpen(xnetstream* pStream);
static void __xnetStreamHandleRecvEvent(xnetstream* pStream, xnetchain* pChain);
static void __xnetStreamHandleOpenTimer(xnetstream* pStream);
static bool __xnetListenerRegisterSyncAcceptWait(xnetlistener* pListener, __xnet_listener_sync_wait_fn pfnWait, __xnet_listener_sync_wait_ready_fn pfnCanAccept, ptr pCtx);
static bool __xnetListenerCancelSyncAcceptWait(xnetlistener* pListener, ptr pCtx);



/* ============================== Internal helpers ============================== */

static xnetworker* __xnetStreamPickWorker(xnetengine* pEngine, xnetlistener* pListener)
{
	uint32 iIndex;
	uint64 iNextId;
	if ( !pEngine || pEngine->iWorkerCount == 0 ) return NULL;
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
	if ( !pEngine || pEngine->iWorkerCount == 0 ) return NULL;
	if ( pCfg && pCfg->tBindAddr.iPort > 0 ) {
		iIndex = pCfg->tBindAddr.iPort % pEngine->iWorkerCount;
	}
	return &pEngine->arrWorkers[iIndex];
}


// 内部函数：__xnetStreamInitQueues
static void __xnetStreamInitQueues(xnetstream* pStream, xnetworker* pWorker)
{
	xnetmemctx* pMemCtx = pWorker ? &pWorker->tMemCtx : NULL;
	if ( !pStream ) return;
	xrtNetChainInitEx(&pStream->tRxChain, pMemCtx);
	xrtNetChainInitEx(&pStream->tSendQ.tQueue, pMemCtx);
}


// 内部函数：绑定流引擎
static void __xnetStreamBindEngine(xnetengine* pEngine)
{
	if ( !pEngine ) return;
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
	#if defined(XXRTL_CORE)
		xrtSetError((str)sError, FALSE);
	#else
		(void)sError;
	#endif
}


// 内部函数：异步增加流 hold
static void __xnetStreamAddAsyncHold(xnetstream* pStream)
{
	if ( !pStream ) return;
	(void)__xnetAtomicAddFetch32(&pStream->iAsyncHoldCount, 1);
}


// 内部函数：异步释放流 hold
static void __xnetStreamReleaseAsyncHold(xnetstream* pStream)
{
	if ( !pStream ) return;
	if ( __xnetAtomicAddFetch32(&pStream->iAsyncHoldCount, -1) == 0 && pStream->bDestroyPending ) {
		__xnetStreamNotifyDestroyWaiters(pStream);
		xrtNetChainClear(&pStream->tRxChain);
		xrtNetChainClear(&pStream->tSendQ.tQueue);
		__xnetStreamFinalizeSocketClose(pStream);
		__xnetStreamDetachTls(pStream);
		__xnetStreamDetachProxy(pStream);
		XNET_FREE(pStream);
	}
}


// 内部函数：__xnetListenerFinalizeDestroy
static void __xnetListenerFinalizeDestroy(xnetlistener* pListener)
{
	if ( !pListener ) return;
	pListener->bRunning = false;
	pListener->bAcceptArmed = false;
	pListener->bDestroyPending = false;
	pListener->iAcceptOpId = 0;
	pListener->tAcceptWait.pfnWait = NULL;
	pListener->tAcceptWait.pfnCanAccept = NULL;
	pListener->tAcceptWait.pCtx = NULL;
	pListener->pEvents = NULL;
	pListener->pStreamEvents = NULL;
	pListener->pUserData = NULL;
	__xnetSocketCloseHandle(&pListener->hSocket);
	XNET_FREE(pListener);
}


// 内部函数：异步增加监听器 hold
static void __xnetListenerAddAsyncHold(xnetlistener* pListener)
{
	if ( !pListener ) return;
	(void)__xnetAtomicAddFetch32(&pListener->iAsyncHoldCount, 1);
}


// 内部函数：__xnetListenerPrepareDeferredDestroy
static void __xnetListenerPrepareDeferredDestroy(xnetlistener* pListener)
{
	if ( !pListener || pListener->bDestroyPending ) return;
	pListener->bDestroyPending = true;
	pListener->pEvents = NULL;
	pListener->pStreamEvents = NULL;
	pListener->pUserData = NULL;
}


// 内部函数：异步释放监听器 hold
static void __xnetListenerReleaseAsyncHold(xnetlistener* pListener)
{
	if ( !pListener ) return;
	if ( __xnetAtomicAddFetch32(&pListener->iAsyncHoldCount, -1) == 0 && pListener->bDestroyPending ) {
		__xnetListenerFinalizeDestroy(pListener);
	}
}


// 内部函数：__xnetListenerCanDispatchAccept
static bool __xnetListenerCanDispatchAccept(xnetlistener* pListener)
{
	if ( !pListener || pListener->tAcceptWait.pfnWait == NULL ) return false;
	if ( pListener->tAcceptWait.pfnCanAccept && !pListener->tAcceptWait.pfnCanAccept(pListener->tAcceptWait.pCtx) ) {
		return false;
	}
	return true;
}


// 内部函数：__xnetListenerNextAcceptOpId
static uint64 __xnetListenerNextAcceptOpId(xnetlistener* pListener)
{
	uint64 iOpId = 0;
	if ( !pListener || !pListener->pEngine ) return 0;
	iOpId = __xnetEngineAllocStreamId(pListener->pEngine);
	return iOpId;
}


// 内部函数：接受监听器 notify 同步
static void __xnetListenerNotifySyncAccept(xnetlistener* pListener, xnet_result iStatus, xnetstream* pStream)
{
	__xnet_listener_sync_wait_fn pfnWait = NULL;
	ptr pCtx = NULL;

	if ( !pListener || pListener->tAcceptWait.pfnWait == NULL ) return;
	pfnWait = pListener->tAcceptWait.pfnWait;
	pCtx = pListener->tAcceptWait.pCtx;
	pListener->tAcceptWait.pfnWait = NULL;
	pListener->tAcceptWait.pfnCanAccept = NULL;
	pListener->tAcceptWait.pCtx = NULL;
	pfnWait(pListener, iStatus, pStream, pCtx);
}


// 内部函数：__xnetStreamGetSyncWaitSlot
static __xnet_stream_wait_slot* __xnetStreamGetSyncWaitSlot(xnetstream* pStream, uint32 iWaitKind)
{
	if ( !pStream || iWaitKind >= __XNET_STREAM_WAIT_COUNT ) return NULL;
	return &pStream->arrSyncWait[iWaitKind];
}


// 内部函数：__xnetStreamResolveSyncWaitNow
static bool __xnetStreamResolveSyncWaitNow(xnetstream* pStream, uint32 iWaitKind, xnet_result* pStatus)
{
	if ( !pStream || !pStatus ) return false;

	switch ( iWaitKind ) {
		case __XNET_STREAM_WAIT_READABLE:
			if ( (pStream->iState & __XNET_STREAM_STATE_CLOSE_EMITTED) != 0 ) {
				*pStatus = pStream->iCloseReason;
				return true;
			}
			if ( xrtNetChainBytes(&pStream->tRxChain) > 0 ) {
				*pStatus = XRT_NET_OK;
				return true;
			}
			return false;

		case __XNET_STREAM_WAIT_WRITABLE:
			if ( (pStream->iState & __XNET_STREAM_STATE_CLOSE_EMITTED) != 0 ) {
				*pStatus = pStream->iCloseReason;
				return true;
			}
			if ( pStream->bClosing ) {
				*pStatus = XRT_NET_CLOSED;
				return true;
			}
			if ( __xnetStreamHasPreOpenGate(pStream) ) {
				return false;
			}
			if ( !pStream->tSendQ.bHighWaterHit || pStream->tSendQ.iQueuedBytes <= pStream->tSendQ.iLowWater ) {
				*pStatus = XRT_NET_OK;
				return true;
			}
			return false;

		case __XNET_STREAM_WAIT_DRAIN:
			if ( (pStream->iState & __XNET_STREAM_STATE_CLOSE_EMITTED) != 0 ) {
				*pStatus = XRT_NET_CLOSED;
				return true;
			}
			if ( pStream->tSendQ.iQueuedBytes == 0 ) {
				*pStatus = pStream->bClosing ? XRT_NET_CLOSED : XRT_NET_OK;
				return true;
			}
			return false;

		case __XNET_STREAM_WAIT_CLOSE:
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

	pSlot = __xnetStreamGetSyncWaitSlot(pStream, iWaitKind);
	if ( pSlot == NULL || pSlot->pfnWait == NULL ) return;

	pfnWait = pSlot->pfnWait;
	pCtx = pSlot->pCtx;
	pSlot->pfnWait = NULL;
	pSlot->pCtx = NULL;
	pfnWait(pStream, iStatus, pCtx);
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
	if ( !pStream || pStream->iCloseReason == XRT_NET_AGAIN ) return XRT_NET_CLOSED;
	return pStream->iCloseReason;
}


// 内部函数：__xnetStreamNotifyDestroyWaiters
static void __xnetStreamNotifyDestroyWaiters(xnetstream* pStream)
{
	xnet_result iReason = __xnetStreamDestroyStatus(pStream);
	if ( !pStream ) return;
	__xnetStreamNotifySyncReadable(pStream, iReason);
	__xnetStreamNotifySyncWritable(pStream, iReason);
	__xnetStreamNotifySyncDrain(pStream, XRT_NET_CLOSED);
	__xnetStreamNotifySyncClose(pStream, iReason);
}


// 内部函数：__xnetStreamPrepareDeferredDestroy
static void __xnetStreamPrepareDeferredDestroy(xnetstream* pStream)
{
	if ( !pStream || pStream->bDestroyPending ) return;
	pStream->bDestroyPending = true;
	pStream->pEvents = NULL;
	pStream->pUserData = NULL;
	__xnetStreamNotifyDestroyWaiters(pStream);
}


// 内部函数：__xnetStreamAbandonUnownedAccepted
static void __xnetStreamAbandonUnownedAccepted(xnetstream* pStream)
{
	if ( !pStream ) return;
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

	if ( !pStream || !pfnWait ) return false;
	if ( pStream->pWorker && !__xnetEngineIsCurrentWorker(pStream->pWorker) ) return false;
	if ( iWaitKind == __XNET_STREAM_WAIT_READABLE && !pStream->bReadPaused ) return false;

	pSlot = __xnetStreamGetSyncWaitSlot(pStream, iWaitKind);
	if ( pSlot == NULL || pSlot->pfnWait != NULL ) return false;

	if ( __xnetStreamResolveSyncWaitNow(pStream, iWaitKind, &iImmediateStatus) ) {
		pfnWait(pStream, iImmediateStatus, pCtx);
		return true;
	}

	pSlot->pfnWait = pfnWait;
	pSlot->pCtx = pCtx;
	return true;
}


// 内部函数：等待流 cancel 同步
static bool __xnetStreamCancelSyncWait(xnetstream* pStream, uint32 iWaitKind, ptr pCtx)
{
	__xnet_stream_wait_slot* pSlot = NULL;

	if ( !pStream ) return false;
	if ( pStream->pWorker && !__xnetEngineIsCurrentWorker(pStream->pWorker) ) return false;

	pSlot = __xnetStreamGetSyncWaitSlot(pStream, iWaitKind);
	if ( pSlot == NULL ) return false;
	if ( pSlot->pfnWait == NULL || pSlot->pCtx != pCtx ) return false;

	pSlot->pfnWait = NULL;
	pSlot->pCtx = NULL;
	return true;
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
	if ( !__xnetSocketIsValid(hSocket) ) return 0;
	#if defined(_WIN32) || defined(_WIN64)
		u_long iAvail = 0;
		if ( ioctlsocket(hSocket, FIONREAD, &iAvail) != 0 ) return 0;
		return (uint32)iAvail;
	#else
		int iAvail = 0;
		if ( ioctl(hSocket, FIONREAD, &iAvail) != 0 || iAvail <= 0 ) return 0;
		return (uint32)iAvail;
	#endif
}


// 内部函数：__xnetSocketCloseHandle
static void __xnetSocketCloseHandle(xsocket* phSocket)
{
	if ( !phSocket || !__xnetSocketIsValid(*phSocket) ) return;
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
	if ( !__xnetSocketIsValid(hSocket) ) return false;
	#if defined(_WIN32) || defined(_WIN64)
		if ( shutdown(hSocket, SD_SEND) == 0 ) return true;
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
		if ( shutdown(hSocket, SHUT_WR) == 0 ) return true;
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
	if ( !__xnetSocketIsValid(hSocket) ) return false;
	#if defined(_WIN32) || defined(_WIN64)
		if ( shutdown(hSocket, SD_BOTH) == 0 ) return true;
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
		if ( shutdown(hSocket, SHUT_RDWR) == 0 ) return true;
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
	if ( !__xnetSocketIsValid(hSocket) ) return false;
	#if defined(_WIN32) || defined(_WIN64)
		u_long iMode = bEnable ? 1u : 0u;
		return ioctlsocket(hSocket, FIONBIO, &iMode) == 0;
	#else
		int iFlags = fcntl(hSocket, F_GETFL, 0);
		if ( iFlags < 0 ) return false;
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
			pStream->pWorker->tPort.pOps == xrtNetPortUringOps() &&
			__xnetPortUringHasNativeRing(&pStream->pWorker->tPort) &&
			__xnetSocketIsValid(pStream->hSocket);
	#else
		(void)pStream;
		return false;
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
			pStream->pWorker->tPort.pOps == xrtNetPortUringOps() &&
			__xnetPortUringHasNativeRing(&pStream->pWorker->tPort);
	#else
		(void)pStream;
		return false;
	#endif
}


// 内部函数：提交监听器套接字 notice
static bool __xnetListenerSubmitSocketNotice(xnetlistener* pListener, uint16 iOpType, xsocket hSocket)
{
	xnetportsubmit tSubmit;
	if ( !pListener || !pListener->pWorker || !__xnetSocketIsValid(hSocket) ) return false;
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
	if ( !pListener || !pListener->bRunning || !__xnetSocketIsValid(pListener->hSocket) ) return false;
	if ( pListener->bAcceptArmed ) return true;
	iOpId = __xnetListenerNextAcceptOpId(pListener);
	if ( iOpId == 0 ) return false;
	pListener->iAcceptOpId = iOpId;
	__xnetListenerAddAsyncHold(pListener);
	if ( !__xnetListenerSubmitSocketNotice(pListener, XNET_PORT_OP_ACCEPT, pListener->hSocket) ) {
		__xnetListenerReleaseAsyncHold(pListener);
		return false;
	}
	pListener->bAcceptArmed = true;
	if ( pListener->pWorker && !__xnetEngineIsCurrentWorker(pListener->pWorker) ) {
		(void)xrtNetPortWake(&pListener->pWorker->tPort);
	}
	return true;
}


// 内部函数：__xnetListenerCancelAcceptWatch
static bool __xnetListenerCancelAcceptWatch(xnetlistener* pListener)
{
	if ( !pListener ) return false;
	if ( !pListener->bAcceptArmed ) return true;
	pListener->bAcceptArmed = false;
	pListener->iAcceptOpId = 0;
	return true;
}


// 内部函数：__xnetSocketSetReuseAddr
static bool __xnetSocketSetReuseAddr(xsocket hSocket)
{
	int iOpt = 1;
	if ( !__xnetSocketIsValid(hSocket) ) return false;
	#if defined(_WIN32) || defined(_WIN64)
		return setsockopt(hSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&iOpt, sizeof(iOpt)) == 0;
	#else
		return setsockopt(hSocket, SOL_SOCKET, SO_REUSEADDR, &iOpt, sizeof(iOpt)) == 0;
	#endif
}


// 内部函数：设置套接字 reuse 端口
static bool __xnetSocketSetReusePort(xsocket hSocket)
{
	#if defined(SO_REUSEPORT)
		int iOpt = 1;
		if ( !__xnetSocketIsValid(hSocket) ) return false;
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
	if ( !__xnetSocketIsValid(hSocket) ) return false;
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
	if ( !__xnetSocketIsValid(hSocket) ) return false;
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
	if ( !__xnetSocketIsValid(hSocket) || !pAddr ) return false;
	memset(&tStorage, 0, sizeof(tStorage));
	if ( getsockname(hSocket, (struct sockaddr*)&tStorage, &iLen) != 0 ) return false;
	return __xnetAddrFromSockAddr(pAddr, (const struct sockaddr*)&tStorage);
}


// 内部函数：__xnetSocketUpdateRemoteAddr
static bool __xnetSocketUpdateRemoteAddr(xsocket hSocket, xnetaddr* pAddr)
{
	struct sockaddr_storage tStorage;
	socklen_t iLen = (socklen_t)sizeof(tStorage);
	if ( !__xnetSocketIsValid(hSocket) || !pAddr ) return false;
	memset(&tStorage, 0, sizeof(tStorage));
	if ( getpeername(hSocket, (struct sockaddr*)&tStorage, &iLen) != 0 ) return false;
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
	if ( !pStream ) return;
	if ( iHighWater == 0 ) iHighWater = 1;
	if ( iLowWater > iHighWater ) iLowWater = iHighWater;
	pStream->tSendQ.iHighWater = iHighWater;
	pStream->tSendQ.iLowWater = iLowWater;
}


// 内部函数：__xnetStreamApplyDefaults
static void __xnetStreamApplyDefaults(xnetstream* pStream, const xnetconnectconfig* pConnectCfg, const xnetlistenconfig* pListenCfg)
{
	uint32 iHighWater;
	uint32 iLowWater;
	uint32 iRecvLimit;

	if ( !pStream || !pStream->pEngine ) return;

	iHighWater = pStream->pEngine->tConfig.iDefaultHighWater;
	iLowWater = pStream->pEngine->tConfig.iDefaultLowWater;
	iRecvLimit = 1048576u;

	if ( pListenCfg ) {
		if ( pListenCfg->iHighWater > 0 ) iHighWater = pListenCfg->iHighWater;
		if ( pListenCfg->iLowWater <= iHighWater ) iLowWater = pListenCfg->iLowWater;
		if ( pListenCfg->iRecvLimit > 0 ) iRecvLimit = pListenCfg->iRecvLimit;
	}
	if ( pConnectCfg ) {
		if ( pConnectCfg->iHighWater > 0 ) iHighWater = pConnectCfg->iHighWater;
		if ( pConnectCfg->iLowWater <= iHighWater ) iLowWater = pConnectCfg->iLowWater;
		if ( pConnectCfg->iRecvLimit > 0 ) iRecvLimit = pConnectCfg->iRecvLimit;
	}

	__xnetStreamApplyWatermark(pStream, iHighWater, iLowWater);
	pStream->iRecvLimit = iRecvLimit;
	pStream->iConnectTimeoutMs = pConnectCfg ? pConnectCfg->iConnectTimeoutMs : 0u;
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
	if ( !pStream || !pStream->pWorker || iTimeoutMs == 0u ) return false;
	if ( __xnetAtomicCompareExchange32(&pStream->iOpenTimerState, 1, 0) != 0 ) return false;
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
	if ( !pStream || !pStream->pWorker ) return;
	if ( __xnetAtomicCompareExchange32(&pStream->iOpenTimerState, 0, 1) != 1 ) return;
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
	if ( !pStream ) return false;
	if ( !pCfg ) return true;
	pStream->pTls = xrtNetTlsSessionCreate(pCfg, bIsServer);
	return pStream->pTls != NULL;
}


// 内部函数：__xnetStreamDetachTls
static void __xnetStreamDetachTls(xnetstream* pStream)
{
	if ( !pStream || !pStream->pTls ) return;
	xrtNetTlsSessionDestroy(pStream->pTls);
	pStream->pTls = NULL;
	pStream->bTlsCloseQueued = false;
}


// 内部函数：清除流代理状态
static void __xnetStreamClearProxyState(xnetstream* pStream)
{
	if ( !pStream || !pStream->pProxyState ) return;
	__xnetProxyStateDestroy(pStream->pProxyState);
	pStream->pProxyState = NULL;
}


// 内部函数：__xnetStreamDetachProxy
static void __xnetStreamDetachProxy(xnetstream* pStream)
{
	if ( !pStream ) return;
	__xnetStreamClearProxyState(pStream);
	if ( pStream->pProxy ) {
		xrtNetProxyRelease(pStream->pProxy);
		pStream->pProxy = NULL;
	}
}


// 内部函数：__xnetStreamTlsReady
static bool __xnetStreamTlsReady(const xnetstream* pStream)
{
	return pStream && pStream->pTls ? xrtNetTlsSessionIsReady(pStream->pTls) : false;
}


// 内部函数：__xnetStreamEmitClose
static void __xnetStreamEmitClose(xnetstream* pStream, xnet_result iReason)
{
	if ( !pStream || (pStream->iState & __XNET_STREAM_STATE_CLOSE_EMITTED) != 0 ) return;
	pStream->iState |= __XNET_STREAM_STATE_CLOSE_EMITTED;
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
	if ( !pStream ) return;

	iQueuedBytes = xrtNetChainBytes(&pStream->tSendQ.tQueue);
	pStream->tSendQ.iQueuedBytes = iQueuedBytes > UINT32_MAX ? UINT32_MAX : (uint32)iQueuedBytes;

	if ( !bPrevHighWater && pStream->tSendQ.iHighWater > 0 && pStream->tSendQ.iQueuedBytes >= pStream->tSendQ.iHighWater ) {
		pStream->tSendQ.bHighWaterHit = true;
		if ( pStream->pEvents && pStream->pEvents->OnHighWater ) {
			pStream->pEvents->OnHighWater(__xnetStreamOwner(pStream), pStream, pStream->tSendQ.iQueuedBytes);
		}
	} else if ( bPrevHighWater && pStream->tSendQ.iQueuedBytes <= pStream->tSendQ.iLowWater ) {
		pStream->tSendQ.bHighWaterHit = false;
		__xnetStreamNotifySyncWritable(pStream, XRT_NET_OK);
		if ( pStream->pEvents && pStream->pEvents->OnLowWater ) {
			pStream->pEvents->OnLowWater(__xnetStreamOwner(pStream), pStream, pStream->tSendQ.iQueuedBytes);
		}
	}

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

	if ( !pStream || iLen == 0 ) return 0;

	iQueuedBytes = xrtNetChainBytes(&pStream->tSendQ.tQueue);
	if ( iQueuedBytes == 0 ) {
		if ( pStream->bClosing ) {
			__xnetStreamEmitClose(pStream, XRT_NET_CLOSED);
		}
		return 0;
	}
	if ( iLen > iQueuedBytes ) iLen = iQueuedBytes;

	iPrevQueuedBytes = pStream->tSendQ.iQueuedBytes;
	bPrevHighWater = pStream->tSendQ.bHighWaterHit;
	xrtNetChainConsume(&pStream->tSendQ.tQueue, iLen);
	__xnetStreamRefreshSendState(pStream, iPrevQueuedBytes, bPrevHighWater);
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
	if ( !pStream || !pStream->pWorker ) return NULL;
	pChain = (xnetchain*)XNET_ALLOC(sizeof(xnetchain));
	if ( !pChain ) return NULL;
	pMemCtx = &pStream->pWorker->tMemCtx;
	xrtNetChainInitEx(pChain, pMemCtx);
	return pChain;
}


// 内部函数：释放流临时 chain
static void __xnetStreamFreeTempChain(xnetchain* pChain)
{
	if ( !pChain ) return;
	xrtNetChainClear(pChain);
	XNET_FREE(pChain);
}


// 内部函数：__xnetStreamAppendSendCopy
static bool __xnetStreamAppendSendCopy(xnetstream* pStream, const void* pData, size_t iLen)
{
	uint32 iPrevQueuedBytes;
	bool bPrevHighWater;
	if ( !pStream || !pData || iLen == 0 ) return false;
	iPrevQueuedBytes = pStream->tSendQ.iQueuedBytes;
	bPrevHighWater = pStream->tSendQ.bHighWaterHit;
	if ( !__xnetStreamQueueCopy(pStream, pData, iLen) ) return false;
	__xnetStreamRefreshSendState(pStream, iPrevQueuedBytes, bPrevHighWater);
	return true;
}


// 内部函数：__xnetStreamAppendSendVec
static bool __xnetStreamAppendSendVec(xnetstream* pStream, const xnetspan* pVec, uint32 iCount)
{
	xnetchain tTemp;
	uint32 iPrevQueuedBytes;
	bool bPrevHighWater;
	if ( !pStream || !pVec || iCount == 0 ) return false;

	xrtNetChainInitEx(&tTemp, (xnetmemctx*)pStream->tSendQ.tQueue.pMemCtx);
	for ( uint32 i = 0; i < iCount; ++i ) {
		if ( !pVec[i].pData || pVec[i].iLen == 0 || !xrtNetChainAppendCopy(&tTemp, pVec[i].pData, pVec[i].iLen) ) {
			xrtNetChainClear(&tTemp);
			return false;
		}
	}

	iPrevQueuedBytes = pStream->tSendQ.iQueuedBytes;
	bPrevHighWater = pStream->tSendQ.bHighWaterHit;
	__xnetChainSplice(&pStream->tSendQ.tQueue, &tTemp);
	__xnetStreamRefreshSendState(pStream, iPrevQueuedBytes, bPrevHighWater);
	return true;
}


// 内部函数：__xnetStreamAppendSendRef
static bool __xnetStreamAppendSendRef(xnetstream* pStream, const xnetbufref* pRef)
{
	uint32 iPrevQueuedBytes;
	bool bPrevHighWater;
	if ( !pStream || !pRef ) return false;
	iPrevQueuedBytes = pStream->tSendQ.iQueuedBytes;
	bPrevHighWater = pStream->tSendQ.bHighWaterHit;
	if ( !__xnetStreamQueueRef(pStream, pRef) ) return false;
	__xnetStreamRefreshSendState(pStream, iPrevQueuedBytes, bPrevHighWater);
	return true;
}


// 内部函数：__xnetStreamAppendRecvCopy
static bool __xnetStreamAppendRecvCopy(xnetstream* pStream, const void* pData, size_t iLen)
{
	bool bOk;
	if ( !pStream || !pData || iLen == 0 ) return false;
	if ( pStream->iRecvLimit > 0 && xrtNetChainBytes(&pStream->tRxChain) + iLen > pStream->iRecvLimit ) {
		if ( pStream->pEvents && pStream->pEvents->OnError ) {
			pStream->pEvents->OnError(__xnetStreamOwner(pStream), pStream, -1);
		}
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return false;
	}
	bOk = xrtNetChainAppendCopy(&pStream->tRxChain, pData, iLen);
	if ( bOk ) {
		__xnetStreamNotifySyncReadable(pStream, XRT_NET_OK);
	}
	return bOk;
}


// 内部函数：__xnetStreamAppendRecvRef
static bool UNUSED_ATTR __xnetStreamAppendRecvRef(xnetstream* pStream, const xnetbufref* pRef)
{
	bool bOk;
	if ( !pStream || !pRef ) return false;
	if ( pStream->iRecvLimit > 0 && xrtNetChainBytes(&pStream->tRxChain) + pRef->iLen > pStream->iRecvLimit ) {
		if ( pStream->pEvents && pStream->pEvents->OnError ) {
			pStream->pEvents->OnError(__xnetStreamOwner(pStream), pStream, -1);
		}
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return false;
	}
	bOk = xrtNetChainAppendRef(&pStream->tRxChain, pRef);
	if ( bOk ) {
		__xnetStreamNotifySyncReadable(pStream, XRT_NET_OK);
	}
	return bOk;
}


// 内部函数：__xnetStreamDispatchRecv
static void __xnetStreamDispatchRecv(xnetstream* pStream)
{
	if ( !pStream || pStream->bReadPaused ) return;
	if ( xrtNetChainBytes(&pStream->tRxChain) == 0 ) return;
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
	if ( !pStream || !pStream->pProxy || !pStream->pProxyState ) return true;

	if ( pStream->pProxyState->iStage == __XNET_PROXY_STAGE_INIT ) {
		iAction = __xnetProxyStateBegin(pStream->pProxyState, pStream->pProxy, aSend, sizeof(aSend), &iSendLen);
	} else {
		iAction = __xnetProxyStateFeed(pStream->pProxyState, pStream->pProxy, pData, iLen, aSend, sizeof(aSend), &iSendLen);
	}

	if ( iAction == __XNET_PROXY_ACTION_WAIT ) {
		return true;
	}
	if ( iAction == __XNET_PROXY_ACTION_SEND ) {
		if ( iSendLen == 0 || !__xnetStreamAppendSendCopy(pStream, aSend, iSendLen) ) {
			iAction = __XNET_PROXY_ACTION_ERROR;
		} else {
			__xnetStreamKickWrite(pStream);
			return true;
		}
	}
	if ( iAction == __XNET_PROXY_ACTION_READY ) {
		if ( pStream->pProxyState->iRecvLen > 0 ) {
			pCarry = __xnetStreamAllocTempChain(pStream);
			if ( !pCarry || !xrtNetChainAppendCopy(pCarry, pStream->pProxyState->aRecv, pStream->pProxyState->iRecvLen) ) {
				__xnetStreamFreeTempChain(pCarry);
				iAction = __XNET_PROXY_ACTION_ERROR;
			}
		}
		if ( iAction == __XNET_PROXY_ACTION_READY ) {
			__xnetStreamClearProxyState(pStream);
			if ( pStream->pTls ) {
				if ( !__xnetStreamDriveTlsHandshake(pStream) ) {
					__xnetStreamFreeTempChain(pCarry);
					return false;
				}
				if ( pCarry ) {
					__xnetStreamHandleRecvEvent(pStream, pCarry);
				}
			} else {
				__xnetStreamEmitOpen(pStream);
				if ( pCarry ) {
					__xnetStreamHandleRecvEvent(pStream, pCarry);
				} else if ( !pStream->bClosing && __xnetSocketIsValid(pStream->hSocket) && !__xnetStreamRecvArmed(pStream) ) {
					(void)__xnetStreamArmRecvWatch(pStream);
				}
			}
			return true;
		}
	}
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
	if ( !pStream || !pOut || iMaxCount == 0 ) return 0;
	return xrtNetChainGetSpans(&pStream->tSendQ.tQueue, pOut, iMaxCount);
}


// 内部函数：__xnetStreamTryBeginWrite
static bool __xnetStreamTryBeginWrite(xnetstream* pStream, xnetspan* pOut, uint32 iMaxCount, uint32* pSpanCount)
{
	uint32 iCount;
	if ( pSpanCount ) *pSpanCount = 0;
	if ( !pStream || !pOut || iMaxCount == 0 ) return false;
	if ( pStream->tSendQ.bWritePosted || pStream->tSendQ.iQueuedBytes == 0 ) return false;
	iCount = __xnetStreamBuildSendSpans(pStream, pOut, iMaxCount);
	if ( iCount == 0 ) return false;
	pStream->tSendQ.bWritePosted = true;
	if ( pSpanCount ) *pSpanCount = iCount;
	return true;
}


// 内部函数：__xnetStreamSubmitWrite
static bool __xnetStreamSubmitWrite(xnetstream* pStream)
{
	xnetspan arrVec[16];
	xnetportsubmit tSubmit;
	uint32 iSpanCount = 0;
	if ( !pStream || !pStream->pWorker ) return false;
	if ( !__xnetStreamTryBeginWrite(pStream, arrVec, 16, &iSpanCount) ) return false;
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
	if ( !pStream ) return;
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


// 内部函数：流队列 TLS cipher相关处理
static bool __xnetStreamQueueTlsCipher(xnetstream* pStream)
{
	char aBuf[4096];
	if ( !pStream || !pStream->pTls ) return false;
	while ( xrtNetTlsSessionPendingCipher(pStream->pTls) > 0 ) {
		size_t iRead = 0;
		if ( xrtNetTlsSessionPeekCipher(pStream->pTls, aBuf, sizeof(aBuf), &iRead) != XRT_NET_OK || iRead == 0 ) {
			return false;
		}
		if ( !__xnetStreamAppendSendCopy(pStream, aBuf, iRead) ) {
			return false;
		}
		xrtNetTlsSessionConsumeCipher(pStream->pTls, iRead);
	}
	return true;
}


// 内部函数：__xnetStreamDrainTlsPlain
static bool __xnetStreamDrainTlsPlain(xnetstream* pStream)
{
	char aBuf[4096];
	bool bReadAny = false;
	if ( !pStream || !pStream->pTls ) return false;
	for ( ;; ) {
		size_t iRead = 0;
		xnet_result iRes = xrtNetTlsSessionReadPlain(pStream->pTls, aBuf, sizeof(aBuf), &iRead);
		if ( iRes == XRT_NET_OK && iRead > 0 ) {
			if ( !__xnetStreamAppendRecvCopy(pStream, aBuf, iRead) ) {
				return false;
			}
			bReadAny = true;
			continue;
		}
		if ( iRes == XRT_NET_AGAIN ) break;
		if ( iRes == XRT_NET_CLOSED ) {
			if ( pStream->bClosing && (pStream->iFlags & XNET_CLOSE_F_ABORT) == 0 ) {
				__xnetStreamFinishClose(pStream, XRT_NET_CLOSED);
			} else {
				xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
			}
			return false;
		}
		if ( iRes != XRT_NET_OK ) {
			if ( pStream->pEvents && pStream->pEvents->OnError ) {
				pStream->pEvents->OnError(__xnetStreamOwner(pStream), pStream, -1);
			}
			xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
			return false;
		}
		break;
	}
	if ( bReadAny ) {
		if ( !pStream->bReadPaused ) {
			__xnetStreamDispatchRecv(pStream);
		}
	}
	return true;
}


// 内部函数：__xnetStreamDriveTlsHandshake
static bool __xnetStreamDriveTlsHandshake(xnetstream* pStream)
{
	xnet_result iRes = XRT_NET_AGAIN;
	uint32 iSpin = 0;
	if ( !pStream || !pStream->pTls || !__xnetSocketIsValid(pStream->hSocket) ) return false;
	if ( pStream->pProxyState ) return true;
	#ifdef DEBUG_TRACE
		printf("    [XNET_TLS] drive stream=%llu server=%d queued=%u ready=%d\n",
			(unsigned long long)pStream->iId,
			pStream->pTls->bIsServer ? 1 : 0,
			pStream->tSendQ.iQueuedBytes,
			__xnetStreamTlsReady(pStream) ? 1 : 0);
	#endif
	if ( pStream->tSendQ.iQueuedBytes > 0 ) {
		__xnetStreamKickWrite(pStream);
		return true;
	}

	for ( iSpin = 0; iSpin < 8; ++iSpin ) {
		iRes = xrtNetTlsSessionDriveHandshake(pStream->pTls);
		#ifdef DEBUG_TRACE
			printf("    [XNET_TLS] step stream=%llu res=%d pendingCipher=%u pendingRecv=%u readable=%u\n",
				(unsigned long long)pStream->iId,
				(int)iRes,
				(uint32)xrtNetTlsSessionPendingCipher(pStream->pTls),
				(uint32)xrtNetTlsSessionPendingRecv(pStream->pTls),
				__xnetSocketBytesAvailable(pStream->hSocket));
		#endif
		if ( !__xnetStreamQueueTlsCipher(pStream) ) {
			if ( pStream->pEvents && pStream->pEvents->OnError ) {
				pStream->pEvents->OnError(__xnetStreamOwner(pStream), pStream, -1);
			}
			xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
			return false;
		}
		if ( pStream->tSendQ.iQueuedBytes > 0 ) {
			__xnetStreamKickWrite(pStream);
			break;
		}
		if ( __xnetStreamTlsReady(pStream) ) break;
		if ( iRes != XRT_NET_AGAIN ) break;
		if ( xrtNetTlsSessionPendingRecv(pStream->pTls) == 0 ) break;
	}
	if ( __xnetStreamTlsReady(pStream) ) {
		__xnetStreamEmitOpen(pStream);
		(void)__xnetStreamDrainTlsPlain(pStream);
	}
	if ( !pStream->bClosing && __xnetSocketIsValid(pStream->hSocket) ) {
		(void)__xnetStreamArmRecvWatch(pStream);
	}
	if ( iRes == XRT_NET_OK || iRes == XRT_NET_AGAIN ) return true;
	if ( pStream->pEvents && pStream->pEvents->OnError ) {
		pStream->pEvents->OnError(__xnetStreamOwner(pStream), pStream, -1);
	}
	xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
	return false;
}


// 内部函数：__xnetStreamAppendTlsPlainCopy
static bool __xnetStreamAppendTlsPlainCopy(xnetstream* pStream, const void* pData, size_t iLen)
{
	size_t iWritten = 0;
	if ( !pStream || !pStream->pTls || !__xnetStreamTlsReady(pStream) || !pData || iLen == 0 ) return false;
	if ( xrtNetTlsSessionWritePlain(pStream->pTls, pData, iLen, &iWritten) != XRT_NET_OK || iWritten != iLen ) return false;
	if ( !__xnetStreamQueueTlsCipher(pStream) ) return false;
	__xnetStreamKickWrite(pStream);
	return true;
}


// 内部函数：__xnetStreamAppendTlsPlainVec
static bool __xnetStreamAppendTlsPlainVec(xnetstream* pStream, const xnetspan* pVec, uint32 iCount)
{
	if ( !pStream || !pVec || iCount == 0 ) return false;
	for ( uint32 i = 0; i < iCount; ++i ) {
		if ( !pVec[i].pData || pVec[i].iLen == 0 ) return false;
		if ( !__xnetStreamAppendTlsPlainCopy(pStream, pVec[i].pData, pVec[i].iLen) ) return false;
	}
	return true;
}


// 内部函数：__xnetStreamAppendTlsPlainRef
static bool __xnetStreamAppendTlsPlainRef(xnetstream* pStream, const xnetbufref* pRef)
{
	if ( !pStream || !pRef || !pRef->pData || pRef->iLen == 0 ) return false;
	return __xnetStreamAppendTlsPlainCopy(pStream, pRef->pData, pRef->iLen);
}


// 内部函数：__xnetStreamEmitOpen
static void __xnetStreamEmitOpen(xnetstream* pStream)
{
	if ( !pStream || (pStream->iState & __XNET_STREAM_STATE_OPEN_EMITTED) != 0 ) return;
	__xnetStreamCancelOpenTimer(pStream);
	pStream->iState |= __XNET_STREAM_STATE_OPEN_EMITTED;
	if ( pStream->pEvents && pStream->pEvents->OnOpen ) {
		pStream->pEvents->OnOpen(__xnetStreamOwner(pStream), pStream);
	}
}


// 内部函数：打开流 submit 事件
static bool __xnetStreamSubmitOpenEvent(xnetstream* pStream, uint16 iOpType)
{
	xnetportsubmit tSubmit;
	if ( !pStream || !pStream->pWorker ) return false;
	memset(&tSubmit, 0, sizeof(tSubmit));
	tSubmit.iOpType = iOpType;
	if ( iOpType == XNET_PORT_OP_ACCEPT ) {
		tSubmit.iFlags = XNET_PORT_EVENT_F_ACCEPTED_OPEN;
	}
	tSubmit.iOpId = pStream->iId;
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
	if ( !pStream || !pStream->pWorker || !__xnetSocketIsValid(hSocket) ) return false;
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
	if ( !pStream || pStream->bClosing || !__xnetSocketIsValid(pStream->hSocket) ) return false;
	if ( __xnetAtomicCompareExchange32(&pStream->bRecvArmed, 1, 0) != 0 ) return false;
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
	if ( !pStream || pStream->bSendArmed || pStream->tSendQ.iQueuedBytes == 0 || !__xnetSocketIsValid(pStream->hSocket) ) return false;
	if ( !__xnetStreamSubmitSocketNotice(pStream, XNET_PORT_OP_SEND, pStream->hSocket) ) return false;
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
	if ( !pStream ) return;
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
	if ( !pStream ) return;
	__xnetStreamCancelOpenTimer(pStream);
	__xnetStreamClearProxyState(pStream);
	__xnetStreamFinalizeSocketClose(pStream);
	__xnetStreamNotifySyncReadable(pStream, iReason);
	__xnetStreamNotifySyncDrain(pStream, iReason);
	__xnetStreamDetachTls(pStream);
	__xnetStreamEmitClose(pStream, iReason);
}


// 内部函数：打开流 handle 定时器
static void __xnetStreamHandleOpenTimer(xnetstream* pStream)
{
	if ( !pStream ) return;
	if ( __xnetAtomicCompareExchange32(&pStream->iOpenTimerState, 0, 1) != 1 ) return;
	if ( (pStream->iState & (__XNET_STREAM_STATE_OPEN_EMITTED | __XNET_STREAM_STATE_CLOSE_EMITTED)) != 0 ) return;
	pStream->bClosing = true;
	pStream->iFlags = XNET_CLOSE_F_ABORT;
	xrtNetChainClear(&pStream->tRxChain);
	xrtNetChainClear(&pStream->tSendQ.tQueue);
	pStream->tSendQ.iQueuedBytes = 0;
	pStream->tSendQ.bHighWaterHit = false;
	__xnetStreamFinishClose(pStream, XRT_NET_TIMEOUT);
}


// 内部函数：__xnetStreamBeginGracefulCloseWait
static void __xnetStreamBeginGracefulCloseWait(xnetstream* pStream)
{
	if ( !pStream ) return;
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
	if ( !pStream ) return 0;
	pStream->tSendQ.bWritePosted = false;
	iBytes = __xnetStreamConsumeSendQueue(pStream, iBytes);
	bNeedResubmit = pStream->tSendQ.iQueuedBytes > 0 && !pStream->tSendQ.bWritePosted &&
		__xnetSocketIsValid(pStream->hSocket);
	if ( bNeedResubmit ) {
		__xnetStreamKickWrite(pStream);
	}
	return iBytes;
}


// 内部函数：__xnetStreamSubmitRecvChain
static bool __xnetStreamSubmitRecvChain(xnetstream* pStream, xnetchain* pChain)
{
	xnetportsubmit tSubmit;
	if ( !pStream || !pStream->pWorker || !pChain ) return false;
	memset(&tSubmit, 0, sizeof(tSubmit));
	tSubmit.iOpType = XNET_PORT_OP_RECV;
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


// 内部函数：__xnetStreamFeedTlsChain
static bool __xnetStreamFeedTlsChain(xnetstream* pStream, xnetchain* pChain)
{
	xnetspan arrSpan[16];
	uint32 iSpanCount;

	if ( !pStream || !pStream->pTls || !pChain ) return false;
	iSpanCount = xrtNetChainGetSpans(pChain, arrSpan, 16);
	for ( uint32 i = 0; i < iSpanCount; ++i ) {
		if ( arrSpan[i].iLen == 0 ) continue;
		if ( xrtNetTlsSessionFeedCipher(pStream->pTls, arrSpan[i].pData, arrSpan[i].iLen) != XRT_NET_OK ) {
			return false;
		}
	}
	return true;
}


// 内部函数：接收流 handle 事件
static void __xnetStreamHandleRecvEvent(xnetstream* pStream, xnetchain* pChain)
{
	if ( !pStream || !pChain ) {
		__xnetStreamFreeTempChain(pChain);
		return;
	}
	if ( pStream->pProxyState ) {
		xnetspan arrSpan[16];
		uint32 iSpanCount = xrtNetChainGetSpans(pChain, arrSpan, 16);
		for ( uint32 i = 0; i < iSpanCount; ++i ) {
			xnetchain* pRemainder = NULL;
			if ( arrSpan[i].iLen == 0 ) continue;
			if ( !pStream->pProxyState ) {
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
				for ( uint32 j = i + 1u; j < iSpanCount; ++j ) {
					if ( arrSpan[j].iLen == 0 ) continue;
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
				__xnetStreamHandleRecvEvent(pStream, pRemainder);
				return;
			}
			if ( !__xnetStreamDriveProxyState(pStream, arrSpan[i].pData, arrSpan[i].iLen) ) {
				__xnetStreamFreeTempChain(pChain);
				return;
			}
		}
		__xnetStreamFreeTempChain(pChain);
		if ( pStream->pProxyState && !pStream->bClosing && __xnetSocketIsValid(pStream->hSocket) && !__xnetStreamRecvArmed(pStream) ) {
			(void)__xnetStreamArmRecvWatch(pStream);
		}
		return;
	}
	if ( pStream->iRecvLimit > 0 && xrtNetChainBytes(&pStream->tRxChain) + xrtNetChainBytes(pChain) > pStream->iRecvLimit ) {
		if ( pStream->pEvents && pStream->pEvents->OnError ) {
			pStream->pEvents->OnError(__xnetStreamOwner(pStream), pStream, -1);
		}
		__xnetStreamFreeTempChain(pChain);
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		return;
	}
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
			(void)__xnetStreamDriveTlsHandshake(pStream);
		} else {
			(void)__xnetStreamDrainTlsPlain(pStream);
			if ( !pStream->bClosing && !pStream->bReadPaused &&
				__xnetSocketIsValid(pStream->hSocket) && !__xnetStreamRecvArmed(pStream) ) {
				(void)__xnetStreamArmRecvWatch(pStream);
			}
		}
		return;
	}
	__xnetChainSplice(&pStream->tRxChain, pChain);
	__xnetStreamFreeTempChain(pChain);
	__xnetStreamNotifySyncReadable(pStream, XRT_NET_OK);
	if ( pStream->bReadPaused ) {
		return;
	}
	__xnetStreamDispatchRecv(pStream);
	if ( !pStream->bClosing && __xnetStreamUseNativePortIO(pStream) && !__xnetStreamRecvArmed(pStream) ) {
		(void)__xnetStreamArmRecvWatch(pStream);
	}
}


// 内部函数：发送流 handle 事件
static void __xnetStreamHandleSendEvent(xnetstream* pStream, const xnetportevent* pEvent)
{
	if ( !pStream || !pEvent ) return;
	pStream->bSendArmed = false;
	#if defined(XNET_DEBUG_IOCP_NATIVE)
		if ( __xnetStreamUseNativePortIO(pStream) ) {
			fprintf(stderr, "[STREAM] send event stream=%llu bytes=%u queued=%u status=%d\n",
				(unsigned long long)pStream->iId, pEvent->iBytes, pStream->tSendQ.iQueuedBytes, (int)pEvent->iStatus);
		}
	#endif
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
	if ( pEvent->iBytes == 0 && __xnetSocketIsValid(pStream->hSocket) ) {
		__xnetStreamKickWrite(pStream);
		return;
	}
	(void)__xnetStreamCompleteWrite(pStream, pEvent->iBytes);
}


// 内部函数：流异步任务相关处理
static void __xnetStreamAsyncTask(xnetworker* pWorker, ptr pArg)
{
	__xnet_stream_async_op* pOp = (__xnet_stream_async_op*)pArg;
	xnetstream* pStream = pOp ? pOp->pStream : NULL;
	(void)pWorker;
	if ( !pOp || !pStream ) {
		if ( pOp ) XNET_FREE(pOp);
		return;
	}
	if ( pStream->bClosing && pOp->iType != __XNET_STREAM_ASYNC_DISPATCH_RECV ) {
		XNET_FREE(pOp);
		__xnetStreamReleaseAsyncHold(pStream);
		return;
	}

	switch ( pOp->iType ) {
		case __XNET_STREAM_ASYNC_SEND_COPY:
			if ( pStream->pTls ) {
				(void)__xnetStreamAppendTlsPlainCopy(pStream, pOp->aData, pOp->iLen);
			} else if ( __xnetStreamAppendSendCopy(pStream, pOp->aData, pOp->iLen) ) {
				__xnetStreamKickWrite(pStream);
			}
			break;
		case __XNET_STREAM_ASYNC_SEND_REF:
			if ( pStream->pTls ) {
				(void)__xnetStreamAppendTlsPlainRef(pStream, &pOp->tRef);
			} else if ( __xnetStreamAppendSendRef(pStream, &pOp->tRef) ) {
				__xnetStreamKickWrite(pStream);
			}
			break;
		case __XNET_STREAM_ASYNC_RECV_COPY:
			{
				xnetchain* pChain = __xnetStreamAllocTempChain(pStream);
				if ( pChain && xrtNetChainAppendCopy(pChain, pOp->aData, pOp->iLen) && __xnetStreamSubmitRecvChain(pStream, pChain) ) {
					pChain = NULL;
				}
				__xnetStreamFreeTempChain(pChain);
			}
			break;
		case __XNET_STREAM_ASYNC_RECV_REF:
			{
				xnetchain* pChain = __xnetStreamAllocTempChain(pStream);
				if ( pChain && xrtNetChainAppendRef(pChain, &pOp->tRef) && __xnetStreamSubmitRecvChain(pStream, pChain) ) {
					pChain = NULL;
				}
				__xnetStreamFreeTempChain(pChain);
			}
			break;
		case __XNET_STREAM_ASYNC_DISPATCH_RECV:
			__xnetStreamDispatchRecv(pStream);
			if ( pStream && !pStream->bReadPaused && !pStream->bClosing &&
				xrtNetChainBytes(&pStream->tRxChain) == 0 &&
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
			(void)__xnetStreamDriveTlsHandshake(pStream);
			break;
		default:
			break;
	}
	XNET_FREE(pOp);
	__xnetStreamReleaseAsyncHold(pStream);
}


// 内部函数：异步复制流 alloc
static __xnet_stream_async_op* __xnetStreamAllocAsyncCopy(xnetstream* pStream, uint32 iType, const void* pData, size_t iLen)
{
	__xnet_stream_async_op* pOp;
	size_t iSize;
	if ( !pStream || !pData || iLen == 0 || iLen > UINT32_MAX ) return NULL;
	iSize = sizeof(__xnet_stream_async_op) + iLen;
	pOp = (__xnet_stream_async_op*)XNET_ALLOC(iSize);
	if ( !pOp ) return NULL;
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
	if ( !pStream || !pRef || !pRef->pData || pRef->iLen == 0 ) return NULL;
	pOp = (__xnet_stream_async_op*)XNET_ALLOC(sizeof(__xnet_stream_async_op));
	if ( !pOp ) return NULL;
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
	if ( !pStream || !pVec || iCount == 0 ) return NULL;
	for ( uint32 i = 0; i < iCount; ++i ) {
		if ( !pVec[i].pData || pVec[i].iLen == 0 ) return NULL;
		iTotal += pVec[i].iLen;
		if ( iTotal > UINT32_MAX ) return NULL;
	}
	iSize = sizeof(__xnet_stream_async_op) + iTotal;
	pOp = (__xnet_stream_async_op*)XNET_ALLOC(iSize);
	if ( !pOp ) return NULL;
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
	if ( !pStream ) return NULL;
	pOp = (__xnet_stream_async_op*)XNET_ALLOC(sizeof(__xnet_stream_async_op));
	if ( !pOp ) return NULL;
	memset(pOp, 0, sizeof(__xnet_stream_async_op));
	pOp->pStream = pStream;
	pOp->iType = iType;
	return pOp;
}


// 内部函数：__xnetStreamPostAsync
static xnet_result __xnetStreamPostAsync(xnetstream* pStream, __xnet_stream_async_op* pOp)
{
	if ( !pStream || !pOp || !pStream->pEngine || !pStream->pWorker ) {
		if ( pOp ) XNET_FREE(pOp);
		return XRT_NET_ERROR;
	}
	__xnetStreamAddAsyncHold(pStream);
	if ( xrtNetEnginePost(pStream->pEngine, pStream->pWorker->iId, __xnetStreamAsyncTask, pOp) != XRT_NET_OK ) {
		__xnetStreamReleaseAsyncHold(pStream);
		XNET_FREE(pOp);
		return XRT_NET_ERROR;
	}
	return XRT_NET_OK;
}


// 内部函数：__xnetStreamPostSendCopy
static xnet_result __xnetStreamPostSendCopy(xnetstream* pStream, const void* pData, size_t iLen)
{
	return __xnetStreamPostAsync(pStream, __xnetStreamAllocAsyncCopy(pStream, __XNET_STREAM_ASYNC_SEND_COPY, pData, iLen));
}


// 内部函数：__xnetStreamPostSendVec
static xnet_result __xnetStreamPostSendVec(xnetstream* pStream, const xnetspan* pVec, uint32 iCount)
{
	return __xnetStreamPostAsync(pStream, __xnetStreamAllocAsyncVecCopy(pStream, __XNET_STREAM_ASYNC_SEND_COPY, pVec, iCount));
}


// 内部函数：__xnetStreamPostSendRef
static xnet_result __xnetStreamPostSendRef(xnetstream* pStream, const xnetbufref* pRef)
{
	return __xnetStreamPostAsync(pStream, __xnetStreamAllocAsyncRef(pStream, __XNET_STREAM_ASYNC_SEND_REF, pRef));
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
static xnet_result __xnetStreamPostRecvDispatch(xnetstream* pStream)
{
	return __xnetStreamPostAsync(pStream, __xnetStreamAllocAsyncSimple(pStream, __XNET_STREAM_ASYNC_DISPATCH_RECV));
}


// 内部函数：__xnetStreamPostTlsHandshake
static xnet_result __xnetStreamPostTlsHandshake(xnetstream* pStream)
{
	if ( !pStream || !pStream->pTls || !__xnetSocketIsValid(pStream->hSocket) ) return XRT_NET_ERROR;
	return __xnetStreamPostAsync(pStream, __xnetStreamAllocAsyncSimple(pStream, __XNET_STREAM_ASYNC_TLS_HANDSHAKE));
}



/* ============================== Listener helpers ============================== */

XXAPI xnetlistener* xrtNetListenerCreate(xnetengine* pEngine, const xnetlistenconfig* pCfg,
	const xnetlistenerevents* pEvents, const xnetstreamevents* pStreamEvents, ptr pUserData)
{
	xnetlistener* pListener;
	if ( !pEngine ) return NULL;

	pListener = (xnetlistener*)XNET_ALLOC(sizeof(xnetlistener));
	if ( !pListener ) return NULL;
	memset(pListener, 0, sizeof(xnetlistener));
	__xnetStreamBindEngine(pEngine);
	pListener->hSocket = XNET_SOCKET_INVALID;
	pListener->pEngine = pEngine;
	pListener->pEvents = pEvents;
	pListener->pStreamEvents = pStreamEvents;
	pListener->pUserData = pUserData;
	if ( pCfg ) {
		pListener->tConfig = *pCfg;
	} else {
		xrtNetListenConfigInit(&pListener->tConfig);
	}
	pListener->iPortCount = ((pListener->tConfig.iFlags & XNET_LISTEN_F_REUSE_PORT) != 0 && pEngine->iWorkerCount > 0)
		? pEngine->iWorkerCount
		: 1u;
	pListener->pWorker = __xnetListenerPickWorker(pEngine, &pListener->tConfig);
	pListener->iNextWorker = 0;
	return pListener;
}


// 销毁网络监听器
XXAPI void xrtNetListenerDestroy(xnetlistener* pListener)
{
	if ( !pListener ) return;
	if ( pListener->tAcceptWait.pfnWait != NULL ) {
		__xnetStreamSetError("cannot destroy listener while an async waiter or task still holds it.");
		return;
	}
	pListener->bRunning = false;
	(void)__xnetListenerCancelAcceptWatch(pListener);
	__xnetSocketCloseHandle(&pListener->hSocket);
	__xnetListenerNotifySyncAccept(pListener, XRT_NET_CLOSED, NULL);
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
	if ( !pListener || !pListener->pEngine || !pListener->pEngine->bRunning ) return XRT_NET_ERROR;
	if ( pListener->bRunning ) return XRT_NET_OK;
	if ( !__xnetAddrToSockAddr(&pListener->tConfig.tBindAddr, &tStorage, &iAddrLen) ) return XRT_NET_ERROR;
	pListener->hSocket = __xnetSocketCreateStream(pListener->tConfig.tBindAddr.iFamily);
	if ( !__xnetSocketIsValid(pListener->hSocket) ) return XRT_NET_ERROR;
	(void)__xnetSocketApplyListenFlags(pListener->hSocket, pListener->tConfig.iFlags);
	(void)__xnetSocketSetNonBlock(pListener->hSocket, true);
	if ( bind(pListener->hSocket, (struct sockaddr*)&tStorage, iAddrLen) != 0 ) {
		if ( pListener->pEvents && pListener->pEvents->OnError ) {
			pListener->pEvents->OnError(pListener->pUserData, pListener, __xnetSocketLastErr());
		}
		__xnetSocketCloseHandle(&pListener->hSocket);
		return XRT_NET_ERROR;
	}
	if ( listen(pListener->hSocket, (int)(pListener->tConfig.iBacklog > 0 ? pListener->tConfig.iBacklog : 128u)) != 0 ) {
		if ( pListener->pEvents && pListener->pEvents->OnError ) {
			pListener->pEvents->OnError(pListener->pUserData, pListener, __xnetSocketLastErr());
		}
		__xnetSocketCloseHandle(&pListener->hSocket);
		return XRT_NET_ERROR;
	}
	(void)__xnetSocketUpdateLocalAddr(pListener->hSocket, &pListener->tConfig.tBindAddr);
	pListener->pWorker = __xnetListenerPickWorker(pListener->pEngine, &pListener->tConfig);
	pListener->iAcceptOpId = 0;
	pListener->bAcceptArmed = false;
	pListener->bRunning = true;
	return XRT_NET_OK;
}


// 停止网络监听器
XXAPI void xrtNetListenerStop(xnetlistener* pListener)
{
	if ( !pListener ) return;
	pListener->bRunning = false;
	(void)__xnetListenerCancelAcceptWatch(pListener);
	__xnetSocketCloseHandle(&pListener->hSocket);
	__xnetListenerNotifySyncAccept(pListener, XRT_NET_CLOSED, NULL);
}


// 内部函数：创建监听器 accepted 流
static xnetstream* __xnetListenerCreateAcceptedStream(xnetlistener* pListener, ptr pUserData)
{
	xnetstream* pStream;
	xnetworker* pWorker;
	bool bAccepted = true;

	if ( !pListener || !pListener->pEngine ) return NULL;

	pStream = (xnetstream*)XNET_ALLOC(sizeof(xnetstream));
	if ( !pStream ) return NULL;
	memset(pStream, 0, sizeof(xnetstream));
	__xnetStreamBindEngine(pListener->pEngine);
	pStream->hSocket = XNET_SOCKET_INVALID;
	pWorker = __xnetStreamPickWorker(pListener->pEngine, pListener);

	pStream->iId = __xnetEngineAllocStreamId(pListener->pEngine);
	pStream->pEngine = pListener->pEngine;
	pStream->pWorker = pWorker;
	pStream->pListener = pListener;
	pStream->pEvents = pListener->pStreamEvents;
	pStream->pUserData = pUserData;
	pStream->iState = __XNET_STREAM_STATE_INIT;
	pStream->iCloseReason = XRT_NET_AGAIN;
	__xnetStreamInitQueues(pStream, pWorker);
	__xnetStreamApplyDefaults(pStream, NULL, &pListener->tConfig);
	if ( !__xnetStreamAttachTls(pStream, pListener->tConfig.pTlsConfig, true) ) {
		xrtNetStreamDestroy(pStream);
		return NULL;
	}

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
	if ( pSysErr ) *pSysErr = 0;
	if ( pRaw ) {
		memset(pRaw, 0, sizeof(*pRaw));
		pRaw->hSocket = XNET_SOCKET_INVALID;
		pRaw->iAddrLen = (socklen_t)sizeof(pRaw->tStorage);
	}
	if ( !pListener || !pRaw || !pListener->bRunning || !__xnetSocketIsValid(pListener->hSocket) ) {
		if ( pSysErr ) *pSysErr = __XNET_LISTENER_ACCEPT_SYS_CLOSED;
		return false;
	}

	pRaw->hSocket = accept(pListener->hSocket, (struct sockaddr*)&pRaw->tStorage, &pRaw->iAddrLen);
	if ( !__xnetSocketIsValid(pRaw->hSocket) ) {
		int iErr = __xnetSocketLastErr();
		if ( pSysErr ) {
			*pSysErr = __xnetSocketWouldBlock(iErr) ? 0 : iErr;
		}
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
	if ( !pListener || !pRaw || !__xnetSocketIsValid(pRaw->hSocket) ) return NULL;

	pStream = __xnetListenerCreateAcceptedStream(pListener, pUserData);
	if ( !pStream ) {
		return NULL;
	}
	pStream->hSocket = pRaw->hSocket;
	(void)__xnetSocketSetNonBlock(pStream->hSocket, false);
	if ( (pListener->tConfig.iFlags & XNET_LISTEN_F_NO_DELAY) != 0 ) {
		(void)__xnetSocketSetNoDelay(pStream->hSocket);
	}
	if ( (pListener->tConfig.iFlags & XNET_LISTEN_F_KEEPALIVE) != 0 ) {
		(void)__xnetSocketSetKeepAlive(pStream->hSocket);
	}
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
	(void)__xnetSocketUpdateLocalAddr(pStream->hSocket, &pStream->tLocalAddr);
	if ( pStream->pTls ) {
		if ( __xnetStreamPostTlsHandshake(pStream) != XRT_NET_OK ) {
			(void)__xnetStreamDriveTlsHandshake(pStream);
		}
	} else if ( !__xnetStreamSubmitOpenEvent(pStream, XNET_PORT_OP_ACCEPT) ) {
		__xnetStreamEmitOpen(pStream);
		(void)__xnetStreamArmRecvWatch(pStream);
	}
	return pStream;
}


// 内部函数：等待监听器 register 同步接受
static bool __xnetListenerRegisterSyncAcceptWait(xnetlistener* pListener, __xnet_listener_sync_wait_fn pfnWait, __xnet_listener_sync_wait_ready_fn pfnCanAccept, ptr pCtx)
{
	__xnet_listener_accept_raw tRaw;
	int iSysErr = 0;
	xnetstream* pStream = NULL;

	if ( !pListener || !pfnWait ) return false;
	if ( pListener->pWorker && !__xnetEngineIsCurrentWorker(pListener->pWorker) ) return false;
	if ( pListener->tAcceptWait.pfnWait != NULL ) return false;

	pListener->tAcceptWait.pfnWait = pfnWait;
	pListener->tAcceptWait.pfnCanAccept = pfnCanAccept;
	pListener->tAcceptWait.pCtx = pCtx;

	if ( !pListener->bRunning || !__xnetSocketIsValid(pListener->hSocket) ) {
		__xnetListenerNotifySyncAccept(pListener, XRT_NET_CLOSED, NULL);
		return true;
	}

	memset(&tRaw, 0, sizeof(tRaw));
	tRaw.hSocket = XNET_SOCKET_INVALID;
	if ( __xnetListenerAcceptSocket(pListener, &tRaw, &iSysErr) ) {
		if ( !__xnetListenerCanDispatchAccept(pListener) ) {
			__xnetSocketCloseHandle(&tRaw.hSocket);
			return true;
		}
		pStream = __xnetListenerWrapAcceptedSocket(pListener, &tRaw, NULL);
		if ( pStream ) {
			__xnetListenerNotifySyncAccept(pListener, XRT_NET_OK, pStream);
			return true;
		}
		__xnetSocketCloseHandle(&tRaw.hSocket);
	}

	if ( iSysErr == __XNET_LISTENER_ACCEPT_SYS_CLOSED ) {
		__xnetListenerNotifySyncAccept(pListener, XRT_NET_CLOSED, NULL);
		return true;
	}
	if ( iSysErr != 0 ) {
		__xnetListenerNotifySyncAccept(pListener, XRT_NET_ERROR, NULL);
		return true;
	}
	if ( !__xnetListenerCanDispatchAccept(pListener) ) {
		return true;
	}
	if ( !__xnetListenerArmAcceptWatch(pListener) ) {
		__xnetListenerNotifySyncAccept(pListener, XRT_NET_ERROR, NULL);
	}
	return true;
}


// 内部函数：等待监听器 cancel 同步接受
static bool __xnetListenerCancelSyncAcceptWait(xnetlistener* pListener, ptr pCtx)
{
	if ( !pListener ) return false;
	if ( pListener->pWorker && !__xnetEngineIsCurrentWorker(pListener->pWorker) ) return false;
	if ( pListener->tAcceptWait.pfnWait == NULL || pListener->tAcceptWait.pCtx != pCtx ) return false;
	pListener->tAcceptWait.pfnWait = NULL;
	pListener->tAcceptWait.pfnCanAccept = NULL;
	pListener->tAcceptWait.pCtx = NULL;
	return true;
}


// 内部函数：接受监听器 handle 事件
static void __xnetListenerHandleAcceptEvent(xnetlistener* pListener)
{
	__xnet_listener_accept_raw tRaw;
	int iSysErr = 0;
	xnetstream* pStream = NULL;

	if ( !pListener ) return;
	pListener->bAcceptArmed = false;
	if ( !__xnetListenerCanDispatchAccept(pListener) ) return;

	memset(&tRaw, 0, sizeof(tRaw));
	tRaw.hSocket = XNET_SOCKET_INVALID;
	if ( __xnetListenerAcceptSocket(pListener, &tRaw, &iSysErr) ) {
		if ( !__xnetListenerCanDispatchAccept(pListener) ) {
			__xnetSocketCloseHandle(&tRaw.hSocket);
			return;
		}
		pStream = __xnetListenerWrapAcceptedSocket(pListener, &tRaw, NULL);
		if ( pStream ) {
			__xnetListenerNotifySyncAccept(pListener, XRT_NET_OK, pStream);
			return;
		}
		__xnetSocketCloseHandle(&tRaw.hSocket);
		if ( __xnetListenerCanDispatchAccept(pListener) && pListener->bRunning && __xnetSocketIsValid(pListener->hSocket) ) {
			(void)__xnetListenerArmAcceptWatch(pListener);
		}
		return;
	}

	if ( iSysErr == 0 ) {
		if ( __xnetListenerCanDispatchAccept(pListener) && pListener->bRunning && __xnetSocketIsValid(pListener->hSocket) ) {
			(void)__xnetListenerArmAcceptWatch(pListener);
		}
		return;
	}
	if ( iSysErr == __XNET_LISTENER_ACCEPT_SYS_CLOSED ) {
		__xnetListenerNotifySyncAccept(pListener, XRT_NET_CLOSED, NULL);
		return;
	}
	__xnetListenerNotifySyncAccept(pListener, XRT_NET_ERROR, NULL);
}


// 内部函数：处理监听器 accepted 套接字事件
static void __xnetListenerHandleAcceptedSocketEvent(xnetlistener* pListener, xsocket hSocket)
{
	__xnet_listener_accept_raw tRaw;
	xnetstream* pStream = NULL;

	if ( !pListener ) return;
	pListener->bAcceptArmed = false;
	if ( !__xnetSocketIsValid(hSocket) ) {
		__xnetListenerHandleAcceptEvent(pListener);
		return;
	}
	if ( !__xnetListenerCanDispatchAccept(pListener) ) {
		__xnetSocketCloseHandle(&hSocket);
		return;
	}

	memset(&tRaw, 0, sizeof(tRaw));
	tRaw.hSocket = hSocket;
	pStream = __xnetListenerWrapAcceptedSocket(pListener, &tRaw, NULL);
	if ( pStream ) {
		__xnetListenerNotifySyncAccept(pListener, XRT_NET_OK, pStream);
		return;
	}
	__xnetSocketCloseHandle(&hSocket);
	if ( __xnetListenerCanDispatchAccept(pListener) &&
		pListener->bRunning && __xnetSocketIsValid(pListener->hSocket) ) {
		(void)__xnetListenerArmAcceptWatch(pListener);
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
		if ( pSysErr ) *pSysErr = 0;
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
	if ( !pEngine ) return NULL;

	pStream = (xnetstream*)XNET_ALLOC(sizeof(xnetstream));
	if ( !pStream ) return NULL;
	memset(pStream, 0, sizeof(xnetstream));
	__xnetStreamBindEngine(pEngine);
	pStream->hSocket = XNET_SOCKET_INVALID;
	pWorker = __xnetStreamPickWorker(pEngine, NULL);

	pStream->iId = __xnetEngineAllocStreamId(pEngine);
	pStream->pEngine = pEngine;
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
	if ( !pStream ) return;
	if ( __xnetAtomicLoad32(&pStream->iAsyncHoldCount) != 0 ) {
		__xnetStreamSetError("cannot destroy stream while an async waiter or task still holds it.");
		return;
	}
	__xnetStreamNotifyDestroyWaiters(pStream);
	xrtNetChainClear(&pStream->tRxChain);
	xrtNetChainClear(&pStream->tSendQ.tQueue);
	__xnetStreamFinalizeSocketClose(pStream);
	__xnetStreamDetachTls(pStream);
	__xnetStreamDetachProxy(pStream);
	XNET_FREE(pStream);
}


// 连接网络流
XXAPI xnet_result xrtNetStreamConnect(xnetstream* pStream, const xnetconnectconfig* pCfg)
{
	struct sockaddr_storage tStorage;
	socklen_t iAddrLen = 0;
	xsocket hSocket;
	const char* sConnectHost;
	uint16 iConnectPort;
	if ( !pStream || !pStream->pEngine || !pStream->pEngine->bRunning ) return XRT_NET_ERROR;
	__xnetStreamApplyDefaults(pStream, pCfg, NULL);
	if ( __xnetSocketIsValid(pStream->hSocket) ) return XRT_NET_ERROR;

	if ( pCfg && pCfg->pProxy ) {
		if ( !pCfg->sHost || !pCfg->sHost[0] || pCfg->iPort == 0 ) return XRT_NET_ERROR;
		pStream->pProxy = xrtNetProxyAddRef(pCfg->pProxy);
		if ( !pStream->pProxy ) return XRT_NET_ERROR;
		pStream->pProxyState = __xnetProxyStateCreate(pStream->pProxy, pCfg->sHost, pCfg->iPort);
		if ( !pStream->pProxyState ) {
			__xnetStreamDetachProxy(pStream);
			return XRT_NET_ERROR;
		}
		sConnectHost = pStream->pProxy->tConfig.sHost;
		iConnectPort = pStream->pProxy->tConfig.iPort;
	} else {
		sConnectHost = pCfg ? pCfg->sHost : NULL;
		iConnectPort = pCfg ? pCfg->iPort : 0;
	}

	if ( sConnectHost && sConnectHost[0] ) {
		xnetaddr tAddr;
		memset(&tAddr, 0, sizeof(tAddr));
		tAddr.iPort = iConnectPort;
		if ( xrtNetResolve(sConnectHost, &tAddr) != XRT_NET_OK ) {
			__xnetStreamDetachProxy(pStream);
			return XRT_NET_ERROR;
		}
		pStream->tRemoteAddr = tAddr;
	}
	if ( !__xnetAddrToSockAddr(&pStream->tRemoteAddr, &tStorage, &iAddrLen) ) {
		__xnetStreamDetachProxy(pStream);
		return XRT_NET_ERROR;
	}
	hSocket = __xnetSocketCreateStream(pStream->tRemoteAddr.iFamily);
	if ( !__xnetSocketIsValid(hSocket) ) {
		__xnetStreamDetachProxy(pStream);
		return XRT_NET_ERROR;
	}
	(void)__xnetSocketApplyConnectFlags(hSocket, pCfg ? pCfg->iFlags : XNET_CONNECT_F_NONE);
	if ( !__xnetStreamUseNativePortOps(pStream) ) {
		__xnetStreamSetError("mainline stream connect requires a native xnet backend.");
		__xnetSocketCloseHandle(&hSocket);
		__xnetStreamDetachProxy(pStream);
		return XRT_NET_ERROR;
	}
	pStream->hSocket = hSocket;
	(void)__xnetSocketSetNonBlock(pStream->hSocket, false);
	if ( !__xnetStreamAttachTls(pStream, pCfg ? pCfg->pTlsConfig : NULL, false) ) {
		__xnetSocketCloseHandle(&pStream->hSocket);
		__xnetStreamDetachProxy(pStream);
		return XRT_NET_ERROR;
	}
	if ( pStream->iConnectTimeoutMs > 0 && !__xnetStreamArmOpenTimer(pStream, pStream->iConnectTimeoutMs) ) {
		__xnetStreamDetachTls(pStream);
		__xnetSocketCloseHandle(&pStream->hSocket);
		__xnetStreamDetachProxy(pStream);
		return XRT_NET_ERROR;
	}
	if ( !__xnetStreamSubmitOpenEvent(pStream, XNET_PORT_OP_CONNECT) ) {
		__xnetStreamCancelOpenTimer(pStream);
		__xnetStreamDetachTls(pStream);
		__xnetSocketCloseHandle(&pStream->hSocket);
		__xnetStreamDetachProxy(pStream);
		return XRT_NET_ERROR;
	}
	return XRT_NET_OK;
}


// 关闭网络流
XXAPI void xrtNetStreamClose(xnetstream* pStream, uint32 iFlags)
{
	if ( !pStream ) return;
	if ( (pStream->iState & __XNET_STREAM_STATE_CLOSE_EMITTED) != 0 ) return;
	if ( pStream->bClosing ) {
		if ( (iFlags & XNET_CLOSE_F_ABORT) == 0 || (pStream->iFlags & XNET_CLOSE_F_ABORT) != 0 ) return;
		pStream->iFlags |= XNET_CLOSE_F_ABORT;
		xrtNetChainClear(&pStream->tRxChain);
		xrtNetChainClear(&pStream->tSendQ.tQueue);
		pStream->tSendQ.iQueuedBytes = 0;
		pStream->tSendQ.bHighWaterHit = false;
		__xnetStreamFinishClose(pStream, XRT_NET_CLOSED);
		return;
	}
	pStream->bClosing = true;
	pStream->iFlags = iFlags;
	if ( (iFlags & XNET_CLOSE_F_ABORT) != 0 ) {
		xrtNetChainClear(&pStream->tRxChain);
		xrtNetChainClear(&pStream->tSendQ.tQueue);
		pStream->tSendQ.iQueuedBytes = 0;
		pStream->tSendQ.bHighWaterHit = false;
		__xnetStreamFinishClose(pStream, XRT_NET_CLOSED);
		return;
	}
	if ( pStream->pTls && __xnetStreamTlsReady(pStream) && !pStream->bTlsCloseQueued ) {
		pStream->bTlsCloseQueued = true;
		if ( xrtNetTlsSessionQueueClose(pStream->pTls) == XRT_NET_OK ) {
			if ( !__xnetStreamQueueTlsCipher(pStream) ) {
				__xnetStreamFinishClose(pStream, XRT_NET_CLOSED);
				return;
			}
		}
	}
	if ( pStream->tSendQ.iQueuedBytes == 0 ) {
		if ( (pStream->iFlags & XNET_CLOSE_F_WAIT_PEER) != 0 ) {
			__xnetStreamBeginGracefulCloseWait(pStream);
		} else {
			__xnetStreamFinishClose(pStream, XRT_NET_CLOSED);
		}
		return;
	}
	__xnetStreamKickWrite(pStream);
}


// 发送网络流
XXAPI xnet_result xrtNetStreamSend(xnetstream* pStream, const void* pData, size_t iLen)
{
	if ( !pStream || !pStream->pEngine || !pStream->pEngine->bRunning || !pData || iLen == 0 ) return XRT_NET_ERROR;
	if ( pStream->bClosing || (pStream->iState & __XNET_STREAM_STATE_CLOSE_EMITTED) != 0 ) return XRT_NET_CLOSED;
	if ( __xnetStreamHasPreOpenGate(pStream) ) return XRT_NET_AGAIN;
	if ( pStream->pTls && !__xnetStreamTlsReady(pStream) ) return XRT_NET_AGAIN;
	if ( __xnetSocketIsValid(pStream->hSocket) ) {
		if ( __xnetEngineIsCurrentWorker(pStream->pWorker) ) {
			if ( pStream->pTls ) {
				return __xnetStreamAppendTlsPlainCopy(pStream, pData, iLen) ? XRT_NET_OK : XRT_NET_ERROR;
			}
			if ( !__xnetStreamAppendSendCopy(pStream, pData, iLen) ) return XRT_NET_ERROR;
			__xnetStreamKickWrite(pStream);
			return XRT_NET_OK;
		}
		return __xnetStreamPostSendCopy(pStream, pData, iLen);
	}
	if ( pStream->pTls ) {
		return __xnetStreamAppendTlsPlainCopy(pStream, pData, iLen) ? XRT_NET_OK : XRT_NET_ERROR;
	}
	return __xnetStreamAppendSendCopy(pStream, pData, iLen) ? XRT_NET_OK : XRT_NET_ERROR;
}


// 发送网络流 vec
XXAPI xnet_result xrtNetStreamSendVec(xnetstream* pStream, const xnetspan* pVec, uint32 iCount)
{
	if ( !pStream || !pStream->pEngine || !pStream->pEngine->bRunning || !pVec || iCount == 0 ) return XRT_NET_ERROR;
	if ( pStream->bClosing || (pStream->iState & __XNET_STREAM_STATE_CLOSE_EMITTED) != 0 ) return XRT_NET_CLOSED;
	if ( __xnetStreamHasPreOpenGate(pStream) ) return XRT_NET_AGAIN;
	if ( pStream->pTls && !__xnetStreamTlsReady(pStream) ) return XRT_NET_AGAIN;
	if ( __xnetSocketIsValid(pStream->hSocket) ) {
		if ( __xnetEngineIsCurrentWorker(pStream->pWorker) ) {
			if ( pStream->pTls ) {
				return __xnetStreamAppendTlsPlainVec(pStream, pVec, iCount) ? XRT_NET_OK : XRT_NET_ERROR;
			}
			if ( !__xnetStreamAppendSendVec(pStream, pVec, iCount) ) return XRT_NET_ERROR;
			__xnetStreamKickWrite(pStream);
			return XRT_NET_OK;
		}
		return __xnetStreamPostSendVec(pStream, pVec, iCount);
	}
	if ( pStream->pTls ) {
		return __xnetStreamAppendTlsPlainVec(pStream, pVec, iCount) ? XRT_NET_OK : XRT_NET_ERROR;
	}
	return __xnetStreamAppendSendVec(pStream, pVec, iCount) ? XRT_NET_OK : XRT_NET_ERROR;
}


// 发送网络流 ref
XXAPI xnet_result xrtNetStreamSendRef(xnetstream* pStream, const xnetbufref* pRef)
{
	if ( !pStream || !pStream->pEngine || !pStream->pEngine->bRunning || !pRef ) return XRT_NET_ERROR;
	if ( pStream->bClosing || (pStream->iState & __XNET_STREAM_STATE_CLOSE_EMITTED) != 0 ) return XRT_NET_CLOSED;
	if ( __xnetStreamHasPreOpenGate(pStream) ) return XRT_NET_AGAIN;
	if ( pStream->pTls && !__xnetStreamTlsReady(pStream) ) return XRT_NET_AGAIN;
	if ( __xnetSocketIsValid(pStream->hSocket) ) {
		if ( __xnetEngineIsCurrentWorker(pStream->pWorker) ) {
			if ( pStream->pTls ) {
				return __xnetStreamAppendTlsPlainRef(pStream, pRef) ? XRT_NET_OK : XRT_NET_ERROR;
			}
			if ( !__xnetStreamAppendSendRef(pStream, pRef) ) return XRT_NET_ERROR;
			__xnetStreamKickWrite(pStream);
			return XRT_NET_OK;
		}
		return __xnetStreamPostSendRef(pStream, pRef);
	}
	if ( pStream->pTls ) {
		return __xnetStreamAppendTlsPlainRef(pStream, pRef) ? XRT_NET_OK : XRT_NET_ERROR;
	}
	return __xnetStreamAppendSendRef(pStream, pRef) ? XRT_NET_OK : XRT_NET_ERROR;
}


// 读取网络流 pause
XXAPI void xrtNetStreamPauseRead(xnetstream* pStream)
{
	if ( !pStream ) return;
	pStream->bReadPaused = true;
}


// 读取网络流 resume
XXAPI void xrtNetStreamResumeRead(xnetstream* pStream)
{
	if ( !pStream ) return;
	pStream->bReadPaused = false;
	if ( xrtNetChainBytes(&pStream->tRxChain) > 0 ) {
		if ( __xnetStreamPostRecvDispatch(pStream) != XRT_NET_OK ) {
			__xnetStreamDispatchRecv(pStream);
		}
		if ( xrtNetChainBytes(&pStream->tRxChain) > 0 ) {
			return;
		}
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


// 发送网络流 pending
XXAPI size_t xrtNetStreamPendingSend(const xnetstream* pStream)
{
	return pStream ? pStream->tSendQ.iQueuedBytes : 0;
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
	if ( !pStream ) return;
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
	if ( !pEvents || iCount == 0 ) return;
	for ( uint32 i = 0; i < iCount; ++i ) {
		const xnetportevent* pEvent = &pEvents[i];
		if ( pEvent->iType == XNET_PORT_EVENT_ACCEPT &&
			(pEvent->iFlags & XNET_PORT_EVENT_F_ACCEPTED_OPEN) == 0 &&
			pEvent->iOpId != 0 ) {
			xnetlistener* pListener = (xnetlistener*)pEvent->pUserData;
			if ( pListener && pListener->iAcceptOpId == pEvent->iOpId ) {
				if ( pEvent->iStatus == XRT_NET_OK && __xnetSocketIsValid((xsocket)pEvent->hSocket) ) {
					__xnetListenerHandleAcceptedSocketEvent(pListener, (xsocket)pEvent->hSocket);
				} else {
					__xnetListenerHandleAcceptEvent(pListener);
				}
			} else if ( pEvent->iStatus == XRT_NET_OK && __xnetSocketIsValid((xsocket)pEvent->hSocket) ) {
				xsocket hStaleSocket = (xsocket)pEvent->hSocket;
				__xnetSocketCloseHandle(&hStaleSocket);
			}
			__xnetListenerReleaseAsyncHold(pListener);
		} else if ( pEvent->iType == XNET_PORT_EVENT_CONNECT || pEvent->iType == XNET_PORT_EVENT_ACCEPT ) {
			xnetstream* pStream = (xnetstream*)pEvent->pUserData;
			if ( pStream ) {
				if ( pEvent->iStatus != XRT_NET_OK ) {
					if ( pStream->pEvents && pStream->pEvents->OnError ) {
						pStream->pEvents->OnError(__xnetStreamOwner(pStream), pStream, -1);
					}
					xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
				} else {
					if ( __xnetSocketIsValid(pStream->hSocket) ) {
						(void)__xnetSocketUpdateLocalAddr(pStream->hSocket, &pStream->tLocalAddr);
						(void)__xnetSocketUpdateRemoteAddr(pStream->hSocket, &pStream->tRemoteAddr);
					}
					if ( pStream->pProxyState ) {
						(void)__xnetStreamDriveProxyState(pStream, NULL, 0u);
					} else if ( pStream->pTls ) {
						(void)__xnetStreamDriveTlsHandshake(pStream);
					} else {
						__xnetStreamEmitOpen(pStream);
						(void)__xnetStreamArmRecvWatch(pStream);
					}
				}
			}
			__xnetStreamReleaseAsyncHold(pStream);
		} else if ( pEvent->iType == XNET_PORT_EVENT_SEND ) {
			xnetstream* pStream = (xnetstream*)pEvent->pUserData;
			__xnetStreamHandleSendEvent(pStream, pEvent);
			__xnetStreamReleaseAsyncHold(pStream);
		} else if ( pEvent->iType == XNET_PORT_EVENT_RECV ) {
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
				__xnetStreamHandleRecvEvent(pStream, pEvent->pChain);
			} else if ( pStream ) {
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
				if ( pEvent->iStatus == XRT_NET_CLOSED || (pEvent->iFlags & XNET_PORT_EVENT_F_EOF) != 0 ) {
					xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
				} else if ( pEvent->iStatus != XRT_NET_OK ) {
					if ( pStream->pEvents && pStream->pEvents->OnError ) {
						pStream->pEvents->OnError(__xnetStreamOwner(pStream), pStream, -1);
					}
					xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
				} else if ( pStream->pProxyState ) {
					(void)__xnetStreamArmRecvWatch(pStream);
				} else if ( pStream->pTls && !__xnetStreamTlsReady(pStream) ) {
					(void)__xnetStreamDriveTlsHandshake(pStream);
				} else if ( !pStream->bReadPaused ) {
					(void)__xnetStreamArmRecvWatch(pStream);
				}
			}
			__xnetStreamReleaseAsyncHold(pStream);
		} else if ( pEvent->iType == XNET_PORT_EVENT_TIMER ) {
			if ( pEvent->iOpId != __XNET_ENGINE_TIMER_PULSE_ID ) {
				xnetstream* pStream = (xnetstream*)(uintptr_t)pEvent->iOpId;
				__xnetStreamHandleOpenTimer(pStream);
				__xnetStreamReleaseAsyncHold(pStream);
			}
		} else if ( pEvent->iType == XNET_PORT_EVENT_ERROR ) {
			xnetstream* pStream = (xnetstream*)pEvent->pUserData;
			if ( pStream && pStream->pEvents && pStream->pEvents->OnError ) {
				pStream->pEvents->OnError(__xnetStreamOwner(pStream), pStream, -1);
			}
		}
	}
}


#endif

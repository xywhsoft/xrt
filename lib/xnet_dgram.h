#ifndef XRT_XNET_DGRAM_H
#define XRT_XNET_DGRAM_H

#if !defined(XRT_BUILD_CORE)
	typedef struct {
		void (*OnRecv)(ptr pOwner, xdgramsock* pSock, const xnetaddr* pFrom, xnetchain* pChain);
		void (*OnError)(ptr pOwner, xdgramsock* pSock, int iSysErr);
	} xnetdgramevents;
#endif

#define __XNET_DGRAM_ASYNC_SEND_COPY   1u
#define __XNET_DGRAM_ASYNC_SEND_NATIVE 2u
#define __XNET_DGRAM_MAX_PAYLOAD       65507u

/* TCC 的精简 Winsock 头可能省略这两个稳定 ABI 常量。 */
#if defined(_WIN32) || defined(_WIN64)
	#ifndef IPV6_UNICAST_HOPS
		#define IPV6_UNICAST_HOPS 4
	#endif
	#ifndef IPV6_V6ONLY
		#define IPV6_V6ONLY 27
	#endif
#endif

typedef void (*__xnet_dgram_send_done_fn)(xdgramsock* pSock, xnet_result iStatus, ptr pArg);

typedef struct {
	xdgramsock* pSock;
	xnetaddr tTo;
	uint32 iType;
	uint32 iLen;
	uint32 iBudgetBytes;
	__xnet_dgram_send_done_fn pfnDone;
	ptr pDoneArg;
	uint8 aData[1];
} __xnet_dgram_async_op;

#if !defined(XRT_BUILD_CORE)
	typedef struct xrt_net_dgram_batch xnetdgrambatch;
	typedef struct xrt_net_dgram_packet {
		xnetaddr tFrom;
		xnetchain tChain;
		struct xrt_net_dgram_packet* pNext;
	} xnetdgrampkt;
#else
	struct xrt_net_dgram_packet {
		xnetaddr tFrom;
		xnetchain tChain;
		struct xrt_net_dgram_packet* pNext;
};
#endif

struct xrt_net_dgram_batch {
	uint32 iCount;
	uint32 iCapacity;
	xnetdgrampkt* arrPackets[1];
};

typedef void (*__xnet_dgram_sync_wait_fn)(xdgramsock* pSock, xnet_result iStatus, xnetdgrampkt* pPacket, ptr pCtx);

typedef struct __xnet_dgram_wait_slot {
	struct __xnet_dgram_wait_slot* pNext;
	__xnet_dgram_sync_wait_fn pfnWait;
	ptr pCtx;
} __xnet_dgram_wait_slot;

struct xrt_net_dgram {
	uint64 iId;
	xnetengine* pEngine;
	xnetworker* pWorker;
	const xnetdgramevents* pEvents;
	xsocket hSocket;
	xnetaddr tLocalAddr;
	xnetaddr tPeerAddr;
	ptr pUserData;
	uint32 iFlags;
	uint32 iRecvBatch;
	uint32 iSendQueueLimit;
	uint32 iRecvQueueLimit;
	uint32 iRecvOverflowPolicy;
	uint32 iRecvBufferSize;
	uint32 iSendBufferSize;
	int32 iHopLimit;
	int32 iTrafficClass;
	volatile long iRecvQueued;
	xnetdgrampkt* pRecvHead;
	xnetdgrampkt* pRecvTail;
	volatile long iAsyncHoldCount;
	volatile int64 iPendingSendBytes;
	volatile int64 iRecvPackets;
	volatile int64 iRecvBytes;
	volatile int64 iSendPackets;
	volatile int64 iSendBytes;
	volatile int64 iRecvDropped;
	volatile int64 iSendAgain;
	volatile int64 iErrors;
	volatile long iRecvArmed;
	volatile long iIoHoldCount;
	volatile long iDestroyState;
	bool bRunning;
	bool bConnected;
	__xnet_dgram_wait_slot tRecvWait;
};

static void __xnetDgramOnPortEvents(xnetworker* pWorker, const xnetportevent* pEvents, uint32 iCount);
static bool __xnetDgramSocketIsValid(xsocket hSocket);
static void __xnetDgramTryFinalizeDestroy(xdgramsock* pSock);


// 内部函数：__xnetDgramPickWorker
static xnetworker* __xnetDgramPickWorker(xnetengine* pEngine, const xnetdgramconfig* pCfg)
{
	uint32 iIndex = 0;
	if ( !pEngine || pEngine->iWorkerCount == 0 ) { return NULL; }
	if ( pCfg && pCfg->tBindAddr.iPort > 0 ) {
		iIndex = pCfg->tBindAddr.iPort % pEngine->iWorkerCount;
	}
	return &pEngine->arrWorkers[iIndex];
}


// 内部函数：绑定数据报引擎
static void __xnetDgramBindEngine(xnetengine* pEngine)
{
	if ( !pEngine ) { return; }
	if ( !pEngine->pfnOnPortEvent || pEngine->pfnOnPortEvent == __xnetDgramOnPortEvents ) {
		pEngine->pfnOnPortEvent = __xnetDgramOnPortEvents;
		return;
	}
	if ( !pEngine->pfnOnPortEvent2 || pEngine->pfnOnPortEvent2 == __xnetDgramOnPortEvents ) {
		pEngine->pfnOnPortEvent2 = __xnetDgramOnPortEvents;
	}
}


// 内部函数：数据报所有权相关处理
static ptr __xnetDgramOwner(xdgramsock* pSock)
{
	return pSock ? pSock->pUserData : NULL;
}


// 内部函数：设置数据报错误
static void __xnetDgramSetError(const char* sError)
{
	__xnetErrorSetEx(XRT_NET_ERROR, XNET_OP_NONE, XNET_PHASE_COMPLETE, XNET_BACKEND_NONE, 0, sError);
}


// 内部函数：__xnetDgramUseNativePortIO
static bool __xnetDgramUseNativePortIO(xdgramsock* pSock)
{
	#if defined(_WIN32) || defined(_WIN64)
		return pSock && pSock->pWorker &&
			pSock->pWorker->tPort.pOps == xrtNetPortIOCPOps() &&
			__xnetDgramSocketIsValid(pSock->hSocket);
	#elif defined(__linux__)
		return pSock && pSock->pWorker &&
			((pSock->pWorker->tPort.pOps == xrtNetPortUringOps() &&
			__xnetPortUringHasNativeRing(&pSock->pWorker->tPort)) ||
			(pSock->pWorker->tPort.pOps == xrtNetPortEpollOps() &&
			__xnetPortEpollReady(&pSock->pWorker->tPort)) ||
			(pSock->pWorker->tPort.pOps == xrtNetPortSelectOps() &&
			__xnetPortSelectReady(&pSock->pWorker->tPort))) &&
			__xnetDgramSocketIsValid(pSock->hSocket);
	#elif defined(__APPLE__) && defined(__MACH__)
		return pSock && pSock->pWorker &&
			((pSock->pWorker->tPort.pOps == xrtNetPortKqueueOps() &&
			__xnetPortKqueueReady(&pSock->pWorker->tPort)) ||
			(pSock->pWorker->tPort.pOps == xrtNetPortSelectOps() &&
			__xnetPortSelectReady(&pSock->pWorker->tPort))) &&
			__xnetDgramSocketIsValid(pSock->hSocket);
	#else
		return pSock && pSock->pWorker &&
			pSock->pWorker->tPort.pOps == xrtNetPortSelectOps() &&
			__xnetPortSelectReady(&pSock->pWorker->tPort) &&
			__xnetDgramSocketIsValid(pSock->hSocket);
	#endif
}


// 内部函数：__xnetDgramUseNativePortOps
static bool __xnetDgramUseNativePortOps(xdgramsock* pSock)
{
	#if defined(_WIN32) || defined(_WIN64)
		return pSock && pSock->pWorker &&
			pSock->pWorker->tPort.pOps == xrtNetPortIOCPOps();
	#elif defined(__linux__)
		return pSock && pSock->pWorker &&
			((pSock->pWorker->tPort.pOps == xrtNetPortUringOps() &&
			__xnetPortUringHasNativeRing(&pSock->pWorker->tPort)) ||
			(pSock->pWorker->tPort.pOps == xrtNetPortEpollOps() &&
			__xnetPortEpollReady(&pSock->pWorker->tPort)) ||
			(pSock->pWorker->tPort.pOps == xrtNetPortSelectOps() &&
			__xnetPortSelectReady(&pSock->pWorker->tPort)));
	#elif defined(__APPLE__) && defined(__MACH__)
		return pSock && pSock->pWorker &&
			((pSock->pWorker->tPort.pOps == xrtNetPortKqueueOps() &&
			__xnetPortKqueueReady(&pSock->pWorker->tPort)) ||
			(pSock->pWorker->tPort.pOps == xrtNetPortSelectOps() &&
			__xnetPortSelectReady(&pSock->pWorker->tPort)));
	#else
		return pSock && pSock->pWorker &&
			pSock->pWorker->tPort.pOps == xrtNetPortSelectOps() &&
			__xnetPortSelectReady(&pSock->pWorker->tPort);
	#endif
}


// 内部函数：异步增加数据报 hold
static void __xnetDgramAddAsyncHold(xdgramsock* pSock)
{
	if ( !pSock ) { return; }
	(void)__xnetAtomicAddFetch32(&pSock->iAsyncHoldCount, 1);
}


// 内部函数：异步释放数据报 hold
static void __xnetDgramReleaseAsyncHold(xdgramsock* pSock)
{
	if ( !pSock ) { return; }
	(void)__xnetAtomicAddFetch32(&pSock->iAsyncHoldCount, -1);
	__xnetDgramTryFinalizeDestroy(pSock);
}


static void __xnetDgramAddIoHold(xdgramsock* pSock)
{
	if ( !pSock ) { return; }
	(void)__xnetAtomicAddFetch32(&pSock->iIoHoldCount, 1);
}


static void __xnetDgramReleaseIoHold(xdgramsock* pSock)
{
	if ( !pSock ) { return; }
	(void)__xnetAtomicAddFetch32(&pSock->iIoHoldCount, -1);
	__xnetDgramTryFinalizeDestroy(pSock);
}


// 创建网络数据报 packet
XXAPI xnetdgrampkt* xrtNetDgramPacketCreate(const xnetaddr* pFrom, const void* pData, size_t iLen)
{
	xnetdgrampkt* pPacket;

	if ( !pFrom || (!pData && iLen > 0) ) { return NULL; }
	pPacket = (xnetdgrampkt*)XNET_ALLOC(sizeof(xnetdgrampkt));
	if ( !pPacket ) { return NULL; }
	memset(pPacket, 0, sizeof(xnetdgrampkt));
	pPacket->tFrom = *pFrom;
	xrtNetChainInit(&pPacket->tChain);
	if ( iLen > 0 && !xrtNetChainAppendCopy(&pPacket->tChain, pData, iLen) ) {
		xrtNetChainClear(&pPacket->tChain);
		XNET_FREE(pPacket);
		return NULL;
	}
	return pPacket;
}


// 销毁网络数据报 packet
XXAPI void xrtNetDgramPacketDestroy(xnetdgrampkt* pPacket)
{
	if ( !pPacket ) { return; }
	xrtNetChainClear(&pPacket->tChain);
	XNET_FREE(pPacket);
}


static bool __xnetDgramTryReserveSend(xdgramsock* pSock, uint32 iBytes)
{
	int64 iPrev;
	int64 iNext;
	uint32 iBudget = iBytes > 0u ? iBytes : 1u;
	if ( !pSock ) { return false; }
	for ( ;; ) {
		iPrev = __xnetAtomicLoad64(&pSock->iPendingSendBytes);
		if ( iPrev < 0 ) { return false; }
		if ( pSock->iSendQueueLimit > 0u && (uint64)iPrev + iBudget > pSock->iSendQueueLimit ) {
			return false;
		}
		iNext = iPrev + iBudget;
		if ( __xnetAtomicCompareExchange64(&pSock->iPendingSendBytes, iNext, iPrev) == iPrev ) {
			return true;
		}
	}
}


static void __xnetDgramReleaseSend(xdgramsock* pSock, uint32 iBytes)
{
	int64 iPrev;
	int64 iNext;
	uint32 iBudget = iBytes > 0u ? iBytes : 1u;
	if ( !pSock ) { return; }
	for ( ;; ) {
		iPrev = __xnetAtomicLoad64(&pSock->iPendingSendBytes);
		if ( iPrev <= 0 ) { return; }
		iNext = iBudget >= (uint64)iPrev ? 0 : iPrev - iBudget;
		if ( __xnetAtomicCompareExchange64(&pSock->iPendingSendBytes, iNext, iPrev) == iPrev ) {
			return;
		}
	}
}


static void __xnetDgramQueueClear(xdgramsock* pSock)
{
	xnetdgrampkt* pCur;
	xnetdgrampkt* pNext;

	if ( !pSock ) { return; }
	pCur = pSock->pRecvHead;
	while ( pCur ) {
		pNext = pCur->pNext;
		pCur->pNext = NULL;
		xrtNetDgramPacketDestroy(pCur);
		pCur = pNext;
	}
	pSock->pRecvHead = NULL;
	pSock->pRecvTail = NULL;
	(void)__xnetAtomicExchange32(&pSock->iRecvQueued, 0);
}


static void __xnetDgramTryFinalizeDestroy(xdgramsock* pSock)
{
	if ( !pSock || __xnetAtomicLoad32(&pSock->iDestroyState) != 1 ) { return; }
	if ( __xnetAtomicLoad32(&pSock->iAsyncHoldCount) != 0 ||
		__xnetAtomicLoad32(&pSock->iIoHoldCount) != 0 ) { return; }
	if ( __xnetAtomicCompareExchange32(&pSock->iDestroyState, 2, 1) != 1 ) { return; }
	__xnetDgramQueueClear(pSock);
	__xnetEngineReleaseLiveObject(pSock->pEngine);
	XNET_FREE(pSock);
}


static xnetdgrampkt* __xnetDgramQueuePop(xdgramsock* pSock)
{
	xnetdgrampkt* pPacket;

	if ( !pSock || !pSock->pRecvHead ) { return NULL; }
	pPacket = pSock->pRecvHead;
	pSock->pRecvHead = pPacket->pNext;
	if ( !pSock->pRecvHead ) { pSock->pRecvTail = NULL; }
	pPacket->pNext = NULL;
	if ( __xnetAtomicLoad32(&pSock->iRecvQueued) > 0 ) { (void)__xnetAtomicAddFetch32(&pSock->iRecvQueued, -1); }
	return pPacket;
}


static xnetdgrambatch* __xnetDgramBatchCreateFromFirst(xdgramsock* pSock,
	xnetdgrampkt* pFirst, uint32 iMaxPackets)
{
	xnetdgrambatch* pBatch;
	size_t iSize;
	if ( !pSock || !pFirst || iMaxPackets == 0u || iMaxPackets > 256u ) { return NULL; }
	iSize = sizeof(*pBatch) + sizeof(pBatch->arrPackets[0]) * (iMaxPackets - 1u);
	pBatch = (xnetdgrambatch*)XNET_ALLOC(iSize);
	if ( !pBatch ) { return NULL; }
	memset(pBatch, 0, iSize);
	pBatch->iCapacity = iMaxPackets;
	pBatch->arrPackets[0] = pFirst;
	pBatch->iCount = 1u;
	while ( pBatch->iCount < iMaxPackets ) {
		xnetdgrampkt* pPacket = __xnetDgramQueuePop(pSock);
		if ( !pPacket ) { break; }
		pBatch->arrPackets[pBatch->iCount++] = pPacket;
	}
	return pBatch;
}


XXAPI uint32 xrtNetDgramBatchCount(const xnetdgrambatch* pBatch)
{
	return pBatch ? pBatch->iCount : 0u;
}


XXAPI xnetdgrampkt* xrtNetDgramBatchPacket(const xnetdgrambatch* pBatch, uint32 iIndex)
{
	return pBatch && iIndex < pBatch->iCount ? pBatch->arrPackets[iIndex] : NULL;
}


XXAPI xnetdgrampkt* xrtNetDgramBatchTake(xnetdgrambatch* pBatch, uint32 iIndex)
{
	xnetdgrampkt* pPacket;
	if ( !pBatch || iIndex >= pBatch->iCount ) { return NULL; }
	pPacket = pBatch->arrPackets[iIndex];
	pBatch->arrPackets[iIndex] = NULL;
	return pPacket;
}


XXAPI void xrtNetDgramBatchDestroy(xnetdgrambatch* pBatch)
{
	uint32 i;
	if ( !pBatch ) { return; }
	for ( i = 0u; i < pBatch->iCount; ++i ) {
		if ( pBatch->arrPackets[i] ) { xrtNetDgramPacketDestroy(pBatch->arrPackets[i]); }
	}
	XNET_FREE(pBatch);
}


static bool __xnetDgramQueuePush(xdgramsock* pSock, xnetdgrampkt* pPacket)
{
	xnetdgrampkt* pDrop;
	uint32 iLimit;

	if ( !pSock || !pPacket ) { return false; }
	iLimit = pSock->iRecvQueueLimit;
	if ( iLimit == 0u ) { iLimit = 256u; }
	if ( (uint32)__xnetAtomicLoad32(&pSock->iRecvQueued) >= iLimit && pSock->iRecvOverflowPolicy != XNET_DGRAM_OVERFLOW_DROP_OLDEST ) {
		(void)__xnetAtomicAddFetch64(&pSock->iRecvDropped, 1);
		if ( pSock->iRecvOverflowPolicy == XNET_DGRAM_OVERFLOW_ERROR ) {
			(void)__xnetAtomicAddFetch64(&pSock->iErrors, 1);
			if ( pSock->pEvents && pSock->pEvents->OnError ) {
				pSock->pEvents->OnError(__xnetDgramOwner(pSock), pSock, -1);
			}
		}
		return false;
	}
	while ( (uint32)__xnetAtomicLoad32(&pSock->iRecvQueued) >= iLimit ) {
		pDrop = __xnetDgramQueuePop(pSock);
		if ( !pDrop ) { break; }
		(void)__xnetAtomicAddFetch64(&pSock->iRecvDropped, 1);
		xrtNetDgramPacketDestroy(pDrop);
	}
	pPacket->pNext = NULL;
	if ( pSock->pRecvTail ) {
		pSock->pRecvTail->pNext = pPacket;
	} else {
		pSock->pRecvHead = pPacket;
	}
	pSock->pRecvTail = pPacket;
	(void)__xnetAtomicAddFetch32(&pSock->iRecvQueued, 1);
	return true;
}


// xrtNetDgramPacketFrom 相关处理
XXAPI const xnetaddr* xrtNetDgramPacketFrom(const xnetdgrampkt* pPacket)
{
	return pPacket ? &pPacket->tFrom : NULL;
}


// xrtNetDgramPacketBytes 相关处理
XXAPI size_t xrtNetDgramPacketBytes(const xnetdgrampkt* pPacket)
{
	return pPacket ? xrtNetChainBytes(&pPacket->tChain) : 0;
}


// 查看网络数据报 packet
XXAPI size_t xrtNetDgramPacketPeek(const xnetdgrampkt* pPacket, ptr pOut, size_t iLen)
{
	return pPacket ? xrtNetChainPeek(&pPacket->tChain, pOut, iLen) : 0;
}


// 内部函数：接收数据报 notify 同步
static bool __xnetDgramEnqueueSyncRecvWait(__xnet_dgram_wait_slot* pHead,
	__xnet_dgram_sync_wait_fn pfnWait, ptr pCtx)
{
	__xnet_dgram_wait_slot* pNode;
	__xnet_dgram_wait_slot* pTail;
	if ( !pHead || !pfnWait ) { return false; }
	if ( pHead->pfnWait == NULL ) {
		pHead->pNext = NULL;
		pHead->pfnWait = pfnWait;
		pHead->pCtx = pCtx;
		return true;
	}
	pNode = (__xnet_dgram_wait_slot*)XNET_ALLOC(sizeof(*pNode));
	if ( !pNode ) { return false; }
	memset(pNode, 0, sizeof(*pNode));
	pNode->pfnWait = pfnWait;
	pNode->pCtx = pCtx;
	pTail = pHead;
	while ( pTail->pNext ) { pTail = pTail->pNext; }
	pTail->pNext = pNode;
	return true;
}


static bool __xnetDgramPopSyncRecvWait(__xnet_dgram_wait_slot* pHead,
	__xnet_dgram_sync_wait_fn* ppfnWait, ptr* ppCtx)
{
	__xnet_dgram_wait_slot* pNext;
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


static bool __xnetDgramRemoveSyncRecvWait(__xnet_dgram_wait_slot* pHead, ptr pCtx)
{
	__xnet_dgram_wait_slot* pPrev;
	__xnet_dgram_wait_slot* pNode;
	if ( !pHead || pHead->pfnWait == NULL ) { return false; }
	if ( pHead->pCtx == pCtx ) {
		__xnet_dgram_sync_wait_fn pfnDiscard;
		ptr pDiscardCtx;
		return __xnetDgramPopSyncRecvWait(pHead, &pfnDiscard, &pDiscardCtx);
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


static void __xnetDgramNotifySyncRecv(xdgramsock* pSock, xnet_result iStatus, xnetdgrampkt* pPacket)
{
	__xnet_dgram_sync_wait_fn pfnWait = NULL;
	ptr pCtx = NULL;

	if ( !pSock ) {
		if ( pPacket ) { xrtNetDgramPacketDestroy(pPacket); }
		return;
	}
	if ( !__xnetDgramPopSyncRecvWait(&pSock->tRecvWait, &pfnWait, &pCtx) ) {
		if ( pPacket ) { xrtNetDgramPacketDestroy(pPacket); }
		return;
	}
	pfnWait(pSock, iStatus, pPacket, pCtx);
}


static void __xnetDgramNotifyAllSyncRecv(xdgramsock* pSock, xnet_result iStatus)
{
	__xnet_dgram_wait_slot* pNode;
	uint32 iWaitCount = 0u;
	uint32 i;
	if ( !pSock ) { return; }
	for ( pNode = &pSock->tRecvWait; pNode && pNode->pfnWait; pNode = pNode->pNext ) {
		++iWaitCount;
	}
	for ( i = 0u; i < iWaitCount; ++i ) {
		__xnetDgramNotifySyncRecv(pSock, iStatus, NULL);
	}
}


// 内部函数：__xnetDgramRegisterSyncRecvWait
static bool __xnetDgramRegisterSyncRecvWait(xdgramsock* pSock, __xnet_dgram_sync_wait_fn pfnWait, ptr pCtx)
{
	xnetdgrampkt* pQueued;

	if ( !pSock || !pfnWait ) { return false; }
	if ( pSock->pWorker && !__xnetEngineIsCurrentWorker(pSock->pWorker) ) { return false; }
	if ( pSock->pEvents && pSock->pEvents->OnRecv ) { return false; }
	pQueued = __xnetDgramQueuePop(pSock);
	if ( pQueued ) {
		pfnWait(pSock, XRT_NET_OK, pQueued, pCtx);
		return true;
	}
	if ( !pSock->bRunning || !__xnetDgramSocketIsValid(pSock->hSocket) ) {
		pfnWait(pSock, XRT_NET_CLOSED, NULL, pCtx);
		return true;
	}

	return __xnetDgramEnqueueSyncRecvWait(&pSock->tRecvWait, pfnWait, pCtx);
}


// 内部函数：__xnetDgramCancelSyncRecvWait
static bool __xnetDgramCancelSyncRecvWait(xdgramsock* pSock, ptr pCtx)
{
	if ( !pSock ) { return false; }
	if ( pSock->pWorker && !__xnetEngineIsCurrentWorker(pSock->pWorker) ) { return false; }
	return __xnetDgramRemoveSyncRecvWait(&pSock->tRecvWait, pCtx);
}


// 内部函数：数据报套接字最后一次 err相关处理
static int __xnetDgramSocketLastErr(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return WSAGetLastError();
	#else
		return errno;
	#endif
}


// 内部函数：判断数据报套接字是否有效
static bool __xnetDgramSocketIsValid(xsocket hSocket)
{
	return hSocket != XNET_SOCKET_INVALID;
}


// 内部函数：创建数据报套接字
static xsocket __xnetDgramSocketCreate(int iFamily)
{
	#if defined(_WIN32) || defined(_WIN64)
		return WSASocket(iFamily, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
	#else
		return socket(iFamily, SOCK_DGRAM, IPPROTO_UDP);
	#endif
}


// 内部函数：处理数据报套接字 close
static void __xnetDgramSocketCloseHandle(xsocket* phSocket)
{
	if ( !phSocket || !__xnetDgramSocketIsValid(*phSocket) ) { return; }
	#if defined(_WIN32) || defined(_WIN64)
		closesocket(*phSocket);
	#else
		close(*phSocket);
	#endif
	*phSocket = XNET_SOCKET_INVALID;
}


// 内部函数：设置数据报套接字 non 块
static bool __xnetDgramSocketSetNonBlock(xsocket hSocket, bool bEnable)
{
	if ( !__xnetDgramSocketIsValid(hSocket) ) { return false; }
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


// 内部函数：__xnetDgramSocketSetReuseAddr
static bool __xnetDgramSocketSetReuseAddr(xsocket hSocket)
{
	int iOpt = 1;
	if ( !__xnetDgramSocketIsValid(hSocket) ) { return false; }
	#if defined(_WIN32) || defined(_WIN64)
		return setsockopt(hSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&iOpt, sizeof(iOpt)) == 0;
	#else
		return setsockopt(hSocket, SOL_SOCKET, SO_REUSEADDR, &iOpt, sizeof(iOpt)) == 0;
	#endif
}


// 内部函数：设置数据报套接字 reuse 端口
static bool __xnetDgramSocketSetReusePort(xsocket hSocket)
{
	#if defined(SO_REUSEPORT)
		int iOpt = 1;
		if ( !__xnetDgramSocketIsValid(hSocket) ) { return false; }
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


// 内部函数：__xnetDgramSocketUpdateLocalAddr
static bool __xnetDgramSocketUpdateLocalAddr(xsocket hSocket, xnetaddr* pAddr)
{
	struct sockaddr_storage tStorage;
	socklen_t iLen = (socklen_t)sizeof(tStorage);
	if ( !__xnetDgramSocketIsValid(hSocket) || !pAddr ) { return false; }
	memset(&tStorage, 0, sizeof(tStorage));
	if ( getsockname(hSocket, (struct sockaddr*)&tStorage, &iLen) != 0 ) { return false; }
	return __xnetAddrFromSockAddr(pAddr, (const struct sockaddr*)&tStorage);
}


// 内部函数：__xnetDgramApplyBindFlags
static void __xnetDgramApplyBindFlags(xsocket hSocket, uint32 iFlags)
{
	if ( !__xnetDgramSocketIsValid(hSocket) ) { return; }
	if ( (iFlags & XNET_DGRAM_F_REUSE_ADDR) != 0 ) {
		(void)__xnetDgramSocketSetReuseAddr(hSocket);
	}
	if ( (iFlags & XNET_DGRAM_F_REUSE_PORT) != 0 ) {
		(void)__xnetDgramSocketSetReusePort(hSocket);
	}
}


// 内部函数：分配数据报临时 chain
static xnetchain* __xnetDgramAllocTempChain(xdgramsock* pSock)
{
	xnetchain* pChain;
	xnetmemctx* pMemCtx;
	if ( !pSock || !pSock->pWorker ) { return NULL; }
	pChain = (xnetchain*)XNET_ALLOC(sizeof(xnetchain));
	if ( !pChain ) { return NULL; }
	pMemCtx = &pSock->pWorker->tMemCtx;
	xrtNetChainInitEx(pChain, pMemCtx);
	return pChain;
}


// 内部函数：释放数据报临时 chain
static void __xnetDgramFreeTempChain(xnetchain* pChain)
{
	if ( !pChain ) { return; }
	xrtNetChainClear(pChain);
	XNET_FREE(pChain);
}


// 内部函数：提交数据报套接字 notice
static bool __xnetDgramSubmitSocketNotice(xdgramsock* pSock, uint16 iOpType, xsocket hSocket)
{
	xnetportsubmit tSubmit;
	if ( !pSock || !pSock->pWorker || !__xnetDgramSocketIsValid(hSocket) ) { return false; }
	memset(&tSubmit, 0, sizeof(tSubmit));
	tSubmit.iOpType = iOpType;
	tSubmit.iOpId = pSock->iId;
	tSubmit.hSocket = (intptr_t)hSocket;
	tSubmit.pUserData = pSock;
	return xrtNetPortSubmit(&pSock->pWorker->tPort, &tSubmit, 1) == XRT_NET_OK;
}


// 内部函数：__xnetDgramArmRecvWatch
static bool __xnetDgramArmRecvWatch(xdgramsock* pSock)
{
	uint32 iTarget;
	bool bSubmitted = false;
	if ( !pSock || !pSock->bRunning || !__xnetDgramSocketIsValid(pSock->hSocket) ) { return false; }
	iTarget = pSock->iRecvBatch > 0u ? pSock->iRecvBatch : 1u;
	if ( iTarget > 256u ) { iTarget = 256u; }
	#if defined(__linux__)
		if ( !pSock->pWorker || pSock->pWorker->tPort.pOps != xrtNetPortUringOps() ||
			!__xnetPortUringHasNativeRing(&pSock->pWorker->tPort) ) { iTarget = 1u; }
	#elif !defined(_WIN32) && !defined(_WIN64)
		iTarget = 1u;
	#endif
	while ( pSock->bRunning && __xnetDgramSocketIsValid(pSock->hSocket) ) {
		long iArmed = __xnetAtomicLoad32(&pSock->iRecvArmed);
		if ( iArmed >= (long)iTarget ) { break; }
		if ( __xnetAtomicCompareExchange32(&pSock->iRecvArmed, iArmed + 1, iArmed) != iArmed ) { continue; }
		__xnetDgramAddIoHold(pSock);
		if ( !__xnetDgramSubmitSocketNotice(pSock, XNET_PORT_OP_RECVFROM, pSock->hSocket) ) {
			(void)__xnetAtomicAddFetch32(&pSock->iRecvArmed, -1);
			__xnetDgramReleaseIoHold(pSock);
			break;
		}
		bSubmitted = true;
	}
	if ( pSock->pWorker && !__xnetEngineIsCurrentWorker(pSock->pWorker) ) {
		(void)xrtNetPortWake(&pSock->pWorker->tPort);
	}
	return bSubmitted || __xnetAtomicLoad32(&pSock->iRecvArmed) > 0;
}


static bool __xnetDgramSocketSetIntOption(xsocket hSocket, int iLevel, int iName, int iValue)
{
	if ( !__xnetDgramSocketIsValid(hSocket) ) { return false; }
	#if defined(_WIN32) || defined(_WIN64)
		return setsockopt(hSocket, iLevel, iName, (const char*)&iValue, sizeof(iValue)) == 0;
	#else
		return setsockopt(hSocket, iLevel, iName, &iValue, sizeof(iValue)) == 0;
	#endif
}


static bool __xnetDgramApplyConfiguredOptions(xdgramsock* pSock)
{
	int iValue;
	if ( !pSock || !__xnetDgramSocketIsValid(pSock->hSocket) ) { return false; }
	if ( (pSock->iFlags & XNET_DGRAM_F_IPV6_ONLY) != 0 &&
		(pSock->iFlags & XNET_DGRAM_F_DUAL_STACK) != 0 ) { return false; }
	if ( pSock->tLocalAddr.iFamily == AF_INET6 ) {
		if ( (pSock->iFlags & XNET_DGRAM_F_IPV6_ONLY) != 0 &&
			!__xnetDgramSocketSetIntOption(pSock->hSocket, IPPROTO_IPV6, IPV6_V6ONLY, 1) ) { return false; }
		if ( (pSock->iFlags & XNET_DGRAM_F_DUAL_STACK) != 0 &&
			!__xnetDgramSocketSetIntOption(pSock->hSocket, IPPROTO_IPV6, IPV6_V6ONLY, 0) ) { return false; }
	}
	if ( (pSock->iFlags & XNET_DGRAM_F_BROADCAST) != 0 &&
		!__xnetDgramSocketSetIntOption(pSock->hSocket, SOL_SOCKET, SO_BROADCAST, 1) ) { return false; }
	if ( pSock->iRecvBufferSize > 0u ) {
		iValue = pSock->iRecvBufferSize > (uint32)INT_MAX ? INT_MAX : (int)pSock->iRecvBufferSize;
		if ( !__xnetDgramSocketSetIntOption(pSock->hSocket, SOL_SOCKET, SO_RCVBUF, iValue) ) { return false; }
	}
	if ( pSock->iSendBufferSize > 0u ) {
		iValue = pSock->iSendBufferSize > (uint32)INT_MAX ? INT_MAX : (int)pSock->iSendBufferSize;
		if ( !__xnetDgramSocketSetIntOption(pSock->hSocket, SOL_SOCKET, SO_SNDBUF, iValue) ) { return false; }
	}
	if ( pSock->iHopLimit >= 0 ) {
		if ( pSock->tLocalAddr.iFamily == AF_INET ) {
			if ( !__xnetDgramSocketSetIntOption(pSock->hSocket, IPPROTO_IP, IP_TTL, pSock->iHopLimit) ) { return false; }
		} else if ( pSock->tLocalAddr.iFamily == AF_INET6 ) {
			if ( !__xnetDgramSocketSetIntOption(pSock->hSocket, IPPROTO_IPV6, IPV6_UNICAST_HOPS, pSock->iHopLimit) ) { return false; }
		}
	}
	if ( pSock->iTrafficClass >= 0 ) {
		if ( pSock->tLocalAddr.iFamily == AF_INET ) {
			if ( !__xnetDgramSocketSetIntOption(pSock->hSocket, IPPROTO_IP, IP_TOS, pSock->iTrafficClass) ) { return false; }
		} else if ( pSock->tLocalAddr.iFamily == AF_INET6 ) {
			#if defined(IPV6_TCLASS)
				if ( !__xnetDgramSocketSetIntOption(pSock->hSocket, IPPROTO_IPV6, IPV6_TCLASS, pSock->iTrafficClass) ) { return false; }
			#else
				return false;
			#endif
		}
	}
	return true;
}


// 内部函数：关闭数据报 finalize 套接字
static void __xnetDgramFinalizeSocketClose(xdgramsock* pSock)
{
	xsocket hSocket;
	if ( !pSock ) { return; }
	hSocket = pSock->hSocket;
	if ( __xnetDgramSocketIsValid(hSocket) ) {
		(void)__xnetDgramSubmitSocketNotice(pSock, XNET_PORT_OP_CLOSE, hSocket);
		__xnetDgramSocketCloseHandle(&pSock->hSocket);
	}
}


// 内部函数：__xnetDgramDispatchPacket
static bool UNUSED_ATTR __xnetDgramDispatchPacket(xdgramsock* pSock, const xnetaddr* pFrom, const void* pData, size_t iLen)
{
	xnetchain* pChain;
	xnetdgrampkt* pPacket = NULL;
	if ( !pSock || !pFrom ) { return false; }
	// 如果存在同步接收等待者，构造数据包并直接通知
	if ( pSock->tRecvWait.pfnWait != NULL ) {
		pPacket = xrtNetDgramPacketCreate(pFrom, pData, iLen);
		if ( !pPacket ) {
			__xnetDgramNotifySyncRecv(pSock, XRT_NET_ERROR, NULL);
			if ( pSock->pEvents && pSock->pEvents->OnError ) {
				pSock->pEvents->OnError(__xnetDgramOwner(pSock), pSock, -1);
			}
			return false;
		}
		__xnetDgramNotifySyncRecv(pSock, XRT_NET_OK, pPacket);
		return true;
	}
	// 否则通过事件回调分发接收到的数据
	pChain = __xnetDgramAllocTempChain(pSock);
	if ( !pChain ) { return false; }
	if ( iLen > 0 && (!pData || !xrtNetChainAppendCopy(pChain, pData, iLen)) ) {
		__xnetDgramFreeTempChain(pChain);
		return false;
	}
	if ( pSock->pEvents && pSock->pEvents->OnRecv ) {
		pSock->pEvents->OnRecv(__xnetDgramOwner(pSock), pSock, pFrom, pChain);
	} else {
		pPacket = xrtNetDgramPacketCreate(pFrom, pData, iLen);
		if ( !pPacket || !__xnetDgramQueuePush(pSock, pPacket) ) {
			if ( pPacket ) { xrtNetDgramPacketDestroy(pPacket); }
			__xnetDgramFreeTempChain(pChain);
			return false;
		}
	}
	__xnetDgramFreeTempChain(pChain);
	return true;
}


// 内部函数：发送数据报 submit 原生
static bool __xnetDgramSubmitNativeSend(xdgramsock* pSock, __xnet_dgram_async_op* pOp)
{
	xnetportsubmit tSubmit;
	xnetspan tSpan;
	if ( !pSock || !pOp || !pSock->pWorker || !__xnetDgramSocketIsValid(pSock->hSocket) ) { return false; }
	memset(&tSubmit, 0, sizeof(tSubmit));
	memset(&tSpan, 0, sizeof(tSpan));
	tSpan.pData = pOp->aData;
	tSpan.iLen = pOp->iLen;
	tSubmit.iOpType = XNET_PORT_OP_SENDTO;
	tSubmit.iOpId = (uint64)(uintptr_t)pOp;
	tSubmit.hSocket = (intptr_t)pSock->hSocket;
	tSubmit.pUserData = pOp;
	tSubmit.pVec = &tSpan;
	tSubmit.iVecCount = 1;
	tSubmit.tAddr = pOp->tTo;
	return xrtNetPortSubmit(&pSock->pWorker->tPort, &tSubmit, 1) == XRT_NET_OK;
}


// 内部函数：数据报异步任务相关处理
static void __xnetDgramAsyncTask(xnetworker* pWorker, ptr pArg)
{
	__xnet_dgram_async_op* pOp = (__xnet_dgram_async_op*)pArg;
	xdgramsock* pSock = pOp ? pOp->pSock : NULL;
	(void)pWorker;
	if ( !pOp || !pSock ) {
		if ( pOp ) { XNET_FREE(pOp); }
		return;
	}

	if ( pOp->iType == __XNET_DGRAM_ASYNC_SEND_COPY ) {
		if ( !__xnetDgramUseNativePortIO(pSock) || !__xnetDgramSubmitNativeSend(pSock, pOp) ) {
			(void)__xnetAtomicAddFetch64(&pSock->iErrors, 1);
			if ( pSock->pEvents && pSock->pEvents->OnError ) {
				pSock->pEvents->OnError(__xnetDgramOwner(pSock), pSock, -1);
			}
			if ( pOp->pfnDone ) { pOp->pfnDone(pSock, XRT_NET_ERROR, pOp->pDoneArg); }
			__xnetDgramReleaseSend(pSock, pOp->iBudgetBytes);
			__xnetDgramReleaseAsyncHold(pSock);
			XNET_FREE(pOp);
		}
		return;
	}
	__xnetDgramReleaseAsyncHold(pSock);
	XNET_FREE(pOp);
}


// 内部函数：异步复制数据报 alloc
static __xnet_dgram_async_op* __xnetDgramAllocAsyncCopy(xdgramsock* pSock, const xnetaddr* pTo, const void* pData, size_t iLen)
{
	__xnet_dgram_async_op* pOp;
	size_t iSize;
	if ( !pSock || !pTo || (!pData && iLen > 0) || iLen > UINT32_MAX ) { return NULL; }
	iSize = sizeof(__xnet_dgram_async_op) + iLen;
	pOp = (__xnet_dgram_async_op*)XNET_ALLOC(iSize);
	if ( !pOp ) { return NULL; }
	memset(pOp, 0, iSize);
	pOp->pSock = pSock;
	pOp->tTo = *pTo;
	pOp->iType = __XNET_DGRAM_ASYNC_SEND_COPY;
	pOp->iLen = (uint32)iLen;
	pOp->iBudgetBytes = iLen > 0u ? (uint32)iLen : 1u;
	if ( iLen > 0 ) { memcpy(pOp->aData, pData, iLen); }
	return pOp;
}


// 内部函数：异步复制数据报 alloc vec
static __xnet_dgram_async_op* __xnetDgramAllocAsyncVecCopy(xdgramsock* pSock, const xnetaddr* pTo, const xnetspan* pVec, uint32 iCount)
{
	__xnet_dgram_async_op* pOp;
	size_t iTotal = 0;
	size_t iOffset = 0;
	size_t iSize;
	if ( !pSock || !pTo || !pVec || iCount == 0 ) { return NULL; }
	// 遍历向量数组，计算总数据长度并验证有效性
	for ( uint32 i = 0; i < iCount; ++i ) {
		if ( !pVec[i].pData || pVec[i].iLen == 0 ) { return NULL; }
		iTotal += pVec[i].iLen;
		if ( iTotal > UINT32_MAX ) { return NULL; }
	}
	// 分配异步操作结构体（含尾部数据缓冲区）
	iSize = sizeof(__xnet_dgram_async_op) + iTotal;
	pOp = (__xnet_dgram_async_op*)XNET_ALLOC(iSize);
	if ( !pOp ) { return NULL; }
	memset(pOp, 0, iSize);
	pOp->pSock = pSock;
	pOp->tTo = *pTo;
	pOp->iType = __XNET_DGRAM_ASYNC_SEND_COPY;
	pOp->iLen = (uint32)iTotal;
	pOp->iBudgetBytes = iTotal > 0u ? (uint32)iTotal : 1u;
	// 将分散的向量数据拷贝到连续缓冲区
	for ( uint32 i = 0; i < iCount; ++i ) {
		memcpy(pOp->aData + iOffset, pVec[i].pData, pVec[i].iLen);
		iOffset += pVec[i].iLen;
	}
	return pOp;
}


// 内部函数：__xnetDgramPostAsync
static xnet_result __xnetDgramPostAsync(xdgramsock* pSock, __xnet_dgram_async_op* pOp)
{
	if ( !pSock || !pOp || !pSock->pEngine || !pSock->pWorker ) {
		if ( pOp ) { __xnetDgramReleaseSend(pSock, pOp->iBudgetBytes); }
		if ( pOp ) { XNET_FREE(pOp); }
		return XRT_NET_ERROR;
	}
	__xnetDgramAddAsyncHold(pSock);
	if ( xrtNetEnginePost(pSock->pEngine, pSock->pWorker->iId, __xnetDgramAsyncTask, pOp) != XRT_NET_OK ) {
		__xnetDgramReleaseAsyncHold(pSock);
		__xnetDgramReleaseSend(pSock, pOp->iBudgetBytes);
		XNET_FREE(pOp);
		return XRT_NET_ERROR;
	}
	return XRT_NET_OK;
}


// 创建网络数据报
XXAPI xdgramsock* xrtNetDgramCreate(xnetengine* pEngine, const xnetdgramconfig* pCfg, const xnetdgramevents* pEvents, ptr pUserData)
{
	xdgramsock* pSock;
	xnetworker* pWorker;
	if ( !pEngine ) { return NULL; }
	if ( pCfg && !__xnetConfigHeaderIsValid(pCfg->iSize, pCfg->iVersion, XNET_DGRAM_CONFIG_V1_SIZE) ) {
		__xnetErrorSetEx(XRT_NET_ERROR, XNET_OP_RECV, XNET_PHASE_VALIDATE, XNET_BACKEND_NONE, 0,
			"invalid datagram config version or size.");
		return NULL;
	}
	if ( pCfg && pCfg->iRecvBatch > 256u ) {
		__xnetErrorSetEx(XRT_NET_ERROR, XNET_OP_RECV, XNET_PHASE_VALIDATE, XNET_BACKEND_NONE, 0,
			"datagram receive batch must be between 0 and 256.");
		return NULL;
	}
	if ( pCfg && (pCfg->tBindAddr.iFamily != AF_INET && pCfg->tBindAddr.iFamily != AF_INET6) ) {
		__xnetErrorSetEx(XRT_NET_ERROR, XNET_OP_RECV, XNET_PHASE_VALIDATE, XNET_BACKEND_NONE, 0,
			"datagram bind address family must be IPv4 or IPv6.");
		return NULL;
	}
	if ( pCfg && pCfg->iRecvOverflowPolicy > XNET_DGRAM_OVERFLOW_ERROR ) {
		__xnetErrorSetEx(XRT_NET_ERROR, XNET_OP_RECV, XNET_PHASE_VALIDATE, XNET_BACKEND_NONE, 0,
			"invalid datagram receive overflow policy.");
		return NULL;
	}
	if ( pCfg && ((pCfg->iFlags & XNET_DGRAM_F_IPV6_ONLY) != 0) &&
		((pCfg->iFlags & XNET_DGRAM_F_DUAL_STACK) != 0) ) {
		__xnetErrorSetEx(XRT_NET_ERROR, XNET_OP_RECV, XNET_PHASE_VALIDATE, XNET_BACKEND_NONE, 0,
			"datagram IPv6-only and dual-stack flags are mutually exclusive.");
		return NULL;
	}
	if ( pCfg && (pCfg->iHopLimit < -1 || pCfg->iHopLimit > 255 ||
		pCfg->iTrafficClass < -1 || pCfg->iTrafficClass > 255) ) {
		__xnetErrorSetEx(XRT_NET_ERROR, XNET_OP_RECV, XNET_PHASE_VALIDATE, XNET_BACKEND_NONE, 0,
			"datagram hop limit and traffic class must be between 0 and 255, or -1.");
		return NULL;
	}
	if ( pCfg && (pCfg->iFlags & XNET_DGRAM_F_CONNECTED) != 0 &&
		(pCfg->tPeerAddr.iPort == 0u || pCfg->tPeerAddr.iFamily != pCfg->tBindAddr.iFamily) ) {
		__xnetErrorSetEx(XRT_NET_ERROR, XNET_OP_CONNECT, XNET_PHASE_VALIDATE, XNET_BACKEND_NONE, 0,
			"connected datagram peer must have the bind address family and a nonzero port.");
		return NULL;
	}
	// 注册数据报端口事件处理回调到引擎
	__xnetDgramBindEngine(pEngine);
	pSock = (xdgramsock*)XNET_ALLOC(sizeof(xdgramsock));
	if ( !pSock ) { return NULL; }
	memset(pSock, 0, sizeof(xdgramsock));
	// 根据绑定端口选择工作线程（取模亲和性）
	pWorker = __xnetDgramPickWorker(pEngine, pCfg);
	pSock->iId = __xnetEngineAllocStreamId(pEngine);
	pSock->pEngine = pEngine;
	__xnetEngineAddLiveObject(pEngine);
	pSock->pWorker = pWorker;
	pSock->pEvents = pEvents;
	pSock->pUserData = pUserData;
	pSock->hSocket = XNET_SOCKET_INVALID;
	// 从用户配置或默认值初始化套接字参数
	if ( pCfg ) {
		pSock->tLocalAddr = pCfg->tBindAddr;
		pSock->tPeerAddr = pCfg->tPeerAddr;
		pSock->iFlags = pCfg->iFlags;
		pSock->iRecvBatch = pCfg->iRecvBatch > 0u ? pCfg->iRecvBatch : 1u;
		pSock->iSendQueueLimit = pCfg->iSendQueueLimit;
		pSock->iRecvQueueLimit = pCfg->iRecvQueueLimit;
		pSock->iRecvOverflowPolicy = pCfg->iRecvOverflowPolicy;
		pSock->iRecvBufferSize = pCfg->iRecvBufferSize;
		pSock->iSendBufferSize = pCfg->iSendBufferSize;
		pSock->iHopLimit = pCfg->iHopLimit;
		pSock->iTrafficClass = pCfg->iTrafficClass;
		pSock->bConnected = (pCfg->iFlags & XNET_DGRAM_F_CONNECTED) != 0;
	} else {
		xnetdgramconfig tCfg;
		xrtNetDgramConfigInit(&tCfg);
		pSock->tLocalAddr = tCfg.tBindAddr;
		pSock->tPeerAddr = tCfg.tPeerAddr;
		pSock->iFlags = tCfg.iFlags;
		pSock->iRecvBatch = tCfg.iRecvBatch;
		pSock->iSendQueueLimit = tCfg.iSendQueueLimit;
		pSock->iRecvQueueLimit = tCfg.iRecvQueueLimit;
		pSock->iRecvOverflowPolicy = tCfg.iRecvOverflowPolicy;
		pSock->iRecvBufferSize = tCfg.iRecvBufferSize;
		pSock->iSendBufferSize = tCfg.iSendBufferSize;
		pSock->iHopLimit = tCfg.iHopLimit;
		pSock->iTrafficClass = tCfg.iTrafficClass;
		pSock->bConnected = false;
	}
	return pSock;
}


static xnet_result __xnetDgramSendToWithCompletion(xdgramsock* pSock,
	const xnetaddr* pTo, const void* pData, size_t iLen,
	__xnet_dgram_send_done_fn pfnDone, ptr pDoneArg)
{
	__xnet_dgram_async_op* pOp;
	if ( !pSock || !pSock->pEngine || __xnetAtomicLoad32(&pSock->pEngine->bRunning) == 0 || !pTo ||
		(!pData && iLen > 0) || iLen > __XNET_DGRAM_MAX_PAYLOAD ) { return XRT_NET_ERROR; }
	if ( !pSock->bRunning || !__xnetDgramSocketIsValid(pSock->hSocket) ) { return XRT_NET_ERROR; }
	if ( !__xnetDgramTryReserveSend(pSock, (uint32)iLen) ) {
		(void)__xnetAtomicAddFetch64(&pSock->iSendAgain, 1);
		return XRT_NET_AGAIN;
	}
	pOp = __xnetDgramAllocAsyncCopy(pSock, pTo, pData, iLen);
	if ( !pOp ) {
		__xnetDgramReleaseSend(pSock, (uint32)iLen);
		return XRT_NET_ERROR;
	}
	pOp->pfnDone = pfnDone;
	pOp->pDoneArg = pDoneArg;
	return __xnetDgramPostAsync(pSock, pOp);
}


// 销毁网络数据报
XXAPI void xrtNetDgramDestroy(xdgramsock* pSock)
{
	if ( !pSock ) { return; }
	if ( __xnetAtomicLoad32(&pSock->iDestroyState) != 0 ) { return; }
	if ( __xnetAtomicLoad32(&pSock->iAsyncHoldCount) != 0 ) {
		__xnetDgramSetError("cannot destroy datagram socket while an async waiter or send still holds it.");
		return;
	}
	pSock->bRunning = false;
	__xnetDgramNotifyAllSyncRecv(pSock, XRT_NET_CLOSED);
	__xnetDgramFinalizeSocketClose(pSock);
	(void)__xnetAtomicCompareExchange32(&pSock->iDestroyState, 1, 0);
	__xnetDgramTryFinalizeDestroy(pSock);
}


// 启动网络数据报
XXAPI xnet_result xrtNetDgramStart(xdgramsock* pSock)
{
	struct sockaddr_storage tStorage;
	socklen_t iAddrLen = 0;
	struct sockaddr_storage tPeerStorage;
	socklen_t iPeerLen = 0;
	if ( !pSock || !pSock->pEngine || !pSock->pEngine->bRunning ) { return XRT_NET_ERROR; }
	if ( __xnetAtomicLoad32(&pSock->iDestroyState) != 0 ) { return XRT_NET_CLOSED; }
	if ( pSock->bRunning ) { return XRT_NET_OK; }
	if ( __xnetAtomicLoad32(&pSock->iIoHoldCount) != 0 ||
		__xnetAtomicLoad32(&pSock->iAsyncHoldCount) != 0 ) { return XRT_NET_AGAIN; }
	// 将本地地址转换为系统 sockaddr 结构
	if ( !__xnetAddrToSockAddr(&pSock->tLocalAddr, &tStorage, &iAddrLen) ) { return XRT_NET_ERROR; }
	// 创建 UDP 套接字
	pSock->hSocket = __xnetDgramSocketCreate(pSock->tLocalAddr.iFamily);
	if ( !__xnetDgramSocketIsValid(pSock->hSocket) ) { return XRT_NET_ERROR; }
	// 设置套接字选项（地址复用、端口复用等）并开启非阻塞模式用于 bind
	__xnetDgramApplyBindFlags(pSock->hSocket, pSock->iFlags);
	if ( !__xnetDgramApplyConfiguredOptions(pSock) ) {
		__xnetErrorSetEx(XRT_NET_ERROR, XNET_OP_RECV, XNET_PHASE_SUBMIT,
			XNET_BACKEND_NONE, __xnetDgramSocketLastErr(), "unable to apply datagram socket options.");
		__xnetDgramFinalizeSocketClose(pSock);
		return XRT_NET_ERROR;
	}
	(void)__xnetDgramSocketSetNonBlock(pSock->hSocket, true);
	// 绑定到本地地址
	if ( bind(pSock->hSocket, (struct sockaddr*)&tStorage, iAddrLen) != 0 ) {
		if ( pSock->pEvents && pSock->pEvents->OnError ) {
			pSock->pEvents->OnError(__xnetDgramOwner(pSock), pSock, __xnetDgramSocketLastErr());
		}
		__xnetDgramFinalizeSocketClose(pSock);
		return XRT_NET_ERROR;
	}
	// 检查底层是否支持原生端口操作（IOCP/io_uring）
	if ( pSock->bConnected ) {
		if ( !__xnetAddrToSockAddr(&pSock->tPeerAddr, &tPeerStorage, &iPeerLen) ||
			connect(pSock->hSocket, (struct sockaddr*)&tPeerStorage, iPeerLen) != 0 ) {
			__xnetErrorSetEx(XRT_NET_ERROR, XNET_OP_CONNECT, XNET_PHASE_SUBMIT,
				XNET_BACKEND_NONE, __xnetDgramSocketLastErr(), "unable to connect datagram peer.");
			__xnetDgramFinalizeSocketClose(pSock);
			return XRT_NET_ERROR;
		}
	}
	if ( !__xnetDgramUseNativePortOps(pSock) ) {
		__xnetDgramSetError("mainline datagram sockets require a native xnet backend.");
		__xnetDgramFinalizeSocketClose(pSock);
		return XRT_NET_ERROR;
	}
	// 关闭非阻塞模式，更新本地地址，标记运行状态并启动接收监听
	(void)__xnetDgramSocketSetNonBlock(pSock->hSocket, false);
	(void)__xnetDgramSocketUpdateLocalAddr(pSock->hSocket, &pSock->tLocalAddr);
	pSock->bRunning = true;
	(void)__xnetDgramArmRecvWatch(pSock);
	return XRT_NET_OK;
}


// 停止网络数据报
XXAPI void xrtNetDgramStop(xdgramsock* pSock)
{
	if ( !pSock ) { return; }
	pSock->bRunning = false;
	__xnetDgramNotifyAllSyncRecv(pSock, XRT_NET_CLOSED);
	__xnetDgramFinalizeSocketClose(pSock);
}


// 发送网络数据报
XXAPI xnet_result xrtNetDgramSendTo(xdgramsock* pSock, const xnetaddr* pTo, const void* pData, size_t iLen)
{
	return __xnetDgramSendToWithCompletion(pSock, pTo, pData, iLen, NULL, NULL);
}


XXAPI xnet_result xrtNetDgramConnect(xdgramsock* pSock, const xnetaddr* pPeer)
{
	if ( !pSock || !pPeer || pPeer->iPort == 0u ||
		(pPeer->iFamily != AF_INET && pPeer->iFamily != AF_INET6) ||
		pPeer->iFamily != pSock->tLocalAddr.iFamily ) {
		__xnetErrorSetEx(XRT_NET_ERROR, XNET_OP_CONNECT, XNET_PHASE_VALIDATE,
			XNET_BACKEND_NONE, 0, "invalid connected datagram peer.");
		return XRT_NET_ERROR;
	}
	if ( pSock->bRunning || __xnetDgramSocketIsValid(pSock->hSocket) ||
		__xnetAtomicLoad32(&pSock->iDestroyState) != 0 ) {
		__xnetErrorSetEx(XRT_NET_ERROR, XNET_OP_CONNECT, XNET_PHASE_VALIDATE,
			XNET_BACKEND_NONE, 0, "datagram peer can only be changed before start.");
		return XRT_NET_ERROR;
	}
	pSock->tPeerAddr = *pPeer;
	pSock->bConnected = true;
	pSock->iFlags |= XNET_DGRAM_F_CONNECTED;
	return XRT_NET_OK;
}


XXAPI xnet_result xrtNetDgramDisconnect(xdgramsock* pSock)
{
	if ( !pSock ) { return XRT_NET_ERROR; }
	if ( pSock->bRunning || __xnetDgramSocketIsValid(pSock->hSocket) ||
		__xnetAtomicLoad32(&pSock->iDestroyState) != 0 ) {
		__xnetErrorSetEx(XRT_NET_ERROR, XNET_OP_CONNECT, XNET_PHASE_VALIDATE,
			XNET_BACKEND_NONE, 0, "datagram peer can only be changed before start.");
		return XRT_NET_ERROR;
	}
	memset(&pSock->tPeerAddr, 0, sizeof(pSock->tPeerAddr));
	pSock->bConnected = false;
	pSock->iFlags &= ~XNET_DGRAM_F_CONNECTED;
	return XRT_NET_OK;
}


// 发送网络数据报 vec
XXAPI xnet_result xrtNetDgramSendVecTo(xdgramsock* pSock, const xnetaddr* pTo, const xnetspan* pVec, uint32 iCount)
{
	__xnet_dgram_async_op* pOp;
	uint64 iTotal = 0u;
	if ( !pSock || !pSock->pEngine || __xnetAtomicLoad32(&pSock->pEngine->bRunning) == 0 || !pTo || !pVec || iCount == 0 ) { return XRT_NET_ERROR; }
	for ( uint32 i = 0; i < iCount; ++i ) {
		if ( !pVec[i].pData || pVec[i].iLen == 0u || iTotal + pVec[i].iLen > __XNET_DGRAM_MAX_PAYLOAD ) {
			return XRT_NET_ERROR;
		}
		iTotal += pVec[i].iLen;
	}
	if ( !pSock->bRunning || !__xnetDgramSocketIsValid(pSock->hSocket) ) { return XRT_NET_ERROR; }
	if ( !__xnetDgramTryReserveSend(pSock, (uint32)iTotal) ) {
		(void)__xnetAtomicAddFetch64(&pSock->iSendAgain, 1);
		return XRT_NET_AGAIN;
	}
	pOp = __xnetDgramAllocAsyncVecCopy(pSock, pTo, pVec, iCount);
	if ( !pOp ) {
		__xnetDgramReleaseSend(pSock, (uint32)iTotal);
		return XRT_NET_ERROR;
	}
	return __xnetDgramPostAsync(pSock, pOp);
}


XXAPI xnet_result xrtNetDgramSend(xdgramsock* pSock, const void* pData, size_t iLen)
{
	if ( !pSock || !pSock->bConnected ) {
		__xnetErrorSetEx(XRT_NET_ERROR, XNET_OP_SEND, XNET_PHASE_VALIDATE,
			XNET_BACKEND_NONE, 0, "datagram socket has no connected peer.");
		return XRT_NET_ERROR;
	}
	return xrtNetDgramSendTo(pSock, &pSock->tPeerAddr, pData, iLen);
}


XXAPI xnet_result xrtNetDgramSendVec(xdgramsock* pSock, const xnetspan* pVec, uint32 iCount)
{
	if ( !pSock || !pSock->bConnected ) {
		__xnetErrorSetEx(XRT_NET_ERROR, XNET_OP_SEND, XNET_PHASE_VALIDATE,
			XNET_BACKEND_NONE, 0, "datagram socket has no connected peer.");
		return XRT_NET_ERROR;
	}
	return xrtNetDgramSendVecTo(pSock, &pSock->tPeerAddr, pVec, iCount);
}


XXAPI xnet_result xrtNetDgramSendBatch(xdgramsock* pSock,
	const xnetdgramsenditem* pItems, uint32 iCount, uint32* pAccepted)
{
	uint32 i;
	xnet_result iStatus;
	if ( pAccepted ) { *pAccepted = 0u; }
	if ( !pSock || !pItems || iCount == 0u ) { return XRT_NET_ERROR; }
	for ( i = 0u; i < iCount; ++i ) {
		const xnetaddr* pTo = pItems[i].pTo ? pItems[i].pTo :
			(pSock->bConnected ? &pSock->tPeerAddr : NULL);
		if ( !pTo || pTo->iFamily != pSock->tLocalAddr.iFamily || pTo->iPort == 0u ||
			(!pItems[i].pData && pItems[i].iLen > 0u) ||
			pItems[i].iLen > __XNET_DGRAM_MAX_PAYLOAD ) {
			__xnetErrorSetEx(XRT_NET_ERROR, XNET_OP_SEND, XNET_PHASE_VALIDATE,
				XNET_BACKEND_NONE, 0, "invalid datagram send batch item.");
			return XRT_NET_ERROR;
		}
	}
	for ( i = 0u; i < iCount; ++i ) {
		const xnetaddr* pTo = pItems[i].pTo ? pItems[i].pTo : &pSock->tPeerAddr;
		iStatus = xrtNetDgramSendTo(pSock, pTo, pItems[i].pData, pItems[i].iLen);
		if ( iStatus != XRT_NET_OK ) {
			if ( pAccepted ) { *pAccepted = i; }
			return iStatus;
		}
	}
	if ( pAccepted ) { *pAccepted = iCount; }
	return XRT_NET_OK;
}


XXAPI const xnetaddr* xrtNetDgramLocalAddr(const xdgramsock* pSock)
{
	return pSock ? &pSock->tLocalAddr : NULL;
}


XXAPI const xnetaddr* xrtNetDgramPeerAddr(const xdgramsock* pSock)
{
	return pSock && pSock->bConnected ? &pSock->tPeerAddr : NULL;
}


XXAPI bool xrtNetDgramIsConnected(const xdgramsock* pSock)
{
	return pSock && pSock->bConnected;
}


static xnet_result __xnetDgramMulticastMembership(xdgramsock* pSock,
	const xnetaddr* pGroup, const xnetaddr* pInterface, bool bJoin)
{
	int iName;
	if ( !pSock || !pGroup || !pSock->bRunning ||
		!__xnetDgramSocketIsValid(pSock->hSocket) ||
		pGroup->iFamily != pSock->tLocalAddr.iFamily ) { return XRT_NET_ERROR; }
	if ( pGroup->iFamily == AF_INET ) {
		struct ip_mreq tRequest;
		if ( pInterface && pInterface->iFamily != AF_INET ) { return XRT_NET_ERROR; }
		memset(&tRequest, 0, sizeof(tRequest));
		memcpy(&tRequest.imr_multiaddr, pGroup->aAddr, 4u);
		if ( pInterface ) { memcpy(&tRequest.imr_interface, pInterface->aAddr, 4u); }
		else { tRequest.imr_interface.s_addr = htonl(INADDR_ANY); }
		iName = bJoin ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP;
		#if defined(_WIN32) || defined(_WIN64)
			if ( setsockopt(pSock->hSocket, IPPROTO_IP, iName,
				(const char*)&tRequest, sizeof(tRequest)) != 0 ) { goto failed; }
		#else
			if ( setsockopt(pSock->hSocket, IPPROTO_IP, iName,
				&tRequest, sizeof(tRequest)) != 0 ) { goto failed; }
		#endif
	} else if ( pGroup->iFamily == AF_INET6 ) {
		struct ipv6_mreq tRequest6;
		if ( pInterface && pInterface->iFamily != AF_INET6 ) { return XRT_NET_ERROR; }
		memset(&tRequest6, 0, sizeof(tRequest6));
		memcpy(&tRequest6.ipv6mr_multiaddr, pGroup->aAddr, 16u);
		tRequest6.ipv6mr_interface = pInterface ? pInterface->iScopeId : 0u;
		iName = bJoin ? IPV6_JOIN_GROUP : IPV6_LEAVE_GROUP;
		#if defined(_WIN32) || defined(_WIN64)
			if ( setsockopt(pSock->hSocket, IPPROTO_IPV6, iName,
				(const char*)&tRequest6, sizeof(tRequest6)) != 0 ) { goto failed; }
		#else
			if ( setsockopt(pSock->hSocket, IPPROTO_IPV6, iName,
				&tRequest6, sizeof(tRequest6)) != 0 ) { goto failed; }
		#endif
	} else {
		return XRT_NET_ERROR;
	}
	return XRT_NET_OK;

failed:
	__xnetErrorSetEx(XRT_NET_ERROR, XNET_OP_RECV, XNET_PHASE_SUBMIT,
		XNET_BACKEND_NONE, __xnetDgramSocketLastErr(),
		bJoin ? "unable to join datagram multicast group." :
		"unable to leave datagram multicast group.");
	return XRT_NET_ERROR;
}


XXAPI xnet_result xrtNetDgramJoinMulticast(xdgramsock* pSock,
	const xnetaddr* pGroup, const xnetaddr* pInterface)
{
	return __xnetDgramMulticastMembership(pSock, pGroup, pInterface, true);
}


XXAPI xnet_result xrtNetDgramLeaveMulticast(xdgramsock* pSock,
	const xnetaddr* pGroup, const xnetaddr* pInterface)
{
	return __xnetDgramMulticastMembership(pSock, pGroup, pInterface, false);
}


XXAPI xnet_result xrtNetDgramSetMulticastLoop(xdgramsock* pSock, bool bEnable)
{
	if ( !pSock || !pSock->bRunning || !__xnetDgramSocketIsValid(pSock->hSocket) ) {
		return XRT_NET_ERROR;
	}
	if ( pSock->tLocalAddr.iFamily == AF_INET ) {
		uint8 iValue = bEnable ? 1u : 0u;
		#if defined(_WIN32) || defined(_WIN64)
			if ( setsockopt(pSock->hSocket, IPPROTO_IP, IP_MULTICAST_LOOP,
				(const char*)&iValue, sizeof(iValue)) == 0 ) { return XRT_NET_OK; }
		#else
			if ( setsockopt(pSock->hSocket, IPPROTO_IP, IP_MULTICAST_LOOP,
				&iValue, sizeof(iValue)) == 0 ) { return XRT_NET_OK; }
		#endif
	} else if ( pSock->tLocalAddr.iFamily == AF_INET6 &&
		__xnetDgramSocketSetIntOption(pSock->hSocket, IPPROTO_IPV6,
			IPV6_MULTICAST_LOOP, bEnable ? 1 : 0) ) {
		return XRT_NET_OK;
	}
	__xnetErrorSetEx(XRT_NET_ERROR, XNET_OP_RECV, XNET_PHASE_SUBMIT,
		XNET_BACKEND_NONE, __xnetDgramSocketLastErr(), "unable to set datagram multicast loop.");
	return XRT_NET_ERROR;
}


XXAPI xnet_result xrtNetDgramSetMulticastHopLimit(xdgramsock* pSock, int iHopLimit)
{
	if ( !pSock || !pSock->bRunning || !__xnetDgramSocketIsValid(pSock->hSocket) ||
		iHopLimit < 0 || iHopLimit > 255 ) { return XRT_NET_ERROR; }
	if ( pSock->tLocalAddr.iFamily == AF_INET ) {
		uint8 iValue = (uint8)iHopLimit;
		#if defined(_WIN32) || defined(_WIN64)
			if ( setsockopt(pSock->hSocket, IPPROTO_IP, IP_MULTICAST_TTL,
				(const char*)&iValue, sizeof(iValue)) == 0 ) { return XRT_NET_OK; }
		#else
			if ( setsockopt(pSock->hSocket, IPPROTO_IP, IP_MULTICAST_TTL,
				&iValue, sizeof(iValue)) == 0 ) { return XRT_NET_OK; }
		#endif
	} else if ( pSock->tLocalAddr.iFamily == AF_INET6 &&
		__xnetDgramSocketSetIntOption(pSock->hSocket, IPPROTO_IPV6,
			IPV6_MULTICAST_HOPS, iHopLimit) ) {
		return XRT_NET_OK;
	}
	__xnetErrorSetEx(XRT_NET_ERROR, XNET_OP_SEND, XNET_PHASE_SUBMIT,
		XNET_BACKEND_NONE, __xnetDgramSocketLastErr(), "unable to set datagram multicast hop limit.");
	return XRT_NET_ERROR;
}


XXAPI size_t xrtNetDgramPendingSend(const xdgramsock* pSock)
{
	int64 iValue = pSock ? __xnetAtomicLoad64(&pSock->iPendingSendBytes) : 0;
	return iValue > 0 ? (size_t)iValue : 0u;
}


XXAPI bool xrtNetDgramGetStats(const xdgramsock* pSock, xnetdgramstats* pOut)
{
	if ( !pSock || !pOut ) { return false; }
	memset(pOut, 0, sizeof(*pOut));
	pOut->iRecvPackets = (uint64)__xnetAtomicLoad64(&pSock->iRecvPackets);
	pOut->iRecvBytes = (uint64)__xnetAtomicLoad64(&pSock->iRecvBytes);
	pOut->iSendPackets = (uint64)__xnetAtomicLoad64(&pSock->iSendPackets);
	pOut->iSendBytes = (uint64)__xnetAtomicLoad64(&pSock->iSendBytes);
	pOut->iRecvDropped = (uint64)__xnetAtomicLoad64(&pSock->iRecvDropped);
	pOut->iSendAgain = (uint64)__xnetAtomicLoad64(&pSock->iSendAgain);
	pOut->iErrors = (uint64)__xnetAtomicLoad64(&pSock->iErrors);
	pOut->iPendingSendBytes = (uint64)__xnetAtomicLoad64(&pSock->iPendingSendBytes);
	pOut->iRecvQueued = (uint32)__xnetAtomicLoad32(&pSock->iRecvQueued);
	pOut->iRecvArmed = (uint32)__xnetAtomicLoad32(&pSock->iRecvArmed);
	return true;
}


// 内部函数：__xnetDgramOnPortEvents
static void __xnetDgramOnPortEvents(xnetworker* pWorker, const xnetportevent* pEvents, uint32 iCount)
{
	(void)pWorker;
	if ( !pEvents || iCount == 0 ) { return; }
	for ( uint32 i = 0; i < iCount; ++i ) {
		const xnetportevent* pEvent = &pEvents[i];
		// 处理数据报接收完成事件
		if ( pEvent->iType == XNET_PORT_EVENT_RECVFROM ) {
			xdgramsock* pSock = (xdgramsock*)pEvent->pUserData;
			#if defined(XNET_DEBUG_IOCP_NATIVE)
				if ( pSock && __xnetDgramUseNativePortIO(pSock) ) {
					fprintf(stderr, "[DGRAM] recvfrom event sock=%llu bytes=%u chain=%p status=%d\n",
						(unsigned long long)pSock->iId, pEvent->iBytes, (void*)pEvent->pChain, (int)pEvent->iStatus);
				}
			#endif
			if ( pSock ) {
				(void)__xnetAtomicAddFetch32(&pSock->iRecvArmed, -1);
				// 有数据链时根据接收模式进行投递
				if ( pEvent->pChain ) {
					(void)__xnetAtomicAddFetch64(&pSock->iRecvPackets, 1);
					(void)__xnetAtomicAddFetch64(&pSock->iRecvBytes, (int64)xrtNetChainBytes(pEvent->pChain));
					// 存在同步等待者时，构造数据包并直接通知
					if ( pSock->tRecvWait.pfnWait != NULL ) {
						xnetdgrampkt* pPacket = xrtNetDgramPacketCreate(&pEvent->tAddr, NULL, 0);
						if ( pPacket ) {
							__xnetChainSplice(&pPacket->tChain, pEvent->pChain);
							__xnetDgramNotifySyncRecv(pSock, XRT_NET_OK, pPacket);
						} else {
							__xnetDgramNotifySyncRecv(pSock, XRT_NET_ERROR, NULL);
						}
					} else if ( pSock->pEvents && pSock->pEvents->OnRecv ) {
						// 通过事件回调投递
						pSock->pEvents->OnRecv(__xnetDgramOwner(pSock), pSock, &pEvent->tAddr, pEvent->pChain);
					} else {
						xnetdgrampkt* pPacket = xrtNetDgramPacketCreate(&pEvent->tAddr, NULL, 0);
						if ( pPacket ) {
							__xnetChainSplice(&pPacket->tChain, pEvent->pChain);
							if ( !__xnetDgramQueuePush(pSock, pPacket) ) {
								xrtNetDgramPacketDestroy(pPacket);
							}
						} else if ( pSock->pEvents && pSock->pEvents->OnError ) {
							pSock->pEvents->OnError(__xnetDgramOwner(pSock), pSock, -1);
						}
					}
					__xnetDgramFreeTempChain(pEvent->pChain);
					// 释放数据链后重新挂起接收监听
					if ( pSock->bRunning && __xnetDgramSocketIsValid(pSock->hSocket) ) {
						(void)__xnetDgramArmRecvWatch(pSock);
					}
				} else {
					// 无数据链表示接收出错，通知同步等待者和错误回调
					if ( pEvent->iStatus != XRT_NET_OK && pSock->tRecvWait.pfnWait != NULL ) {
						__xnetDgramNotifyAllSyncRecv(pSock, pEvent->iStatus);
					}
					if ( pEvent->iStatus != XRT_NET_OK ) {
						(void)__xnetAtomicAddFetch64(&pSock->iErrors, 1);
					}
					if ( pEvent->iStatus != XRT_NET_OK && pSock->pEvents && pSock->pEvents->OnError ) {
						pSock->pEvents->OnError(__xnetDgramOwner(pSock), pSock, -1);
					}
					if ( pSock->bRunning && __xnetDgramSocketIsValid(pSock->hSocket) ) {
						(void)__xnetDgramArmRecvWatch(pSock);
					}
				}
				__xnetDgramReleaseIoHold(pSock);
			}
		// 处理端口错误事件
		} else if ( pEvent->iType == XNET_PORT_EVENT_ERROR ) {
			xdgramsock* pSock = (xdgramsock*)pEvent->pUserData;
			if ( pSock ) {
				(void)__xnetAtomicAddFetch64(&pSock->iErrors, 1);
				__xnetDgramNotifyAllSyncRecv(pSock, XRT_NET_ERROR);
			}
			if ( pSock && pSock->pEvents && pSock->pEvents->OnError ) {
				pSock->pEvents->OnError(__xnetDgramOwner(pSock), pSock, -1);
			}
		// 处理异步发送完成事件，释放持有计数和操作结构体
		} else if ( pEvent->iType == XNET_PORT_EVENT_SENDTO ) {
			__xnet_dgram_async_op* pOp = (__xnet_dgram_async_op*)pEvent->pUserData;
			xdgramsock* pSock = pOp ? pOp->pSock : NULL;
			if ( pSock && pEvent->iStatus != XRT_NET_OK && pSock->pEvents && pSock->pEvents->OnError ) {
				pSock->pEvents->OnError(__xnetDgramOwner(pSock), pSock, -1);
			}
			if ( pSock ) {
				if ( pEvent->iStatus == XRT_NET_OK ) {
					(void)__xnetAtomicAddFetch64(&pSock->iSendPackets, 1);
					(void)__xnetAtomicAddFetch64(&pSock->iSendBytes, pOp ? pOp->iLen : 0u);
				} else {
					(void)__xnetAtomicAddFetch64(&pSock->iErrors, 1);
				}
				if ( pOp && pOp->pfnDone ) {
					pOp->pfnDone(pSock, pEvent->iStatus, pOp->pDoneArg);
				}
				__xnetDgramReleaseSend(pSock, pOp ? pOp->iBudgetBytes : 0u);
				__xnetDgramReleaseAsyncHold(pSock);
			}
			if ( pOp ) {
				XNET_FREE(pOp);
			}
		}
	}
}


#endif

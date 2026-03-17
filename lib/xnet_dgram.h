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

typedef struct {
	xdgramsock* pSock;
	xnetaddr tTo;
	uint32 iType;
	uint32 iLen;
	uint8 aData[1];
} __xnet_dgram_async_op;

#if !defined(XRT_BUILD_CORE)
typedef struct xrt_net_dgram_packet {
	xnetaddr tFrom;
	xnetchain tChain;
} xnetdgrampkt;
#else
struct xrt_net_dgram_packet {
	xnetaddr tFrom;
	xnetchain tChain;
};
#endif

typedef void (*__xnet_dgram_sync_wait_fn)(xdgramsock* pSock, xnet_result iStatus, xnetdgrampkt* pPacket, ptr pCtx);

typedef struct {
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
	ptr pUserData;
	uint32 iFlags;
	uint32 iRecvBatch;
	uint32 iSendQueueLimit;
	volatile long iAsyncHoldCount;
	bool bRunning;
	bool bRecvArmed;
	__xnet_dgram_wait_slot tRecvWait;
};

static void __xnetDgramOnPortEvents(xnetworker* pWorker, const xnetportevent* pEvents, uint32 iCount);
static bool __xnetDgramSocketIsValid(xsocket hSocket);

static xnetworker* __xnetDgramPickWorker(xnetengine* pEngine, const xnetdgramconfig* pCfg)
{
	uint32 iIndex = 0;
	if ( !pEngine || pEngine->iWorkerCount == 0 ) return NULL;
	if ( pCfg && pCfg->tBindAddr.iPort > 0 ) {
		iIndex = pCfg->tBindAddr.iPort % pEngine->iWorkerCount;
	}
	return &pEngine->arrWorkers[iIndex];
}

static void __xnetDgramBindEngine(xnetengine* pEngine)
{
	if ( !pEngine ) return;
	if ( !pEngine->pfnOnPortEvent || pEngine->pfnOnPortEvent == __xnetDgramOnPortEvents ) {
		pEngine->pfnOnPortEvent = __xnetDgramOnPortEvents;
		return;
	}
	if ( !pEngine->pfnOnPortEvent2 || pEngine->pfnOnPortEvent2 == __xnetDgramOnPortEvents ) {
		pEngine->pfnOnPortEvent2 = __xnetDgramOnPortEvents;
	}
}

static ptr __xnetDgramOwner(xdgramsock* pSock)
{
	return pSock ? pSock->pUserData : NULL;
}

static void __xnetDgramSetError(const char* sError)
{
	#if defined(XXRTL_CORE)
		xrtSetError((str)sError, FALSE);
	#else
		(void)sError;
	#endif
}

static bool __xnetDgramUseNativePortIO(xdgramsock* pSock)
{
	#if defined(_WIN32) || defined(_WIN64)
		return pSock && pSock->pWorker &&
			pSock->pWorker->tPort.pOps == xrtNetPortIOCPOps() &&
			__xnetDgramSocketIsValid(pSock->hSocket);
	#elif defined(__linux__)
		return pSock && pSock->pWorker &&
			pSock->pWorker->tPort.pOps == xrtNetPortUringOps() &&
			__xnetPortUringHasNativeRing(&pSock->pWorker->tPort) &&
			__xnetDgramSocketIsValid(pSock->hSocket);
	#else
		(void)pSock;
		return false;
	#endif
}

static bool __xnetDgramUseNativePortOps(xdgramsock* pSock)
{
	#if defined(_WIN32) || defined(_WIN64)
		return pSock && pSock->pWorker &&
			pSock->pWorker->tPort.pOps == xrtNetPortIOCPOps();
	#elif defined(__linux__)
		return pSock && pSock->pWorker &&
			pSock->pWorker->tPort.pOps == xrtNetPortUringOps() &&
			__xnetPortUringHasNativeRing(&pSock->pWorker->tPort);
	#else
		(void)pSock;
		return false;
	#endif
}

static void __xnetDgramAddAsyncHold(xdgramsock* pSock)
{
	if ( !pSock ) return;
	(void)__xnetAtomicAddFetch32(&pSock->iAsyncHoldCount, 1);
}

static void __xnetDgramReleaseAsyncHold(xdgramsock* pSock)
{
	if ( !pSock ) return;
	(void)__xnetAtomicAddFetch32(&pSock->iAsyncHoldCount, -1);
}

XXAPI xnetdgrampkt* xrtNetDgramPacketCreate(const xnetaddr* pFrom, const void* pData, size_t iLen)
{
	xnetdgrampkt* pPacket;

	if ( !pFrom || (!pData && iLen > 0) ) return NULL;
	pPacket = (xnetdgrampkt*)XNET_ALLOC(sizeof(xnetdgrampkt));
	if ( !pPacket ) return NULL;
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

XXAPI void xrtNetDgramPacketDestroy(xnetdgrampkt* pPacket)
{
	if ( !pPacket ) return;
	xrtNetChainClear(&pPacket->tChain);
	XNET_FREE(pPacket);
}

XXAPI const xnetaddr* xrtNetDgramPacketFrom(const xnetdgrampkt* pPacket)
{
	return pPacket ? &pPacket->tFrom : NULL;
}

XXAPI size_t xrtNetDgramPacketBytes(const xnetdgrampkt* pPacket)
{
	return pPacket ? xrtNetChainBytes(&pPacket->tChain) : 0;
}

XXAPI size_t xrtNetDgramPacketPeek(const xnetdgrampkt* pPacket, ptr pOut, size_t iLen)
{
	return pPacket ? xrtNetChainPeek(&pPacket->tChain, pOut, iLen) : 0;
}

static void __xnetDgramNotifySyncRecv(xdgramsock* pSock, xnet_result iStatus, xnetdgrampkt* pPacket)
{
	__xnet_dgram_wait_slot* pSlot = NULL;
	__xnet_dgram_sync_wait_fn pfnWait = NULL;
	ptr pCtx = NULL;

	if ( !pSock ) {
		if ( pPacket ) xrtNetDgramPacketDestroy(pPacket);
		return;
	}

	pSlot = &pSock->tRecvWait;
	if ( pSlot->pfnWait == NULL ) {
		if ( pPacket ) xrtNetDgramPacketDestroy(pPacket);
		return;
	}

	pfnWait = pSlot->pfnWait;
	pCtx = pSlot->pCtx;
	pSlot->pfnWait = NULL;
	pSlot->pCtx = NULL;
	pfnWait(pSock, iStatus, pPacket, pCtx);
}

static bool __xnetDgramRegisterSyncRecvWait(xdgramsock* pSock, __xnet_dgram_sync_wait_fn pfnWait, ptr pCtx)
{
	if ( !pSock || !pfnWait ) return false;
	if ( pSock->pWorker && !__xnetEngineIsCurrentWorker(pSock->pWorker) ) return false;
	if ( pSock->tRecvWait.pfnWait != NULL ) return false;
	if ( pSock->pEvents && pSock->pEvents->OnRecv ) return false;
	if ( !pSock->bRunning || !__xnetDgramSocketIsValid(pSock->hSocket) ) {
		pfnWait(pSock, XRT_NET_CLOSED, NULL, pCtx);
		return true;
	}

	pSock->tRecvWait.pfnWait = pfnWait;
	pSock->tRecvWait.pCtx = pCtx;
	return true;
}

static bool __xnetDgramCancelSyncRecvWait(xdgramsock* pSock, ptr pCtx)
{
	if ( !pSock ) return false;
	if ( pSock->pWorker && !__xnetEngineIsCurrentWorker(pSock->pWorker) ) return false;
	if ( pSock->tRecvWait.pfnWait == NULL || pSock->tRecvWait.pCtx != pCtx ) return false;
	pSock->tRecvWait.pfnWait = NULL;
	pSock->tRecvWait.pCtx = NULL;
	return true;
}

static int __xnetDgramSocketLastErr(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return WSAGetLastError();
	#else
		return errno;
	#endif
}

static bool __xnetDgramSocketIsValid(xsocket hSocket)
{
	return hSocket != XNET_SOCKET_INVALID;
}

static xsocket __xnetDgramSocketCreate(int iFamily)
{
	#if defined(_WIN32) || defined(_WIN64)
		return WSASocket(iFamily, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
	#else
		return socket(iFamily, SOCK_DGRAM, IPPROTO_UDP);
	#endif
}

static void __xnetDgramSocketCloseHandle(xsocket* phSocket)
{
	if ( !phSocket || !__xnetDgramSocketIsValid(*phSocket) ) return;
	#if defined(_WIN32) || defined(_WIN64)
		closesocket(*phSocket);
	#else
		close(*phSocket);
	#endif
	*phSocket = XNET_SOCKET_INVALID;
}

static bool __xnetDgramSocketSetNonBlock(xsocket hSocket, bool bEnable)
{
	if ( !__xnetDgramSocketIsValid(hSocket) ) return false;
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

static bool __xnetDgramSocketSetReuseAddr(xsocket hSocket)
{
	int iOpt = 1;
	if ( !__xnetDgramSocketIsValid(hSocket) ) return false;
	#if defined(_WIN32) || defined(_WIN64)
		return setsockopt(hSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&iOpt, sizeof(iOpt)) == 0;
	#else
		return setsockopt(hSocket, SOL_SOCKET, SO_REUSEADDR, &iOpt, sizeof(iOpt)) == 0;
	#endif
}

static bool __xnetDgramSocketSetReusePort(xsocket hSocket)
{
	#if defined(SO_REUSEPORT)
		int iOpt = 1;
		if ( !__xnetDgramSocketIsValid(hSocket) ) return false;
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

static bool __xnetDgramSocketUpdateLocalAddr(xsocket hSocket, xnetaddr* pAddr)
{
	struct sockaddr_storage tStorage;
	socklen_t iLen = (socklen_t)sizeof(tStorage);
	if ( !__xnetDgramSocketIsValid(hSocket) || !pAddr ) return false;
	memset(&tStorage, 0, sizeof(tStorage));
	if ( getsockname(hSocket, (struct sockaddr*)&tStorage, &iLen) != 0 ) return false;
	return __xnetAddrFromSockAddr(pAddr, (const struct sockaddr*)&tStorage);
}

static void __xnetDgramApplyBindFlags(xsocket hSocket, uint32 iFlags)
{
	if ( !__xnetDgramSocketIsValid(hSocket) ) return;
	if ( (iFlags & XNET_DGRAM_F_REUSE_ADDR) != 0 ) {
		(void)__xnetDgramSocketSetReuseAddr(hSocket);
	}
	if ( (iFlags & XNET_DGRAM_F_REUSE_PORT) != 0 ) {
		(void)__xnetDgramSocketSetReusePort(hSocket);
	}
}

static xnetchain* __xnetDgramAllocTempChain(xdgramsock* pSock)
{
	xnetchain* pChain;
	xnetmemctx* pMemCtx;
	if ( !pSock || !pSock->pWorker ) return NULL;
	pChain = (xnetchain*)XNET_ALLOC(sizeof(xnetchain));
	if ( !pChain ) return NULL;
	pMemCtx = &pSock->pWorker->tMemCtx;
	xrtNetChainInitEx(pChain, pMemCtx);
	return pChain;
}

static void __xnetDgramFreeTempChain(xnetchain* pChain)
{
	if ( !pChain ) return;
	xrtNetChainClear(pChain);
	XNET_FREE(pChain);
}

static bool __xnetDgramSubmitSocketNotice(xdgramsock* pSock, uint16 iOpType, xsocket hSocket)
{
	xnetportsubmit tSubmit;
	if ( !pSock || !pSock->pWorker || !__xnetDgramSocketIsValid(hSocket) ) return false;
	memset(&tSubmit, 0, sizeof(tSubmit));
	tSubmit.iOpType = iOpType;
	tSubmit.iOpId = pSock->iId;
	tSubmit.hSocket = (intptr_t)hSocket;
	tSubmit.pUserData = pSock;
	return xrtNetPortSubmit(&pSock->pWorker->tPort, &tSubmit, 1) == XRT_NET_OK;
}

static bool __xnetDgramArmRecvWatch(xdgramsock* pSock)
{
	if ( !pSock || !pSock->bRunning || pSock->bRecvArmed || !__xnetDgramSocketIsValid(pSock->hSocket) ) return false;
	if ( !__xnetDgramSubmitSocketNotice(pSock, XNET_PORT_OP_RECVFROM, pSock->hSocket) ) return false;
	pSock->bRecvArmed = true;
	if ( pSock->pWorker && !__xnetEngineIsCurrentWorker(pSock->pWorker) ) {
		(void)xrtNetPortWake(&pSock->pWorker->tPort);
	}
	return true;
}

static void __xnetDgramFinalizeSocketClose(xdgramsock* pSock)
{
	xsocket hSocket;
	if ( !pSock ) return;
	hSocket = pSock->hSocket;
	pSock->bRecvArmed = false;
	if ( __xnetDgramSocketIsValid(hSocket) ) {
		(void)__xnetDgramSubmitSocketNotice(pSock, XNET_PORT_OP_CLOSE, hSocket);
		__xnetDgramSocketCloseHandle(&pSock->hSocket);
	}
}

static bool __xnetDgramDispatchPacket(xdgramsock* pSock, const xnetaddr* pFrom, const void* pData, size_t iLen)
{
	xnetchain* pChain;
	xnetdgrampkt* pPacket = NULL;
	if ( !pSock || !pFrom ) return false;
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
	pChain = __xnetDgramAllocTempChain(pSock);
	if ( !pChain ) return false;
	if ( iLen > 0 && (!pData || !xrtNetChainAppendCopy(pChain, pData, iLen)) ) {
		__xnetDgramFreeTempChain(pChain);
		return false;
	}
	if ( pSock->pEvents && pSock->pEvents->OnRecv ) {
		pSock->pEvents->OnRecv(__xnetDgramOwner(pSock), pSock, pFrom, pChain);
	}
	__xnetDgramFreeTempChain(pChain);
	return true;
}

static bool __xnetDgramSubmitNativeSend(xdgramsock* pSock, __xnet_dgram_async_op* pOp)
{
	xnetportsubmit tSubmit;
	xnetspan tSpan;
	if ( !pSock || !pOp || !pSock->pWorker || !__xnetDgramSocketIsValid(pSock->hSocket) ) return false;
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

static void __xnetDgramAsyncTask(xnetworker* pWorker, ptr pArg)
{
	__xnet_dgram_async_op* pOp = (__xnet_dgram_async_op*)pArg;
	xdgramsock* pSock = pOp ? pOp->pSock : NULL;
	(void)pWorker;
	if ( !pOp || !pSock ) {
		if ( pOp ) XNET_FREE(pOp);
		return;
	}

	if ( pOp->iType == __XNET_DGRAM_ASYNC_SEND_COPY ) {
		if ( !__xnetDgramUseNativePortIO(pSock) || !__xnetDgramSubmitNativeSend(pSock, pOp) ) {
			if ( pSock->pEvents && pSock->pEvents->OnError ) {
				pSock->pEvents->OnError(__xnetDgramOwner(pSock), pSock, -1);
			}
			__xnetDgramReleaseAsyncHold(pSock);
			XNET_FREE(pOp);
		}
		return;
	}
	__xnetDgramReleaseAsyncHold(pSock);
	XNET_FREE(pOp);
}

static __xnet_dgram_async_op* __xnetDgramAllocAsyncCopy(xdgramsock* pSock, const xnetaddr* pTo, const void* pData, size_t iLen)
{
	__xnet_dgram_async_op* pOp;
	size_t iSize;
	if ( !pSock || !pTo || (!pData && iLen > 0) || iLen > UINT32_MAX ) return NULL;
	iSize = sizeof(__xnet_dgram_async_op) + iLen;
	pOp = (__xnet_dgram_async_op*)XNET_ALLOC(iSize);
	if ( !pOp ) return NULL;
	memset(pOp, 0, iSize);
	pOp->pSock = pSock;
	pOp->tTo = *pTo;
	pOp->iType = __XNET_DGRAM_ASYNC_SEND_COPY;
	pOp->iLen = (uint32)iLen;
	if ( iLen > 0 ) memcpy(pOp->aData, pData, iLen);
	return pOp;
}

static __xnet_dgram_async_op* __xnetDgramAllocAsyncVecCopy(xdgramsock* pSock, const xnetaddr* pTo, const xnetspan* pVec, uint32 iCount)
{
	__xnet_dgram_async_op* pOp;
	size_t iTotal = 0;
	size_t iOffset = 0;
	size_t iSize;
	if ( !pSock || !pTo || !pVec || iCount == 0 ) return NULL;
	for ( uint32 i = 0; i < iCount; ++i ) {
		if ( !pVec[i].pData || pVec[i].iLen == 0 ) return NULL;
		iTotal += pVec[i].iLen;
		if ( iTotal > UINT32_MAX ) return NULL;
	}
	iSize = sizeof(__xnet_dgram_async_op) + iTotal;
	pOp = (__xnet_dgram_async_op*)XNET_ALLOC(iSize);
	if ( !pOp ) return NULL;
	memset(pOp, 0, iSize);
	pOp->pSock = pSock;
	pOp->tTo = *pTo;
	pOp->iType = __XNET_DGRAM_ASYNC_SEND_COPY;
	pOp->iLen = (uint32)iTotal;
	for ( uint32 i = 0; i < iCount; ++i ) {
		memcpy(pOp->aData + iOffset, pVec[i].pData, pVec[i].iLen);
		iOffset += pVec[i].iLen;
	}
	return pOp;
}

static xnet_result __xnetDgramPostAsync(xdgramsock* pSock, __xnet_dgram_async_op* pOp)
{
	if ( !pSock || !pOp || !pSock->pEngine || !pSock->pWorker ) {
		if ( pOp ) XNET_FREE(pOp);
		return XRT_NET_ERROR;
	}
	__xnetDgramAddAsyncHold(pSock);
	if ( xrtNetEnginePost(pSock->pEngine, pSock->pWorker->iId, __xnetDgramAsyncTask, pOp) != XRT_NET_OK ) {
		__xnetDgramReleaseAsyncHold(pSock);
		XNET_FREE(pOp);
		return XRT_NET_ERROR;
	}
	return XRT_NET_OK;
}

XXAPI xdgramsock* xrtNetDgramCreate(xnetengine* pEngine, const xnetdgramconfig* pCfg, const xnetdgramevents* pEvents, ptr pUserData)
{
	xdgramsock* pSock;
	xnetworker* pWorker;
	if ( !pEngine ) return NULL;
	__xnetDgramBindEngine(pEngine);
	pSock = (xdgramsock*)XNET_ALLOC(sizeof(xdgramsock));
	if ( !pSock ) return NULL;
	memset(pSock, 0, sizeof(xdgramsock));
	pWorker = __xnetDgramPickWorker(pEngine, pCfg);
	pSock->iId = pEngine->iNextStreamId++;
	pSock->pEngine = pEngine;
	pSock->pWorker = pWorker;
	pSock->pEvents = pEvents;
	pSock->pUserData = pUserData;
	pSock->hSocket = XNET_SOCKET_INVALID;
	if ( pCfg ) {
		pSock->tLocalAddr = pCfg->tBindAddr;
		pSock->iFlags = pCfg->iFlags;
		pSock->iRecvBatch = pCfg->iRecvBatch;
		pSock->iSendQueueLimit = pCfg->iSendQueueLimit;
	} else {
		xnetdgramconfig tCfg;
		xrtNetDgramConfigInit(&tCfg);
		pSock->tLocalAddr = tCfg.tBindAddr;
		pSock->iFlags = tCfg.iFlags;
		pSock->iRecvBatch = tCfg.iRecvBatch;
		pSock->iSendQueueLimit = tCfg.iSendQueueLimit;
	}
	return pSock;
}

XXAPI void xrtNetDgramDestroy(xdgramsock* pSock)
{
	if ( !pSock ) return;
	if ( __xnetAtomicLoad32(&pSock->iAsyncHoldCount) != 0 ) {
		__xnetDgramSetError("cannot destroy datagram socket while an async waiter or task still holds it.");
		return;
	}
	__xnetDgramNotifySyncRecv(pSock, XRT_NET_CLOSED, NULL);
	__xnetDgramFinalizeSocketClose(pSock);
	XNET_FREE(pSock);
}

XXAPI xnet_result xrtNetDgramStart(xdgramsock* pSock)
{
	struct sockaddr_storage tStorage;
	socklen_t iAddrLen = 0;
	if ( !pSock || !pSock->pEngine || !pSock->pEngine->bRunning ) return XRT_NET_ERROR;
	if ( pSock->bRunning ) return XRT_NET_OK;
	if ( !__xnetAddrToSockAddr(&pSock->tLocalAddr, &tStorage, &iAddrLen) ) return XRT_NET_ERROR;
	pSock->hSocket = __xnetDgramSocketCreate(pSock->tLocalAddr.iFamily);
	if ( !__xnetDgramSocketIsValid(pSock->hSocket) ) return XRT_NET_ERROR;
	__xnetDgramApplyBindFlags(pSock->hSocket, pSock->iFlags);
	(void)__xnetDgramSocketSetNonBlock(pSock->hSocket, true);
	if ( bind(pSock->hSocket, (struct sockaddr*)&tStorage, iAddrLen) != 0 ) {
		if ( pSock->pEvents && pSock->pEvents->OnError ) {
			pSock->pEvents->OnError(__xnetDgramOwner(pSock), pSock, __xnetDgramSocketLastErr());
		}
		__xnetDgramFinalizeSocketClose(pSock);
		return XRT_NET_ERROR;
	}
	if ( !__xnetDgramUseNativePortOps(pSock) ) {
		__xnetDgramSetError("mainline datagram sockets require a native xnet backend.");
		__xnetDgramFinalizeSocketClose(pSock);
		return XRT_NET_ERROR;
	}
	(void)__xnetDgramSocketSetNonBlock(pSock->hSocket, false);
	(void)__xnetDgramSocketUpdateLocalAddr(pSock->hSocket, &pSock->tLocalAddr);
	pSock->bRunning = true;
	(void)__xnetDgramArmRecvWatch(pSock);
	return XRT_NET_OK;
}

XXAPI void xrtNetDgramStop(xdgramsock* pSock)
{
	if ( !pSock ) return;
	if ( __xnetAtomicLoad32(&pSock->iAsyncHoldCount) != 0 ) {
		__xnetDgramSetError("cannot stop datagram socket while an async waiter or task still holds it.");
		return;
	}
	pSock->bRunning = false;
	__xnetDgramNotifySyncRecv(pSock, XRT_NET_CLOSED, NULL);
	__xnetDgramFinalizeSocketClose(pSock);
}

XXAPI xnet_result xrtNetDgramSendTo(xdgramsock* pSock, const xnetaddr* pTo, const void* pData, size_t iLen)
{
	if ( !pSock || !pSock->pEngine || !pSock->pEngine->bRunning || !pTo || (!pData && iLen > 0) ) return XRT_NET_ERROR;
	if ( !pSock->bRunning || !__xnetDgramSocketIsValid(pSock->hSocket) ) return XRT_NET_ERROR;
	return __xnetDgramPostAsync(pSock, __xnetDgramAllocAsyncCopy(pSock, pTo, pData, iLen));
}

XXAPI xnet_result xrtNetDgramSendVecTo(xdgramsock* pSock, const xnetaddr* pTo, const xnetspan* pVec, uint32 iCount)
{
	if ( !pSock || !pSock->pEngine || !pSock->pEngine->bRunning || !pTo || !pVec || iCount == 0 ) return XRT_NET_ERROR;
	if ( !pSock->bRunning || !__xnetDgramSocketIsValid(pSock->hSocket) ) return XRT_NET_ERROR;
	return __xnetDgramPostAsync(pSock, __xnetDgramAllocAsyncVecCopy(pSock, pTo, pVec, iCount));
}

static void __xnetDgramOnPortEvents(xnetworker* pWorker, const xnetportevent* pEvents, uint32 iCount)
{
	(void)pWorker;
	if ( !pEvents || iCount == 0 ) return;
	for ( uint32 i = 0; i < iCount; ++i ) {
		const xnetportevent* pEvent = &pEvents[i];
		if ( pEvent->iType == XNET_PORT_EVENT_RECVFROM ) {
			xdgramsock* pSock = (xdgramsock*)pEvent->pUserData;
			#if defined(XNET_DEBUG_IOCP_NATIVE)
				if ( pSock && __xnetDgramUseNativePortIO(pSock) ) {
					fprintf(stderr, "[DGRAM] recvfrom event sock=%llu bytes=%u chain=%p status=%d\n",
						(unsigned long long)pSock->iId, pEvent->iBytes, (void*)pEvent->pChain, (int)pEvent->iStatus);
				}
			#endif
			if ( pSock ) {
				pSock->bRecvArmed = false;
				if ( pEvent->pChain ) {
					if ( pSock->tRecvWait.pfnWait != NULL ) {
						xnetdgrampkt* pPacket = xrtNetDgramPacketCreate(&pEvent->tAddr, NULL, 0);
						if ( pPacket ) {
							__xnetChainSplice(&pPacket->tChain, pEvent->pChain);
							__xnetDgramNotifySyncRecv(pSock, XRT_NET_OK, pPacket);
						} else {
							__xnetDgramNotifySyncRecv(pSock, XRT_NET_ERROR, NULL);
						}
					} else if ( pSock->pEvents && pSock->pEvents->OnRecv ) {
						pSock->pEvents->OnRecv(__xnetDgramOwner(pSock), pSock, &pEvent->tAddr, pEvent->pChain);
					}
					__xnetDgramFreeTempChain(pEvent->pChain);
					if ( pSock->bRunning && __xnetDgramSocketIsValid(pSock->hSocket) && !pSock->bRecvArmed ) {
						(void)__xnetDgramArmRecvWatch(pSock);
					}
				} else {
					if ( pEvent->iStatus != XRT_NET_OK && pSock->tRecvWait.pfnWait != NULL ) {
						__xnetDgramNotifySyncRecv(pSock, pEvent->iStatus, NULL);
					}
					if ( pEvent->iStatus != XRT_NET_OK && pSock->pEvents && pSock->pEvents->OnError ) {
						pSock->pEvents->OnError(__xnetDgramOwner(pSock), pSock, -1);
					}
					if ( pSock->bRunning && __xnetDgramSocketIsValid(pSock->hSocket) && !pSock->bRecvArmed ) {
						(void)__xnetDgramArmRecvWatch(pSock);
					}
				}
			}
		} else if ( pEvent->iType == XNET_PORT_EVENT_ERROR ) {
			xdgramsock* pSock = (xdgramsock*)pEvent->pUserData;
			if ( pSock ) {
				__xnetDgramNotifySyncRecv(pSock, XRT_NET_ERROR, NULL);
			}
			if ( pSock && pSock->pEvents && pSock->pEvents->OnError ) {
				pSock->pEvents->OnError(__xnetDgramOwner(pSock), pSock, -1);
			}
		} else if ( pEvent->iType == XNET_PORT_EVENT_SENDTO ) {
			__xnet_dgram_async_op* pOp = (__xnet_dgram_async_op*)pEvent->pUserData;
			xdgramsock* pSock = pOp ? pOp->pSock : NULL;
			if ( pSock && pEvent->iStatus != XRT_NET_OK && pSock->pEvents && pSock->pEvents->OnError ) {
				pSock->pEvents->OnError(__xnetDgramOwner(pSock), pSock, -1);
			}
			if ( pSock ) {
				__xnetDgramReleaseAsyncHold(pSock);
			}
			if ( pOp ) {
				XNET_FREE(pOp);
			}
		}
	}
}


#endif

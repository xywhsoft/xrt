#ifndef XRT_XNET_DGRAM_H
#define XRT_XNET_DGRAM_H

#include "xnet_engine.h"

#if !defined(_WIN32) && !defined(_WIN64)
	#include <errno.h>
	#include <fcntl.h>
	#include <unistd.h>
#endif


typedef struct {
	void (*OnRecv)(ptr pOwner, xdgramsock* pSock, const xnetaddr* pFrom, xnetchain* pChain);
	void (*OnError)(ptr pOwner, xdgramsock* pSock, int iSysErr);
} xnetdgramevents;

#define __XNET_DGRAM_ASYNC_SEND_COPY  1u

typedef struct {
	xdgramsock* pSock;
	xnetaddr tTo;
	uint32 iType;
	uint32 iLen;
	uint8 aData[1];
} __xnet_dgram_async_op;

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
	bool bRunning;
	bool bRecvArmed;
};

static void __xnetDgramOnPortEvents(xnetworker* pWorker, const xnetportevent* pEvents, uint32 iCount);

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

static int __xnetDgramSocketLastErr(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return WSAGetLastError();
	#else
		return errno;
	#endif
}

static bool __xnetDgramSocketWouldBlock(int iErr)
{
	#if defined(_WIN32) || defined(_WIN64)
		return iErr == WSAEWOULDBLOCK || iErr == WSAEINPROGRESS || iErr == WSAEALREADY;
	#else
		return iErr == EINPROGRESS || iErr == EWOULDBLOCK || iErr == EAGAIN || iErr == EALREADY;
	#endif
}

static bool __xnetDgramSocketIsValid(xsocket hSocket)
{
	return hSocket != XNET_SOCKET_INVALID;
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
	if ( !pSock || !pFrom ) return false;
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

static bool __xnetDgramSocketPumpRecv(xdgramsock* pSock)
{
	char aBuf[8192];
	uint32 iPackets = 0;
	bool bReadAny = false;
	if ( !pSock || !__xnetDgramSocketIsValid(pSock->hSocket) ) return false;
	for ( ;; ) {
		struct sockaddr_storage tStorage;
		socklen_t iAddrLen = (socklen_t)sizeof(tStorage);
		xnetaddr tFrom;
		int iRecv;
		memset(&tStorage, 0, sizeof(tStorage));
		memset(&tFrom, 0, sizeof(tFrom));
		iRecv = recvfrom(pSock->hSocket, aBuf, sizeof(aBuf), 0, (struct sockaddr*)&tStorage, &iAddrLen);
		if ( iRecv >= 0 ) {
			bReadAny = true;
			(void)__xnetAddrFromSockAddr(&tFrom, (const struct sockaddr*)&tStorage);
			if ( !__xnetDgramDispatchPacket(pSock, &tFrom, aBuf, (size_t)iRecv) ) {
				return bReadAny;
			}
			iPackets++;
			if ( pSock->iRecvBatch > 0 && iPackets >= pSock->iRecvBatch ) break;
			continue;
		}
		{
			int iErr = __xnetDgramSocketLastErr();
			if ( __xnetDgramSocketWouldBlock(iErr) ) break;
			if ( pSock->pEvents && pSock->pEvents->OnError ) {
				pSock->pEvents->OnError(__xnetDgramOwner(pSock), pSock, iErr);
			}
			break;
		}
	}
	if ( pSock->bRunning && __xnetDgramSocketIsValid(pSock->hSocket) ) {
		(void)__xnetDgramArmRecvWatch(pSock);
	}
	return bReadAny;
}

static xnet_result __xnetDgramSocketSendTo(xdgramsock* pSock, const xnetaddr* pTo, const void* pData, size_t iLen)
{
	struct sockaddr_storage tStorage;
	socklen_t iAddrLen = 0;
	int iSent;
	if ( !pSock || !pTo || !__xnetDgramSocketIsValid(pSock->hSocket) || (!pData && iLen > 0) ) return XRT_NET_ERROR;
	if ( !__xnetAddrToSockAddr(pTo, &tStorage, &iAddrLen) ) return XRT_NET_ERROR;
	iSent = sendto(pSock->hSocket, (const char*)pData, (int)iLen, 0, (const struct sockaddr*)&tStorage, iAddrLen);
	if ( iSent >= 0 ) return XRT_NET_OK;
	if ( pSock->pEvents && pSock->pEvents->OnError ) {
		pSock->pEvents->OnError(__xnetDgramOwner(pSock), pSock, __xnetDgramSocketLastErr());
	}
	return XRT_NET_ERROR;
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
		(void)__xnetDgramSocketSendTo(pSock, &pOp->tTo, pOp->aData, pOp->iLen);
	}
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
	if ( xrtNetEnginePost(pSock->pEngine, pSock->pWorker->iId, __xnetDgramAsyncTask, pOp) != XRT_NET_OK ) {
		XNET_FREE(pOp);
		return XRT_NET_ERROR;
	}
	return XRT_NET_OK;
}

static xdgramsock* xrtNetDgramCreate(xnetengine* pEngine, const xnetdgramconfig* pCfg, const xnetdgramevents* pEvents, ptr pUserData)
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

static void xrtNetDgramDestroy(xdgramsock* pSock)
{
	if ( !pSock ) return;
	__xnetDgramFinalizeSocketClose(pSock);
	XNET_FREE(pSock);
}

static xnet_result xrtNetDgramStart(xdgramsock* pSock)
{
	struct sockaddr_storage tStorage;
	socklen_t iAddrLen = 0;
	if ( !pSock || !pSock->pEngine || !pSock->pEngine->bRunning ) return XRT_NET_ERROR;
	if ( pSock->bRunning ) return XRT_NET_OK;
	if ( !__xnetAddrToSockAddr(&pSock->tLocalAddr, &tStorage, &iAddrLen) ) return XRT_NET_ERROR;
	pSock->hSocket = socket(pSock->tLocalAddr.iFamily, SOCK_DGRAM, IPPROTO_UDP);
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
	(void)__xnetDgramSocketUpdateLocalAddr(pSock->hSocket, &pSock->tLocalAddr);
	pSock->bRunning = true;
	(void)__xnetDgramArmRecvWatch(pSock);
	return XRT_NET_OK;
}

static void xrtNetDgramStop(xdgramsock* pSock)
{
	if ( !pSock ) return;
	pSock->bRunning = false;
	__xnetDgramFinalizeSocketClose(pSock);
}

static xnet_result xrtNetDgramSendTo(xdgramsock* pSock, const xnetaddr* pTo, const void* pData, size_t iLen)
{
	if ( !pSock || !pSock->pEngine || !pSock->pEngine->bRunning || !pTo || (!pData && iLen > 0) ) return XRT_NET_ERROR;
	if ( !pSock->bRunning || !__xnetDgramSocketIsValid(pSock->hSocket) ) return XRT_NET_ERROR;
	return __xnetDgramPostAsync(pSock, __xnetDgramAllocAsyncCopy(pSock, pTo, pData, iLen));
}

static xnet_result xrtNetDgramSendVecTo(xdgramsock* pSock, const xnetaddr* pTo, const xnetspan* pVec, uint32 iCount)
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
			if ( pSock ) {
				pSock->bRecvArmed = false;
				(void)__xnetDgramSocketPumpRecv(pSock);
			}
		} else if ( pEvent->iType == XNET_PORT_EVENT_ERROR ) {
			xdgramsock* pSock = (xdgramsock*)pEvent->pUserData;
			if ( pSock && pSock->pEvents && pSock->pEvents->OnError ) {
				pSock->pEvents->OnError(__xnetDgramOwner(pSock), pSock, -1);
			}
		}
	}
}


#endif

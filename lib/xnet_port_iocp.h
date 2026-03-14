#ifndef XRT_XNET_PORT_IOCP_H
#define XRT_XNET_PORT_IOCP_H

#include "xnet_port.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>

	typedef struct __xnet_iocp_post {
		OVERLAPPED tOverlapped;
		xnetportevent tEvent;
	} __xnet_iocp_post;

	typedef struct __xnet_iocp_timer {
		struct __xnet_iocp_timer* pNext;
		uint64 iTimerId;
		uint64 iDueMs;
	} __xnet_iocp_timer;

	typedef struct __xnet_iocp_watch {
		struct __xnet_iocp_watch* pNext;
		uint16 iOpType;
		uint16 iReserved;
		uint64 iOpId;
		intptr_t hSocket;
		ptr pUserData;
		xnetaddr tAddr;
	} __xnet_iocp_watch;

	typedef struct {
		HANDLE hIOCP;
		__xnet_iocp_timer* pTimers;
		__xnet_iocp_watch* pWatches;
		__xnet_iocp_watch* pFreeWatches;
		CRITICAL_SECTION tWatchLock;
	} __xnet_iocp_ctx;

	/*
	    The staged Windows backend routes posts through IOCP but socket readiness still
	    uses watch/select. A long watch slice directly inflates serialized echo latency,
	    especially for TLS small-message loops, so keep the slice short.
	*/
	#define __XNET_IOCP_WATCH_SLICE_MS 1u

	static uint16 __xnetPortIOCPEventType(uint16 iOpType)
	{
		switch ( iOpType ) {
			case XNET_PORT_OP_ACCEPT: return XNET_PORT_EVENT_ACCEPT;
			case XNET_PORT_OP_CONNECT: return XNET_PORT_EVENT_CONNECT;
			case XNET_PORT_OP_RECV: return XNET_PORT_EVENT_RECV;
			case XNET_PORT_OP_SEND: return XNET_PORT_EVENT_SEND;
			case XNET_PORT_OP_RECVFROM: return XNET_PORT_EVENT_RECVFROM;
			case XNET_PORT_OP_SENDTO: return XNET_PORT_EVENT_SENDTO;
			case XNET_PORT_OP_TIMER: return XNET_PORT_EVENT_TIMER;
			case XNET_PORT_OP_WAKE: return XNET_PORT_EVENT_WAKE;
			case XNET_PORT_OP_CLOSE: return XNET_PORT_EVENT_CLOSE;
			default: return XNET_PORT_EVENT_NONE;
		}
	}

	static uint32 __xnetPortIOCPSubmitBytes(const xnetportsubmit* pOp)
	{
		uint64 iBytes = 0;
		if ( !pOp ) return 0;
		if ( pOp->pVec && pOp->iVecCount > 0 ) {
			for ( uint32 i = 0; i < pOp->iVecCount; ++i ) {
				iBytes += pOp->pVec[i].iLen;
			}
		} else if ( pOp->pChain ) {
			iBytes = xrtNetChainBytes(pOp->pChain);
		}
		if ( iBytes > 0xffffffffu ) iBytes = 0xffffffffu;
		return (uint32)iBytes;
	}

	static uint64 __xnetPortIOCPNowMs(void)
	{
		return (uint64)GetTickCount64();
	}

	static bool __xnetPortIOCPValidOp(uint16 iType)
	{
		switch ( iType ) {
			case XNET_PORT_OP_ACCEPT:
			case XNET_PORT_OP_CONNECT:
			case XNET_PORT_OP_RECV:
			case XNET_PORT_OP_SEND:
			case XNET_PORT_OP_RECVFROM:
			case XNET_PORT_OP_SENDTO:
			case XNET_PORT_OP_TIMER:
			case XNET_PORT_OP_WAKE:
			case XNET_PORT_OP_CLOSE:
				return true;
			default:
				return false;
		}
	}

	static __xnet_iocp_post* __xnetPortIOCPAllocPost(uint16 iType, uint16 iFlags, xnet_result iStatus, uint32 iBytes, uint64 iOpId)
	{
		__xnet_iocp_post* pPost = (__xnet_iocp_post*)XNET_ALLOC(sizeof(__xnet_iocp_post));
		if ( !pPost ) return NULL;
		memset(pPost, 0, sizeof(__xnet_iocp_post));
		pPost->tEvent.iType = iType;
		pPost->tEvent.iFlags = iFlags;
		pPost->tEvent.iStatus = iStatus;
		pPost->tEvent.iBytes = iBytes;
		pPost->tEvent.iOpId = iOpId;
		return pPost;
	}

	static bool __xnetPortIOCPIsSocketWatchOp(const xnetportsubmit* pOp)
	{
		if ( !pOp || pOp->hSocket == (intptr_t)XNET_SOCKET_INVALID ) return false;
		if ( pOp->iOpId == 0 ) return false;
		if ( pOp->pChain || (pOp->pVec && pOp->iVecCount > 0) ) return false;
		return pOp->iOpType == XNET_PORT_OP_ACCEPT || pOp->iOpType == XNET_PORT_OP_RECV ||
			pOp->iOpType == XNET_PORT_OP_RECVFROM || pOp->iOpType == XNET_PORT_OP_SEND;
	}

	static void __xnetPortIOCPFreeWatches(__xnet_iocp_ctx* pCtx)
	{
		if ( !pCtx ) return;
		EnterCriticalSection(&pCtx->tWatchLock);
		while ( pCtx->pWatches ) {
			__xnet_iocp_watch* pNext = pCtx->pWatches->pNext;
			XNET_FREE(pCtx->pWatches);
			pCtx->pWatches = pNext;
		}
		while ( pCtx->pFreeWatches ) {
			__xnet_iocp_watch* pNext = pCtx->pFreeWatches->pNext;
			XNET_FREE(pCtx->pFreeWatches);
			pCtx->pFreeWatches = pNext;
		}
		LeaveCriticalSection(&pCtx->tWatchLock);
	}

	static void __xnetPortIOCPRemoveWatches(__xnet_iocp_ctx* pCtx, uint16 iOpType, intptr_t hSocket, ptr pUserData)
	{
		__xnet_iocp_watch** ppCur;
		if ( !pCtx ) return;
		EnterCriticalSection(&pCtx->tWatchLock);
		ppCur = &pCtx->pWatches;
		while ( *ppCur ) {
			__xnet_iocp_watch* pNode = *ppCur;
			bool bTypeMatch = (iOpType == 0 || pNode->iOpType == iOpType);
			bool bSocketMatch = (hSocket == 0 || pNode->hSocket == hSocket);
			bool bUserMatch = (pUserData == NULL || pNode->pUserData == pUserData);
			if ( bTypeMatch && bSocketMatch && bUserMatch ) {
				*ppCur = pNode->pNext;
				pNode->pNext = pCtx->pFreeWatches;
				pCtx->pFreeWatches = pNode;
				continue;
			}
			ppCur = &pNode->pNext;
		}
		LeaveCriticalSection(&pCtx->tWatchLock);
	}

	static bool __xnetPortIOCPRegisterWatch(__xnet_iocp_ctx* pCtx, const xnetportsubmit* pOp)
	{
		__xnet_iocp_watch* pNode;
		if ( !pCtx || !pOp ) return false;
		EnterCriticalSection(&pCtx->tWatchLock);
		for ( __xnet_iocp_watch* pCur = pCtx->pWatches; pCur; pCur = pCur->pNext ) {
			if ( pCur->iOpType == pOp->iOpType && pCur->hSocket == pOp->hSocket && pCur->pUserData == pOp->pUserData ) {
				LeaveCriticalSection(&pCtx->tWatchLock);
				return true;
			}
		}
		if ( pCtx->pFreeWatches ) {
			pNode = pCtx->pFreeWatches;
			pCtx->pFreeWatches = pNode->pNext;
		} else {
			pNode = (__xnet_iocp_watch*)XNET_ALLOC(sizeof(__xnet_iocp_watch));
		}
		if ( !pNode ) {
			LeaveCriticalSection(&pCtx->tWatchLock);
			return false;
		}
		memset(pNode, 0, sizeof(__xnet_iocp_watch));
		pNode->iOpType = pOp->iOpType;
		pNode->iOpId = pOp->iOpId;
		pNode->hSocket = pOp->hSocket;
		pNode->pUserData = pOp->pUserData;
		pNode->tAddr = pOp->tAddr;
		pNode->pNext = pCtx->pWatches;
		pCtx->pWatches = pNode;
		LeaveCriticalSection(&pCtx->tWatchLock);
		return true;
	}

	static bool __xnetPortIOCPHasWatches(__xnet_iocp_ctx* pCtx)
	{
		bool bHas = false;
		if ( !pCtx ) return false;
		EnterCriticalSection(&pCtx->tWatchLock);
		bHas = pCtx->pWatches != NULL;
		LeaveCriticalSection(&pCtx->tWatchLock);
		return bHas;
	}

	static uint32 __xnetPortIOCPHarvestPosted(__xnet_iocp_ctx* pCtx, xnetportevent* pEvents, uint32 iMaxEvents, uint32 iWaitMs)
	{
		uint32 iCount = 0;
		if ( !pCtx || !pEvents || iMaxEvents == 0 ) return 0;
		for ( ;; ) {
			DWORD iBytes = 0;
			ULONG_PTR iKey = 0;
			LPOVERLAPPED pOv = NULL;
			DWORD iWait = (iCount > 0) ? 0 : iWaitMs;
			BOOL bOk = GetQueuedCompletionStatus(pCtx->hIOCP, &iBytes, &iKey, &pOv, iWait);
			if ( !bOk && !pOv ) break;
			if ( !pOv ) break;
			{
				__xnet_iocp_post* pPost = (__xnet_iocp_post*)pOv;
				pPost->tEvent.iBytes = iBytes;
				if ( !bOk && pPost->tEvent.iStatus == XRT_NET_OK ) {
					pPost->tEvent.iStatus = XRT_NET_ERROR;
				}
				pEvents[iCount++] = pPost->tEvent;
				XNET_FREE(pPost);
			}
			(void)iKey;
			if ( iCount >= iMaxEvents ) break;
		}
		return iCount;
	}

	static uint32 __xnetPortIOCPHarvestWatches(__xnet_iocp_ctx* pCtx, xnetportevent* pEvents, uint32 iMaxEvents, uint32 iTimeoutMs)
	{
		__xnet_iocp_watch arrSnap[FD_SETSIZE];
		uint32 iSnapCount = 0;
		uint32 iCount = 0;
		fd_set tReadSet;
		fd_set tWriteSet;
		struct timeval tTv;
		int iReady;
		if ( !pCtx || !pEvents || iMaxEvents == 0 ) return 0;

		FD_ZERO(&tReadSet);
		FD_ZERO(&tWriteSet);
		EnterCriticalSection(&pCtx->tWatchLock);
		for ( __xnet_iocp_watch* pNode = pCtx->pWatches; pNode && iSnapCount < FD_SETSIZE; pNode = pNode->pNext ) {
			arrSnap[iSnapCount] = *pNode;
			if ( pNode->iOpType == XNET_PORT_OP_SEND ) {
				FD_SET((SOCKET)pNode->hSocket, &tWriteSet);
			} else {
				FD_SET((SOCKET)pNode->hSocket, &tReadSet);
			}
			iSnapCount++;
		}
		LeaveCriticalSection(&pCtx->tWatchLock);
		if ( iSnapCount == 0 ) return 0;

		tTv.tv_sec = (long)(iTimeoutMs / 1000u);
		tTv.tv_usec = (long)((iTimeoutMs % 1000u) * 1000u);
		iReady = select(0, &tReadSet, &tWriteSet, NULL, &tTv);
		if ( iReady <= 0 ) return 0;

		for ( uint32 i = 0; i < iSnapCount && iCount < iMaxEvents; ++i ) {
			bool bReady = (arrSnap[i].iOpType == XNET_PORT_OP_SEND)
				? (FD_ISSET((SOCKET)arrSnap[i].hSocket, &tWriteSet) != 0)
				: (FD_ISSET((SOCKET)arrSnap[i].hSocket, &tReadSet) != 0);
			if ( !bReady ) continue;
			memset(&pEvents[iCount], 0, sizeof(xnetportevent));
			pEvents[iCount].iType = __xnetPortIOCPEventType(arrSnap[i].iOpType);
			pEvents[iCount].iStatus = XRT_NET_OK;
			pEvents[iCount].iOpId = arrSnap[i].iOpId;
			pEvents[iCount].hSocket = arrSnap[i].hSocket;
			pEvents[iCount].pUserData = arrSnap[i].pUserData;
			pEvents[iCount].tAddr = arrSnap[i].tAddr;
			iCount++;
			__xnetPortIOCPRemoveWatches(pCtx, arrSnap[i].iOpType, arrSnap[i].hSocket, arrSnap[i].pUserData);
		}

		return iCount;
	}

	static void __xnetPortIOCPFreeTimers(__xnet_iocp_ctx* pCtx)
	{
		while ( pCtx && pCtx->pTimers ) {
			__xnet_iocp_timer* pNext = pCtx->pTimers->pNext;
			XNET_FREE(pCtx->pTimers);
			pCtx->pTimers = pNext;
		}
	}

	static uint32 __xnetPortIOCPHarvestTimers(__xnet_iocp_ctx* pCtx, xnetportevent* pEvents, uint32 iMaxEvents)
	{
		uint32 iCount = 0;
		uint64 iNowMs = __xnetPortIOCPNowMs();
		__xnet_iocp_timer** ppCur = pCtx ? &pCtx->pTimers : NULL;
		while ( ppCur && *ppCur && iCount < iMaxEvents ) {
			__xnet_iocp_timer* pNode = *ppCur;
			if ( pNode->iDueMs > iNowMs ) {
				ppCur = &pNode->pNext;
				continue;
			}
			memset(&pEvents[iCount], 0, sizeof(xnetportevent));
			pEvents[iCount].iType = XNET_PORT_EVENT_TIMER;
			pEvents[iCount].iStatus = XRT_NET_OK;
			pEvents[iCount].iOpId = pNode->iTimerId;
			iCount++;
			*ppCur = pNode->pNext;
			XNET_FREE(pNode);
		}
		return iCount;
	}

	static void __xnetPortIOCPDrainPosted(__xnet_iocp_ctx* pCtx)
	{
		if ( !pCtx || !pCtx->hIOCP ) return;
		for ( ;; ) {
			DWORD iBytes = 0;
			ULONG_PTR iKey = 0;
			LPOVERLAPPED pOv = NULL;
			BOOL bOk = GetQueuedCompletionStatus(pCtx->hIOCP, &iBytes, &iKey, &pOv, 0);
			if ( !bOk && !pOv ) break;
			if ( pOv ) {
				__xnet_iocp_post* pPost = (__xnet_iocp_post*)pOv;
				XNET_FREE(pPost);
			}
		}
	}

	static xnet_result __xnetPortIOCPInit(xnetport* pPort, const xnetportconfig* pCfg, ptr pOwner)
	{
		(void)pOwner;
		if ( !pPort || !pCfg ) return XRT_NET_ERROR;
		__xnet_iocp_ctx* pCtx = (__xnet_iocp_ctx*)XNET_ALLOC(sizeof(__xnet_iocp_ctx));
		if ( !pCtx ) return XRT_NET_ERROR;
		memset(pCtx, 0, sizeof(__xnet_iocp_ctx));
		InitializeCriticalSection(&pCtx->tWatchLock);
		pCtx->hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
		if ( !pCtx->hIOCP ) {
			DeleteCriticalSection(&pCtx->tWatchLock);
			XNET_FREE(pCtx);
			return XRT_NET_ERROR;
		}
		pPort->pCtx = pCtx;
		return XRT_NET_OK;
	}

	static void __xnetPortIOCPUnit(xnetport* pPort)
	{
		__xnet_iocp_ctx* pCtx = pPort ? (__xnet_iocp_ctx*)pPort->pCtx : NULL;
		if ( !pCtx ) return;
		__xnetPortIOCPDrainPosted(pCtx);
		__xnetPortIOCPFreeTimers(pCtx);
		__xnetPortIOCPFreeWatches(pCtx);
		if ( pCtx->hIOCP ) {
			CloseHandle(pCtx->hIOCP);
		}
		DeleteCriticalSection(&pCtx->tWatchLock);
		XNET_FREE(pCtx);
		if ( pPort ) pPort->pCtx = NULL;
	}

	static xnet_result __xnetPortIOCPSumbit(xnetport* pPort, const xnetportsubmit* pOps, uint32 iCount)
	{
		uint32 i;
		__xnet_iocp_ctx* pCtx = pPort ? (__xnet_iocp_ctx*)pPort->pCtx : NULL;
		if ( !pCtx || !pCtx->hIOCP || !pOps || iCount == 0 ) return XRT_NET_ERROR;
		for ( i = 0; i < iCount; ++i ) {
			__xnet_iocp_post* pPost;
			uint16 iEventType;
			if ( !__xnetPortIOCPValidOp(pOps[i].iOpType) ) {
				return XRT_NET_ERROR;
			}
			iEventType = __xnetPortIOCPEventType(pOps[i].iOpType);
			if ( iEventType == XNET_PORT_EVENT_NONE ) {
				return XRT_NET_ERROR;
			}
			if ( pOps[i].iOpType == XNET_PORT_OP_CLOSE && pOps[i].hSocket != (intptr_t)XNET_SOCKET_INVALID ) {
				__xnetPortIOCPRemoveWatches(pCtx, 0, pOps[i].hSocket, pOps[i].pUserData);
				continue;
			}
			if ( __xnetPortIOCPIsSocketWatchOp(&pOps[i]) ) {
				if ( !__xnetPortIOCPRegisterWatch(pCtx, &pOps[i]) ) return XRT_NET_ERROR;
				continue;
			}
			pPost = __xnetPortIOCPAllocPost(iEventType, XNET_PORT_EVENT_F_NONE, XRT_NET_OK,
				__xnetPortIOCPSubmitBytes(&pOps[i]), pOps[i].iOpId);
			if ( !pPost ) return XRT_NET_ERROR;
			pPost->tEvent.hSocket = pOps[i].hSocket;
			pPost->tEvent.pUserData = pOps[i].pUserData;
			pPost->tEvent.pChain = pOps[i].pChain;
			pPost->tEvent.tAddr = pOps[i].tAddr;
			if ( !PostQueuedCompletionStatus(pCtx->hIOCP, pPost->tEvent.iBytes, 0, &pPost->tOverlapped) ) {
				XNET_FREE(pPost);
				return XRT_NET_ERROR;
			}
		}
		return XRT_NET_OK;
	}

	static uint32 __xnetPortIOCPHarvest(xnetport* pPort, xnetportevent* pEvents, uint32 iMaxEvents, uint32 iTimeoutMs)
	{
		uint32 iCount = 0;
		uint64 iStartMs;
		__xnet_iocp_ctx* pCtx = pPort ? (__xnet_iocp_ctx*)pPort->pCtx : NULL;
		if ( !pCtx || !pEvents || iMaxEvents == 0 ) return 0;

		iCount += __xnetPortIOCPHarvestTimers(pCtx, pEvents + iCount, iMaxEvents - iCount);
		if ( iCount >= iMaxEvents ) return iCount;
		iCount += __xnetPortIOCPHarvestPosted(pCtx, pEvents + iCount, iMaxEvents - iCount, 0);
		if ( iCount >= iMaxEvents || iCount > 0 || iTimeoutMs == 0 ) return iCount;
		if ( !__xnetPortIOCPHasWatches(pCtx) ) {
			iCount += __xnetPortIOCPHarvestPosted(pCtx, pEvents + iCount, iMaxEvents - iCount, iTimeoutMs);
			return iCount;
		}

		iStartMs = __xnetPortIOCPNowMs();
		for ( ;; ) {
			uint64 iNowMs = __xnetPortIOCPNowMs();
			uint32 iElapsedMs = (uint32)(iNowMs - iStartMs);
			uint32 iRemainMs = (iElapsedMs >= iTimeoutMs) ? 0u : (iTimeoutMs - iElapsedMs);
			uint32 iSliceMs = iRemainMs > __XNET_IOCP_WATCH_SLICE_MS ? __XNET_IOCP_WATCH_SLICE_MS : iRemainMs;
			if ( iRemainMs == 0 ) break;
			iCount += __xnetPortIOCPHarvestWatches(pCtx, pEvents + iCount, iMaxEvents - iCount, iSliceMs);
			if ( iCount >= iMaxEvents || iCount > 0 ) break;
			iCount += __xnetPortIOCPHarvestPosted(pCtx, pEvents + iCount, iMaxEvents - iCount, iSliceMs);
			if ( iCount >= iMaxEvents || iCount > 0 ) break;
		}
		return iCount;
	}

	static xnet_result __xnetPortIOCPWake(xnetport* pPort)
	{
		__xnet_iocp_ctx* pCtx = pPort ? (__xnet_iocp_ctx*)pPort->pCtx : NULL;
		__xnet_iocp_post* pPost;
		if ( !pCtx || !pCtx->hIOCP ) return XRT_NET_ERROR;
		pPost = __xnetPortIOCPAllocPost(XNET_PORT_EVENT_WAKE, XNET_PORT_EVENT_F_NONE, XRT_NET_OK, 0, 0);
		if ( !pPost ) return XRT_NET_ERROR;
		if ( !PostQueuedCompletionStatus(pCtx->hIOCP, 0, 0, &pPost->tOverlapped) ) {
			XNET_FREE(pPost);
			return XRT_NET_ERROR;
		}
		return XRT_NET_OK;
	}

	static xnet_result __xnetPortIOCPArmTimer(xnetport* pPort, uint64 iTimerId, uint32 iDelayMs)
	{
		__xnet_iocp_ctx* pCtx = pPort ? (__xnet_iocp_ctx*)pPort->pCtx : NULL;
		__xnet_iocp_timer* pNode;
		if ( !pCtx ) return XRT_NET_ERROR;
		pNode = (__xnet_iocp_timer*)XNET_ALLOC(sizeof(__xnet_iocp_timer));
		if ( !pNode ) return XRT_NET_ERROR;
		memset(pNode, 0, sizeof(__xnet_iocp_timer));
		pNode->iTimerId = iTimerId;
		pNode->iDueMs = __xnetPortIOCPNowMs() + (uint64)iDelayMs;
		pNode->pNext = pCtx->pTimers;
		pCtx->pTimers = pNode;
		return XRT_NET_OK;
	}

	static xnet_result __xnetPortIOCPCancelTimer(xnetport* pPort, uint64 iTimerId)
	{
		__xnet_iocp_ctx* pCtx = pPort ? (__xnet_iocp_ctx*)pPort->pCtx : NULL;
		__xnet_iocp_timer** ppCur = pCtx ? &pCtx->pTimers : NULL;
		if ( !ppCur ) return XRT_NET_ERROR;
		while ( *ppCur ) {
			__xnet_iocp_timer* pNode = *ppCur;
			if ( pNode->iTimerId == iTimerId ) {
				*ppCur = pNode->pNext;
				XNET_FREE(pNode);
				return XRT_NET_OK;
			}
			ppCur = &pNode->pNext;
		}
		return XRT_NET_ERROR;
	}

	static const xnetportops __g_xnetPortIOCPOps = {
		"xnet-port-iocp",
		__xnetPortIOCPInit,
		__xnetPortIOCPUnit,
		__xnetPortIOCPSumbit,
		__xnetPortIOCPHarvest,
		__xnetPortIOCPWake,
		__xnetPortIOCPArmTimer,
		__xnetPortIOCPCancelTimer
	};

	static const xnetportops* xrtNetPortIOCPOps(void)
	{
		return &__g_xnetPortIOCPOps;
	}

#else

	static const xnetportops* xrtNetPortIOCPOps(void)
	{
		return NULL;
	}

#endif


#endif

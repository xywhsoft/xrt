#ifndef XRT_XNET_PORT_URING_H
#define XRT_XNET_PORT_URING_H

#include "xnet_port.h"

#if defined(__linux__)
	#include <errno.h>
	#include <poll.h>
	#include <pthread.h>
	#include <sys/eventfd.h>
	#include <unistd.h>

	typedef struct __xnet_uring_timer {
		struct __xnet_uring_timer* pNext;
		uint64 iTimerId;
		uint64 iDueMs;
	} __xnet_uring_timer;

	typedef struct __xnet_uring_post {
		struct __xnet_uring_post* pNext;
		xnetportevent tEvent;
	} __xnet_uring_post;

	typedef struct __xnet_uring_watch {
		struct __xnet_uring_watch* pNext;
		uint16 iOpType;
		uint16 iReserved;
		uint64 iOpId;
		intptr_t hSocket;
		ptr pUserData;
		xnetaddr tAddr;
	} __xnet_uring_watch;

	typedef struct {
		int hRing;
		int hWakeFd;
		__xnet_uring_timer* pTimers;
		__xnet_uring_post* pPostedHead;
		__xnet_uring_post* pPostedTail;
		pthread_mutex_t tPostedLock;
		__xnet_uring_watch* pWatches;
		pthread_mutex_t tWatchLock;
	} __xnet_uring_ctx;

	static xnet_result __xnetPortUringWake(xnetport* pPort);

	static uint16 __xnetPortUringEventType(uint16 iOpType)
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

	static uint32 __xnetPortUringSubmitBytes(const xnetportsubmit* pOp)
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

	static uint64 __xnetPortUringNowMs(void)
	{
		struct timespec tNow;
		clock_gettime(CLOCK_MONOTONIC, &tNow);
		return (uint64)tNow.tv_sec * 1000u + (uint64)(tNow.tv_nsec / 1000000u);
	}

	static bool __xnetPortUringValidOp(uint16 iType)
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

	static void __xnetPortUringFreeTimers(__xnet_uring_ctx* pCtx)
	{
		while ( pCtx && pCtx->pTimers ) {
			__xnet_uring_timer* pNext = pCtx->pTimers->pNext;
			XNET_FREE(pCtx->pTimers);
			pCtx->pTimers = pNext;
		}
	}

	static void __xnetPortUringFreePosts(__xnet_uring_ctx* pCtx)
	{
		if ( !pCtx ) return;
		while ( pCtx->pPostedHead ) {
			__xnet_uring_post* pNext = pCtx->pPostedHead->pNext;
			XNET_FREE(pCtx->pPostedHead);
			pCtx->pPostedHead = pNext;
		}
		pCtx->pPostedTail = NULL;
	}

	static bool __xnetPortUringQueueEvent(__xnet_uring_ctx* pCtx, const xnetportevent* pEvent)
	{
		__xnet_uring_post* pPost;
		if ( !pCtx || !pEvent ) return false;
		pPost = (__xnet_uring_post*)XNET_ALLOC(sizeof(__xnet_uring_post));
		if ( !pPost ) return false;
		memset(pPost, 0, sizeof(__xnet_uring_post));
		pPost->tEvent = *pEvent;
		pthread_mutex_lock(&pCtx->tPostedLock);
		if ( pCtx->pPostedTail ) {
			pCtx->pPostedTail->pNext = pPost;
		} else {
			pCtx->pPostedHead = pPost;
		}
		pCtx->pPostedTail = pPost;
		pthread_mutex_unlock(&pCtx->tPostedLock);
		return true;
	}

	static bool __xnetPortUringIsSocketWatchOp(const xnetportsubmit* pOp)
	{
		if ( !pOp || pOp->hSocket == (intptr_t)XNET_SOCKET_INVALID ) return false;
		if ( pOp->iOpId == 0 ) return false;
		if ( pOp->pChain || (pOp->pVec && pOp->iVecCount > 0) ) return false;
		return pOp->iOpType == XNET_PORT_OP_ACCEPT || pOp->iOpType == XNET_PORT_OP_RECV ||
			pOp->iOpType == XNET_PORT_OP_RECVFROM || pOp->iOpType == XNET_PORT_OP_SEND;
	}

	static void __xnetPortUringFreeWatches(__xnet_uring_ctx* pCtx)
	{
		if ( !pCtx ) return;
		pthread_mutex_lock(&pCtx->tWatchLock);
		while ( pCtx->pWatches ) {
			__xnet_uring_watch* pNext = pCtx->pWatches->pNext;
			XNET_FREE(pCtx->pWatches);
			pCtx->pWatches = pNext;
		}
		pthread_mutex_unlock(&pCtx->tWatchLock);
	}

	static void __xnetPortUringRemoveWatches(__xnet_uring_ctx* pCtx, uint16 iOpType, intptr_t hSocket, ptr pUserData)
	{
		__xnet_uring_watch** ppCur;
		if ( !pCtx ) return;
		pthread_mutex_lock(&pCtx->tWatchLock);
		ppCur = &pCtx->pWatches;
		while ( *ppCur ) {
			__xnet_uring_watch* pNode = *ppCur;
			bool bTypeMatch = (iOpType == 0 || pNode->iOpType == iOpType);
			bool bSocketMatch = (hSocket == 0 || pNode->hSocket == hSocket);
			bool bUserMatch = (pUserData == NULL || pNode->pUserData == pUserData);
			if ( bTypeMatch && bSocketMatch && bUserMatch ) {
				*ppCur = pNode->pNext;
				XNET_FREE(pNode);
				continue;
			}
			ppCur = &pNode->pNext;
		}
		pthread_mutex_unlock(&pCtx->tWatchLock);
	}

	static bool __xnetPortUringRegisterWatch(__xnet_uring_ctx* pCtx, const xnetportsubmit* pOp)
	{
		__xnet_uring_watch* pNode;
		if ( !pCtx || !pOp ) return false;
		pNode = (__xnet_uring_watch*)XNET_ALLOC(sizeof(__xnet_uring_watch));
		if ( !pNode ) return false;
		memset(pNode, 0, sizeof(__xnet_uring_watch));
		pNode->iOpType = pOp->iOpType;
		pNode->iOpId = pOp->iOpId;
		pNode->hSocket = pOp->hSocket;
		pNode->pUserData = pOp->pUserData;
		pNode->tAddr = pOp->tAddr;

		pthread_mutex_lock(&pCtx->tWatchLock);
		for ( __xnet_uring_watch* pCur = pCtx->pWatches; pCur; pCur = pCur->pNext ) {
			if ( pCur->iOpType == pNode->iOpType && pCur->hSocket == pNode->hSocket && pCur->pUserData == pNode->pUserData ) {
				pthread_mutex_unlock(&pCtx->tWatchLock);
				XNET_FREE(pNode);
				return true;
			}
		}
		pNode->pNext = pCtx->pWatches;
		pCtx->pWatches = pNode;
		pthread_mutex_unlock(&pCtx->tWatchLock);
		return true;
	}

	static bool __xnetPortUringHasWatches(__xnet_uring_ctx* pCtx)
	{
		bool bHas = false;
		if ( !pCtx ) return false;
		pthread_mutex_lock(&pCtx->tWatchLock);
		bHas = pCtx->pWatches != NULL;
		pthread_mutex_unlock(&pCtx->tWatchLock);
		return bHas;
	}

	static uint32 __xnetPortUringDrainPosted(__xnet_uring_ctx* pCtx, xnetportevent* pEvents, uint32 iMaxEvents)
	{
		uint32 iCount = 0;
		__xnet_uring_post* pHead;
		__xnet_uring_post* pTail;
		if ( !pCtx || !pEvents || iMaxEvents == 0 ) return 0;
		pthread_mutex_lock(&pCtx->tPostedLock);
		pHead = pCtx->pPostedHead;
		pTail = pCtx->pPostedTail;
		pCtx->pPostedHead = NULL;
		pCtx->pPostedTail = NULL;
		pthread_mutex_unlock(&pCtx->tPostedLock);
		(void)pTail;

		while ( pHead && iCount < iMaxEvents ) {
			__xnet_uring_post* pNext = pHead->pNext;
			pEvents[iCount++] = pHead->tEvent;
			XNET_FREE(pHead);
			pHead = pNext;
		}

		while ( pHead ) {
			__xnet_uring_post* pNext = pHead->pNext;
			pthread_mutex_lock(&pCtx->tPostedLock);
			if ( pCtx->pPostedTail ) {
				pCtx->pPostedTail->pNext = pHead;
			} else {
				pCtx->pPostedHead = pHead;
			}
			pCtx->pPostedTail = pHead;
			pHead->pNext = NULL;
			pthread_mutex_unlock(&pCtx->tPostedLock);
			pHead = pNext;
		}

		return iCount;
	}

	static uint32 __xnetPortUringHarvestTimers(__xnet_uring_ctx* pCtx, xnetportevent* pEvents, uint32 iMaxEvents)
	{
		uint32 iCount = 0;
		uint64 iNowMs = __xnetPortUringNowMs();
		__xnet_uring_timer** ppCur = pCtx ? &pCtx->pTimers : NULL;
		while ( ppCur && *ppCur && iCount < iMaxEvents ) {
			__xnet_uring_timer* pNode = *ppCur;
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

	static xnet_result __xnetPortUringInit(xnetport* pPort, const xnetportconfig* pCfg, ptr pOwner)
	{
		__xnet_uring_ctx* pCtx;
		(void)pOwner;
		if ( !pPort || !pCfg ) return XRT_NET_ERROR;
		pCtx = (__xnet_uring_ctx*)XNET_ALLOC(sizeof(__xnet_uring_ctx));
		if ( !pCtx ) return XRT_NET_ERROR;
		memset(pCtx, 0, sizeof(__xnet_uring_ctx));
		pCtx->hRing = -1;
		pCtx->hWakeFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
		if ( pCtx->hWakeFd < 0 ) {
			XNET_FREE(pCtx);
			return XRT_NET_ERROR;
		}
		if ( pthread_mutex_init(&pCtx->tPostedLock, NULL) != 0 ) {
			close(pCtx->hWakeFd);
			XNET_FREE(pCtx);
			return XRT_NET_ERROR;
		}
		if ( pthread_mutex_init(&pCtx->tWatchLock, NULL) != 0 ) {
			pthread_mutex_destroy(&pCtx->tPostedLock);
			close(pCtx->hWakeFd);
			XNET_FREE(pCtx);
			return XRT_NET_ERROR;
		}
		pPort->pCtx = pCtx;
		return XRT_NET_OK;
	}

	static void __xnetPortUringUnit(xnetport* pPort)
	{
		__xnet_uring_ctx* pCtx = pPort ? (__xnet_uring_ctx*)pPort->pCtx : NULL;
		if ( !pCtx ) return;
		__xnetPortUringFreeTimers(pCtx);
		__xnetPortUringFreePosts(pCtx);
		__xnetPortUringFreeWatches(pCtx);
		pthread_mutex_destroy(&pCtx->tPostedLock);
		pthread_mutex_destroy(&pCtx->tWatchLock);
		if ( pCtx->hWakeFd >= 0 ) {
			close(pCtx->hWakeFd);
		}
		XNET_FREE(pCtx);
		if ( pPort ) pPort->pCtx = NULL;
	}

	static xnet_result __xnetPortUringSubmit(xnetport* pPort, const xnetportsubmit* pOps, uint32 iCount)
	{
		uint32 i;
		__xnet_uring_ctx* pCtx = pPort ? (__xnet_uring_ctx*)pPort->pCtx : NULL;
		if ( !pCtx || !pOps || iCount == 0 ) return XRT_NET_ERROR;
		for ( i = 0; i < iCount; ++i ) {
			xnetportevent tEvent;
			if ( !__xnetPortUringValidOp(pOps[i].iOpType) ) {
				return XRT_NET_ERROR;
			}
			if ( pOps[i].iOpType == XNET_PORT_OP_CLOSE && pOps[i].hSocket != (intptr_t)XNET_SOCKET_INVALID ) {
				__xnetPortUringRemoveWatches(pCtx, 0, pOps[i].hSocket, pOps[i].pUserData);
				continue;
			}
			if ( __xnetPortUringIsSocketWatchOp(&pOps[i]) ) {
				if ( !__xnetPortUringRegisterWatch(pCtx, &pOps[i]) ) return XRT_NET_ERROR;
				continue;
			}
			memset(&tEvent, 0, sizeof(tEvent));
			tEvent.iType = __xnetPortUringEventType(pOps[i].iOpType);
			tEvent.iStatus = XRT_NET_OK;
			tEvent.iBytes = __xnetPortUringSubmitBytes(&pOps[i]);
			tEvent.iOpId = pOps[i].iOpId;
			tEvent.hSocket = pOps[i].hSocket;
			tEvent.pUserData = pOps[i].pUserData;
			tEvent.pChain = pOps[i].pChain;
			tEvent.tAddr = pOps[i].tAddr;
			if ( tEvent.iType == XNET_PORT_EVENT_NONE || !__xnetPortUringQueueEvent(pCtx, &tEvent) ) {
				return XRT_NET_ERROR;
			}
		}
		(void)__xnetPortUringWake(pPort);
		return XRT_NET_OK;
	}

	static uint32 __xnetPortUringHarvest(xnetport* pPort, xnetportevent* pEvents, uint32 iMaxEvents, uint32 iTimeoutMs)
	{
		uint32 iCount = 0;
		__xnet_uring_ctx* pCtx = pPort ? (__xnet_uring_ctx*)pPort->pCtx : NULL;
		if ( !pCtx || !pEvents || iMaxEvents == 0 ) return 0;

		iCount += __xnetPortUringHarvestTimers(pCtx, pEvents + iCount, iMaxEvents - iCount);
		if ( iCount >= iMaxEvents ) return iCount;

		iCount += __xnetPortUringDrainPosted(pCtx, pEvents + iCount, iMaxEvents - iCount);
		if ( iCount >= iMaxEvents ) return iCount;

		if ( !__xnetPortUringHasWatches(pCtx) && pCtx->hWakeFd >= 0 ) {
			struct pollfd tPoll;
			int iPollRet;
			uint64 iWakeCount = 0;
			memset(&tPoll, 0, sizeof(tPoll));
			tPoll.fd = pCtx->hWakeFd;
			tPoll.events = POLLIN;
			iPollRet = poll(&tPoll, 1, (iCount > 0) ? 0 : (int)iTimeoutMs);
			if ( iPollRet > 0 && (tPoll.revents & POLLIN) ) {
				if ( read(pCtx->hWakeFd, &iWakeCount, sizeof(iWakeCount)) == (ssize_t)sizeof(iWakeCount) ) {
					uint32 iPosted = __xnetPortUringDrainPosted(pCtx, pEvents + iCount, iMaxEvents - iCount);
					iCount += iPosted;
					if ( iPosted == 0 && iCount < iMaxEvents ) {
						memset(&pEvents[iCount], 0, sizeof(xnetportevent));
						pEvents[iCount].iType = XNET_PORT_EVENT_WAKE;
						pEvents[iCount].iStatus = XRT_NET_OK;
						pEvents[iCount].iBytes = (uint32)((iWakeCount > 0xffffffffu) ? 0xffffffffu : iWakeCount);
						iCount++;
					}
				}
			}
		}

		if ( __xnetPortUringHasWatches(pCtx) ) {
			__xnet_uring_watch arrSnap[256];
			struct pollfd arrPoll[257];
			nfds_t iPollCount = 0;
			uint32 iSnapCount = 0;
			int iPollRet;

			memset(arrPoll, 0, sizeof(arrPoll));
			pthread_mutex_lock(&pCtx->tWatchLock);
			for ( __xnet_uring_watch* pNode = pCtx->pWatches; pNode && iSnapCount < 256; pNode = pNode->pNext ) {
				arrSnap[iSnapCount] = *pNode;
				++iSnapCount;
			}
			pthread_mutex_unlock(&pCtx->tWatchLock);

			if ( pCtx->hWakeFd >= 0 ) {
				arrPoll[iPollCount].fd = pCtx->hWakeFd;
				arrPoll[iPollCount].events = POLLIN;
				++iPollCount;
			}
			for ( uint32 i = 0; i < iSnapCount; ++i ) {
				arrPoll[iPollCount].fd = (int)arrSnap[i].hSocket;
				arrPoll[iPollCount].events = (short)((arrSnap[i].iOpType == XNET_PORT_OP_SEND) ? POLLOUT : POLLIN);
				++iPollCount;
			}

			if ( iPollCount > 0 ) {
				iPollRet = poll(arrPoll, iPollCount, (iCount > 0) ? 0 : (int)iTimeoutMs);
				if ( iPollRet > 0 ) {
					if ( pCtx->hWakeFd >= 0 && (arrPoll[0].revents & POLLIN) ) {
						uint64 iWakeCount = 0;
						if ( read(pCtx->hWakeFd, &iWakeCount, sizeof(iWakeCount)) == (ssize_t)sizeof(iWakeCount) ) {
							uint32 iPosted = __xnetPortUringDrainPosted(pCtx, pEvents + iCount, iMaxEvents - iCount);
							iCount += iPosted;
							if ( iPosted == 0 && iCount < iMaxEvents ) {
								memset(&pEvents[iCount], 0, sizeof(xnetportevent));
								pEvents[iCount].iType = XNET_PORT_EVENT_WAKE;
								pEvents[iCount].iStatus = XRT_NET_OK;
								pEvents[iCount].iBytes = (uint32)((iWakeCount > 0xffffffffu) ? 0xffffffffu : iWakeCount);
								iCount++;
							}
						}
					}
					for ( uint32 i = 0; i < iSnapCount && iCount < iMaxEvents; ++i ) {
						nfds_t iIndex = (pCtx->hWakeFd >= 0) ? (i + 1u) : i;
						short iWant = (short)((arrSnap[i].iOpType == XNET_PORT_OP_SEND) ? POLLOUT : POLLIN);
						if ( (arrPoll[iIndex].revents & iWant) == 0 ) continue;
						memset(&pEvents[iCount], 0, sizeof(xnetportevent));
						pEvents[iCount].iType = __xnetPortUringEventType(arrSnap[i].iOpType);
						pEvents[iCount].iStatus = XRT_NET_OK;
						pEvents[iCount].iOpId = arrSnap[i].iOpId;
						pEvents[iCount].hSocket = arrSnap[i].hSocket;
						pEvents[iCount].pUserData = arrSnap[i].pUserData;
						pEvents[iCount].tAddr = arrSnap[i].tAddr;
						iCount++;
						__xnetPortUringRemoveWatches(pCtx, arrSnap[i].iOpType, arrSnap[i].hSocket, arrSnap[i].pUserData);
					}
				}
			}
		}

		return iCount;
	}

	static xnet_result __xnetPortUringWake(xnetport* pPort)
	{
		__xnet_uring_ctx* pCtx = pPort ? (__xnet_uring_ctx*)pPort->pCtx : NULL;
		uint64 iWakeCount = 1;
		if ( !pCtx || pCtx->hWakeFd < 0 ) return XRT_NET_ERROR;
		if ( write(pCtx->hWakeFd, &iWakeCount, sizeof(iWakeCount)) != (ssize_t)sizeof(iWakeCount) ) {
			if ( errno == EAGAIN ) return XRT_NET_OK;
			return XRT_NET_ERROR;
		}
		return XRT_NET_OK;
	}

	static xnet_result __xnetPortUringArmTimer(xnetport* pPort, uint64 iTimerId, uint32 iDelayMs)
	{
		__xnet_uring_ctx* pCtx = pPort ? (__xnet_uring_ctx*)pPort->pCtx : NULL;
		__xnet_uring_timer* pNode;
		if ( !pCtx ) return XRT_NET_ERROR;
		pNode = (__xnet_uring_timer*)XNET_ALLOC(sizeof(__xnet_uring_timer));
		if ( !pNode ) return XRT_NET_ERROR;
		memset(pNode, 0, sizeof(__xnet_uring_timer));
		pNode->iTimerId = iTimerId;
		pNode->iDueMs = __xnetPortUringNowMs() + (uint64)iDelayMs;
		pNode->pNext = pCtx->pTimers;
		pCtx->pTimers = pNode;
		return XRT_NET_OK;
	}

	static xnet_result __xnetPortUringCancelTimer(xnetport* pPort, uint64 iTimerId)
	{
		__xnet_uring_ctx* pCtx = pPort ? (__xnet_uring_ctx*)pPort->pCtx : NULL;
		__xnet_uring_timer** ppCur = pCtx ? &pCtx->pTimers : NULL;
		if ( !ppCur ) return XRT_NET_ERROR;
		while ( *ppCur ) {
			__xnet_uring_timer* pNode = *ppCur;
			if ( pNode->iTimerId == iTimerId ) {
				*ppCur = pNode->pNext;
				XNET_FREE(pNode);
				return XRT_NET_OK;
			}
			ppCur = &pNode->pNext;
		}
		return XRT_NET_ERROR;
	}

	static const xnetportops __g_xnetPortUringOps = {
		"xnet-port-uring",
		__xnetPortUringInit,
		__xnetPortUringUnit,
		__xnetPortUringSubmit,
		__xnetPortUringHarvest,
		__xnetPortUringWake,
		__xnetPortUringArmTimer,
		__xnetPortUringCancelTimer
	};

	static const xnetportops* xrtNetPortUringOps(void)
	{
		return &__g_xnetPortUringOps;
	}

#else

	static const xnetportops* xrtNetPortUringOps(void)
	{
		return NULL;
	}

#endif


#endif

#ifndef XRT_XNET_PORT_SELECT_H
#define XRT_XNET_PORT_SELECT_H

#if !defined(_WIN32) && !defined(_WIN64)

	#define __XNET_SELECT_INLINE_RECV 8192u
	#define __XNET_SELECT_MAX_IOVEC   16u

	typedef struct __xnet_select_timer {
		struct __xnet_select_timer* pNext;
		uint64 iTimerId;
		uint64 iDueMs;
	} __xnet_select_timer;

	typedef struct __xnet_select_post {
		struct __xnet_select_post* pNext;
		xnetportevent tEvent;
	} __xnet_select_post;

	typedef struct __xnet_select_watch {
		struct __xnet_select_watch* pNext;
		int hSocket;
		uint32 iMask;
		xnetportsubmit tAccept;
		xnetportsubmit tConnect;
		xnetportsubmit tRecv;
		xnetportsubmit tSend;
		xnetportsubmit tRecvFrom;
		xnetportsubmit tSendTo;
		xnetspan arrSendVec[__XNET_SELECT_MAX_IOVEC];
		xnetspan arrSendToVec[__XNET_SELECT_MAX_IOVEC];
	} __xnet_select_watch;

	typedef struct {
		int arrWake[2];
		__xnet_select_watch* pWatches;
		__xnet_select_timer* pTimers;
		__xnet_select_post* pPostedHead;
		__xnet_select_post* pPostedTail;
		pthread_mutex_t tPostedLock;
		pthread_mutex_t tWatchLock;
	} __xnet_select_ctx;

	#define __XNET_SELECT_W_ACCEPT   0x00000001u
	#define __XNET_SELECT_W_CONNECT  0x00000002u
	#define __XNET_SELECT_W_RECV     0x00000004u
	#define __XNET_SELECT_W_SEND     0x00000008u
	#define __XNET_SELECT_W_RECVFROM 0x00000010u
	#define __XNET_SELECT_W_SENDTO   0x00000020u

	#define __XNET_SELECT_READY_READ  0x00000001u
	#define __XNET_SELECT_READY_WRITE 0x00000002u
	#define __XNET_SELECT_READY_ERROR 0x00000004u


	// 内部函数：判断 select 后端是否可用
	static bool UNUSED_ATTR __xnetPortSelectReady(const xnetport* pPort)
	{
		const __xnet_select_ctx* pCtx = pPort ? (const __xnet_select_ctx*)pPort->pCtx : NULL;
		return pCtx != NULL;
	}


	// 内部函数：select now 毫秒相关处理
	static uint64 __xnetPortSelectNowMs(void)
	{
		struct timespec tNow;
		clock_gettime(CLOCK_MONOTONIC, &tNow);
		return (uint64)tNow.tv_sec * 1000u + (uint64)(tNow.tv_nsec / 1000000u);
	}


	// 内部函数：设置 fd close-on-exec
	static void __xnetPortSelectSetCloseOnExec(int hFd)
	{
		int iFlags;
		if ( hFd < 0 ) { return; }
		iFlags = fcntl(hFd, F_GETFD, 0);
		if ( iFlags >= 0 ) {
			(void)fcntl(hFd, F_SETFD, iFlags | FD_CLOEXEC);
		}
	}


	// 内部函数：设置 fd nonblock
	static void __xnetPortSelectSetNonBlock(int hFd)
	{
		int iFlags;
		if ( hFd < 0 ) { return; }
		iFlags = fcntl(hFd, F_GETFL, 0);
		if ( iFlags >= 0 ) {
			(void)fcntl(hFd, F_SETFL, iFlags | O_NONBLOCK);
		}
	}



	// 内部函数：端口 select 事件 type相关处理
	static uint16 __xnetPortSelectEventType(uint16 iOpType)
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


	// 内部函数：端口 select valid op相关处理
	static bool __xnetPortSelectValidOp(uint16 iType)
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


	// 内部函数：分配端口 select 事件 chain
	static xnetchain* __xnetPortSelectAllocEventChain(const void* pData, size_t iLen)
	{
		xnetchain* pChain = NULL;
		if ( (!pData && iLen > 0) || iLen > UINT32_MAX ) { return NULL; }
		pChain = (xnetchain*)XNET_ALLOC(sizeof(xnetchain));
		if ( !pChain ) { return NULL; }
		xrtNetChainInit(pChain);
		if ( iLen > 0 && !xrtNetChainAppendCopy(pChain, pData, iLen) ) {
			xrtNetChainClear(pChain);
			XNET_FREE(pChain);
			return NULL;
		}
		return pChain;
	}


	// 内部函数：提交端口 select bytes
	static uint32 __xnetPortSelectSubmitBytes(const xnetportsubmit* pOp)
	{
		uint64 iBytes = 0;
		if ( !pOp ) { return 0; }
		if ( pOp->pVec && pOp->iVecCount > 0 ) {
			for ( uint32 i = 0; i < pOp->iVecCount; ++i ) {
				iBytes += pOp->pVec[i].iLen;
			}
		} else if ( pOp->pChain ) {
			iBytes = xrtNetChainBytes(pOp->pChain);
		}
		if ( iBytes > 0xffffffffu ) { iBytes = 0xffffffffu; }
		return (uint32)iBytes;
	}


	// 内部函数：释放端口 select timers
	static void __xnetPortSelectFreeTimers(__xnet_select_ctx* pCtx)
	{
		while ( pCtx && pCtx->pTimers ) {
			__xnet_select_timer* pNext = pCtx->pTimers->pNext;
			XNET_FREE(pCtx->pTimers);
			pCtx->pTimers = pNext;
		}
	}


	// 内部函数：释放端口 select posts
	static void __xnetPortSelectFreePosts(__xnet_select_ctx* pCtx)
	{
		if ( !pCtx ) { return; }
		while ( pCtx->pPostedHead ) {
			__xnet_select_post* pNext = pCtx->pPostedHead->pNext;
			XNET_FREE(pCtx->pPostedHead);
			pCtx->pPostedHead = pNext;
		}
		pCtx->pPostedTail = NULL;
	}


	// 内部函数：释放端口 select watches
	static void __xnetPortSelectFreeWatches(__xnet_select_ctx* pCtx)
	{
		if ( !pCtx ) { return; }
		while ( pCtx->pWatches ) {
			__xnet_select_watch* pNext = pCtx->pWatches->pNext;
			XNET_FREE(pCtx->pWatches);
			pCtx->pWatches = pNext;
		}
	}


	// 内部函数：端口 select 队列事件相关处理
	static bool __xnetPortSelectQueueEvent(__xnet_select_ctx* pCtx, const xnetportevent* pEvent)
	{
		__xnet_select_post* pPost;
		if ( !pCtx || !pEvent ) { return false; }
		pPost = (__xnet_select_post*)XNET_ALLOC(sizeof(__xnet_select_post));
		if ( !pPost ) { return false; }
		memset(pPost, 0, sizeof(__xnet_select_post));
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


	// 内部函数：提取端口 select posted events
	static uint32 __xnetPortSelectDrainPosted(__xnet_select_ctx* pCtx, xnetportevent* pEvents, uint32 iMaxEvents)
	{
		uint32 iCount = 0;
		__xnet_select_post* pHead;
		if ( !pCtx || !pEvents || iMaxEvents == 0 ) { return 0; }
		pthread_mutex_lock(&pCtx->tPostedLock);
		pHead = pCtx->pPostedHead;
		pCtx->pPostedHead = NULL;
		pCtx->pPostedTail = NULL;
		pthread_mutex_unlock(&pCtx->tPostedLock);
		while ( pHead && iCount < iMaxEvents ) {
			__xnet_select_post* pNext = pHead->pNext;
			pEvents[iCount++] = pHead->tEvent;
			XNET_FREE(pHead);
			pHead = pNext;
		}
		while ( pHead ) {
			__xnet_select_post* pNext = pHead->pNext;
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


	// 内部函数：提取端口 select timers
	static uint32 __xnetPortSelectHarvestTimers(__xnet_select_ctx* pCtx, xnetportevent* pEvents, uint32 iMaxEvents)
	{
		uint32 iCount = 0;
		uint64 iNowMs = __xnetPortSelectNowMs();
		__xnet_select_timer** ppCur = pCtx ? &pCtx->pTimers : NULL;
		while ( ppCur && *ppCur && iCount < iMaxEvents ) {
			__xnet_select_timer* pNode = *ppCur;
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


	static uint32 __xnetPortSelectTimerWait(__xnet_select_ctx* pCtx, uint32 iTimeoutMs)
	{
		__xnet_select_timer* pNode = pCtx ? pCtx->pTimers : NULL;
		uint64 iNextDueMs = UINT64_MAX;
		uint64 iNowMs;
		uint64 iDelayMs;
		while ( pNode ) {
			if ( pNode->iDueMs < iNextDueMs ) { iNextDueMs = pNode->iDueMs; }
			pNode = pNode->pNext;
		}
		if ( iNextDueMs == UINT64_MAX ) { return iTimeoutMs; }
		iNowMs = __xnetPortSelectNowMs();
		if ( iNextDueMs <= iNowMs ) { return 0u; }
		iDelayMs = iNextDueMs - iNowMs;
		return iDelayMs < (uint64)iTimeoutMs ? (uint32)iDelayMs : iTimeoutMs;
	}


	// 内部函数：查找端口 select watch
	static __xnet_select_watch* __xnetPortSelectFindWatch(__xnet_select_ctx* pCtx, int hSocket)
	{
		__xnet_select_watch* pWatch = pCtx ? pCtx->pWatches : NULL;
		while ( pWatch ) {
			if ( pWatch->hSocket == hSocket ) { return pWatch; }
			pWatch = pWatch->pNext;
		}
		return NULL;
	}


	// 内部函数：获取端口 select watch
	static __xnet_select_watch* __xnetPortSelectGetWatch(__xnet_select_ctx* pCtx, int hSocket)
	{
		__xnet_select_watch* pWatch;
		if ( !pCtx || hSocket < 0 ) { return NULL; }
		pWatch = __xnetPortSelectFindWatch(pCtx, hSocket);
		if ( pWatch ) { return pWatch; }
		pWatch = (__xnet_select_watch*)XNET_ALLOC(sizeof(__xnet_select_watch));
		if ( !pWatch ) { return NULL; }
		memset(pWatch, 0, sizeof(__xnet_select_watch));
		pWatch->hSocket = hSocket;
		pWatch->pNext = pCtx->pWatches;
		pCtx->pWatches = pWatch;
		return pWatch;
	}



	// 内部函数：刷新端口 select watch
	static bool __xnetPortSelectRefreshWatch(__xnet_select_ctx* pCtx, __xnet_select_watch* pWatch)
	{
		return pCtx != NULL && pWatch != NULL;
	}


	// 内部函数：移除端口 select watch op
	static void __xnetPortSelectClearWatchBit(__xnet_select_ctx* pCtx, __xnet_select_watch* pWatch, uint32 iBit)
	{
		if ( !pCtx || !pWatch ) { return; }
		pWatch->iMask &= ~iBit;
		(void)__xnetPortSelectRefreshWatch(pCtx, pWatch);
	}


	// 内部函数：移除端口 select watch
	static void __xnetPortSelectRemoveWatch(__xnet_select_ctx* pCtx, int hSocket)
	{
		__xnet_select_watch** ppCur;
		if ( !pCtx || hSocket < 0 ) { return; }
		pthread_mutex_lock(&pCtx->tWatchLock);
		ppCur = &pCtx->pWatches;
		while ( *ppCur ) {
			__xnet_select_watch* pWatch = *ppCur;
			if ( pWatch->hSocket == hSocket ) {
				*ppCur = pWatch->pNext;
				XNET_FREE(pWatch);
				break;
			}
			ppCur = &pWatch->pNext;
		}
		pthread_mutex_unlock(&pCtx->tWatchLock);
	}


	// 内部函数：复制提交向量
	static bool __xnetPortSelectCopyVec(xnetportsubmit* pDst, xnetspan* pStorage, const xnetportsubmit* pSrc)
	{
		if ( !pDst || !pStorage || !pSrc ) { return false; }
		*pDst = *pSrc;
		if ( pSrc->pVec && pSrc->iVecCount > 0 ) {
			if ( pSrc->iVecCount > __XNET_SELECT_MAX_IOVEC ) { return false; }
			memcpy(pStorage, pSrc->pVec, sizeof(xnetspan) * pSrc->iVecCount);
			pDst->pVec = pStorage;
		}
		return true;
	}


	// 内部函数：构建 select iovec
	static uint32 __xnetPortSelectBuildIov(const xnetportsubmit* pOp, struct iovec* arrIov, uint32 iMaxCount)
	{
		if ( !pOp || !arrIov || iMaxCount == 0 ) { return 0; }
		if ( pOp->pVec && pOp->iVecCount > 0 ) {
			uint32 iCount = pOp->iVecCount < iMaxCount ? pOp->iVecCount : iMaxCount;
			for ( uint32 i = 0; i < iCount; ++i ) {
				arrIov[i].iov_base = (void*)pOp->pVec[i].pData;
				arrIov[i].iov_len = (size_t)pOp->pVec[i].iLen;
			}
			return iCount;
		}
		if ( pOp->pChain ) {
			xnetspan arrSpan[__XNET_SELECT_MAX_IOVEC];
			uint32 iCount = xrtNetChainGetSpans(pOp->pChain, arrSpan, iMaxCount);
			for ( uint32 i = 0; i < iCount; ++i ) {
				arrIov[i].iov_base = (void*)arrSpan[i].pData;
				arrIov[i].iov_len = (size_t)arrSpan[i].iLen;
			}
			return iCount;
		}
		return 0;
	}


	// 内部函数：提交端口 select watch
	static xnet_result __xnetPortSelectSubmitWatch(__xnet_select_ctx* pCtx, const xnetportsubmit* pOp, uint32 iBit)
	{
		__xnet_select_watch* pWatch;
		if ( !pCtx || !pOp || pOp->hSocket == (intptr_t)XNET_SOCKET_INVALID || pOp->hSocket == 0 ) {
			return XRT_NET_ERROR;
		}
		__xnetPortSelectSetNonBlock((int)pOp->hSocket);
		pthread_mutex_lock(&pCtx->tWatchLock);
		pWatch = __xnetPortSelectGetWatch(pCtx, (int)pOp->hSocket);
		if ( !pWatch ) {
			pthread_mutex_unlock(&pCtx->tWatchLock);
			return XRT_NET_ERROR;
		}
		if ( iBit == __XNET_SELECT_W_ACCEPT ) {
			pWatch->tAccept = *pOp;
		} else if ( iBit == __XNET_SELECT_W_RECV ) {
			pWatch->tRecv = *pOp;
		} else if ( iBit == __XNET_SELECT_W_RECVFROM ) {
			pWatch->tRecvFrom = *pOp;
		} else if ( iBit == __XNET_SELECT_W_SEND ) {
			if ( !__xnetPortSelectCopyVec(&pWatch->tSend, pWatch->arrSendVec, pOp) ) {
				pthread_mutex_unlock(&pCtx->tWatchLock);
				return XRT_NET_ERROR;
			}
		} else if ( iBit == __XNET_SELECT_W_SENDTO ) {
			if ( !__xnetPortSelectCopyVec(&pWatch->tSendTo, pWatch->arrSendToVec, pOp) ) {
				pthread_mutex_unlock(&pCtx->tWatchLock);
				return XRT_NET_ERROR;
			}
		} else if ( iBit == __XNET_SELECT_W_CONNECT ) {
			pWatch->tConnect = *pOp;
		}
		pWatch->iMask |= iBit;
		if ( !__xnetPortSelectRefreshWatch(pCtx, pWatch) ) {
			pthread_mutex_unlock(&pCtx->tWatchLock);
			return XRT_NET_ERROR;
		}
		pthread_mutex_unlock(&pCtx->tWatchLock);
		return XRT_NET_OK;
	}


	// 内部函数：立即投递端口 select 事件
	static bool __xnetPortSelectPostSubmitEvent(__xnet_select_ctx* pCtx, const xnetportsubmit* pOp, xnet_result iStatus)
	{
		xnetportevent tEvent;
		if ( !pCtx || !pOp ) { return false; }
		memset(&tEvent, 0, sizeof(tEvent));
		tEvent.iType = __xnetPortSelectEventType(pOp->iOpType);
		tEvent.iStatus = iStatus;
		tEvent.iFlags = pOp->iFlags;
		tEvent.iBytes = __xnetPortSelectSubmitBytes(pOp);
		tEvent.iOpId = pOp->iOpId;
		tEvent.hSocket = pOp->hSocket;
		if ( pOp->iOpType == XNET_PORT_OP_ACCEPT ) {
			tEvent.hSocket = (intptr_t)XNET_SOCKET_INVALID;
		}
		tEvent.pUserData = pOp->pUserData;
		tEvent.pChain = pOp->pChain;
		tEvent.tAddr = pOp->tAddr;
		return tEvent.iType != XNET_PORT_EVENT_NONE && __xnetPortSelectQueueEvent(pCtx, &tEvent);
	}


	// 内部函数：提交端口 select connect
	static xnet_result __xnetPortSelectSubmitConnect(__xnet_select_ctx* pCtx, const xnetportsubmit* pOp)
	{
		struct sockaddr_storage tStorage;
		socklen_t iAddrLen = 0;
		int iRet;
		if ( !pCtx || !pOp || !__xnetAddrToSockAddr(&pOp->tAddr, &tStorage, &iAddrLen) ) { return XRT_NET_ERROR; }
		__xnetPortSelectSetNonBlock((int)pOp->hSocket);
		iRet = connect((int)pOp->hSocket, (struct sockaddr*)&tStorage, iAddrLen);
		if ( iRet == 0 ) {
			return __xnetPortSelectPostSubmitEvent(pCtx, pOp, XRT_NET_OK) ? XRT_NET_OK : XRT_NET_ERROR;
		}
		if ( errno == EINPROGRESS || errno == EALREADY || errno == EWOULDBLOCK ) {
			return __xnetPortSelectSubmitWatch(pCtx, pOp, __XNET_SELECT_W_CONNECT);
		}
		return __xnetPortSelectPostSubmitEvent(pCtx, pOp, XRT_NET_ERROR) ? XRT_NET_OK : XRT_NET_ERROR;
	}


	// 内部函数：构建端口 select recv 事件
	static bool __xnetPortSelectBuildRecvEvent(const xnetportsubmit* pOp, xnetportevent* pEvent)
	{
		char aBuf[__XNET_SELECT_INLINE_RECV];
		ssize_t iRet;
		if ( !pOp || !pEvent ) { return false; }
		iRet = recv((int)pOp->hSocket, aBuf, sizeof(aBuf), 0);
		if ( iRet < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) ) { return false; }
		memset(pEvent, 0, sizeof(xnetportevent));
		pEvent->iType = XNET_PORT_EVENT_RECV;
		pEvent->iStatus = (iRet > 0) ? XRT_NET_OK : XRT_NET_ERROR;
		pEvent->iOpId = pOp->iOpId;
		pEvent->hSocket = pOp->hSocket;
		pEvent->pUserData = pOp->pUserData;
		pEvent->tAddr = pOp->tAddr;
		if ( iRet > 0 ) {
			pEvent->iBytes = (uint32)iRet;
			pEvent->pChain = __xnetPortSelectAllocEventChain(aBuf, (size_t)iRet);
			if ( !pEvent->pChain ) {
				pEvent->iStatus = XRT_NET_ERROR;
				pEvent->iBytes = 0;
			}
		} else {
			pEvent->iStatus = XRT_NET_CLOSED;
			pEvent->iFlags |= XNET_PORT_EVENT_F_EOF;
		}
		return true;
	}


	// 内部函数：构建端口 select send 事件
	static bool __xnetPortSelectBuildSendEvent(const xnetportsubmit* pOp, xnetportevent* pEvent)
	{
		struct iovec arrIov[__XNET_SELECT_MAX_IOVEC];
		uint32 iCount;
		ssize_t iRet;
		if ( !pOp || !pEvent ) { return false; }
		iCount = __xnetPortSelectBuildIov(pOp, arrIov, __XNET_SELECT_MAX_IOVEC);
		if ( iCount == 0 ) { return false; }
		iRet = writev((int)pOp->hSocket, arrIov, (int)iCount);
		if ( iRet < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) ) { return false; }
		memset(pEvent, 0, sizeof(xnetportevent));
		pEvent->iType = XNET_PORT_EVENT_SEND;
		pEvent->iStatus = (iRet >= 0) ? XRT_NET_OK : XRT_NET_ERROR;
		pEvent->iBytes = (iRet > 0) ? (uint32)iRet : 0u;
		pEvent->iOpId = pOp->iOpId;
		pEvent->hSocket = pOp->hSocket;
		pEvent->pUserData = pOp->pUserData;
		pEvent->tAddr = pOp->tAddr;
		return true;
	}


	// 内部函数：构建端口 select recvfrom 事件
	static bool __xnetPortSelectBuildRecvFromEvent(const xnetportsubmit* pOp, xnetportevent* pEvent)
	{
		char aBuf[__XNET_SELECT_INLINE_RECV];
		struct sockaddr_storage tStorage;
		socklen_t iAddrLen = (socklen_t)sizeof(tStorage);
		ssize_t iRet;
		if ( !pOp || !pEvent ) { return false; }
		memset(&tStorage, 0, sizeof(tStorage));
		iRet = recvfrom((int)pOp->hSocket, aBuf, sizeof(aBuf), 0, (struct sockaddr*)&tStorage, &iAddrLen);
		if ( iRet < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) ) { return false; }
		memset(pEvent, 0, sizeof(xnetportevent));
		pEvent->iType = XNET_PORT_EVENT_RECVFROM;
		pEvent->iStatus = (iRet >= 0) ? XRT_NET_OK : XRT_NET_ERROR;
		pEvent->iBytes = (iRet > 0) ? (uint32)iRet : 0u;
		pEvent->iOpId = pOp->iOpId;
		pEvent->hSocket = pOp->hSocket;
		pEvent->pUserData = pOp->pUserData;
		if ( iRet >= 0 ) {
			pEvent->pChain = __xnetPortSelectAllocEventChain(aBuf, (size_t)iRet);
			if ( !pEvent->pChain ) {
				pEvent->iStatus = XRT_NET_ERROR;
				pEvent->iBytes = 0;
			}
			(void)__xnetAddrFromSockAddr(&pEvent->tAddr, (const struct sockaddr*)&tStorage);
		}
		return true;
	}


	// 内部函数：构建端口 select sendto 事件
	static bool __xnetPortSelectBuildSendToEvent(const xnetportsubmit* pOp, xnetportevent* pEvent)
	{
		struct iovec arrIov[__XNET_SELECT_MAX_IOVEC];
		struct sockaddr_storage tStorage;
		struct msghdr tMsg;
		socklen_t iAddrLen = 0;
		uint32 iCount;
		ssize_t iRet;
		if ( !pOp || !pEvent || !__xnetAddrToSockAddr(&pOp->tAddr, &tStorage, &iAddrLen) ) { return false; }
		iCount = __xnetPortSelectBuildIov(pOp, arrIov, __XNET_SELECT_MAX_IOVEC);
		if ( iCount == 0 ) { return false; }
		memset(&tMsg, 0, sizeof(tMsg));
		tMsg.msg_name = &tStorage;
		tMsg.msg_namelen = iAddrLen;
		tMsg.msg_iov = arrIov;
		tMsg.msg_iovlen = iCount;
		iRet = sendmsg((int)pOp->hSocket, &tMsg, 0);
		if ( iRet < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) ) { return false; }
		memset(pEvent, 0, sizeof(xnetportevent));
		pEvent->iType = XNET_PORT_EVENT_SENDTO;
		pEvent->iStatus = (iRet >= 0) ? XRT_NET_OK : XRT_NET_ERROR;
		pEvent->iBytes = (iRet > 0) ? (uint32)iRet : 0u;
		pEvent->iOpId = pOp->iOpId;
		pEvent->hSocket = pOp->hSocket;
		pEvent->pUserData = pOp->pUserData;
		pEvent->tAddr = pOp->tAddr;
		return true;
	}


	// 内部函数：构建端口 select connect 事件
	static bool __xnetPortSelectBuildConnectEvent(const xnetportsubmit* pOp, xnetportevent* pEvent)
	{
		int iErr = 0;
		socklen_t iLen = (socklen_t)sizeof(iErr);
		if ( !pOp || !pEvent ) { return false; }
		if ( getsockopt((int)pOp->hSocket, SOL_SOCKET, SO_ERROR, &iErr, &iLen) != 0 ) {
			iErr = errno;
		}
		memset(pEvent, 0, sizeof(xnetportevent));
		pEvent->iType = XNET_PORT_EVENT_CONNECT;
		pEvent->iStatus = (iErr == 0) ? XRT_NET_OK : XRT_NET_ERROR;
		pEvent->iOpId = pOp->iOpId;
		pEvent->hSocket = pOp->hSocket;
		pEvent->pUserData = pOp->pUserData;
		pEvent->tAddr = pOp->tAddr;
		return true;
	}


	// 内部函数：处理端口 select watch 事件
		static uint32 __xnetPortSelectProcessWatch(__xnet_select_ctx* pCtx, __xnet_select_watch* pWatch, uint32 iReady, xnetportevent* pEvents, uint32 iMaxEvents)
	{
		xnetportevent tEvent;
		uint32 iCount = 0;
		if ( !pCtx || !pWatch || !pEvents || iMaxEvents == 0 ) { return 0; }
		pthread_mutex_lock(&pCtx->tWatchLock);
		if ( (iReady & (__XNET_SELECT_READY_ERROR | __XNET_SELECT_READY_WRITE)) && (pWatch->iMask & __XNET_SELECT_W_CONNECT) && iCount < iMaxEvents ) {
			if ( __xnetPortSelectBuildConnectEvent(&pWatch->tConnect, &tEvent) ) {
				__xnetPortSelectClearWatchBit(pCtx, pWatch, __XNET_SELECT_W_CONNECT);
				pEvents[iCount++] = tEvent;
			}
		}
		if ( (iReady & (__XNET_SELECT_READY_ERROR | __XNET_SELECT_READY_WRITE)) && (pWatch->iMask & __XNET_SELECT_W_SEND) && iCount < iMaxEvents ) {
			if ( __xnetPortSelectBuildSendEvent(&pWatch->tSend, &tEvent) ) {
				__xnetPortSelectClearWatchBit(pCtx, pWatch, __XNET_SELECT_W_SEND);
				pEvents[iCount++] = tEvent;
			}
		}
		if ( (iReady & (__XNET_SELECT_READY_ERROR | __XNET_SELECT_READY_WRITE)) && (pWatch->iMask & __XNET_SELECT_W_SENDTO) && iCount < iMaxEvents ) {
			if ( __xnetPortSelectBuildSendToEvent(&pWatch->tSendTo, &tEvent) ) {
				__xnetPortSelectClearWatchBit(pCtx, pWatch, __XNET_SELECT_W_SENDTO);
				pEvents[iCount++] = tEvent;
			}
		}
		if ( (iReady & (__XNET_SELECT_READY_ERROR | __XNET_SELECT_READY_READ)) && (pWatch->iMask & __XNET_SELECT_W_ACCEPT) && iCount < iMaxEvents ) {
			memset(&tEvent, 0, sizeof(tEvent));
			tEvent.iType = XNET_PORT_EVENT_ACCEPT;
			tEvent.iStatus = XRT_NET_OK;
			tEvent.iOpId = pWatch->tAccept.iOpId;
			tEvent.hSocket = (intptr_t)XNET_SOCKET_INVALID;
			tEvent.pUserData = pWatch->tAccept.pUserData;
			__xnetPortSelectClearWatchBit(pCtx, pWatch, __XNET_SELECT_W_ACCEPT);
			pEvents[iCount++] = tEvent;
		}
		if ( (iReady & (__XNET_SELECT_READY_ERROR | __XNET_SELECT_READY_READ)) && (pWatch->iMask & __XNET_SELECT_W_RECV) && iCount < iMaxEvents ) {
			if ( __xnetPortSelectBuildRecvEvent(&pWatch->tRecv, &tEvent) ) {
				__xnetPortSelectClearWatchBit(pCtx, pWatch, __XNET_SELECT_W_RECV);
				pEvents[iCount++] = tEvent;
			}
		}
		if ( (iReady & (__XNET_SELECT_READY_ERROR | __XNET_SELECT_READY_READ)) && (pWatch->iMask & __XNET_SELECT_W_RECVFROM) && iCount < iMaxEvents ) {
			if ( __xnetPortSelectBuildRecvFromEvent(&pWatch->tRecvFrom, &tEvent) ) {
				__xnetPortSelectClearWatchBit(pCtx, pWatch, __XNET_SELECT_W_RECVFROM);
				pEvents[iCount++] = tEvent;
			}
		}
			pthread_mutex_unlock(&pCtx->tWatchLock);
			return iCount;
		}


		// 内部函数：轮询端口 select 写侧 watch
		static uint32 __xnetPortSelectPollWriteWatches(__xnet_select_ctx* pCtx, xnetportevent* pEvents, uint32 iMaxEvents)
		{
			uint32 iCount = 0;
			if ( !pCtx || !pEvents || iMaxEvents == 0 ) { return 0; }
			pthread_mutex_lock(&pCtx->tWatchLock);
			for ( __xnet_select_watch* pWatch = pCtx->pWatches; pWatch && iCount < iMaxEvents; pWatch = pWatch->pNext ) {
				xnetportevent tEvent;
				if ( (pWatch->iMask & __XNET_SELECT_W_SEND) && iCount < iMaxEvents ) {
					if ( __xnetPortSelectBuildSendEvent(&pWatch->tSend, &tEvent) ) {
						__xnetPortSelectClearWatchBit(pCtx, pWatch, __XNET_SELECT_W_SEND);
						pEvents[iCount++] = tEvent;
					}
				}
				if ( (pWatch->iMask & __XNET_SELECT_W_SENDTO) && iCount < iMaxEvents ) {
					if ( __xnetPortSelectBuildSendToEvent(&pWatch->tSendTo, &tEvent) ) {
						__xnetPortSelectClearWatchBit(pCtx, pWatch, __XNET_SELECT_W_SENDTO);
						pEvents[iCount++] = tEvent;
					}
				}
			}
			pthread_mutex_unlock(&pCtx->tWatchLock);
			return iCount;
		}


	// 内部函数：初始化端口 select
	static xnet_result __xnetPortSelectInit(xnetport* pPort, const xnetportconfig* pCfg, ptr pOwner)
	{
		__xnet_select_ctx* pCtx;
		(void)pCfg;
		(void)pOwner;
		if ( !pPort ) { return XRT_NET_ERROR; }
		pCtx = (__xnet_select_ctx*)XNET_ALLOC(sizeof(__xnet_select_ctx));
		if ( !pCtx ) { return XRT_NET_ERROR; }
		memset(pCtx, 0, sizeof(__xnet_select_ctx));
		pCtx->arrWake[0] = -1;
		pCtx->arrWake[1] = -1;
		if ( pipe(pCtx->arrWake) != 0 ) {
			XNET_FREE(pCtx);
			return XRT_NET_ERROR;
		}
		__xnetPortSelectSetCloseOnExec(pCtx->arrWake[0]);
		__xnetPortSelectSetCloseOnExec(pCtx->arrWake[1]);
		__xnetPortSelectSetNonBlock(pCtx->arrWake[0]);
		__xnetPortSelectSetNonBlock(pCtx->arrWake[1]);
		if ( pthread_mutex_init(&pCtx->tPostedLock, NULL) != 0 ) {
			close(pCtx->arrWake[0]);
			close(pCtx->arrWake[1]);
			XNET_FREE(pCtx);
			return XRT_NET_ERROR;
		}
		if ( pthread_mutex_init(&pCtx->tWatchLock, NULL) != 0 ) {
			pthread_mutex_destroy(&pCtx->tPostedLock);
			close(pCtx->arrWake[0]);
			close(pCtx->arrWake[1]);
			XNET_FREE(pCtx);
			return XRT_NET_ERROR;
		}
		pPort->pCtx = pCtx;
		return XRT_NET_OK;
	}


	// 内部函数：释放端口 select
	static void __xnetPortSelectUnit(xnetport* pPort)
	{
		__xnet_select_ctx* pCtx = pPort ? (__xnet_select_ctx*)pPort->pCtx : NULL;
		if ( !pCtx ) { return; }
		__xnetPortSelectFreeTimers(pCtx);
		__xnetPortSelectFreePosts(pCtx);
		__xnetPortSelectFreeWatches(pCtx);
		pthread_mutex_destroy(&pCtx->tWatchLock);
		pthread_mutex_destroy(&pCtx->tPostedLock);
		if ( pCtx->arrWake[0] >= 0 ) { close(pCtx->arrWake[0]); }
		if ( pCtx->arrWake[1] >= 0 ) { close(pCtx->arrWake[1]); }
		XNET_FREE(pCtx);
		if ( pPort ) { pPort->pCtx = NULL; }
	}


	// 内部函数：提交端口 select
	static xnet_result __xnetPortSelectSubmit(xnetport* pPort, const xnetportsubmit* pOps, uint32 iCount)
	{
		__xnet_select_ctx* pCtx = pPort ? (__xnet_select_ctx*)pPort->pCtx : NULL;
		if ( !pCtx || !pOps || iCount == 0 ) { return XRT_NET_ERROR; }
		for ( uint32 i = 0; i < iCount; ++i ) {
			if ( !__xnetPortSelectValidOp(pOps[i].iOpType) ) { return XRT_NET_ERROR; }
			if ( pOps[i].iOpType == XNET_PORT_OP_CLOSE ) {
				if ( pOps[i].hSocket != (intptr_t)XNET_SOCKET_INVALID && pOps[i].hSocket != 0 ) {
					__xnetPortSelectRemoveWatch(pCtx, (int)pOps[i].hSocket);
				}
				continue;
			}
			if ( pOps[i].iOpType == XNET_PORT_OP_CONNECT ) {
				if ( __xnetPortSelectSubmitConnect(pCtx, &pOps[i]) != XRT_NET_OK ) { return XRT_NET_ERROR; }
			} else if ( pOps[i].iOpType == XNET_PORT_OP_ACCEPT ) {
				if ( (pOps[i].iFlags & XNET_PORT_EVENT_F_ACCEPTED_OPEN) != 0 || pOps[i].tAddr.iFamily != 0 ) {
					if ( !__xnetPortSelectPostSubmitEvent(pCtx, &pOps[i], XRT_NET_OK) ) { return XRT_NET_ERROR; }
				} else if ( __xnetPortSelectSubmitWatch(pCtx, &pOps[i], __XNET_SELECT_W_ACCEPT) != XRT_NET_OK ) {
					return XRT_NET_ERROR;
				}
			} else if ( pOps[i].iOpType == XNET_PORT_OP_RECV && pOps[i].pChain ) {
				if ( !__xnetPortSelectPostSubmitEvent(pCtx, &pOps[i], XRT_NET_OK) ) { return XRT_NET_ERROR; }
			} else if ( pOps[i].iOpType == XNET_PORT_OP_RECV ) {
				if ( __xnetPortSelectSubmitWatch(pCtx, &pOps[i], __XNET_SELECT_W_RECV) != XRT_NET_OK ) { return XRT_NET_ERROR; }
			} else if ( pOps[i].iOpType == XNET_PORT_OP_SEND ) {
				xnetportevent tEvent;
				if ( pOps[i].hSocket == (intptr_t)XNET_SOCKET_INVALID || pOps[i].hSocket == 0 ) {
					if ( !__xnetPortSelectPostSubmitEvent(pCtx, &pOps[i], XRT_NET_OK) ) { return XRT_NET_ERROR; }
				} else if ( __xnetPortSelectBuildSendEvent(&pOps[i], &tEvent) ) {
					if ( !__xnetPortSelectQueueEvent(pCtx, &tEvent) ) { return XRT_NET_ERROR; }
				} else if ( __xnetPortSelectSubmitWatch(pCtx, &pOps[i], __XNET_SELECT_W_SEND) != XRT_NET_OK ) {
					return XRT_NET_ERROR;
				}
			} else if ( pOps[i].iOpType == XNET_PORT_OP_RECVFROM ) {
				if ( __xnetPortSelectSubmitWatch(pCtx, &pOps[i], __XNET_SELECT_W_RECVFROM) != XRT_NET_OK ) { return XRT_NET_ERROR; }
			} else if ( pOps[i].iOpType == XNET_PORT_OP_SENDTO ) {
				xnetportevent tEvent;
				if ( pOps[i].hSocket == (intptr_t)XNET_SOCKET_INVALID || pOps[i].hSocket == 0 ) {
					if ( !__xnetPortSelectPostSubmitEvent(pCtx, &pOps[i], XRT_NET_OK) ) { return XRT_NET_ERROR; }
				} else if ( __xnetPortSelectBuildSendToEvent(&pOps[i], &tEvent) ) {
					if ( !__xnetPortSelectQueueEvent(pCtx, &tEvent) ) { return XRT_NET_ERROR; }
				} else if ( __xnetPortSelectSubmitWatch(pCtx, &pOps[i], __XNET_SELECT_W_SENDTO) != XRT_NET_OK ) {
					return XRT_NET_ERROR;
				}
			} else if ( !__xnetPortSelectPostSubmitEvent(pCtx, &pOps[i], XRT_NET_OK) ) {
				return XRT_NET_ERROR;
			}
		}
		(void)pPort->pOps->Wake(pPort);
		return XRT_NET_OK;
	}


	// 内部函数：提取端口 select
	static uint32 __xnetPortSelectHarvest(xnetport* pPort, xnetportevent* pEvents, uint32 iMaxEvents, uint32 iTimeoutMs)
	{
		fd_set tReadSet;
		fd_set tWriteSet;
		fd_set tErrorSet;
		struct timeval tTimeout;
		struct timeval* pTimeout = &tTimeout;
		int iMaxFd = -1;
		uint32 iCount = 0;
		uint32 iWaitMs;
		__xnet_select_ctx* pCtx = pPort ? (__xnet_select_ctx*)pPort->pCtx : NULL;
		if ( !pCtx || !pEvents || iMaxEvents == 0 ) { return 0; }
		iCount += __xnetPortSelectHarvestTimers(pCtx, pEvents + iCount, iMaxEvents - iCount);
		if ( iCount >= iMaxEvents ) { return iCount; }
			iCount += __xnetPortSelectDrainPosted(pCtx, pEvents + iCount, iMaxEvents - iCount);
			if ( iCount >= iMaxEvents ) { return iCount; }
			iCount += __xnetPortSelectPollWriteWatches(pCtx, pEvents + iCount, iMaxEvents - iCount);
			if ( iCount >= iMaxEvents ) { return iCount; }
			FD_ZERO(&tReadSet);
		FD_ZERO(&tWriteSet);
		FD_ZERO(&tErrorSet);
		if ( pCtx->arrWake[0] >= 0 && pCtx->arrWake[0] < FD_SETSIZE ) {
			FD_SET(pCtx->arrWake[0], &tReadSet);
			iMaxFd = pCtx->arrWake[0];
		}
		pthread_mutex_lock(&pCtx->tWatchLock);
		for ( __xnet_select_watch* pWatch = pCtx->pWatches; pWatch; pWatch = pWatch->pNext ) {
			if ( pWatch->hSocket < 0 || pWatch->hSocket >= FD_SETSIZE || pWatch->iMask == 0 ) { continue; }
			if ( pWatch->iMask & (__XNET_SELECT_W_ACCEPT | __XNET_SELECT_W_RECV | __XNET_SELECT_W_RECVFROM) ) {
				FD_SET(pWatch->hSocket, &tReadSet);
			}
			if ( pWatch->iMask & (__XNET_SELECT_W_CONNECT | __XNET_SELECT_W_SEND | __XNET_SELECT_W_SENDTO) ) {
				FD_SET(pWatch->hSocket, &tWriteSet);
			}
			FD_SET(pWatch->hSocket, &tErrorSet);
			if ( pWatch->hSocket > iMaxFd ) { iMaxFd = pWatch->hSocket; }
		}
		pthread_mutex_unlock(&pCtx->tWatchLock);
		iWaitMs = iCount > 0 ? 0u : __xnetPortSelectTimerWait(pCtx, iTimeoutMs);
		tTimeout.tv_sec = (time_t)(iWaitMs / 1000u);
		tTimeout.tv_usec = (suseconds_t)((iWaitMs % 1000u) * 1000u);
		if ( iWaitMs == 0xffffffffu ) { pTimeout = NULL; }
		if ( iMaxFd >= 0 && select(iMaxFd + 1, &tReadSet, &tWriteSet, &tErrorSet, pTimeout) > 0 ) {
			if ( pCtx->arrWake[0] >= 0 && FD_ISSET(pCtx->arrWake[0], &tReadSet) ) {
				char aBuf[128];
				while ( read(pCtx->arrWake[0], aBuf, sizeof(aBuf)) > 0 ) {}
				iCount += __xnetPortSelectDrainPosted(pCtx, pEvents + iCount, iMaxEvents - iCount);
				if ( iCount < iMaxEvents ) {
					memset(&pEvents[iCount], 0, sizeof(xnetportevent));
					pEvents[iCount].iType = XNET_PORT_EVENT_WAKE;
					pEvents[iCount].iStatus = XRT_NET_OK;
					pEvents[iCount].iBytes = 1;
					++iCount;
				}
			}
			pthread_mutex_lock(&pCtx->tWatchLock);
			for ( __xnet_select_watch* pWatch = pCtx->pWatches; pWatch && iCount < iMaxEvents; pWatch = pWatch->pNext ) {
				uint32 iReady = 0;
				if ( pWatch->hSocket < 0 || pWatch->hSocket >= FD_SETSIZE ) { continue; }
				if ( FD_ISSET(pWatch->hSocket, &tReadSet) ) { iReady |= __XNET_SELECT_READY_READ; }
				if ( FD_ISSET(pWatch->hSocket, &tWriteSet) ) { iReady |= __XNET_SELECT_READY_WRITE; }
				if ( FD_ISSET(pWatch->hSocket, &tErrorSet) ) { iReady |= __XNET_SELECT_READY_ERROR; }
				if ( iReady != 0 ) {
					pthread_mutex_unlock(&pCtx->tWatchLock);
					iCount += __xnetPortSelectProcessWatch(pCtx, pWatch, iReady, pEvents + iCount, iMaxEvents - iCount);
					pthread_mutex_lock(&pCtx->tWatchLock);
				}
			}
			pthread_mutex_unlock(&pCtx->tWatchLock);
		}
		if ( iCount < iMaxEvents ) {
			iCount += __xnetPortSelectHarvestTimers(pCtx, pEvents + iCount, iMaxEvents - iCount);
		}
		return iCount;
	}


	// 内部函数：唤醒端口 select
	static xnet_result __xnetPortSelectWake(xnetport* pPort)
	{
		__xnet_select_ctx* pCtx = pPort ? (__xnet_select_ctx*)pPort->pCtx : NULL;
		char cWake = 1;
		if ( !pCtx || pCtx->arrWake[1] < 0 ) { return XRT_NET_ERROR; }
		if ( write(pCtx->arrWake[1], &cWake, 1) != 1 ) {
			if ( errno == EAGAIN || errno == EWOULDBLOCK ) { return XRT_NET_OK; }
			return XRT_NET_ERROR;
		}
		return XRT_NET_OK;
	}


	// 内部函数：设置端口 select 定时器
	static xnet_result __xnetPortSelectArmTimer(xnetport* pPort, uint64 iTimerId, uint32 iDelayMs)
	{
		__xnet_select_ctx* pCtx = pPort ? (__xnet_select_ctx*)pPort->pCtx : NULL;
		__xnet_select_timer* pNode;
		__xnet_select_timer* pExisting;
		uint64 iDueMs;
		if ( !pCtx || iTimerId == 0u ) { return XRT_NET_ERROR; }
		pNode = (__xnet_select_timer*)XNET_ALLOC(sizeof(__xnet_select_timer));
		if ( !pNode ) { return XRT_NET_ERROR; }
		memset(pNode, 0, sizeof(__xnet_select_timer));
		pNode->iTimerId = iTimerId;
		iDueMs = __xnetPortSelectNowMs() + (uint64)iDelayMs;
		pNode->iDueMs = iDueMs;
		pExisting = pCtx->pTimers;
		while ( pExisting && pExisting->iTimerId != iTimerId ) { pExisting = pExisting->pNext; }
		if ( pExisting ) {
			pExisting->iDueMs = iDueMs;
			XNET_FREE(pNode);
		} else {
			pNode->pNext = pCtx->pTimers;
			pCtx->pTimers = pNode;
		}
		return XRT_NET_OK;
	}


	// 内部函数：取消端口 select 定时器
	static xnet_result __xnetPortSelectCancelTimer(xnetport* pPort, uint64 iTimerId)
	{
		__xnet_select_ctx* pCtx = pPort ? (__xnet_select_ctx*)pPort->pCtx : NULL;
		__xnet_select_timer** ppCur = pCtx ? &pCtx->pTimers : NULL;
		if ( !ppCur ) { return XRT_NET_ERROR; }
		while ( *ppCur ) {
			__xnet_select_timer* pNode = *ppCur;
			if ( pNode->iTimerId == iTimerId ) {
				*ppCur = pNode->pNext;
				XNET_FREE(pNode);
				return XRT_NET_OK;
			}
			ppCur = &pNode->pNext;
		}
		return XRT_NET_ERROR;
	}

	static const xnetportops __g_xnetPortSelectOps = {
		"xnet-port-select",
		__xnetPortSelectInit,
		__xnetPortSelectUnit,
		__xnetPortSelectSubmit,
		__xnetPortSelectHarvest,
		__xnetPortSelectWake,
		__xnetPortSelectArmTimer,
		__xnetPortSelectCancelTimer
	};

	// 网络端口 select ops相关处理
	static const xnetportops* xrtNetPortSelectOps(void)
	{
		return &__g_xnetPortSelectOps;
	}

#else

	// 内部函数：判断 select 后端是否可用
	static bool UNUSED_ATTR __xnetPortSelectReady(const xnetport* pPort)
	{
		(void)pPort;
		return false;
	}

	// 网络端口 select ops相关处理
	static const xnetportops* xrtNetPortSelectOps(void)
	{
		return NULL;
	}

#endif

#endif

#ifndef XRT_XNET_PORT_KQUEUE_H
#define XRT_XNET_PORT_KQUEUE_H

#if defined(__APPLE__) && defined(__MACH__)

	#define __XNET_KQUEUE_INLINE_RECV 8192u
	#define __XNET_KQUEUE_MAX_IOVEC   16u

	typedef struct __xnet_kqueue_timer {
		struct __xnet_kqueue_timer* pNext;
		uint64 iTimerId;
		uint64 iDueMs;
	} __xnet_kqueue_timer;

	typedef struct __xnet_kqueue_post {
		struct __xnet_kqueue_post* pNext;
		xnetportevent tEvent;
	} __xnet_kqueue_post;

	typedef struct __xnet_kqueue_watch {
		struct __xnet_kqueue_watch* pNext;
		int hSocket;
		bool bDead;
		uint32 iMask;
		uint32 iKernelMask;
		xnetportsubmit tAccept;
		xnetportsubmit tConnect;
		xnetportsubmit tRecv;
		xnetportsubmit tSend;
		xnetportsubmit tRecvFrom;
		xnetportsubmit tSendTo;
		xnetspan arrSendVec[__XNET_KQUEUE_MAX_IOVEC];
		xnetspan arrSendToVec[__XNET_KQUEUE_MAX_IOVEC];
	} __xnet_kqueue_watch;

	typedef struct {
		int hKqueue;
		int arrWake[2];
		__xnet_kqueue_watch* pWatches;
		__xnet_kqueue_timer* pTimers;
		__xnet_kqueue_post* pPostedHead;
		__xnet_kqueue_post* pPostedTail;
		pthread_mutex_t tPostedLock;
		pthread_mutex_t tWatchLock;
	} __xnet_kqueue_ctx;

	#define __XNET_KQUEUE_W_ACCEPT   0x00000001u
	#define __XNET_KQUEUE_W_CONNECT  0x00000002u
	#define __XNET_KQUEUE_W_RECV     0x00000004u
	#define __XNET_KQUEUE_W_SEND     0x00000008u
	#define __XNET_KQUEUE_W_RECVFROM 0x00000010u
	#define __XNET_KQUEUE_W_SENDTO   0x00000020u

	#define __XNET_KQUEUE_READY_READ  0x00000001u
	#define __XNET_KQUEUE_READY_WRITE 0x00000002u
	#define __XNET_KQUEUE_READY_ERROR 0x00000004u


	// 内部函数：判断 kqueue 后端是否可用
	static bool UNUSED_ATTR __xnetPortKqueueReady(const xnetport* pPort)
	{
		const __xnet_kqueue_ctx* pCtx = pPort ? (const __xnet_kqueue_ctx*)pPort->pCtx : NULL;
		return pCtx && pCtx->hKqueue >= 0;
	}


	// 内部函数：kqueue now 毫秒相关处理
	static uint64 __xnetPortKqueueNowMs(void)
	{
		struct timespec tNow;
		clock_gettime(CLOCK_MONOTONIC, &tNow);
		return (uint64)tNow.tv_sec * 1000u + (uint64)(tNow.tv_nsec / 1000000u);
	}


	// 内部函数：设置 fd close-on-exec
	static void __xnetPortKqueueSetCloseOnExec(int hFd)
	{
		int iFlags;
		if ( hFd < 0 ) { return; }
		iFlags = fcntl(hFd, F_GETFD, 0);
		if ( iFlags >= 0 ) {
			(void)fcntl(hFd, F_SETFD, iFlags | FD_CLOEXEC);
		}
	}


	// 内部函数：设置 fd nonblock
	static void __xnetPortKqueueSetNonBlock(int hFd)
	{
		int iFlags;
		if ( hFd < 0 ) { return; }
		iFlags = fcntl(hFd, F_GETFL, 0);
		if ( iFlags >= 0 ) {
			(void)fcntl(hFd, F_SETFL, iFlags | O_NONBLOCK);
		}
	}


	// 内部函数：创建 kqueue fd
	static int __xnetPortKqueueCreateFd(void)
	{
		int hFd = kqueue();
		if ( hFd >= 0 ) {
			__xnetPortKqueueSetCloseOnExec(hFd);
		}
		return hFd;
	}


	// 内部函数：端口 kqueue 事件 type相关处理
	static uint16 __xnetPortKqueueEventType(uint16 iOpType)
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


	// 内部函数：端口 kqueue valid op相关处理
	static bool __xnetPortKqueueValidOp(uint16 iType)
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


	// 内部函数：分配端口 kqueue 事件 chain
	static xnetchain* __xnetPortKqueueAllocEventChain(const void* pData, size_t iLen)
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


	// 内部函数：提交端口 kqueue bytes
	static uint32 __xnetPortKqueueSubmitBytes(const xnetportsubmit* pOp)
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


	// 内部函数：释放端口 kqueue timers
	static void __xnetPortKqueueFreeTimers(__xnet_kqueue_ctx* pCtx)
	{
		while ( pCtx && pCtx->pTimers ) {
			__xnet_kqueue_timer* pNext = pCtx->pTimers->pNext;
			XNET_FREE(pCtx->pTimers);
			pCtx->pTimers = pNext;
		}
	}


	// 内部函数：释放端口 kqueue posts
	static void __xnetPortKqueueFreePosts(__xnet_kqueue_ctx* pCtx)
	{
		if ( !pCtx ) { return; }
		while ( pCtx->pPostedHead ) {
			__xnet_kqueue_post* pNext = pCtx->pPostedHead->pNext;
			XNET_FREE(pCtx->pPostedHead);
			pCtx->pPostedHead = pNext;
		}
		pCtx->pPostedTail = NULL;
	}


	// 内部函数：释放端口 kqueue watches
	static void __xnetPortKqueueFreeWatches(__xnet_kqueue_ctx* pCtx)
	{
		if ( !pCtx ) { return; }
		while ( pCtx->pWatches ) {
			__xnet_kqueue_watch* pNext = pCtx->pWatches->pNext;
			XNET_FREE(pCtx->pWatches);
			pCtx->pWatches = pNext;
		}
	}


	// 内部函数：端口 kqueue 队列事件相关处理
	static bool __xnetPortKqueueQueueEvent(__xnet_kqueue_ctx* pCtx, const xnetportevent* pEvent)
	{
		__xnet_kqueue_post* pPost;
		if ( !pCtx || !pEvent ) { return false; }
		pPost = (__xnet_kqueue_post*)XNET_ALLOC(sizeof(__xnet_kqueue_post));
		if ( !pPost ) { return false; }
		memset(pPost, 0, sizeof(__xnet_kqueue_post));
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


	// 内部函数：提取端口 kqueue posted events
	static uint32 __xnetPortKqueueDrainPosted(__xnet_kqueue_ctx* pCtx, xnetportevent* pEvents, uint32 iMaxEvents)
	{
		uint32 iCount = 0;
		__xnet_kqueue_post* pHead;
		if ( !pCtx || !pEvents || iMaxEvents == 0 ) { return 0; }
		pthread_mutex_lock(&pCtx->tPostedLock);
		pHead = pCtx->pPostedHead;
		pCtx->pPostedHead = NULL;
		pCtx->pPostedTail = NULL;
		pthread_mutex_unlock(&pCtx->tPostedLock);
		while ( pHead && iCount < iMaxEvents ) {
			__xnet_kqueue_post* pNext = pHead->pNext;
			pEvents[iCount++] = pHead->tEvent;
			XNET_FREE(pHead);
			pHead = pNext;
		}
		while ( pHead ) {
			__xnet_kqueue_post* pNext = pHead->pNext;
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


	// 内部函数：提取端口 kqueue timers
	static uint32 __xnetPortKqueueHarvestTimers(__xnet_kqueue_ctx* pCtx, xnetportevent* pEvents, uint32 iMaxEvents)
	{
		uint32 iCount = 0;
		uint64 iNowMs = __xnetPortKqueueNowMs();
		__xnet_kqueue_timer** ppCur = pCtx ? &pCtx->pTimers : NULL;
		while ( ppCur && *ppCur && iCount < iMaxEvents ) {
			__xnet_kqueue_timer* pNode = *ppCur;
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


	// 内部函数：查找端口 kqueue watch
	static __xnet_kqueue_watch* __xnetPortKqueueFindWatch(__xnet_kqueue_ctx* pCtx, int hSocket)
	{
		__xnet_kqueue_watch* pWatch = pCtx ? pCtx->pWatches : NULL;
		while ( pWatch ) {
			if ( pWatch->hSocket == hSocket ) { return pWatch; }
			pWatch = pWatch->pNext;
		}
		return NULL;
	}


	// 内部函数：获取端口 kqueue watch
	static __xnet_kqueue_watch* __xnetPortKqueueGetWatch(__xnet_kqueue_ctx* pCtx, int hSocket)
	{
		__xnet_kqueue_watch* pWatch;
		if ( !pCtx || hSocket < 0 ) { return NULL; }
		pWatch = __xnetPortKqueueFindWatch(pCtx, hSocket);
		if ( pWatch && !pWatch->bDead ) { return pWatch; }
		pWatch = (__xnet_kqueue_watch*)XNET_ALLOC(sizeof(__xnet_kqueue_watch));
		if ( !pWatch ) { return NULL; }
		memset(pWatch, 0, sizeof(__xnet_kqueue_watch));
		pWatch->hSocket = hSocket;
		pWatch->pNext = pCtx->pWatches;
		pCtx->pWatches = pWatch;
		return pWatch;
	}


	// 内部函数：刷新端口 kqueue watch
	static bool __xnetPortKqueueRefreshWatch(__xnet_kqueue_ctx* pCtx, __xnet_kqueue_watch* pWatch)
	{
		struct kevent arrChange[2];
		int iCount = 0;
		uint32 iWantKernelMask = 0;
		if ( !pCtx || !pWatch || pWatch->bDead || pWatch->hSocket < 0 || pCtx->hKqueue < 0 ) { return false; }
		if ( pWatch->iMask & (__XNET_KQUEUE_W_ACCEPT | __XNET_KQUEUE_W_RECV | __XNET_KQUEUE_W_RECVFROM) ) {
			iWantKernelMask |= __XNET_KQUEUE_READY_READ;
		}
		if ( pWatch->iMask & (__XNET_KQUEUE_W_CONNECT | __XNET_KQUEUE_W_SEND | __XNET_KQUEUE_W_SENDTO) ) {
			iWantKernelMask |= __XNET_KQUEUE_READY_WRITE;
		}
		if ( (iWantKernelMask & __XNET_KQUEUE_READY_READ) && !(pWatch->iKernelMask & __XNET_KQUEUE_READY_READ) ) {
			EV_SET(&arrChange[iCount++], (uintptr_t)pWatch->hSocket, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, pWatch);
		} else if ( !(iWantKernelMask & __XNET_KQUEUE_READY_READ) && (pWatch->iKernelMask & __XNET_KQUEUE_READY_READ) ) {
			EV_SET(&arrChange[iCount++], (uintptr_t)pWatch->hSocket, EVFILT_READ, EV_DELETE, 0, 0, NULL);
		}
		if ( (iWantKernelMask & __XNET_KQUEUE_READY_WRITE) && !(pWatch->iKernelMask & __XNET_KQUEUE_READY_WRITE) ) {
			EV_SET(&arrChange[iCount++], (uintptr_t)pWatch->hSocket, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, pWatch);
		} else if ( !(iWantKernelMask & __XNET_KQUEUE_READY_WRITE) && (pWatch->iKernelMask & __XNET_KQUEUE_READY_WRITE) ) {
			EV_SET(&arrChange[iCount++], (uintptr_t)pWatch->hSocket, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
		}
		if ( iCount == 0 || kevent(pCtx->hKqueue, arrChange, iCount, NULL, 0, NULL) == 0 ) {
			pWatch->iKernelMask = iWantKernelMask;
			return true;
		}
		return false;
	}


	// 内部函数：移除端口 kqueue watch op
	static void __xnetPortKqueueClearWatchBit(__xnet_kqueue_ctx* pCtx, __xnet_kqueue_watch* pWatch, uint32 iBit)
	{
		if ( !pCtx || !pWatch ) { return; }
		pWatch->iMask &= ~iBit;
		(void)__xnetPortKqueueRefreshWatch(pCtx, pWatch);
	}


	// 内部函数：移除端口 kqueue watch
	static void __xnetPortKqueueRemoveWatch(__xnet_kqueue_ctx* pCtx, int hSocket)
	{
		__xnet_kqueue_watch* pWatch;
		struct kevent arrChange[2];
		if ( !pCtx || hSocket < 0 ) { return; }
		pthread_mutex_lock(&pCtx->tWatchLock);
		pWatch = pCtx->pWatches;
		while ( pWatch ) {
			if ( pWatch->hSocket == hSocket && !pWatch->bDead ) {
				pWatch->bDead = true;
				pWatch->hSocket = -1;
				pWatch->iMask = 0;
				pWatch->iKernelMask = 0;
				break;
			}
			pWatch = pWatch->pNext;
		}
		pthread_mutex_unlock(&pCtx->tWatchLock);
		EV_SET(&arrChange[0], (uintptr_t)hSocket, EVFILT_READ, EV_DELETE, 0, 0, NULL);
		EV_SET(&arrChange[1], (uintptr_t)hSocket, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
		(void)kevent(pCtx->hKqueue, arrChange, 2, NULL, 0, NULL);
	}


	// 内部函数：复制提交向量
	static bool __xnetPortKqueueCopyVec(xnetportsubmit* pDst, xnetspan* pStorage, const xnetportsubmit* pSrc)
	{
		if ( !pDst || !pStorage || !pSrc ) { return false; }
		*pDst = *pSrc;
		if ( pSrc->pVec && pSrc->iVecCount > 0 ) {
			if ( pSrc->iVecCount > __XNET_KQUEUE_MAX_IOVEC ) { return false; }
			memcpy(pStorage, pSrc->pVec, sizeof(xnetspan) * pSrc->iVecCount);
			pDst->pVec = pStorage;
		}
		return true;
	}


	// 内部函数：构建 kqueue iovec
	static uint32 __xnetPortKqueueBuildIov(const xnetportsubmit* pOp, struct iovec* arrIov, uint32 iMaxCount)
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
			xnetspan arrSpan[__XNET_KQUEUE_MAX_IOVEC];
			uint32 iCount = xrtNetChainGetSpans(pOp->pChain, arrSpan, iMaxCount);
			for ( uint32 i = 0; i < iCount; ++i ) {
				arrIov[i].iov_base = (void*)arrSpan[i].pData;
				arrIov[i].iov_len = (size_t)arrSpan[i].iLen;
			}
			return iCount;
		}
		return 0;
	}


	// 内部函数：提交端口 kqueue watch
	static xnet_result __xnetPortKqueueSubmitWatch(__xnet_kqueue_ctx* pCtx, const xnetportsubmit* pOp, uint32 iBit)
	{
		__xnet_kqueue_watch* pWatch;
		if ( !pCtx || !pOp || pOp->hSocket == (intptr_t)XNET_SOCKET_INVALID || pOp->hSocket == 0 ) {
			return XRT_NET_ERROR;
		}
		__xnetPortKqueueSetNonBlock((int)pOp->hSocket);
		pthread_mutex_lock(&pCtx->tWatchLock);
		pWatch = __xnetPortKqueueGetWatch(pCtx, (int)pOp->hSocket);
		if ( !pWatch ) {
			pthread_mutex_unlock(&pCtx->tWatchLock);
			return XRT_NET_ERROR;
		}
		if ( iBit == __XNET_KQUEUE_W_ACCEPT ) {
			pWatch->tAccept = *pOp;
		} else if ( iBit == __XNET_KQUEUE_W_RECV ) {
			pWatch->tRecv = *pOp;
		} else if ( iBit == __XNET_KQUEUE_W_RECVFROM ) {
			pWatch->tRecvFrom = *pOp;
		} else if ( iBit == __XNET_KQUEUE_W_SEND ) {
			if ( !__xnetPortKqueueCopyVec(&pWatch->tSend, pWatch->arrSendVec, pOp) ) {
				pthread_mutex_unlock(&pCtx->tWatchLock);
				return XRT_NET_ERROR;
			}
		} else if ( iBit == __XNET_KQUEUE_W_SENDTO ) {
			if ( !__xnetPortKqueueCopyVec(&pWatch->tSendTo, pWatch->arrSendToVec, pOp) ) {
				pthread_mutex_unlock(&pCtx->tWatchLock);
				return XRT_NET_ERROR;
			}
		} else if ( iBit == __XNET_KQUEUE_W_CONNECT ) {
			pWatch->tConnect = *pOp;
		}
		pWatch->iMask |= iBit;
		if ( !__xnetPortKqueueRefreshWatch(pCtx, pWatch) ) {
			pthread_mutex_unlock(&pCtx->tWatchLock);
			return XRT_NET_ERROR;
		}
		pthread_mutex_unlock(&pCtx->tWatchLock);
		return XRT_NET_OK;
	}


	// 内部函数：立即投递端口 kqueue 事件
	static bool __xnetPortKqueuePostSubmitEvent(__xnet_kqueue_ctx* pCtx, const xnetportsubmit* pOp, xnet_result iStatus)
	{
		xnetportevent tEvent;
		if ( !pCtx || !pOp ) { return false; }
		memset(&tEvent, 0, sizeof(tEvent));
		tEvent.iType = __xnetPortKqueueEventType(pOp->iOpType);
		tEvent.iStatus = iStatus;
		tEvent.iFlags = pOp->iFlags;
		tEvent.iBytes = __xnetPortKqueueSubmitBytes(pOp);
		tEvent.iOpId = pOp->iOpId;
		tEvent.hSocket = pOp->hSocket;
		if ( pOp->iOpType == XNET_PORT_OP_ACCEPT ) {
			tEvent.hSocket = (intptr_t)XNET_SOCKET_INVALID;
		}
		tEvent.pUserData = pOp->pUserData;
		tEvent.pChain = pOp->pChain;
		tEvent.tAddr = pOp->tAddr;
		return tEvent.iType != XNET_PORT_EVENT_NONE && __xnetPortKqueueQueueEvent(pCtx, &tEvent);
	}


	// 内部函数：提交端口 kqueue connect
	static xnet_result __xnetPortKqueueSubmitConnect(__xnet_kqueue_ctx* pCtx, const xnetportsubmit* pOp)
	{
		struct sockaddr_storage tStorage;
		socklen_t iAddrLen = 0;
		int iRet;
		if ( !pCtx || !pOp || !__xnetAddrToSockAddr(&pOp->tAddr, &tStorage, &iAddrLen) ) { return XRT_NET_ERROR; }
		__xnetPortKqueueSetNonBlock((int)pOp->hSocket);
		iRet = connect((int)pOp->hSocket, (struct sockaddr*)&tStorage, iAddrLen);
		if ( iRet == 0 ) {
			return __xnetPortKqueuePostSubmitEvent(pCtx, pOp, XRT_NET_OK) ? XRT_NET_OK : XRT_NET_ERROR;
		}
		if ( errno == EINPROGRESS || errno == EALREADY || errno == EWOULDBLOCK ) {
			return __xnetPortKqueueSubmitWatch(pCtx, pOp, __XNET_KQUEUE_W_CONNECT);
		}
		return __xnetPortKqueuePostSubmitEvent(pCtx, pOp, XRT_NET_ERROR) ? XRT_NET_OK : XRT_NET_ERROR;
	}


	// 内部函数：构建端口 kqueue recv 事件
	static bool __xnetPortKqueueBuildRecvEvent(const xnetportsubmit* pOp, xnetportevent* pEvent)
	{
		char aBuf[__XNET_KQUEUE_INLINE_RECV];
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
			pEvent->pChain = __xnetPortKqueueAllocEventChain(aBuf, (size_t)iRet);
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


	// 内部函数：构建端口 kqueue send 事件
	static bool __xnetPortKqueueBuildSendEvent(const xnetportsubmit* pOp, xnetportevent* pEvent)
	{
		struct iovec arrIov[__XNET_KQUEUE_MAX_IOVEC];
		uint32 iCount;
		ssize_t iRet;
		if ( !pOp || !pEvent ) { return false; }
		iCount = __xnetPortKqueueBuildIov(pOp, arrIov, __XNET_KQUEUE_MAX_IOVEC);
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


	// 内部函数：构建端口 kqueue recvfrom 事件
	static bool __xnetPortKqueueBuildRecvFromEvent(const xnetportsubmit* pOp, xnetportevent* pEvent)
	{
		char aBuf[__XNET_KQUEUE_INLINE_RECV];
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
			pEvent->pChain = __xnetPortKqueueAllocEventChain(aBuf, (size_t)iRet);
			if ( !pEvent->pChain ) {
				pEvent->iStatus = XRT_NET_ERROR;
				pEvent->iBytes = 0;
			}
			(void)__xnetAddrFromSockAddr(&pEvent->tAddr, (const struct sockaddr*)&tStorage);
		}
		return true;
	}


	// 内部函数：构建端口 kqueue sendto 事件
	static bool __xnetPortKqueueBuildSendToEvent(const xnetportsubmit* pOp, xnetportevent* pEvent)
	{
		struct iovec arrIov[__XNET_KQUEUE_MAX_IOVEC];
		struct sockaddr_storage tStorage;
		struct msghdr tMsg;
		socklen_t iAddrLen = 0;
		uint32 iCount;
		ssize_t iRet;
		if ( !pOp || !pEvent || !__xnetAddrToSockAddr(&pOp->tAddr, &tStorage, &iAddrLen) ) { return false; }
		iCount = __xnetPortKqueueBuildIov(pOp, arrIov, __XNET_KQUEUE_MAX_IOVEC);
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


	// 内部函数：构建端口 kqueue connect 事件
	static bool __xnetPortKqueueBuildConnectEvent(const xnetportsubmit* pOp, xnetportevent* pEvent)
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


	// 内部函数：处理端口 kqueue watch 事件
	static uint32 __xnetPortKqueueProcessWatch(__xnet_kqueue_ctx* pCtx, __xnet_kqueue_watch* pWatch, uint32 iReady, xnetportevent* pEvents, uint32 iMaxEvents)
	{
		xnetportevent tEvent;
		uint32 iCount = 0;
		if ( !pCtx || !pWatch || !pEvents || iMaxEvents == 0 ) { return 0; }
		pthread_mutex_lock(&pCtx->tWatchLock);
		if ( pWatch->bDead || pWatch->hSocket < 0 ) {
			pthread_mutex_unlock(&pCtx->tWatchLock);
			return 0;
		}
		if ( (iReady & (__XNET_KQUEUE_READY_ERROR | __XNET_KQUEUE_READY_WRITE)) && (pWatch->iMask & __XNET_KQUEUE_W_CONNECT) && iCount < iMaxEvents ) {
			if ( __xnetPortKqueueBuildConnectEvent(&pWatch->tConnect, &tEvent) ) {
				__xnetPortKqueueClearWatchBit(pCtx, pWatch, __XNET_KQUEUE_W_CONNECT);
				pEvents[iCount++] = tEvent;
			}
		}
		if ( (iReady & (__XNET_KQUEUE_READY_ERROR | __XNET_KQUEUE_READY_WRITE)) && (pWatch->iMask & __XNET_KQUEUE_W_SEND) && iCount < iMaxEvents ) {
			if ( __xnetPortKqueueBuildSendEvent(&pWatch->tSend, &tEvent) ) {
				__xnetPortKqueueClearWatchBit(pCtx, pWatch, __XNET_KQUEUE_W_SEND);
				pEvents[iCount++] = tEvent;
			}
		}
		if ( (iReady & (__XNET_KQUEUE_READY_ERROR | __XNET_KQUEUE_READY_WRITE)) && (pWatch->iMask & __XNET_KQUEUE_W_SENDTO) && iCount < iMaxEvents ) {
			if ( __xnetPortKqueueBuildSendToEvent(&pWatch->tSendTo, &tEvent) ) {
				__xnetPortKqueueClearWatchBit(pCtx, pWatch, __XNET_KQUEUE_W_SENDTO);
				pEvents[iCount++] = tEvent;
			}
		}
		if ( (iReady & (__XNET_KQUEUE_READY_ERROR | __XNET_KQUEUE_READY_READ)) && (pWatch->iMask & __XNET_KQUEUE_W_ACCEPT) && iCount < iMaxEvents ) {
			memset(&tEvent, 0, sizeof(tEvent));
			tEvent.iType = XNET_PORT_EVENT_ACCEPT;
			tEvent.iStatus = XRT_NET_OK;
			tEvent.iOpId = pWatch->tAccept.iOpId;
			tEvent.hSocket = (intptr_t)XNET_SOCKET_INVALID;
			tEvent.pUserData = pWatch->tAccept.pUserData;
			__xnetPortKqueueClearWatchBit(pCtx, pWatch, __XNET_KQUEUE_W_ACCEPT);
			pEvents[iCount++] = tEvent;
		}
		if ( (iReady & (__XNET_KQUEUE_READY_ERROR | __XNET_KQUEUE_READY_READ)) && (pWatch->iMask & __XNET_KQUEUE_W_RECV) && iCount < iMaxEvents ) {
			if ( __xnetPortKqueueBuildRecvEvent(&pWatch->tRecv, &tEvent) ) {
				__xnetPortKqueueClearWatchBit(pCtx, pWatch, __XNET_KQUEUE_W_RECV);
				pEvents[iCount++] = tEvent;
			}
		}
		if ( (iReady & (__XNET_KQUEUE_READY_ERROR | __XNET_KQUEUE_READY_READ)) && (pWatch->iMask & __XNET_KQUEUE_W_RECVFROM) && iCount < iMaxEvents ) {
			if ( __xnetPortKqueueBuildRecvFromEvent(&pWatch->tRecvFrom, &tEvent) ) {
				__xnetPortKqueueClearWatchBit(pCtx, pWatch, __XNET_KQUEUE_W_RECVFROM);
				pEvents[iCount++] = tEvent;
			}
		}
		pthread_mutex_unlock(&pCtx->tWatchLock);
		return iCount;
	}


	// 内部函数：初始化端口 kqueue
	static xnet_result __xnetPortKqueueInit(xnetport* pPort, const xnetportconfig* pCfg, ptr pOwner)
	{
		__xnet_kqueue_ctx* pCtx;
		struct kevent tEv;
		(void)pCfg;
		(void)pOwner;
		if ( !pPort ) { return XRT_NET_ERROR; }
		pCtx = (__xnet_kqueue_ctx*)XNET_ALLOC(sizeof(__xnet_kqueue_ctx));
		if ( !pCtx ) { return XRT_NET_ERROR; }
		memset(pCtx, 0, sizeof(__xnet_kqueue_ctx));
		pCtx->hKqueue = -1;
		pCtx->arrWake[0] = -1;
		pCtx->arrWake[1] = -1;
		pCtx->hKqueue = __xnetPortKqueueCreateFd();
		if ( pCtx->hKqueue < 0 ) {
			XNET_FREE(pCtx);
			return XRT_NET_ERROR;
		}
		if ( pipe(pCtx->arrWake) != 0 ) {
			close(pCtx->hKqueue);
			XNET_FREE(pCtx);
			return XRT_NET_ERROR;
		}
		__xnetPortKqueueSetCloseOnExec(pCtx->arrWake[0]);
		__xnetPortKqueueSetCloseOnExec(pCtx->arrWake[1]);
		__xnetPortKqueueSetNonBlock(pCtx->arrWake[0]);
		__xnetPortKqueueSetNonBlock(pCtx->arrWake[1]);
		if ( pthread_mutex_init(&pCtx->tPostedLock, NULL) != 0 ) {
			close(pCtx->arrWake[0]);
			close(pCtx->arrWake[1]);
			close(pCtx->hKqueue);
			XNET_FREE(pCtx);
			return XRT_NET_ERROR;
		}
		if ( pthread_mutex_init(&pCtx->tWatchLock, NULL) != 0 ) {
			pthread_mutex_destroy(&pCtx->tPostedLock);
			close(pCtx->arrWake[0]);
			close(pCtx->arrWake[1]);
			close(pCtx->hKqueue);
			XNET_FREE(pCtx);
			return XRT_NET_ERROR;
		}
		EV_SET(&tEv, (uintptr_t)pCtx->arrWake[0], EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
		if ( kevent(pCtx->hKqueue, &tEv, 1, NULL, 0, NULL) != 0 ) {
			pthread_mutex_destroy(&pCtx->tWatchLock);
			pthread_mutex_destroy(&pCtx->tPostedLock);
			close(pCtx->arrWake[0]);
			close(pCtx->arrWake[1]);
			close(pCtx->hKqueue);
			XNET_FREE(pCtx);
			return XRT_NET_ERROR;
		}
		pPort->pCtx = pCtx;
		return XRT_NET_OK;
	}


	// 内部函数：释放端口 kqueue
	static void __xnetPortKqueueUnit(xnetport* pPort)
	{
		__xnet_kqueue_ctx* pCtx = pPort ? (__xnet_kqueue_ctx*)pPort->pCtx : NULL;
		if ( !pCtx ) { return; }
		__xnetPortKqueueFreeTimers(pCtx);
		__xnetPortKqueueFreePosts(pCtx);
		__xnetPortKqueueFreeWatches(pCtx);
		pthread_mutex_destroy(&pCtx->tWatchLock);
		pthread_mutex_destroy(&pCtx->tPostedLock);
		if ( pCtx->arrWake[0] >= 0 ) { close(pCtx->arrWake[0]); }
		if ( pCtx->arrWake[1] >= 0 ) { close(pCtx->arrWake[1]); }
		if ( pCtx->hKqueue >= 0 ) { close(pCtx->hKqueue); }
		XNET_FREE(pCtx);
		if ( pPort ) { pPort->pCtx = NULL; }
	}


	// 内部函数：提交端口 kqueue
	static xnet_result __xnetPortKqueueSubmit(xnetport* pPort, const xnetportsubmit* pOps, uint32 iCount)
	{
		__xnet_kqueue_ctx* pCtx = pPort ? (__xnet_kqueue_ctx*)pPort->pCtx : NULL;
		if ( !pCtx || !pOps || iCount == 0 ) { return XRT_NET_ERROR; }
		for ( uint32 i = 0; i < iCount; ++i ) {
			if ( !__xnetPortKqueueValidOp(pOps[i].iOpType) ) { return XRT_NET_ERROR; }
			if ( pOps[i].iOpType == XNET_PORT_OP_CLOSE ) {
				if ( pOps[i].hSocket != (intptr_t)XNET_SOCKET_INVALID && pOps[i].hSocket != 0 ) {
					__xnetPortKqueueRemoveWatch(pCtx, (int)pOps[i].hSocket);
				}
				continue;
			}
			if ( pOps[i].iOpType == XNET_PORT_OP_CONNECT ) {
				if ( __xnetPortKqueueSubmitConnect(pCtx, &pOps[i]) != XRT_NET_OK ) { return XRT_NET_ERROR; }
			} else if ( pOps[i].iOpType == XNET_PORT_OP_ACCEPT ) {
				if ( (pOps[i].iFlags & XNET_PORT_EVENT_F_ACCEPTED_OPEN) != 0 || pOps[i].tAddr.iFamily != 0 ) {
					if ( !__xnetPortKqueuePostSubmitEvent(pCtx, &pOps[i], XRT_NET_OK) ) { return XRT_NET_ERROR; }
				} else if ( __xnetPortKqueueSubmitWatch(pCtx, &pOps[i], __XNET_KQUEUE_W_ACCEPT) != XRT_NET_OK ) {
					return XRT_NET_ERROR;
				}
			} else if ( pOps[i].iOpType == XNET_PORT_OP_RECV && pOps[i].pChain ) {
				if ( !__xnetPortKqueuePostSubmitEvent(pCtx, &pOps[i], XRT_NET_OK) ) { return XRT_NET_ERROR; }
			} else if ( pOps[i].iOpType == XNET_PORT_OP_RECV ) {
				if ( __xnetPortKqueueSubmitWatch(pCtx, &pOps[i], __XNET_KQUEUE_W_RECV) != XRT_NET_OK ) { return XRT_NET_ERROR; }
			} else if ( pOps[i].iOpType == XNET_PORT_OP_SEND ) {
				xnetportevent tEvent;
				if ( pOps[i].hSocket == (intptr_t)XNET_SOCKET_INVALID || pOps[i].hSocket == 0 ) {
					if ( !__xnetPortKqueuePostSubmitEvent(pCtx, &pOps[i], XRT_NET_OK) ) { return XRT_NET_ERROR; }
				} else if ( __xnetPortKqueueBuildSendEvent(&pOps[i], &tEvent) ) {
					if ( !__xnetPortKqueueQueueEvent(pCtx, &tEvent) ) { return XRT_NET_ERROR; }
				} else if ( __xnetPortKqueueSubmitWatch(pCtx, &pOps[i], __XNET_KQUEUE_W_SEND) != XRT_NET_OK ) {
					return XRT_NET_ERROR;
				}
			} else if ( pOps[i].iOpType == XNET_PORT_OP_RECVFROM ) {
				if ( __xnetPortKqueueSubmitWatch(pCtx, &pOps[i], __XNET_KQUEUE_W_RECVFROM) != XRT_NET_OK ) { return XRT_NET_ERROR; }
			} else if ( pOps[i].iOpType == XNET_PORT_OP_SENDTO ) {
				xnetportevent tEvent;
				if ( pOps[i].hSocket == (intptr_t)XNET_SOCKET_INVALID || pOps[i].hSocket == 0 ) {
					if ( !__xnetPortKqueuePostSubmitEvent(pCtx, &pOps[i], XRT_NET_OK) ) { return XRT_NET_ERROR; }
				} else if ( __xnetPortKqueueBuildSendToEvent(&pOps[i], &tEvent) ) {
					if ( !__xnetPortKqueueQueueEvent(pCtx, &tEvent) ) { return XRT_NET_ERROR; }
				} else if ( __xnetPortKqueueSubmitWatch(pCtx, &pOps[i], __XNET_KQUEUE_W_SENDTO) != XRT_NET_OK ) {
					return XRT_NET_ERROR;
				}
			} else if ( !__xnetPortKqueuePostSubmitEvent(pCtx, &pOps[i], XRT_NET_OK) ) {
				return XRT_NET_ERROR;
			}
		}
		(void)pPort->pOps->Wake(pPort);
		return XRT_NET_OK;
	}


	// 内部函数：提取端口 kqueue
	static uint32 __xnetPortKqueueHarvest(xnetport* pPort, xnetportevent* pEvents, uint32 iMaxEvents, uint32 iTimeoutMs)
	{
		struct kevent arrReady[64];
		struct timespec tTimeout;
		struct timespec* pTimeout = &tTimeout;
		uint32 iCount = 0;
		__xnet_kqueue_ctx* pCtx = pPort ? (__xnet_kqueue_ctx*)pPort->pCtx : NULL;
		if ( !pCtx || !pEvents || iMaxEvents == 0 ) { return 0; }
		iCount += __xnetPortKqueueHarvestTimers(pCtx, pEvents + iCount, iMaxEvents - iCount);
		if ( iCount >= iMaxEvents ) { return iCount; }
		iCount += __xnetPortKqueueDrainPosted(pCtx, pEvents + iCount, iMaxEvents - iCount);
		if ( iCount >= iMaxEvents ) { return iCount; }
		{
			tTimeout.tv_sec = (time_t)(iTimeoutMs / 1000u);
			tTimeout.tv_nsec = (long)((iTimeoutMs % 1000u) * 1000000u);
			if ( iTimeoutMs == 0xffffffffu ) { pTimeout = NULL; }
			int iRet = kevent(pCtx->hKqueue, NULL, 0, arrReady, 64, pTimeout);
			if ( iRet > 0 ) {
				for ( int i = 0; i < iRet && iCount < iMaxEvents; ++i ) {
					if ( arrReady[i].udata == NULL ) {
						char aBuf[128];
						while ( read(pCtx->arrWake[0], aBuf, sizeof(aBuf)) > 0 ) {}
						iCount += __xnetPortKqueueDrainPosted(pCtx, pEvents + iCount, iMaxEvents - iCount);
						if ( iCount < iMaxEvents ) {
							memset(&pEvents[iCount], 0, sizeof(xnetportevent));
							pEvents[iCount].iType = XNET_PORT_EVENT_WAKE;
							pEvents[iCount].iStatus = XRT_NET_OK;
							pEvents[iCount].iBytes = 1;
							++iCount;
						}
					} else {
						uint32 iReady = 0;
						if ( arrReady[i].filter == EVFILT_READ ) { iReady |= __XNET_KQUEUE_READY_READ; }
						if ( arrReady[i].filter == EVFILT_WRITE ) { iReady |= __XNET_KQUEUE_READY_WRITE; }
						if ( arrReady[i].flags & EV_ERROR ) { iReady |= __XNET_KQUEUE_READY_ERROR; }
						iCount += __xnetPortKqueueProcessWatch(pCtx, (__xnet_kqueue_watch*)arrReady[i].udata,
							iReady, pEvents + iCount, iMaxEvents - iCount);
					}
				}
			}
		}
		return iCount;
	}


	// 内部函数：唤醒端口 kqueue
	static xnet_result __xnetPortKqueueWake(xnetport* pPort)
	{
		__xnet_kqueue_ctx* pCtx = pPort ? (__xnet_kqueue_ctx*)pPort->pCtx : NULL;
		char cWake = 1;
		if ( !pCtx || pCtx->arrWake[1] < 0 ) { return XRT_NET_ERROR; }
		if ( write(pCtx->arrWake[1], &cWake, 1) != 1 ) {
			if ( errno == EAGAIN || errno == EWOULDBLOCK ) { return XRT_NET_OK; }
			return XRT_NET_ERROR;
		}
		return XRT_NET_OK;
	}


	// 内部函数：设置端口 kqueue 定时器
	static xnet_result __xnetPortKqueueArmTimer(xnetport* pPort, uint64 iTimerId, uint32 iDelayMs)
	{
		__xnet_kqueue_ctx* pCtx = pPort ? (__xnet_kqueue_ctx*)pPort->pCtx : NULL;
		__xnet_kqueue_timer* pNode;
		if ( !pCtx ) { return XRT_NET_ERROR; }
		pNode = (__xnet_kqueue_timer*)XNET_ALLOC(sizeof(__xnet_kqueue_timer));
		if ( !pNode ) { return XRT_NET_ERROR; }
		memset(pNode, 0, sizeof(__xnet_kqueue_timer));
		pNode->iTimerId = iTimerId;
		pNode->iDueMs = __xnetPortKqueueNowMs() + (uint64)iDelayMs;
		pNode->pNext = pCtx->pTimers;
		pCtx->pTimers = pNode;
		return XRT_NET_OK;
	}


	// 内部函数：取消端口 kqueue 定时器
	static xnet_result __xnetPortKqueueCancelTimer(xnetport* pPort, uint64 iTimerId)
	{
		__xnet_kqueue_ctx* pCtx = pPort ? (__xnet_kqueue_ctx*)pPort->pCtx : NULL;
		__xnet_kqueue_timer** ppCur = pCtx ? &pCtx->pTimers : NULL;
		if ( !ppCur ) { return XRT_NET_ERROR; }
		while ( *ppCur ) {
			__xnet_kqueue_timer* pNode = *ppCur;
			if ( pNode->iTimerId == iTimerId ) {
				*ppCur = pNode->pNext;
				XNET_FREE(pNode);
				return XRT_NET_OK;
			}
			ppCur = &pNode->pNext;
		}
		return XRT_NET_ERROR;
	}

	static const xnetportops __g_xnetPortKqueueOps = {
		"xnet-port-kqueue",
		__xnetPortKqueueInit,
		__xnetPortKqueueUnit,
		__xnetPortKqueueSubmit,
		__xnetPortKqueueHarvest,
		__xnetPortKqueueWake,
		__xnetPortKqueueArmTimer,
		__xnetPortKqueueCancelTimer
	};

	// 网络端口 kqueue ops相关处理
	static const xnetportops* xrtNetPortKqueueOps(void)
	{
		return &__g_xnetPortKqueueOps;
	}

#else

	// 内部函数：判断 kqueue 后端是否可用
	static bool UNUSED_ATTR __xnetPortKqueueReady(const xnetport* pPort)
	{
		(void)pPort;
		return false;
	}

	// 网络端口 kqueue ops相关处理
	static const xnetportops* xrtNetPortKqueueOps(void)
	{
		return NULL;
	}

#endif

#endif

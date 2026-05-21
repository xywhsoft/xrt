#ifndef XRT_XNET_PORT_EPOLL_H
#define XRT_XNET_PORT_EPOLL_H

#if defined(__linux__)

	#define __XNET_EPOLL_INLINE_RECV 8192u
	#define __XNET_EPOLL_MAX_IOVEC   16u

	typedef struct __xnet_epoll_timer {
		struct __xnet_epoll_timer* pNext;
		uint64 iTimerId;
		uint64 iDueMs;
	} __xnet_epoll_timer;

	typedef struct __xnet_epoll_post {
		struct __xnet_epoll_post* pNext;
		xnetportevent tEvent;
	} __xnet_epoll_post;

	typedef struct __xnet_epoll_watch {
		struct __xnet_epoll_watch* pNext;
		int hSocket;
		uint32 iMask;
		xnetportsubmit tAccept;
		xnetportsubmit tConnect;
		xnetportsubmit tRecv;
		xnetportsubmit tSend;
		xnetportsubmit tRecvFrom;
		xnetportsubmit tSendTo;
		xnetspan arrSendVec[__XNET_EPOLL_MAX_IOVEC];
		xnetspan arrSendToVec[__XNET_EPOLL_MAX_IOVEC];
	} __xnet_epoll_watch;

	typedef struct {
		int hEpoll;
		int arrWake[2];
		__xnet_epoll_watch* pWatches;
		__xnet_epoll_timer* pTimers;
		__xnet_epoll_post* pPostedHead;
		__xnet_epoll_post* pPostedTail;
		pthread_mutex_t tPostedLock;
		pthread_mutex_t tWatchLock;
	} __xnet_epoll_ctx;

	#define __XNET_EPOLL_W_ACCEPT   0x00000001u
	#define __XNET_EPOLL_W_CONNECT  0x00000002u
	#define __XNET_EPOLL_W_RECV     0x00000004u
	#define __XNET_EPOLL_W_SEND     0x00000008u
	#define __XNET_EPOLL_W_RECVFROM 0x00000010u
	#define __XNET_EPOLL_W_SENDTO   0x00000020u


	// 内部函数：判断 epoll 后端是否可用
	static bool UNUSED_ATTR __xnetPortEpollReady(const xnetport* pPort)
	{
		const __xnet_epoll_ctx* pCtx = pPort ? (const __xnet_epoll_ctx*)pPort->pCtx : NULL;
		return pCtx && pCtx->hEpoll >= 0;
	}


	// 内部函数：epoll now 毫秒相关处理
	static uint64 __xnetPortEpollNowMs(void)
	{
		struct timespec tNow;
		clock_gettime(CLOCK_MONOTONIC, &tNow);
		return (uint64)tNow.tv_sec * 1000u + (uint64)(tNow.tv_nsec / 1000000u);
	}


	// 内部函数：设置 fd close-on-exec
	static void __xnetPortEpollSetCloseOnExec(int hFd)
	{
		int iFlags;
		if ( hFd < 0 ) { return; }
		iFlags = fcntl(hFd, F_GETFD, 0);
		if ( iFlags >= 0 ) {
			(void)fcntl(hFd, F_SETFD, iFlags | FD_CLOEXEC);
		}
	}


	// 内部函数：设置 fd nonblock
	static void __xnetPortEpollSetNonBlock(int hFd)
	{
		int iFlags;
		if ( hFd < 0 ) { return; }
		iFlags = fcntl(hFd, F_GETFL, 0);
		if ( iFlags >= 0 ) {
			(void)fcntl(hFd, F_SETFL, iFlags | O_NONBLOCK);
		}
	}


	// 内部函数：创建 epoll fd
	static int __xnetPortEpollCreateFd(void)
	{
		int hFd;
		#if defined(EPOLL_CLOEXEC)
			hFd = epoll_create1(EPOLL_CLOEXEC);
			if ( hFd >= 0 ) { return hFd; }
		#endif
		hFd = epoll_create(256);
		if ( hFd >= 0 ) {
			__xnetPortEpollSetCloseOnExec(hFd);
		}
		return hFd;
	}


	// 内部函数：端口 epoll 事件 type相关处理
	static uint16 __xnetPortEpollEventType(uint16 iOpType)
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


	// 内部函数：端口 epoll valid op相关处理
	static bool __xnetPortEpollValidOp(uint16 iType)
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


	// 内部函数：分配端口 epoll 事件 chain
	static xnetchain* __xnetPortEpollAllocEventChain(const void* pData, size_t iLen)
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


	// 内部函数：提交端口 epoll bytes
	static uint32 __xnetPortEpollSubmitBytes(const xnetportsubmit* pOp)
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


	// 内部函数：释放端口 epoll timers
	static void __xnetPortEpollFreeTimers(__xnet_epoll_ctx* pCtx)
	{
		while ( pCtx && pCtx->pTimers ) {
			__xnet_epoll_timer* pNext = pCtx->pTimers->pNext;
			XNET_FREE(pCtx->pTimers);
			pCtx->pTimers = pNext;
		}
	}


	// 内部函数：释放端口 epoll posts
	static void __xnetPortEpollFreePosts(__xnet_epoll_ctx* pCtx)
	{
		if ( !pCtx ) { return; }
		while ( pCtx->pPostedHead ) {
			__xnet_epoll_post* pNext = pCtx->pPostedHead->pNext;
			XNET_FREE(pCtx->pPostedHead);
			pCtx->pPostedHead = pNext;
		}
		pCtx->pPostedTail = NULL;
	}


	// 内部函数：释放端口 epoll watches
	static void __xnetPortEpollFreeWatches(__xnet_epoll_ctx* pCtx)
	{
		if ( !pCtx ) { return; }
		while ( pCtx->pWatches ) {
			__xnet_epoll_watch* pNext = pCtx->pWatches->pNext;
			XNET_FREE(pCtx->pWatches);
			pCtx->pWatches = pNext;
		}
	}


	// 内部函数：端口 epoll 队列事件相关处理
	static bool __xnetPortEpollQueueEvent(__xnet_epoll_ctx* pCtx, const xnetportevent* pEvent)
	{
		__xnet_epoll_post* pPost;
		if ( !pCtx || !pEvent ) { return false; }
		pPost = (__xnet_epoll_post*)XNET_ALLOC(sizeof(__xnet_epoll_post));
		if ( !pPost ) { return false; }
		memset(pPost, 0, sizeof(__xnet_epoll_post));
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


	// 内部函数：提取端口 epoll posted events
	static uint32 __xnetPortEpollDrainPosted(__xnet_epoll_ctx* pCtx, xnetportevent* pEvents, uint32 iMaxEvents)
	{
		uint32 iCount = 0;
		__xnet_epoll_post* pHead;
		if ( !pCtx || !pEvents || iMaxEvents == 0 ) { return 0; }
		pthread_mutex_lock(&pCtx->tPostedLock);
		pHead = pCtx->pPostedHead;
		pCtx->pPostedHead = NULL;
		pCtx->pPostedTail = NULL;
		pthread_mutex_unlock(&pCtx->tPostedLock);
		while ( pHead && iCount < iMaxEvents ) {
			__xnet_epoll_post* pNext = pHead->pNext;
			pEvents[iCount++] = pHead->tEvent;
			XNET_FREE(pHead);
			pHead = pNext;
		}
		while ( pHead ) {
			__xnet_epoll_post* pNext = pHead->pNext;
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


	// 内部函数：提取端口 epoll timers
	static uint32 __xnetPortEpollHarvestTimers(__xnet_epoll_ctx* pCtx, xnetportevent* pEvents, uint32 iMaxEvents)
	{
		uint32 iCount = 0;
		uint64 iNowMs = __xnetPortEpollNowMs();
		__xnet_epoll_timer** ppCur = pCtx ? &pCtx->pTimers : NULL;
		while ( ppCur && *ppCur && iCount < iMaxEvents ) {
			__xnet_epoll_timer* pNode = *ppCur;
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


	// 内部函数：查找端口 epoll watch
	static __xnet_epoll_watch* __xnetPortEpollFindWatch(__xnet_epoll_ctx* pCtx, int hSocket)
	{
		__xnet_epoll_watch* pWatch = pCtx ? pCtx->pWatches : NULL;
		while ( pWatch ) {
			if ( pWatch->hSocket == hSocket ) { return pWatch; }
			pWatch = pWatch->pNext;
		}
		return NULL;
	}


	// 内部函数：获取端口 epoll watch
	static __xnet_epoll_watch* __xnetPortEpollGetWatch(__xnet_epoll_ctx* pCtx, int hSocket)
	{
		__xnet_epoll_watch* pWatch;
		if ( !pCtx || hSocket < 0 ) { return NULL; }
		pWatch = __xnetPortEpollFindWatch(pCtx, hSocket);
		if ( pWatch ) { return pWatch; }
		pWatch = (__xnet_epoll_watch*)XNET_ALLOC(sizeof(__xnet_epoll_watch));
		if ( !pWatch ) { return NULL; }
		memset(pWatch, 0, sizeof(__xnet_epoll_watch));
		pWatch->hSocket = hSocket;
		pWatch->pNext = pCtx->pWatches;
		pCtx->pWatches = pWatch;
		return pWatch;
	}


	// 内部函数：端口 epoll mask 转换
	static uint32 __xnetPortEpollEventsFromMask(uint32 iMask)
	{
		uint32 iEvents = EPOLLERR | EPOLLHUP;
		if ( iMask & (__XNET_EPOLL_W_ACCEPT | __XNET_EPOLL_W_RECV | __XNET_EPOLL_W_RECVFROM) ) {
			iEvents |= EPOLLIN;
		}
		if ( iMask & (__XNET_EPOLL_W_CONNECT | __XNET_EPOLL_W_SEND | __XNET_EPOLL_W_SENDTO) ) {
			iEvents |= EPOLLOUT;
		}
		#if defined(EPOLLRDHUP)
			if ( iMask & __XNET_EPOLL_W_RECV ) {
				iEvents |= EPOLLRDHUP;
			}
		#endif
		return iEvents;
	}


	// 内部函数：刷新端口 epoll watch
	static bool __xnetPortEpollRefreshWatch(__xnet_epoll_ctx* pCtx, __xnet_epoll_watch* pWatch)
	{
		struct epoll_event tEv;
		int iCtl;
		if ( !pCtx || !pWatch || pCtx->hEpoll < 0 ) { return false; }
		if ( pWatch->iMask == 0 ) {
			(void)epoll_ctl(pCtx->hEpoll, EPOLL_CTL_DEL, pWatch->hSocket, NULL);
			return true;
		}
		memset(&tEv, 0, sizeof(tEv));
		tEv.events = __xnetPortEpollEventsFromMask(pWatch->iMask);
		tEv.data.ptr = pWatch;
		iCtl = epoll_ctl(pCtx->hEpoll, EPOLL_CTL_MOD, pWatch->hSocket, &tEv);
		if ( iCtl == 0 ) { return true; }
		if ( errno == ENOENT ) {
			return epoll_ctl(pCtx->hEpoll, EPOLL_CTL_ADD, pWatch->hSocket, &tEv) == 0;
		}
		return false;
	}


	// 内部函数：移除端口 epoll watch op
	static void __xnetPortEpollClearWatchBit(__xnet_epoll_ctx* pCtx, __xnet_epoll_watch* pWatch, uint32 iBit)
	{
		if ( !pCtx || !pWatch ) { return; }
		pWatch->iMask &= ~iBit;
		(void)__xnetPortEpollRefreshWatch(pCtx, pWatch);
	}


	// 内部函数：复制提交向量
	static bool __xnetPortEpollCopyVec(xnetportsubmit* pDst, xnetspan* pStorage, const xnetportsubmit* pSrc)
	{
		if ( !pDst || !pStorage || !pSrc ) { return false; }
		*pDst = *pSrc;
		if ( pSrc->pVec && pSrc->iVecCount > 0 ) {
			if ( pSrc->iVecCount > __XNET_EPOLL_MAX_IOVEC ) { return false; }
			memcpy(pStorage, pSrc->pVec, sizeof(xnetspan) * pSrc->iVecCount);
			pDst->pVec = pStorage;
		}
		return true;
	}


	// 内部函数：构建 epoll iovec
	static uint32 __xnetPortEpollBuildIov(const xnetportsubmit* pOp, struct iovec* arrIov, uint32 iMaxCount)
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
			xnetspan arrSpan[__XNET_EPOLL_MAX_IOVEC];
			uint32 iCount = xrtNetChainGetSpans(pOp->pChain, arrSpan, iMaxCount);
			for ( uint32 i = 0; i < iCount; ++i ) {
				arrIov[i].iov_base = (void*)arrSpan[i].pData;
				arrIov[i].iov_len = (size_t)arrSpan[i].iLen;
			}
			return iCount;
		}
		return 0;
	}


	// 内部函数：提交端口 epoll watch
	static xnet_result __xnetPortEpollSubmitWatch(__xnet_epoll_ctx* pCtx, const xnetportsubmit* pOp, uint32 iBit)
	{
		__xnet_epoll_watch* pWatch;
		if ( !pCtx || !pOp || pOp->hSocket == (intptr_t)XNET_SOCKET_INVALID || pOp->hSocket == 0 ) {
			return XRT_NET_ERROR;
		}
		__xnetPortEpollSetNonBlock((int)pOp->hSocket);
		pthread_mutex_lock(&pCtx->tWatchLock);
		pWatch = __xnetPortEpollGetWatch(pCtx, (int)pOp->hSocket);
		if ( !pWatch ) {
			pthread_mutex_unlock(&pCtx->tWatchLock);
			return XRT_NET_ERROR;
		}
		if ( iBit == __XNET_EPOLL_W_ACCEPT ) {
			pWatch->tAccept = *pOp;
		} else if ( iBit == __XNET_EPOLL_W_RECV ) {
			pWatch->tRecv = *pOp;
		} else if ( iBit == __XNET_EPOLL_W_RECVFROM ) {
			pWatch->tRecvFrom = *pOp;
		} else if ( iBit == __XNET_EPOLL_W_SEND ) {
			if ( !__xnetPortEpollCopyVec(&pWatch->tSend, pWatch->arrSendVec, pOp) ) {
				pthread_mutex_unlock(&pCtx->tWatchLock);
				return XRT_NET_ERROR;
			}
		} else if ( iBit == __XNET_EPOLL_W_SENDTO ) {
			if ( !__xnetPortEpollCopyVec(&pWatch->tSendTo, pWatch->arrSendToVec, pOp) ) {
				pthread_mutex_unlock(&pCtx->tWatchLock);
				return XRT_NET_ERROR;
			}
		} else if ( iBit == __XNET_EPOLL_W_CONNECT ) {
			pWatch->tConnect = *pOp;
		}
		pWatch->iMask |= iBit;
		if ( !__xnetPortEpollRefreshWatch(pCtx, pWatch) ) {
			pthread_mutex_unlock(&pCtx->tWatchLock);
			return XRT_NET_ERROR;
		}
		pthread_mutex_unlock(&pCtx->tWatchLock);
		return XRT_NET_OK;
	}


	// 内部函数：立即投递端口 epoll 事件
	static bool __xnetPortEpollPostSubmitEvent(__xnet_epoll_ctx* pCtx, const xnetportsubmit* pOp, xnet_result iStatus)
	{
		xnetportevent tEvent;
		if ( !pCtx || !pOp ) { return false; }
		memset(&tEvent, 0, sizeof(tEvent));
		tEvent.iType = __xnetPortEpollEventType(pOp->iOpType);
		tEvent.iStatus = iStatus;
		tEvent.iFlags = pOp->iFlags;
		tEvent.iBytes = __xnetPortEpollSubmitBytes(pOp);
		tEvent.iOpId = pOp->iOpId;
		tEvent.hSocket = pOp->hSocket;
		if ( pOp->iOpType == XNET_PORT_OP_ACCEPT ) {
			tEvent.hSocket = (intptr_t)XNET_SOCKET_INVALID;
		}
		tEvent.pUserData = pOp->pUserData;
		tEvent.pChain = pOp->pChain;
		tEvent.tAddr = pOp->tAddr;
		return tEvent.iType != XNET_PORT_EVENT_NONE && __xnetPortEpollQueueEvent(pCtx, &tEvent);
	}


	// 内部函数：提交端口 epoll connect
	static xnet_result __xnetPortEpollSubmitConnect(__xnet_epoll_ctx* pCtx, const xnetportsubmit* pOp)
	{
		struct sockaddr_storage tStorage;
		socklen_t iAddrLen = 0;
		int iRet;
		if ( !pCtx || !pOp || !__xnetAddrToSockAddr(&pOp->tAddr, &tStorage, &iAddrLen) ) { return XRT_NET_ERROR; }
		__xnetPortEpollSetNonBlock((int)pOp->hSocket);
		iRet = connect((int)pOp->hSocket, (struct sockaddr*)&tStorage, iAddrLen);
		if ( iRet == 0 ) {
			return __xnetPortEpollPostSubmitEvent(pCtx, pOp, XRT_NET_OK) ? XRT_NET_OK : XRT_NET_ERROR;
		}
		if ( errno == EINPROGRESS || errno == EALREADY || errno == EWOULDBLOCK ) {
			return __xnetPortEpollSubmitWatch(pCtx, pOp, __XNET_EPOLL_W_CONNECT);
		}
		return __xnetPortEpollPostSubmitEvent(pCtx, pOp, XRT_NET_ERROR) ? XRT_NET_OK : XRT_NET_ERROR;
	}


	// 内部函数：构建端口 epoll recv 事件
	static bool __xnetPortEpollBuildRecvEvent(const xnetportsubmit* pOp, xnetportevent* pEvent)
	{
		char aBuf[__XNET_EPOLL_INLINE_RECV];
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
			pEvent->pChain = __xnetPortEpollAllocEventChain(aBuf, (size_t)iRet);
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


	// 内部函数：构建端口 epoll send 事件
	static bool __xnetPortEpollBuildSendEvent(const xnetportsubmit* pOp, xnetportevent* pEvent)
	{
		struct iovec arrIov[__XNET_EPOLL_MAX_IOVEC];
		uint32 iCount;
		ssize_t iRet;
		if ( !pOp || !pEvent ) { return false; }
		iCount = __xnetPortEpollBuildIov(pOp, arrIov, __XNET_EPOLL_MAX_IOVEC);
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


	// 内部函数：构建端口 epoll recvfrom 事件
	static bool __xnetPortEpollBuildRecvFromEvent(const xnetportsubmit* pOp, xnetportevent* pEvent)
	{
		char aBuf[__XNET_EPOLL_INLINE_RECV];
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
			pEvent->pChain = __xnetPortEpollAllocEventChain(aBuf, (size_t)iRet);
			if ( !pEvent->pChain ) {
				pEvent->iStatus = XRT_NET_ERROR;
				pEvent->iBytes = 0;
			}
			(void)__xnetAddrFromSockAddr(&pEvent->tAddr, (const struct sockaddr*)&tStorage);
		}
		return true;
	}


	// 内部函数：构建端口 epoll sendto 事件
	static bool __xnetPortEpollBuildSendToEvent(const xnetportsubmit* pOp, xnetportevent* pEvent)
	{
		struct iovec arrIov[__XNET_EPOLL_MAX_IOVEC];
		struct sockaddr_storage tStorage;
		struct msghdr tMsg;
		socklen_t iAddrLen = 0;
		uint32 iCount;
		ssize_t iRet;
		if ( !pOp || !pEvent || !__xnetAddrToSockAddr(&pOp->tAddr, &tStorage, &iAddrLen) ) { return false; }
		iCount = __xnetPortEpollBuildIov(pOp, arrIov, __XNET_EPOLL_MAX_IOVEC);
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


	// 内部函数：构建端口 epoll connect 事件
	static bool __xnetPortEpollBuildConnectEvent(const xnetportsubmit* pOp, xnetportevent* pEvent)
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


	// 内部函数：处理端口 epoll watch 事件
	static uint32 __xnetPortEpollProcessWatch(__xnet_epoll_ctx* pCtx, __xnet_epoll_watch* pWatch, uint32 iReady, xnetportevent* pEvents, uint32 iMaxEvents)
	{
		xnetportevent tEvent;
		uint32 iCount = 0;
		if ( !pCtx || !pWatch || !pEvents || iMaxEvents == 0 ) { return 0; }
		pthread_mutex_lock(&pCtx->tWatchLock);
		if ( (iReady & (EPOLLERR | EPOLLHUP | EPOLLOUT)) && (pWatch->iMask & __XNET_EPOLL_W_CONNECT) && iCount < iMaxEvents ) {
			if ( __xnetPortEpollBuildConnectEvent(&pWatch->tConnect, &tEvent) ) {
				__xnetPortEpollClearWatchBit(pCtx, pWatch, __XNET_EPOLL_W_CONNECT);
				pEvents[iCount++] = tEvent;
			}
		}
		if ( (iReady & (EPOLLERR | EPOLLHUP | EPOLLOUT)) && (pWatch->iMask & __XNET_EPOLL_W_SEND) && iCount < iMaxEvents ) {
			if ( __xnetPortEpollBuildSendEvent(&pWatch->tSend, &tEvent) ) {
				__xnetPortEpollClearWatchBit(pCtx, pWatch, __XNET_EPOLL_W_SEND);
				pEvents[iCount++] = tEvent;
			}
		}
		if ( (iReady & (EPOLLERR | EPOLLHUP | EPOLLOUT)) && (pWatch->iMask & __XNET_EPOLL_W_SENDTO) && iCount < iMaxEvents ) {
			if ( __xnetPortEpollBuildSendToEvent(&pWatch->tSendTo, &tEvent) ) {
				__xnetPortEpollClearWatchBit(pCtx, pWatch, __XNET_EPOLL_W_SENDTO);
				pEvents[iCount++] = tEvent;
			}
		}
		if ( (iReady & (EPOLLERR | EPOLLHUP | EPOLLIN)) && (pWatch->iMask & __XNET_EPOLL_W_ACCEPT) && iCount < iMaxEvents ) {
			memset(&tEvent, 0, sizeof(tEvent));
			tEvent.iType = XNET_PORT_EVENT_ACCEPT;
			tEvent.iStatus = XRT_NET_OK;
			tEvent.iOpId = pWatch->tAccept.iOpId;
			tEvent.hSocket = (intptr_t)XNET_SOCKET_INVALID;
			tEvent.pUserData = pWatch->tAccept.pUserData;
			__xnetPortEpollClearWatchBit(pCtx, pWatch, __XNET_EPOLL_W_ACCEPT);
			pEvents[iCount++] = tEvent;
		}
		if ( (iReady & (EPOLLERR | EPOLLHUP | EPOLLIN
			#if defined(EPOLLRDHUP)
				| EPOLLRDHUP
			#endif
			)) && (pWatch->iMask & __XNET_EPOLL_W_RECV) && iCount < iMaxEvents ) {
			if ( __xnetPortEpollBuildRecvEvent(&pWatch->tRecv, &tEvent) ) {
				__xnetPortEpollClearWatchBit(pCtx, pWatch, __XNET_EPOLL_W_RECV);
				pEvents[iCount++] = tEvent;
			}
		}
		if ( (iReady & (EPOLLERR | EPOLLHUP | EPOLLIN)) && (pWatch->iMask & __XNET_EPOLL_W_RECVFROM) && iCount < iMaxEvents ) {
			if ( __xnetPortEpollBuildRecvFromEvent(&pWatch->tRecvFrom, &tEvent) ) {
				__xnetPortEpollClearWatchBit(pCtx, pWatch, __XNET_EPOLL_W_RECVFROM);
				pEvents[iCount++] = tEvent;
			}
		}
		pthread_mutex_unlock(&pCtx->tWatchLock);
		return iCount;
	}


	// 内部函数：初始化端口 epoll
	static xnet_result __xnetPortEpollInit(xnetport* pPort, const xnetportconfig* pCfg, ptr pOwner)
	{
		__xnet_epoll_ctx* pCtx;
		struct epoll_event tEv;
		(void)pCfg;
		(void)pOwner;
		if ( !pPort ) { return XRT_NET_ERROR; }
		pCtx = (__xnet_epoll_ctx*)XNET_ALLOC(sizeof(__xnet_epoll_ctx));
		if ( !pCtx ) { return XRT_NET_ERROR; }
		memset(pCtx, 0, sizeof(__xnet_epoll_ctx));
		pCtx->hEpoll = -1;
		pCtx->arrWake[0] = -1;
		pCtx->arrWake[1] = -1;
		pCtx->hEpoll = __xnetPortEpollCreateFd();
		if ( pCtx->hEpoll < 0 ) {
			XNET_FREE(pCtx);
			return XRT_NET_ERROR;
		}
		if ( pipe(pCtx->arrWake) != 0 ) {
			close(pCtx->hEpoll);
			XNET_FREE(pCtx);
			return XRT_NET_ERROR;
		}
		__xnetPortEpollSetCloseOnExec(pCtx->arrWake[0]);
		__xnetPortEpollSetCloseOnExec(pCtx->arrWake[1]);
		__xnetPortEpollSetNonBlock(pCtx->arrWake[0]);
		__xnetPortEpollSetNonBlock(pCtx->arrWake[1]);
		if ( pthread_mutex_init(&pCtx->tPostedLock, NULL) != 0 ) {
			close(pCtx->arrWake[0]);
			close(pCtx->arrWake[1]);
			close(pCtx->hEpoll);
			XNET_FREE(pCtx);
			return XRT_NET_ERROR;
		}
		if ( pthread_mutex_init(&pCtx->tWatchLock, NULL) != 0 ) {
			pthread_mutex_destroy(&pCtx->tPostedLock);
			close(pCtx->arrWake[0]);
			close(pCtx->arrWake[1]);
			close(pCtx->hEpoll);
			XNET_FREE(pCtx);
			return XRT_NET_ERROR;
		}
		memset(&tEv, 0, sizeof(tEv));
		tEv.events = EPOLLIN;
		tEv.data.ptr = NULL;
		if ( epoll_ctl(pCtx->hEpoll, EPOLL_CTL_ADD, pCtx->arrWake[0], &tEv) != 0 ) {
			pthread_mutex_destroy(&pCtx->tWatchLock);
			pthread_mutex_destroy(&pCtx->tPostedLock);
			close(pCtx->arrWake[0]);
			close(pCtx->arrWake[1]);
			close(pCtx->hEpoll);
			XNET_FREE(pCtx);
			return XRT_NET_ERROR;
		}
		pPort->pCtx = pCtx;
		return XRT_NET_OK;
	}


	// 内部函数：释放端口 epoll
	static void __xnetPortEpollUnit(xnetport* pPort)
	{
		__xnet_epoll_ctx* pCtx = pPort ? (__xnet_epoll_ctx*)pPort->pCtx : NULL;
		if ( !pCtx ) { return; }
		__xnetPortEpollFreeTimers(pCtx);
		__xnetPortEpollFreePosts(pCtx);
		__xnetPortEpollFreeWatches(pCtx);
		pthread_mutex_destroy(&pCtx->tWatchLock);
		pthread_mutex_destroy(&pCtx->tPostedLock);
		if ( pCtx->arrWake[0] >= 0 ) { close(pCtx->arrWake[0]); }
		if ( pCtx->arrWake[1] >= 0 ) { close(pCtx->arrWake[1]); }
		if ( pCtx->hEpoll >= 0 ) { close(pCtx->hEpoll); }
		XNET_FREE(pCtx);
		if ( pPort ) { pPort->pCtx = NULL; }
	}


	// 内部函数：提交端口 epoll
	static xnet_result __xnetPortEpollSubmit(xnetport* pPort, const xnetportsubmit* pOps, uint32 iCount)
	{
		__xnet_epoll_ctx* pCtx = pPort ? (__xnet_epoll_ctx*)pPort->pCtx : NULL;
		if ( !pCtx || !pOps || iCount == 0 ) { return XRT_NET_ERROR; }
		for ( uint32 i = 0; i < iCount; ++i ) {
			if ( !__xnetPortEpollValidOp(pOps[i].iOpType) ) { return XRT_NET_ERROR; }
			if ( pOps[i].iOpType == XNET_PORT_OP_CLOSE ) {
				if ( pOps[i].hSocket != (intptr_t)XNET_SOCKET_INVALID && pOps[i].hSocket != 0 ) {
					(void)epoll_ctl(pCtx->hEpoll, EPOLL_CTL_DEL, (int)pOps[i].hSocket, NULL);
				}
				continue;
			}
			if ( pOps[i].iOpType == XNET_PORT_OP_CONNECT ) {
				if ( __xnetPortEpollSubmitConnect(pCtx, &pOps[i]) != XRT_NET_OK ) { return XRT_NET_ERROR; }
			} else if ( pOps[i].iOpType == XNET_PORT_OP_ACCEPT ) {
				if ( (pOps[i].iFlags & XNET_PORT_EVENT_F_ACCEPTED_OPEN) != 0 || pOps[i].tAddr.iFamily != 0 ) {
					if ( !__xnetPortEpollPostSubmitEvent(pCtx, &pOps[i], XRT_NET_OK) ) { return XRT_NET_ERROR; }
				} else if ( __xnetPortEpollSubmitWatch(pCtx, &pOps[i], __XNET_EPOLL_W_ACCEPT) != XRT_NET_OK ) {
					return XRT_NET_ERROR;
				}
			} else if ( pOps[i].iOpType == XNET_PORT_OP_RECV && pOps[i].pChain ) {
				if ( !__xnetPortEpollPostSubmitEvent(pCtx, &pOps[i], XRT_NET_OK) ) { return XRT_NET_ERROR; }
			} else if ( pOps[i].iOpType == XNET_PORT_OP_RECV ) {
				if ( __xnetPortEpollSubmitWatch(pCtx, &pOps[i], __XNET_EPOLL_W_RECV) != XRT_NET_OK ) { return XRT_NET_ERROR; }
			} else if ( pOps[i].iOpType == XNET_PORT_OP_SEND ) {
				xnetportevent tEvent;
				if ( pOps[i].hSocket == (intptr_t)XNET_SOCKET_INVALID || pOps[i].hSocket == 0 ) {
					if ( !__xnetPortEpollPostSubmitEvent(pCtx, &pOps[i], XRT_NET_OK) ) { return XRT_NET_ERROR; }
				} else if ( __xnetPortEpollBuildSendEvent(&pOps[i], &tEvent) ) {
					if ( !__xnetPortEpollQueueEvent(pCtx, &tEvent) ) { return XRT_NET_ERROR; }
				} else if ( __xnetPortEpollSubmitWatch(pCtx, &pOps[i], __XNET_EPOLL_W_SEND) != XRT_NET_OK ) {
					return XRT_NET_ERROR;
				}
			} else if ( pOps[i].iOpType == XNET_PORT_OP_RECVFROM ) {
				if ( __xnetPortEpollSubmitWatch(pCtx, &pOps[i], __XNET_EPOLL_W_RECVFROM) != XRT_NET_OK ) { return XRT_NET_ERROR; }
			} else if ( pOps[i].iOpType == XNET_PORT_OP_SENDTO ) {
				xnetportevent tEvent;
				if ( pOps[i].hSocket == (intptr_t)XNET_SOCKET_INVALID || pOps[i].hSocket == 0 ) {
					if ( !__xnetPortEpollPostSubmitEvent(pCtx, &pOps[i], XRT_NET_OK) ) { return XRT_NET_ERROR; }
				} else if ( __xnetPortEpollBuildSendToEvent(&pOps[i], &tEvent) ) {
					if ( !__xnetPortEpollQueueEvent(pCtx, &tEvent) ) { return XRT_NET_ERROR; }
				} else if ( __xnetPortEpollSubmitWatch(pCtx, &pOps[i], __XNET_EPOLL_W_SENDTO) != XRT_NET_OK ) {
					return XRT_NET_ERROR;
				}
			} else if ( !__xnetPortEpollPostSubmitEvent(pCtx, &pOps[i], XRT_NET_OK) ) {
				return XRT_NET_ERROR;
			}
		}
		(void)pPort->pOps->Wake(pPort);
		return XRT_NET_OK;
	}


	// 内部函数：提取端口 epoll
	static uint32 __xnetPortEpollHarvest(xnetport* pPort, xnetportevent* pEvents, uint32 iMaxEvents, uint32 iTimeoutMs)
	{
		struct epoll_event arrReady[64];
		uint32 iCount = 0;
		__xnet_epoll_ctx* pCtx = pPort ? (__xnet_epoll_ctx*)pPort->pCtx : NULL;
		if ( !pCtx || !pEvents || iMaxEvents == 0 ) { return 0; }
		iCount += __xnetPortEpollHarvestTimers(pCtx, pEvents + iCount, iMaxEvents - iCount);
		if ( iCount >= iMaxEvents ) { return iCount; }
		iCount += __xnetPortEpollDrainPosted(pCtx, pEvents + iCount, iMaxEvents - iCount);
		if ( iCount >= iMaxEvents ) { return iCount; }
		{
			int iRet = epoll_wait(pCtx->hEpoll, arrReady, 64, (int)iTimeoutMs);
			if ( iRet > 0 ) {
				for ( int i = 0; i < iRet && iCount < iMaxEvents; ++i ) {
					if ( arrReady[i].data.ptr == NULL ) {
						char aBuf[128];
						while ( read(pCtx->arrWake[0], aBuf, sizeof(aBuf)) > 0 ) {}
						iCount += __xnetPortEpollDrainPosted(pCtx, pEvents + iCount, iMaxEvents - iCount);
						if ( iCount < iMaxEvents ) {
							memset(&pEvents[iCount], 0, sizeof(xnetportevent));
							pEvents[iCount].iType = XNET_PORT_EVENT_WAKE;
							pEvents[iCount].iStatus = XRT_NET_OK;
							pEvents[iCount].iBytes = 1;
							++iCount;
						}
					} else {
						iCount += __xnetPortEpollProcessWatch(pCtx, (__xnet_epoll_watch*)arrReady[i].data.ptr,
							arrReady[i].events, pEvents + iCount, iMaxEvents - iCount);
					}
				}
			}
		}
		return iCount;
	}


	// 内部函数：唤醒端口 epoll
	static xnet_result __xnetPortEpollWake(xnetport* pPort)
	{
		__xnet_epoll_ctx* pCtx = pPort ? (__xnet_epoll_ctx*)pPort->pCtx : NULL;
		char cWake = 1;
		if ( !pCtx || pCtx->arrWake[1] < 0 ) { return XRT_NET_ERROR; }
		if ( write(pCtx->arrWake[1], &cWake, 1) != 1 ) {
			if ( errno == EAGAIN || errno == EWOULDBLOCK ) { return XRT_NET_OK; }
			return XRT_NET_ERROR;
		}
		return XRT_NET_OK;
	}


	// 内部函数：设置端口 epoll 定时器
	static xnet_result __xnetPortEpollArmTimer(xnetport* pPort, uint64 iTimerId, uint32 iDelayMs)
	{
		__xnet_epoll_ctx* pCtx = pPort ? (__xnet_epoll_ctx*)pPort->pCtx : NULL;
		__xnet_epoll_timer* pNode;
		if ( !pCtx ) { return XRT_NET_ERROR; }
		pNode = (__xnet_epoll_timer*)XNET_ALLOC(sizeof(__xnet_epoll_timer));
		if ( !pNode ) { return XRT_NET_ERROR; }
		memset(pNode, 0, sizeof(__xnet_epoll_timer));
		pNode->iTimerId = iTimerId;
		pNode->iDueMs = __xnetPortEpollNowMs() + (uint64)iDelayMs;
		pNode->pNext = pCtx->pTimers;
		pCtx->pTimers = pNode;
		return XRT_NET_OK;
	}


	// 内部函数：取消端口 epoll 定时器
	static xnet_result __xnetPortEpollCancelTimer(xnetport* pPort, uint64 iTimerId)
	{
		__xnet_epoll_ctx* pCtx = pPort ? (__xnet_epoll_ctx*)pPort->pCtx : NULL;
		__xnet_epoll_timer** ppCur = pCtx ? &pCtx->pTimers : NULL;
		if ( !ppCur ) { return XRT_NET_ERROR; }
		while ( *ppCur ) {
			__xnet_epoll_timer* pNode = *ppCur;
			if ( pNode->iTimerId == iTimerId ) {
				*ppCur = pNode->pNext;
				XNET_FREE(pNode);
				return XRT_NET_OK;
			}
			ppCur = &pNode->pNext;
		}
		return XRT_NET_ERROR;
	}

	static const xnetportops __g_xnetPortEpollOps = {
		"xnet-port-epoll",
		__xnetPortEpollInit,
		__xnetPortEpollUnit,
		__xnetPortEpollSubmit,
		__xnetPortEpollHarvest,
		__xnetPortEpollWake,
		__xnetPortEpollArmTimer,
		__xnetPortEpollCancelTimer
	};

	// 网络端口 epoll ops相关处理
	static const xnetportops* xrtNetPortEpollOps(void)
	{
		return &__g_xnetPortEpollOps;
	}

#else

	// 内部函数：判断 epoll 后端是否可用
	static bool UNUSED_ATTR __xnetPortEpollReady(const xnetport* pPort)
	{
		(void)pPort;
		return false;
	}

	// 网络端口 epoll ops相关处理
	static const xnetportops* xrtNetPortEpollOps(void)
	{
		return NULL;
	}

#endif

#endif

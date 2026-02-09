



/*
	NetPoll - IO 模型抽象层 [Ver1.0]
	
	Windows: IOCP (I/O Completion Port)
	Linux:   io_uring (内核 >= 5.1, 无 epoll 回退)
	
	基于完成端口模型的统一异步 IO 接口
*/



/* ============================== 通用定义 ============================== */

#define __XRT_POLL_OP_RECV    1
#define __XRT_POLL_OP_SEND    2
#define __XRT_POLL_OP_ACCEPT  3

#define __XRT_POLL_MAX_OPS    4096
#define __XRT_POLL_RECV_SIZE  8192



/* ============================== Windows IOCP 实现 ============================== */

#if defined(_WIN32) || defined(_WIN64)

// IOCP 操作结构
typedef struct {
	WSAOVERLAPPED tOverlapped;
	xnetconn* pConn;
	xnetconn* pAcceptConn;       // 用于 accept 操作的客户端连接
	char aRecvBuf[__XRT_POLL_RECV_SIZE];
	WSABUF tWSABuf;
	DWORD iBytes;
	DWORD iFlags;
	int iOpType;
	bool bInUse;
} __xrt_iocp_op;

// Poller 结构定义
struct xrt_net_poller {
	HANDLE hIOCP;
	__xrt_iocp_op* pOps;
	int iOpCount;
	int iOpCapacity;
	CRITICAL_SECTION tLock;
	xpoll_fn pfnCallback;
	ptr pUserData;
	size_t iRecvBufSize;
};

static __xrt_iocp_op* __xrt_iocp_alloc_op(xnetpoller* pPoller)
{
	EnterCriticalSection(&pPoller->tLock);
	
	// 先找空闲的
	for ( int i = 0; i < pPoller->iOpCount; i++ ) {
		if ( !pPoller->pOps[i].bInUse ) {
			pPoller->pOps[i].bInUse = true;
			LeaveCriticalSection(&pPoller->tLock);
			return &pPoller->pOps[i];
		}
	}
	
	// 没有空闲的，追加
	if ( pPoller->iOpCount < pPoller->iOpCapacity ) {
		__xrt_iocp_op* pOp = &pPoller->pOps[pPoller->iOpCount++];
		memset(pOp, 0, sizeof(__xrt_iocp_op));
		pOp->bInUse = true;
		LeaveCriticalSection(&pPoller->tLock);
		return pOp;
	}
	
	LeaveCriticalSection(&pPoller->tLock);
	return NULL;
}

XXAPI xnetpoller* xrtPollCreate(xnetconfig* pConfig, xpoll_fn pfnCallback, ptr pUserData)
{
	xnetpoller* pPoller = (xnetpoller*)xrtCalloc(1, sizeof(xnetpoller));
	if ( !pPoller ) return NULL;
	
	pPoller->hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if ( !pPoller->hIOCP ) {
		xrtFree(pPoller);
		return NULL;
	}
	
	pPoller->iOpCapacity = __XRT_POLL_MAX_OPS;
	pPoller->pOps = (__xrt_iocp_op*)xrtCalloc(pPoller->iOpCapacity, sizeof(__xrt_iocp_op));
	if ( !pPoller->pOps ) {
		CloseHandle(pPoller->hIOCP);
		xrtFree(pPoller);
		return NULL;
	}
	
	InitializeCriticalSection(&pPoller->tLock);
	pPoller->pfnCallback = pfnCallback;
	pPoller->pUserData = pUserData;
	pPoller->iRecvBufSize = (pConfig && pConfig->iRecvBufSize > 0) ? pConfig->iRecvBufSize : __XRT_POLL_RECV_SIZE;
	
	return pPoller;
}

XXAPI void xrtPollDestroy(xnetpoller* pPoller)
{
	if ( !pPoller ) return;
	
	if ( pPoller->hIOCP ) {
		CloseHandle(pPoller->hIOCP);
	}
	if ( pPoller->pOps ) {
		xrtFree(pPoller->pOps);
	}
	DeleteCriticalSection(&pPoller->tLock);
	xrtFree(pPoller);
}

XXAPI xnet_result xrtPollAdd(xnetpoller* pPoller, xnetconn* pConn, int iEvents)
{
	if ( !pPoller || !pConn || pConn->hSocket == XSOCKET_INVALID ) return XRT_NET_ERROR;
	
	HANDLE hResult = CreateIoCompletionPort((HANDLE)pConn->hSocket, pPoller->hIOCP, (ULONG_PTR)pConn, 0);
	if ( !hResult ) {
		return XRT_NET_ERROR;
	}
	
	return XRT_NET_OK;
}

XXAPI xnet_result xrtPollRemove(xnetpoller* pPoller, xnetconn* pConn)
{
	if ( !pPoller || !pConn ) return XRT_NET_ERROR;
	
	EnterCriticalSection(&pPoller->tLock);
	for ( int i = 0; i < pPoller->iOpCount; i++ ) {
		if ( pPoller->pOps[i].pConn == pConn && pPoller->pOps[i].bInUse ) {
			pPoller->pOps[i].bInUse = false;
		}
	}
	LeaveCriticalSection(&pPoller->tLock);
	
	return XRT_NET_OK;
}

XXAPI xnet_result xrtPollPostRecv(xnetpoller* pPoller, xnetconn* pConn)
{
	if ( !pPoller || !pConn ) return XRT_NET_ERROR;
	
	__xrt_iocp_op* pOp = __xrt_iocp_alloc_op(pPoller);
	if ( !pOp ) return XRT_NET_ERROR;
	
	memset(&pOp->tOverlapped, 0, sizeof(WSAOVERLAPPED));
	pOp->pConn = pConn;
	pOp->tWSABuf.buf = pOp->aRecvBuf;
	pOp->tWSABuf.len = __XRT_POLL_RECV_SIZE;
	pOp->iOpType = __XRT_POLL_OP_RECV;
	pOp->iFlags = 0;
	
	int iResult = WSARecv(pConn->hSocket, &pOp->tWSABuf, 1, &pOp->iBytes, &pOp->iFlags, &pOp->tOverlapped, NULL);
	if ( iResult == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING ) {
		pOp->bInUse = false;
		return XRT_NET_ERROR;
	}
	
	return XRT_NET_OK;
}

XXAPI xnet_result xrtPollPostSend(xnetpoller* pPoller, xnetconn* pConn, const char* pData, size_t iLen)
{
	if ( !pPoller || !pConn || !pData || iLen == 0 ) return XRT_NET_ERROR;
	
	__xrt_iocp_op* pOp = __xrt_iocp_alloc_op(pPoller);
	if ( !pOp ) return XRT_NET_ERROR;
	
	// 复制数据到操作缓冲区，解决 buffer 生命周期问题
	size_t iCopyLen = (iLen > __XRT_POLL_RECV_SIZE) ? __XRT_POLL_RECV_SIZE : iLen;
	memcpy(pOp->aRecvBuf, pData, iCopyLen);
	
	memset(&pOp->tOverlapped, 0, sizeof(WSAOVERLAPPED));
	pOp->pConn = pConn;
	pOp->tWSABuf.buf = pOp->aRecvBuf;
	pOp->tWSABuf.len = (ULONG)iCopyLen;
	pOp->iOpType = __XRT_POLL_OP_SEND;
	pOp->iFlags = 0;
	
	DWORD iBytesSent;
	int iResult = WSASend(pConn->hSocket, &pOp->tWSABuf, 1, &iBytesSent, 0, &pOp->tOverlapped, NULL);
	if ( iResult == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING ) {
		pOp->bInUse = false;
		return XRT_NET_ERROR;
	}
	
	return XRT_NET_OK;
}

XXAPI xnet_result xrtPollPostAccept(xnetpoller* pPoller, xnetconn* pServer, xnetconn* pClient)
{
	// IOCP accept: 使用非阻塞 accept 方式（不使用 AcceptEx 以简化实现）
	// AcceptEx 需要额外的 Winsock 扩展函数加载，简化版使用 completion notification
	(void)pPoller; (void)pServer; (void)pClient;
	// Accept 将在 PollWait 的用户回调中通过 xrtSockAccept 同步处理
	return XRT_NET_OK;
}

XXAPI xnet_result xrtPollWait(xnetpoller* pPoller, int iTimeoutMs)
{
	if ( !pPoller ) return XRT_NET_ERROR;
	
	DWORD iBytesTransferred;
	ULONG_PTR iCompletionKey;
	LPOVERLAPPED pOverlapped;
	
	BOOL bSuccess = GetQueuedCompletionStatus(
		pPoller->hIOCP,
		&iBytesTransferred,
		&iCompletionKey,
		&pOverlapped,
		(DWORD)iTimeoutMs
	);
	
	if ( !bSuccess && !pOverlapped ) {
		if ( GetLastError() == WAIT_TIMEOUT ) {
			return XRT_NET_TIMEOUT;
		}
		return XRT_NET_ERROR;
	}
	
	// Wakeup 信号 (completion key = 0, overlapped = NULL)
	if ( !pOverlapped ) {
		return XRT_NET_OK;
	}
	
	__xrt_iocp_op* pOp = (__xrt_iocp_op*)pOverlapped;
	xnetconn* pConn = pOp->pConn;
	
	if ( !pOp->bInUse ) {
		return XRT_NET_OK;
	}
	
	// 连接关闭
	if ( iBytesTransferred == 0 && pOp->iOpType != __XRT_POLL_OP_ACCEPT ) {
		if ( pPoller->pfnCallback ) {
			pPoller->pfnCallback(pPoller, pConn, XRT_POLL_CLOSE, NULL, 0);
		}
		pOp->bInUse = false;
		return XRT_NET_CLOSED;
	}
	
	// 分发事件
	switch ( pOp->iOpType ) {
		case __XRT_POLL_OP_RECV:
			if ( pPoller->pfnCallback ) {
				pPoller->pfnCallback(pPoller, pConn, XRT_POLL_READ, pOp->aRecvBuf, (size_t)iBytesTransferred);
			}
			break;
		
		case __XRT_POLL_OP_SEND:
			if ( pPoller->pfnCallback ) {
				pPoller->pfnCallback(pPoller, pConn, XRT_POLL_WRITE, NULL, (size_t)iBytesTransferred);
			}
			break;
		
		case __XRT_POLL_OP_ACCEPT:
			if ( pPoller->pfnCallback ) {
				pPoller->pfnCallback(pPoller, pConn, XRT_POLL_ACCEPT, NULL, 0);
			}
			break;
	}
	
	pOp->bInUse = false;
	return XRT_NET_OK;
}

XXAPI void xrtPollWakeup(xnetpoller* pPoller)
{
	if ( !pPoller || !pPoller->hIOCP ) return;
	PostQueuedCompletionStatus(pPoller->hIOCP, 0, 0, NULL);
}

XXAPI ptr xrtPollGetUserData(xnetpoller* pPoller)
{
	if ( !pPoller ) return NULL;
	return pPoller->pUserData;
}



/* ============================== Linux io_uring 实现 ============================== */

#else  /* Linux */

/*
	io_uring 零依赖实现 (不使用 liburing)
	直接通过 syscall 调用内核接口
	最低要求: Linux 5.1+ (io_uring_setup, io_uring_enter, io_uring_register)
*/

#include <sys/syscall.h>
#include <sys/mman.h>
#include <linux/io_uring.h>

#define __XRT_URING_ENTRIES  256

// io_uring syscall 封装
static inline int __xrt_io_uring_setup(unsigned iEntries, struct io_uring_params* pParams)
{
	return (int)syscall(SYS_io_uring_setup, iEntries, pParams);
}

static inline int __xrt_io_uring_enter(int iFd, unsigned iToSubmit, unsigned iMinComplete, unsigned iFlags, void* pSig)
{
	return (int)syscall(SYS_io_uring_enter, iFd, iToSubmit, iMinComplete, iFlags, pSig, 0);
}

// io_uring 环形缓冲区
typedef struct {
	unsigned* pHead;
	unsigned* pTail;
	unsigned* pRingMask;
	unsigned* pRingEntries;
	unsigned* pFlags;
	unsigned* pArray;
	struct io_uring_sqe* pSQEs;
	size_t iRingSize;
	void* pRingPtr;
} __xrt_uring_sq;

typedef struct {
	unsigned* pHead;
	unsigned* pTail;
	unsigned* pRingMask;
	unsigned* pRingEntries;
	struct io_uring_cqe* pCQEs;
	size_t iRingSize;
	void* pRingPtr;
} __xrt_uring_cq;

// io_uring 操作结构
typedef struct {
	xnetconn* pConn;
	xnetconn* pAcceptConn;
	char aRecvBuf[__XRT_POLL_RECV_SIZE];
	int iOpType;
	bool bInUse;
	struct sockaddr_in tAcceptAddr;
	socklen_t iAcceptAddrLen;
} __xrt_uring_op;

// Poller 结构定义
struct xrt_net_poller {
	int iFd;                      // io_uring fd
	__xrt_uring_sq tSQ;
	__xrt_uring_cq tCQ;
	__xrt_uring_op* pOps;
	int iOpCount;
	int iOpCapacity;
	int iWakeupFd;                // eventfd 用于唤醒
	xpoll_fn pfnCallback;
	ptr pUserData;
	size_t iRecvBufSize;
	pthread_mutex_t tLock;
};

static __xrt_uring_op* __xrt_uring_alloc_op(xnetpoller* pPoller)
{
	pthread_mutex_lock(&pPoller->tLock);
	
	for ( int i = 0; i < pPoller->iOpCount; i++ ) {
		if ( !pPoller->pOps[i].bInUse ) {
			pPoller->pOps[i].bInUse = true;
			pthread_mutex_unlock(&pPoller->tLock);
			return &pPoller->pOps[i];
		}
	}
	
	if ( pPoller->iOpCount < pPoller->iOpCapacity ) {
		__xrt_uring_op* pOp = &pPoller->pOps[pPoller->iOpCount++];
		memset(pOp, 0, sizeof(__xrt_uring_op));
		pOp->bInUse = true;
		pthread_mutex_unlock(&pPoller->tLock);
		return pOp;
	}
	
	pthread_mutex_unlock(&pPoller->tLock);
	return NULL;
}

static struct io_uring_sqe* __xrt_uring_get_sqe(xnetpoller* pPoller)
{
	__xrt_uring_sq* pSQ = &pPoller->tSQ;
	unsigned iHead = __atomic_load_n(pSQ->pHead, __ATOMIC_ACQUIRE);
	unsigned iTail = *pSQ->pTail;
	unsigned iMask = *pSQ->pRingMask;
	
	if ( iTail - iHead >= *pSQ->pRingEntries ) {
		return NULL;  // SQ 已满
	}
	
	struct io_uring_sqe* pSQE = &pSQ->pSQEs[iTail & iMask];
	memset(pSQE, 0, sizeof(struct io_uring_sqe));
	return pSQE;
}

static void __xrt_uring_submit_sqe(xnetpoller* pPoller)
{
	__xrt_uring_sq* pSQ = &pPoller->tSQ;
	unsigned iTail = *pSQ->pTail;
	unsigned iMask = *pSQ->pRingMask;
	
	pSQ->pArray[iTail & iMask] = iTail & iMask;
	__atomic_store_n(pSQ->pTail, iTail + 1, __ATOMIC_RELEASE);
}

XXAPI xnetpoller* xrtPollCreate(xnetconfig* pConfig, xpoll_fn pfnCallback, ptr pUserData)
{
	xnetpoller* pPoller = (xnetpoller*)xrtCalloc(1, sizeof(xnetpoller));
	if ( !pPoller ) return NULL;
	
	// 初始化 io_uring
	struct io_uring_params tParams;
	memset(&tParams, 0, sizeof(tParams));
	
	pPoller->iFd = __xrt_io_uring_setup(__XRT_URING_ENTRIES, &tParams);
	if ( pPoller->iFd < 0 ) {
		xrtFree(pPoller);
		return NULL;
	}
	
	// 映射 SQ ring
	size_t iSQSize = tParams.sq_off.array + tParams.sq_entries * sizeof(unsigned);
	void* pSQPtr = mmap(NULL, iSQSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, pPoller->iFd, IORING_OFF_SQ_RING);
	if ( pSQPtr == MAP_FAILED ) {
		close(pPoller->iFd);
		xrtFree(pPoller);
		return NULL;
	}
	
	pPoller->tSQ.pRingPtr = pSQPtr;
	pPoller->tSQ.iRingSize = iSQSize;
	pPoller->tSQ.pHead = (unsigned*)((char*)pSQPtr + tParams.sq_off.head);
	pPoller->tSQ.pTail = (unsigned*)((char*)pSQPtr + tParams.sq_off.tail);
	pPoller->tSQ.pRingMask = (unsigned*)((char*)pSQPtr + tParams.sq_off.ring_mask);
	pPoller->tSQ.pRingEntries = (unsigned*)((char*)pSQPtr + tParams.sq_off.ring_entries);
	pPoller->tSQ.pFlags = (unsigned*)((char*)pSQPtr + tParams.sq_off.flags);
	pPoller->tSQ.pArray = (unsigned*)((char*)pSQPtr + tParams.sq_off.array);
	
	// 映射 SQEs
	size_t iSQESize = tParams.sq_entries * sizeof(struct io_uring_sqe);
	pPoller->tSQ.pSQEs = (struct io_uring_sqe*)mmap(NULL, iSQESize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, pPoller->iFd, IORING_OFF_SQES);
	if ( pPoller->tSQ.pSQEs == MAP_FAILED ) {
		munmap(pSQPtr, iSQSize);
		close(pPoller->iFd);
		xrtFree(pPoller);
		return NULL;
	}
	
	// 映射 CQ ring
	size_t iCQSize = tParams.cq_off.cqes + tParams.cq_entries * sizeof(struct io_uring_cqe);
	void* pCQPtr = mmap(NULL, iCQSize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, pPoller->iFd, IORING_OFF_CQ_RING);
	if ( pCQPtr == MAP_FAILED ) {
		munmap(pPoller->tSQ.pSQEs, iSQESize);
		munmap(pSQPtr, iSQSize);
		close(pPoller->iFd);
		xrtFree(pPoller);
		return NULL;
	}
	
	pPoller->tCQ.pRingPtr = pCQPtr;
	pPoller->tCQ.iRingSize = iCQSize;
	pPoller->tCQ.pHead = (unsigned*)((char*)pCQPtr + tParams.cq_off.head);
	pPoller->tCQ.pTail = (unsigned*)((char*)pCQPtr + tParams.cq_off.tail);
	pPoller->tCQ.pRingMask = (unsigned*)((char*)pCQPtr + tParams.cq_off.ring_mask);
	pPoller->tCQ.pRingEntries = (unsigned*)((char*)pCQPtr + tParams.cq_off.ring_entries);
	pPoller->tCQ.pCQEs = (struct io_uring_cqe*)((char*)pCQPtr + tParams.cq_off.cqes);
	
	// 分配操作池
	pPoller->iOpCapacity = __XRT_POLL_MAX_OPS;
	pPoller->pOps = (__xrt_uring_op*)xrtCalloc(pPoller->iOpCapacity, sizeof(__xrt_uring_op));
	if ( !pPoller->pOps ) {
		munmap(pCQPtr, iCQSize);
		munmap(pPoller->tSQ.pSQEs, iSQESize);
		munmap(pSQPtr, iSQSize);
		close(pPoller->iFd);
		xrtFree(pPoller);
		return NULL;
	}
	
	// 创建 eventfd 用于唤醒
	pPoller->iWakeupFd = eventfd(0, EFD_NONBLOCK);
	
	pthread_mutex_init(&pPoller->tLock, NULL);
	pPoller->pfnCallback = pfnCallback;
	pPoller->pUserData = pUserData;
	pPoller->iRecvBufSize = (pConfig && pConfig->iRecvBufSize > 0) ? pConfig->iRecvBufSize : __XRT_POLL_RECV_SIZE;
	
	return pPoller;
}

XXAPI void xrtPollDestroy(xnetpoller* pPoller)
{
	if ( !pPoller ) return;
	
	if ( pPoller->pOps ) {
		xrtFree(pPoller->pOps);
	}
	
	if ( pPoller->iWakeupFd >= 0 ) {
		close(pPoller->iWakeupFd);
	}
	
	// 释放 mmap 映射
	if ( pPoller->tCQ.pRingPtr && pPoller->tCQ.pRingPtr != MAP_FAILED ) {
		munmap(pPoller->tCQ.pRingPtr, pPoller->tCQ.iRingSize);
	}
	if ( pPoller->tSQ.pSQEs && (void*)pPoller->tSQ.pSQEs != MAP_FAILED ) {
		// SQEs 的大小需要重新计算，这里用近似值
		munmap(pPoller->tSQ.pSQEs, __XRT_URING_ENTRIES * sizeof(struct io_uring_sqe));
	}
	if ( pPoller->tSQ.pRingPtr && pPoller->tSQ.pRingPtr != MAP_FAILED ) {
		munmap(pPoller->tSQ.pRingPtr, pPoller->tSQ.iRingSize);
	}
	
	if ( pPoller->iFd >= 0 ) {
		close(pPoller->iFd);
	}
	
	pthread_mutex_destroy(&pPoller->tLock);
	xrtFree(pPoller);
}

XXAPI xnet_result xrtPollAdd(xnetpoller* pPoller, xnetconn* pConn, int iEvents)
{
	// io_uring 不需要显式注册 socket，操作在 Post 时提交
	(void)pPoller; (void)pConn; (void)iEvents;
	return XRT_NET_OK;
}

XXAPI xnet_result xrtPollRemove(xnetpoller* pPoller, xnetconn* pConn)
{
	if ( !pPoller || !pConn ) return XRT_NET_ERROR;
	
	pthread_mutex_lock(&pPoller->tLock);
	for ( int i = 0; i < pPoller->iOpCount; i++ ) {
		if ( pPoller->pOps[i].pConn == pConn && pPoller->pOps[i].bInUse ) {
			// 提交 cancel 请求
			struct io_uring_sqe* pSQE = __xrt_uring_get_sqe(pPoller);
			if ( pSQE ) {
				pSQE->opcode = IORING_OP_ASYNC_CANCEL;
				pSQE->addr = (unsigned long long)(uintptr_t)&pPoller->pOps[i];
				__xrt_uring_submit_sqe(pPoller);
			}
			pPoller->pOps[i].bInUse = false;
		}
	}
	pthread_mutex_unlock(&pPoller->tLock);
	
	// 提交取消请求
	__xrt_io_uring_enter(pPoller->iFd, 1, 0, IORING_ENTER_GETEVENTS, NULL);
	
	return XRT_NET_OK;
}

XXAPI xnet_result xrtPollPostRecv(xnetpoller* pPoller, xnetconn* pConn)
{
	if ( !pPoller || !pConn ) return XRT_NET_ERROR;
	
	__xrt_uring_op* pOp = __xrt_uring_alloc_op(pPoller);
	if ( !pOp ) return XRT_NET_ERROR;
	
	pOp->pConn = pConn;
	pOp->iOpType = __XRT_POLL_OP_RECV;
	
	pthread_mutex_lock(&pPoller->tLock);
	struct io_uring_sqe* pSQE = __xrt_uring_get_sqe(pPoller);
	if ( !pSQE ) {
		pOp->bInUse = false;
		pthread_mutex_unlock(&pPoller->tLock);
		return XRT_NET_ERROR;
	}
	
	pSQE->opcode = IORING_OP_RECV;
	pSQE->fd = pConn->hSocket;
	pSQE->addr = (unsigned long long)(uintptr_t)pOp->aRecvBuf;
	pSQE->len = __XRT_POLL_RECV_SIZE;
	pSQE->user_data = (unsigned long long)(uintptr_t)pOp;
	
	__xrt_uring_submit_sqe(pPoller);
	pthread_mutex_unlock(&pPoller->tLock);
	
	int iRet = __xrt_io_uring_enter(pPoller->iFd, 1, 0, 0, NULL);
	if ( iRet < 0 ) {
		pOp->bInUse = false;
		return XRT_NET_ERROR;
	}
	
	return XRT_NET_OK;
}

XXAPI xnet_result xrtPollPostSend(xnetpoller* pPoller, xnetconn* pConn, const char* pData, size_t iLen)
{
	if ( !pPoller || !pConn || !pData || iLen == 0 ) return XRT_NET_ERROR;
	
	__xrt_uring_op* pOp = __xrt_uring_alloc_op(pPoller);
	if ( !pOp ) return XRT_NET_ERROR;
	
	// 复制数据解决生命周期问题
	size_t iCopyLen = (iLen > __XRT_POLL_RECV_SIZE) ? __XRT_POLL_RECV_SIZE : iLen;
	memcpy(pOp->aRecvBuf, pData, iCopyLen);
	
	pOp->pConn = pConn;
	pOp->iOpType = __XRT_POLL_OP_SEND;
	
	pthread_mutex_lock(&pPoller->tLock);
	struct io_uring_sqe* pSQE = __xrt_uring_get_sqe(pPoller);
	if ( !pSQE ) {
		pOp->bInUse = false;
		pthread_mutex_unlock(&pPoller->tLock);
		return XRT_NET_ERROR;
	}
	
	pSQE->opcode = IORING_OP_SEND;
	pSQE->fd = pConn->hSocket;
	pSQE->addr = (unsigned long long)(uintptr_t)pOp->aRecvBuf;
	pSQE->len = (unsigned)iCopyLen;
	pSQE->user_data = (unsigned long long)(uintptr_t)pOp;
	
	__xrt_uring_submit_sqe(pPoller);
	pthread_mutex_unlock(&pPoller->tLock);
	
	int iRet = __xrt_io_uring_enter(pPoller->iFd, 1, 0, 0, NULL);
	if ( iRet < 0 ) {
		pOp->bInUse = false;
		return XRT_NET_ERROR;
	}
	
	return XRT_NET_OK;
}

XXAPI xnet_result xrtPollPostAccept(xnetpoller* pPoller, xnetconn* pServer, xnetconn* pClient)
{
	if ( !pPoller || !pServer || !pClient ) return XRT_NET_ERROR;
	
	__xrt_uring_op* pOp = __xrt_uring_alloc_op(pPoller);
	if ( !pOp ) return XRT_NET_ERROR;
	
	pOp->pConn = pServer;
	pOp->pAcceptConn = pClient;
	pOp->iOpType = __XRT_POLL_OP_ACCEPT;
	pOp->iAcceptAddrLen = sizeof(struct sockaddr_in);
	
	pthread_mutex_lock(&pPoller->tLock);
	struct io_uring_sqe* pSQE = __xrt_uring_get_sqe(pPoller);
	if ( !pSQE ) {
		pOp->bInUse = false;
		pthread_mutex_unlock(&pPoller->tLock);
		return XRT_NET_ERROR;
	}
	
	pSQE->opcode = IORING_OP_ACCEPT;
	pSQE->fd = pServer->hSocket;
	pSQE->addr = (unsigned long long)(uintptr_t)&pOp->tAcceptAddr;
	pSQE->addr2 = (unsigned long long)(uintptr_t)&pOp->iAcceptAddrLen;
	pSQE->user_data = (unsigned long long)(uintptr_t)pOp;
	
	__xrt_uring_submit_sqe(pPoller);
	pthread_mutex_unlock(&pPoller->tLock);
	
	int iRet = __xrt_io_uring_enter(pPoller->iFd, 1, 0, 0, NULL);
	if ( iRet < 0 ) {
		pOp->bInUse = false;
		return XRT_NET_ERROR;
	}
	
	return XRT_NET_OK;
}

XXAPI xnet_result xrtPollWait(xnetpoller* pPoller, int iTimeoutMs)
{
	if ( !pPoller ) return XRT_NET_ERROR;
	
	// 等待至少一个完成事件
	struct __kernel_timespec tTimeout;
	struct __kernel_timespec* pTimeout = NULL;
	if ( iTimeoutMs >= 0 ) {
		tTimeout.tv_sec = iTimeoutMs / 1000;
		tTimeout.tv_nsec = (long)(iTimeoutMs % 1000) * 1000000L;
		pTimeout = &tTimeout;
	}
	
	int iRet = __xrt_io_uring_enter(pPoller->iFd, 0, 1, IORING_ENTER_GETEVENTS, pTimeout);
	if ( iRet < 0 ) {
		if ( errno == ETIME || errno == EINTR ) {
			return XRT_NET_TIMEOUT;
		}
		return XRT_NET_ERROR;
	}
	
	// 处理 CQ 事件
	__xrt_uring_cq* pCQ = &pPoller->tCQ;
	unsigned iHead = __atomic_load_n(pCQ->pHead, __ATOMIC_ACQUIRE);
	unsigned iTail = __atomic_load_n(pCQ->pTail, __ATOMIC_ACQUIRE);
	
	if ( iHead == iTail ) {
		return XRT_NET_TIMEOUT;
	}
	
	unsigned iMask = *pCQ->pRingMask;
	
	while ( iHead != iTail ) {
		struct io_uring_cqe* pCQE = &pCQ->pCQEs[iHead & iMask];
		__xrt_uring_op* pOp = (__xrt_uring_op*)(uintptr_t)pCQE->user_data;
		
		if ( pOp && pOp->bInUse ) {
			xnetconn* pConn = pOp->pConn;
			
			if ( pOp->iOpType == __XRT_POLL_OP_ACCEPT ) {
				if ( pCQE->res >= 0 && pOp->pAcceptConn ) {
					pOp->pAcceptConn->hSocket = pCQE->res;
					xrtNetAddrFromSockAddr(&pOp->pAcceptConn->tRemoteAddr, &pOp->tAcceptAddr);
					pOp->pAcceptConn->iType = 0;  // TCP
					if ( pPoller->pfnCallback ) {
						pPoller->pfnCallback(pPoller, pOp->pAcceptConn, XRT_POLL_ACCEPT, NULL, 0);
					}
				}
			} else if ( pCQE->res <= 0 ) {
				// 连接关闭或错误
				if ( pCQE->res == 0 || pCQE->res == -ECONNRESET ) {
					if ( pPoller->pfnCallback ) {
						pPoller->pfnCallback(pPoller, pConn, XRT_POLL_CLOSE, NULL, 0);
					}
				} else {
					if ( pPoller->pfnCallback ) {
						pPoller->pfnCallback(pPoller, pConn, XRT_POLL_ERROR, NULL, 0);
					}
				}
			} else {
				switch ( pOp->iOpType ) {
					case __XRT_POLL_OP_RECV:
						if ( pPoller->pfnCallback ) {
							pPoller->pfnCallback(pPoller, pConn, XRT_POLL_READ, pOp->aRecvBuf, (size_t)pCQE->res);
						}
						break;
					
					case __XRT_POLL_OP_SEND:
						if ( pPoller->pfnCallback ) {
							pPoller->pfnCallback(pPoller, pConn, XRT_POLL_WRITE, NULL, (size_t)pCQE->res);
						}
						break;
				}
			}
			
			pOp->bInUse = false;
		}
		
		iHead++;
	}
	
	__atomic_store_n(pCQ->pHead, iHead, __ATOMIC_RELEASE);
	
	return XRT_NET_OK;
}

XXAPI void xrtPollWakeup(xnetpoller* pPoller)
{
	if ( !pPoller || pPoller->iWakeupFd < 0 ) return;
	uint64 iVal = 1;
	write(pPoller->iWakeupFd, &iVal, sizeof(iVal));
}

XXAPI ptr xrtPollGetUserData(xnetpoller* pPoller)
{
	if ( !pPoller ) return NULL;
	return pPoller->pUserData;
}

#endif /* Linux */



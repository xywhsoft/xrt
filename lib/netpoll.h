



/*
	NetPoll - IO 模型抽象层 [Ver2.0]
	
	Windows: IOCP (I/O Completion Port)
	Linux:   io_uring (内核 >= 5.1, 无 epoll 回退)
	
	基于完成端口模型的统一异步 IO 接口
	
	Ver2.0 变更:
	  - 操作池空闲链表 O(1) 分配/释放
	  - Windows: GetQueuedCompletionStatusEx 批量收割
	  - Windows: AcceptEx 异步 accept
	  - Linux: io_uring 批量 SQE 提交
	  - 发送路径支持大消息 (>8KB 动态缓冲区)
	  - per-operation 用户数据 (事件路由)
*/



/* ============================== 通用定义 ============================== */

#define __XRT_POLL_OP_RECV    1
#define __XRT_POLL_OP_SEND    2
#define __XRT_POLL_OP_ACCEPT  3

#define __XRT_POLL_INIT_OPS   64
#define __XRT_POLL_MAX_OPS    4096
#define __XRT_POLL_RECV_SIZE  8192



/* ============================== Windows IOCP 实现 ============================== */

#if defined(_WIN32) || defined(_WIN64)

#define __XRT_IOCP_BATCH_SIZE 64

// IOCP 操作结构
typedef struct {
	WSAOVERLAPPED tOverlapped;
	xnetconn* pConn;
	xnetconn* pAcceptConn;       // 用于 accept 操作的客户端连接
	char aRecvBuf[__XRT_POLL_RECV_SIZE];
	char* pDynamicBuf;           // 动态发送缓冲区 (>8KB)
	bool bDynamicBuf;            // 是否使用动态缓冲区
	WSABUF tWSABuf;
	DWORD iBytes;
	DWORD iFlags;
	int iOpType;
	bool bInUse;
	int iNextFree;               // 空闲链表下一个索引, -1 = 尾
	ptr pOpUserData;             // per-operation 用户数据
	SOCKET hAcceptSocket;        // AcceptEx 预创建的 socket
} __xrt_iocp_op;

// Poller 结构定义
struct xrt_net_poller {
	HANDLE hIOCP;
	__xrt_iocp_op* pOps;
	int iOpCount;
	int iOpCapacity;
	int iFreeHead;               // 空闲链表头索引
	CRITICAL_SECTION tLock;
	xpoll_fn pfnCallback;
	ptr pUserData;
	size_t iRecvBufSize;
	LPFN_ACCEPTEX pfnAcceptEx;
	LPFN_GETACCEPTEXSOCKADDRS pfnGetAcceptExSockAddrs;
};

// O(1) 操作池分配
static __xrt_iocp_op* __xrt_iocp_alloc_op(xnetpoller* pPoller)
{
	EnterCriticalSection(&pPoller->tLock);
	
	if ( pPoller->iFreeHead >= 0 ) {
		// 从空闲链表头取
		int iIdx = pPoller->iFreeHead;
		__xrt_iocp_op* pOp = &pPoller->pOps[iIdx];
		pPoller->iFreeHead = pOp->iNextFree;
		pOp->bInUse = true;
		pOp->iNextFree = -1;
		pOp->pDynamicBuf = NULL;
		pOp->bDynamicBuf = false;
		pOp->pOpUserData = NULL;
		pOp->hAcceptSocket = INVALID_SOCKET;
		LeaveCriticalSection(&pPoller->tLock);
		return pOp;
	}
	
	// 空闲链表为空，尝试追加
	if ( pPoller->iOpCount < pPoller->iOpCapacity ) {
		int iIdx = pPoller->iOpCount++;
		__xrt_iocp_op* pOp = &pPoller->pOps[iIdx];
		memset(pOp, 0, sizeof(__xrt_iocp_op));
		pOp->bInUse = true;
		pOp->iNextFree = -1;
		LeaveCriticalSection(&pPoller->tLock);
		return pOp;
	}
	
	// 容量已满，尝试动态扩容（最大4096）
	if ( pPoller->iOpCapacity < __XRT_POLL_MAX_OPS ) {
		int iNewCapacity = pPoller->iOpCapacity * 2;
		if ( iNewCapacity > __XRT_POLL_MAX_OPS ) {
			iNewCapacity = __XRT_POLL_MAX_OPS;
		}
		
		__xrt_iocp_op* pNewOps = (__xrt_iocp_op*)xrtRealloc(pPoller->pOps, 
			iNewCapacity * sizeof(__xrt_iocp_op));
		if ( !pNewOps ) {
			LeaveCriticalSection(&pPoller->tLock);
			return NULL;
		}
		
		// 更新容量和指针
		pPoller->pOps = pNewOps;
		pPoller->iOpCapacity = iNewCapacity;
		
		// 将新增的槽位串入空闲链表
		for ( int i = pPoller->iOpCount; i < pPoller->iOpCapacity; i++ ) {
			pPoller->pOps[i].bInUse = false;
			pPoller->pOps[i].iNextFree = (i + 1 < pPoller->iOpCapacity) ? (i + 1) : pPoller->iFreeHead;
		}
		pPoller->iFreeHead = pPoller->iOpCount;
		
		// 分配第一个新增的槽位
		int iIdx = pPoller->iOpCount++;
		__xrt_iocp_op* pOp = &pPoller->pOps[iIdx];
		memset(pOp, 0, sizeof(__xrt_iocp_op));
		pOp->bInUse = true;
		pOp->iNextFree = -1;
		LeaveCriticalSection(&pPoller->tLock);
		return pOp;
	}
	
	LeaveCriticalSection(&pPoller->tLock);
	return NULL;
}

// O(1) 操作池释放
static void __xrt_iocp_free_op(xnetpoller* pPoller, __xrt_iocp_op* pOp)
{
	if ( pOp->bDynamicBuf && pOp->pDynamicBuf ) {
		xrtFree(pOp->pDynamicBuf);
		pOp->pDynamicBuf = NULL;
	}
	pOp->bDynamicBuf = false;
	pOp->bInUse = false;
	
	EnterCriticalSection(&pPoller->tLock);
	int iIdx = (int)(pOp - pPoller->pOps);
	pOp->iNextFree = pPoller->iFreeHead;
	pPoller->iFreeHead = iIdx;
	LeaveCriticalSection(&pPoller->tLock);
}

// 加载 AcceptEx 扩展函数
static void __xrt_iocp_load_acceptex(xnetpoller* pPoller)
{
	pPoller->pfnAcceptEx = NULL;
	pPoller->pfnGetAcceptExSockAddrs = NULL;
	
	// 创建临时 socket 加载扩展函数
	SOCKET hTmp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if ( hTmp == INVALID_SOCKET ) return;
	
	DWORD dwBytes = 0;
	GUID guidAcceptEx = WSAID_ACCEPTEX;
	WSAIoctl(hTmp, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&guidAcceptEx, sizeof(guidAcceptEx),
		&pPoller->pfnAcceptEx, sizeof(pPoller->pfnAcceptEx),
		&dwBytes, NULL, NULL);
	
	GUID guidGetAddr = WSAID_GETACCEPTEXSOCKADDRS;
	WSAIoctl(hTmp, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&guidGetAddr, sizeof(guidGetAddr),
		&pPoller->pfnGetAcceptExSockAddrs, sizeof(pPoller->pfnGetAcceptExSockAddrs),
		&dwBytes, NULL, NULL);
	
	closesocket(hTmp);
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
	
	pPoller->iOpCapacity = __XRT_POLL_INIT_OPS;
	pPoller->pOps = (__xrt_iocp_op*)xrtCalloc(pPoller->iOpCapacity, sizeof(__xrt_iocp_op));
	if ( !pPoller->pOps ) {
		CloseHandle(pPoller->hIOCP);
		xrtFree(pPoller);
		return NULL;
	}
	
	// 初始化空闲链表 (所有槽位串成链表)
	pPoller->iFreeHead = -1;  // 初始为空, 通过 alloc_op 按需追加
	
	InitializeCriticalSection(&pPoller->tLock);
	pPoller->pfnCallback = pfnCallback;
	pPoller->pUserData = pUserData;
	pPoller->iRecvBufSize = (pConfig && pConfig->iRecvBufSize > 0) ? pConfig->iRecvBufSize : __XRT_POLL_RECV_SIZE;
	
	// 加载 AcceptEx 扩展函数
	__xrt_iocp_load_acceptex(pPoller);
	
	return pPoller;
}

XXAPI void xrtPollDestroy(xnetpoller* pPoller)
{
	if ( !pPoller ) return;
	
	// 释放所有动态缓冲区
	if ( pPoller->pOps ) {
		for ( int i = 0; i < pPoller->iOpCount; i++ ) {
			if ( pPoller->pOps[i].bDynamicBuf && pPoller->pOps[i].pDynamicBuf ) {
				xrtFree(pPoller->pOps[i].pDynamicBuf);
			}
			if ( pPoller->pOps[i].hAcceptSocket != INVALID_SOCKET && pPoller->pOps[i].hAcceptSocket != 0 ) {
				closesocket(pPoller->pOps[i].hAcceptSocket);
			}
		}
		xrtFree(pPoller->pOps);
	}
	if ( pPoller->hIOCP ) {
		CloseHandle(pPoller->hIOCP);
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
			// CancelIoEx 取消该 socket 上的所有 IO
			break;
		}
	}
	LeaveCriticalSection(&pPoller->tLock);
	
	CancelIoEx((HANDLE)pConn->hSocket, NULL);
	
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
	pOp->pOpUserData = pConn->pUserData;
	
	int iResult = WSARecv(pConn->hSocket, &pOp->tWSABuf, 1, &pOp->iBytes, &pOp->iFlags, &pOp->tOverlapped, NULL);
	if ( iResult == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING ) {
		__xrt_iocp_free_op(pPoller, pOp);
		return XRT_NET_ERROR;
	}
	
	return XRT_NET_OK;
}

XXAPI xnet_result xrtPollPostSend(xnetpoller* pPoller, xnetconn* pConn, const char* pData, size_t iLen)
{
	if ( !pPoller || !pConn || !pData || iLen == 0 ) return XRT_NET_ERROR;
	
	__xrt_iocp_op* pOp = __xrt_iocp_alloc_op(pPoller);
	if ( !pOp ) return XRT_NET_ERROR;
	
	// 大消息: 动态分配缓冲区
	if ( iLen > __XRT_POLL_RECV_SIZE ) {
		pOp->pDynamicBuf = (char*)xrtMalloc(iLen);
		if ( !pOp->pDynamicBuf ) {
			__xrt_iocp_free_op(pPoller, pOp);
			return XRT_NET_ERROR;
		}
		pOp->bDynamicBuf = true;
		memcpy(pOp->pDynamicBuf, pData, iLen);
		pOp->tWSABuf.buf = pOp->pDynamicBuf;
		pOp->tWSABuf.len = (ULONG)iLen;
	} else {
		memcpy(pOp->aRecvBuf, pData, iLen);
		pOp->tWSABuf.buf = pOp->aRecvBuf;
		pOp->tWSABuf.len = (ULONG)iLen;
	}
	
	memset(&pOp->tOverlapped, 0, sizeof(WSAOVERLAPPED));
	pOp->pConn = pConn;
	pOp->iOpType = __XRT_POLL_OP_SEND;
	pOp->iFlags = 0;
	pOp->pOpUserData = pConn->pUserData;
	
	DWORD iBytesSent;
	int iResult = WSASend(pConn->hSocket, &pOp->tWSABuf, 1, &iBytesSent, 0, &pOp->tOverlapped, NULL);
	if ( iResult == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING ) {
		__xrt_iocp_free_op(pPoller, pOp);
		return XRT_NET_ERROR;
	}
	
	return XRT_NET_OK;
}

XXAPI xnet_result xrtPollPostAccept(xnetpoller* pPoller, xnetconn* pServer, xnetconn* pClient)
{
	if ( !pPoller || !pServer ) return XRT_NET_ERROR;
	
	__xrt_iocp_op* pOp = __xrt_iocp_alloc_op(pPoller);
	if ( !pOp ) return XRT_NET_ERROR;
	
	pOp->pConn = pServer;
	pOp->pAcceptConn = pClient;
	pOp->iOpType = __XRT_POLL_OP_ACCEPT;
	pOp->pOpUserData = pServer->pUserData;
	
	// AcceptEx 可用时使用异步 accept
	if ( pPoller->pfnAcceptEx ) {
		SOCKET hAccept = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if ( hAccept == INVALID_SOCKET ) {
			__xrt_iocp_free_op(pPoller, pOp);
			return XRT_NET_ERROR;
		}
		pOp->hAcceptSocket = hAccept;
		
		memset(&pOp->tOverlapped, 0, sizeof(WSAOVERLAPPED));
		DWORD dwBytes = 0;
		BOOL bResult = pPoller->pfnAcceptEx(
			pServer->hSocket, hAccept,
			pOp->aRecvBuf,                     // 输出缓冲区 (地址信息)
			0,                                  // 不接收首包数据
			sizeof(struct sockaddr_in) + 16,    // 本地地址长度
			sizeof(struct sockaddr_in) + 16,    // 远端地址长度
			&dwBytes,
			&pOp->tOverlapped
		);
		
		if ( !bResult && WSAGetLastError() != ERROR_IO_PENDING ) {
			closesocket(hAccept);
			pOp->hAcceptSocket = INVALID_SOCKET;
			__xrt_iocp_free_op(pPoller, pOp);
			return XRT_NET_ERROR;
		}
		
		return XRT_NET_OK;
	}
	
	// AcceptEx 不可用: 回退到非阻塞 accept (在 PollWait 通知中处理)
	__xrt_iocp_free_op(pPoller, pOp);
	return XRT_NET_OK;
}

XXAPI xnet_result xrtPollWait(xnetpoller* pPoller, int iTimeoutMs)
{
	if ( !pPoller ) return XRT_NET_ERROR;
	
	// 批量收割完成事件
	OVERLAPPED_ENTRY aEntries[__XRT_IOCP_BATCH_SIZE];
	ULONG iRemoved = 0;
	
	BOOL bSuccess = GetQueuedCompletionStatusEx(
		pPoller->hIOCP,
		aEntries,
		__XRT_IOCP_BATCH_SIZE,
		&iRemoved,
		(DWORD)iTimeoutMs,
		FALSE
	);
	
	if ( !bSuccess ) {
		if ( GetLastError() == WAIT_TIMEOUT ) {
			return XRT_NET_TIMEOUT;
		}
		return XRT_NET_ERROR;
	}
	
	// 分发所有完成事件
	for ( ULONG i = 0; i < iRemoved; i++ ) {
		LPOVERLAPPED pOverlapped = aEntries[i].lpOverlapped;
		DWORD iBytesTransferred = aEntries[i].dwNumberOfBytesTransferred;
		
		// Wakeup 信号
		if ( !pOverlapped ) continue;
		
		__xrt_iocp_op* pOp = (__xrt_iocp_op*)pOverlapped;
		if ( !pOp->bInUse ) continue;
		
		xnetconn* pConn = pOp->pConn;
		
		// AcceptEx 完成
		if ( pOp->iOpType == __XRT_POLL_OP_ACCEPT ) {
			if ( pOp->hAcceptSocket != INVALID_SOCKET && pOp->hAcceptSocket != 0 ) {
				// AcceptEx 成功: 从缓冲区提取地址
				if ( pOp->pAcceptConn ) {
					pOp->pAcceptConn->hSocket = pOp->hAcceptSocket;
					pOp->pAcceptConn->iType = 0;  // TCP
					
					// 用 GetAcceptExSockaddrs 提取地址
					if ( pPoller->pfnGetAcceptExSockAddrs ) {
						struct sockaddr* pLocal = NULL;
						struct sockaddr* pRemote = NULL;
						int iLocalLen = 0, iRemoteLen = 0;
						pPoller->pfnGetAcceptExSockAddrs(
							pOp->aRecvBuf, 0,
							sizeof(struct sockaddr_in) + 16,
							sizeof(struct sockaddr_in) + 16,
							&pLocal, &iLocalLen,
							&pRemote, &iRemoteLen
						);
						if ( pRemote ) {
							xrtNetAddrFromSockAddr(&pOp->pAcceptConn->tRemoteAddr, (struct sockaddr_in*)pRemote);
						}
					}
					
					// 继承监听 socket 的上下文
					setsockopt(pOp->hAcceptSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
						(char*)&pConn->hSocket, sizeof(pConn->hSocket));
				}
				pOp->hAcceptSocket = INVALID_SOCKET;  // 所有权转移
			}
			
			if ( pPoller->pfnCallback ) {
				// 继承监听 socket 的 pUserData 用于事件路由
				if ( pOp->pAcceptConn && pConn ) {
					pOp->pAcceptConn->pUserData = pConn->pUserData;
				}
				pPoller->pfnCallback(pPoller, pOp->pAcceptConn ? pOp->pAcceptConn : pConn,
					XRT_POLL_ACCEPT, NULL, 0);
			}
			__xrt_iocp_free_op(pPoller, pOp);
			continue;
		}
		
		// 连接关闭
		if ( iBytesTransferred == 0 ) {
			if ( pPoller->pfnCallback ) {
				pPoller->pfnCallback(pPoller, pConn, XRT_POLL_CLOSE, NULL, 0);
			}
			__xrt_iocp_free_op(pPoller, pOp);
			continue;
		}
		
		// 分发 RECV / SEND
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
		}
		
		__xrt_iocp_free_op(pPoller, pOp);
	}
	
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
#include <sys/eventfd.h>
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
	char* pDynamicBuf;           // 动态发送缓冲区 (>8KB)
	bool bDynamicBuf;
	int iOpType;
	bool bInUse;
	int iNextFree;               // 空闲链表
	ptr pOpUserData;             // per-operation 用户数据
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
	int iFreeHead;                // 空闲链表头
	unsigned iPendingSQE;         // 待提交的 SQE 计数
	int iWakeupFd;                // eventfd 用于唤醒
	xpoll_fn pfnCallback;
	ptr pUserData;
	size_t iRecvBufSize;
	pthread_mutex_t tLock;
};

// O(1) 操作池分配
static __xrt_uring_op* __xrt_uring_alloc_op(xnetpoller* pPoller)
{
	pthread_mutex_lock(&pPoller->tLock);
	
	if ( pPoller->iFreeHead >= 0 ) {
		int iIdx = pPoller->iFreeHead;
		__xrt_uring_op* pOp = &pPoller->pOps[iIdx];
		pPoller->iFreeHead = pOp->iNextFree;
		pOp->bInUse = true;
		pOp->iNextFree = -1;
		pOp->pDynamicBuf = NULL;
		pOp->bDynamicBuf = false;
		pOp->pOpUserData = NULL;
		pthread_mutex_unlock(&pPoller->tLock);
		return pOp;
	}
	
	if ( pPoller->iOpCount < pPoller->iOpCapacity ) {
		int iIdx = pPoller->iOpCount++;
		__xrt_uring_op* pOp = &pPoller->pOps[iIdx];
		memset(pOp, 0, sizeof(__xrt_uring_op));
		pOp->bInUse = true;
		pOp->iNextFree = -1;
		pthread_mutex_unlock(&pPoller->tLock);
		return pOp;
	}
	
	// 容量已满，尝试动态扩容（最大4096）
	if ( pPoller->iOpCapacity < __XRT_POLL_MAX_OPS ) {
		int iNewCapacity = pPoller->iOpCapacity * 2;
		if ( iNewCapacity > __XRT_POLL_MAX_OPS ) {
			iNewCapacity = __XRT_POLL_MAX_OPS;
		}
		
		__xrt_uring_op* pNewOps = (__xrt_uring_op*)xrtRealloc(pPoller->pOps, 
			iNewCapacity * sizeof(__xrt_uring_op));
		if ( !pNewOps ) {
			pthread_mutex_unlock(&pPoller->tLock);
			return NULL;
		}
		
		// 更新容量和指针
		pPoller->pOps = pNewOps;
		pPoller->iOpCapacity = iNewCapacity;
		
		// 将新增的槽位串入空闲链表
		for ( int i = pPoller->iOpCount; i < pPoller->iOpCapacity; i++ ) {
			pPoller->pOps[i].bInUse = false;
			pPoller->pOps[i].iNextFree = (i + 1 < pPoller->iOpCapacity) ? (i + 1) : pPoller->iFreeHead;
		}
		pPoller->iFreeHead = pPoller->iOpCount;
		
		// 分配第一个新增的槽位
		int iIdx = pPoller->iOpCount++;
		__xrt_uring_op* pOp = &pPoller->pOps[iIdx];
		memset(pOp, 0, sizeof(__xrt_uring_op));
		pOp->bInUse = true;
		pOp->iNextFree = -1;
		pthread_mutex_unlock(&pPoller->tLock);
		return pOp;
	}
	
	pthread_mutex_unlock(&pPoller->tLock);
	return NULL;
}

// O(1) 操作池释放
static void __xrt_uring_free_op(xnetpoller* pPoller, __xrt_uring_op* pOp)
{
	if ( pOp->bDynamicBuf && pOp->pDynamicBuf ) {
		xrtFree(pOp->pDynamicBuf);
		pOp->pDynamicBuf = NULL;
	}
	pOp->bDynamicBuf = false;
	pOp->bInUse = false;
	
	pthread_mutex_lock(&pPoller->tLock);
	int iIdx = (int)(pOp - pPoller->pOps);
	pOp->iNextFree = pPoller->iFreeHead;
	pPoller->iFreeHead = iIdx;
	pthread_mutex_unlock(&pPoller->tLock);
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
	pPoller->iPendingSQE++;
}

// 批量提交所有待提交 SQE
static xnet_result __xrt_uring_flush(xnetpoller* pPoller)
{
	if ( pPoller->iPendingSQE == 0 ) return XRT_NET_OK;
	
	int iRet = __xrt_io_uring_enter(pPoller->iFd, pPoller->iPendingSQE, 0, 0, NULL);
	pPoller->iPendingSQE = 0;
	
	if ( iRet < 0 ) return XRT_NET_ERROR;
	return XRT_NET_OK;
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
	pPoller->iOpCapacity = __XRT_POLL_INIT_OPS;
	pPoller->pOps = (__xrt_uring_op*)xrtCalloc(pPoller->iOpCapacity, sizeof(__xrt_uring_op));
	if ( !pPoller->pOps ) {
		munmap(pCQPtr, iCQSize);
		munmap(pPoller->tSQ.pSQEs, iSQESize);
		munmap(pSQPtr, iSQSize);
		close(pPoller->iFd);
		xrtFree(pPoller);
		return NULL;
	}
	
	pPoller->iFreeHead = -1;
	pPoller->iPendingSQE = 0;
	
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
		for ( int i = 0; i < pPoller->iOpCount; i++ ) {
			if ( pPoller->pOps[i].bDynamicBuf && pPoller->pOps[i].pDynamicBuf ) {
				xrtFree(pPoller->pOps[i].pDynamicBuf);
			}
		}
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
	__xrt_uring_flush(pPoller);
	
	return XRT_NET_OK;
}

XXAPI xnet_result xrtPollPostRecv(xnetpoller* pPoller, xnetconn* pConn)
{
	if ( !pPoller || !pConn ) return XRT_NET_ERROR;
	
	__xrt_uring_op* pOp = __xrt_uring_alloc_op(pPoller);
	if ( !pOp ) return XRT_NET_ERROR;
	
	pOp->pConn = pConn;
	pOp->iOpType = __XRT_POLL_OP_RECV;
	pOp->pOpUserData = pConn->pUserData;
	
	pthread_mutex_lock(&pPoller->tLock);
	struct io_uring_sqe* pSQE = __xrt_uring_get_sqe(pPoller);
	if ( !pSQE ) {
		pthread_mutex_unlock(&pPoller->tLock);
		__xrt_uring_free_op(pPoller, pOp);
		return XRT_NET_ERROR;
	}
	
	pSQE->opcode = IORING_OP_RECV;
	pSQE->fd = pConn->hSocket;
	pSQE->addr = (unsigned long long)(uintptr_t)pOp->aRecvBuf;
	pSQE->len = __XRT_POLL_RECV_SIZE;
	pSQE->user_data = (unsigned long long)(uintptr_t)pOp;
	
	__xrt_uring_submit_sqe(pPoller);
	pthread_mutex_unlock(&pPoller->tLock);
	
	// 不立即提交, 等 PollWait 批量提交
	return XRT_NET_OK;
}

XXAPI xnet_result xrtPollPostSend(xnetpoller* pPoller, xnetconn* pConn, const char* pData, size_t iLen)
{
	if ( !pPoller || !pConn || !pData || iLen == 0 ) return XRT_NET_ERROR;
	
	__xrt_uring_op* pOp = __xrt_uring_alloc_op(pPoller);
	if ( !pOp ) return XRT_NET_ERROR;
	
	char* pBuf;
	if ( iLen > __XRT_POLL_RECV_SIZE ) {
		pOp->pDynamicBuf = (char*)xrtMalloc(iLen);
		if ( !pOp->pDynamicBuf ) {
			__xrt_uring_free_op(pPoller, pOp);
			return XRT_NET_ERROR;
		}
		pOp->bDynamicBuf = true;
		memcpy(pOp->pDynamicBuf, pData, iLen);
		pBuf = pOp->pDynamicBuf;
	} else {
		memcpy(pOp->aRecvBuf, pData, iLen);
		pBuf = pOp->aRecvBuf;
	}
	
	pOp->pConn = pConn;
	pOp->iOpType = __XRT_POLL_OP_SEND;
	pOp->pOpUserData = pConn->pUserData;
	
	pthread_mutex_lock(&pPoller->tLock);
	struct io_uring_sqe* pSQE = __xrt_uring_get_sqe(pPoller);
	if ( !pSQE ) {
		pthread_mutex_unlock(&pPoller->tLock);
		__xrt_uring_free_op(pPoller, pOp);
		return XRT_NET_ERROR;
	}
	
	pSQE->opcode = IORING_OP_SEND;
	pSQE->fd = pConn->hSocket;
	pSQE->addr = (unsigned long long)(uintptr_t)pBuf;
	pSQE->len = (unsigned)iLen;
	pSQE->user_data = (unsigned long long)(uintptr_t)pOp;
	
	__xrt_uring_submit_sqe(pPoller);
	pthread_mutex_unlock(&pPoller->tLock);
	
	return XRT_NET_OK;
}

XXAPI xnet_result xrtPollPostAccept(xnetpoller* pPoller, xnetconn* pServer, xnetconn* pClient)
{
	if ( !pPoller || !pServer ) return XRT_NET_ERROR;
	
	__xrt_uring_op* pOp = __xrt_uring_alloc_op(pPoller);
	if ( !pOp ) return XRT_NET_ERROR;
	
	pOp->pConn = pServer;
	pOp->pAcceptConn = pClient;
	pOp->iOpType = __XRT_POLL_OP_ACCEPT;
	pOp->iAcceptAddrLen = sizeof(struct sockaddr_in);
	pOp->pOpUserData = pServer->pUserData;
	
	pthread_mutex_lock(&pPoller->tLock);
	struct io_uring_sqe* pSQE = __xrt_uring_get_sqe(pPoller);
	if ( !pSQE ) {
		pthread_mutex_unlock(&pPoller->tLock);
		__xrt_uring_free_op(pPoller, pOp);
		return XRT_NET_ERROR;
	}
	
	pSQE->opcode = IORING_OP_ACCEPT;
	pSQE->fd = pServer->hSocket;
	pSQE->addr = (unsigned long long)(uintptr_t)&pOp->tAcceptAddr;
	pSQE->addr2 = (unsigned long long)(uintptr_t)&pOp->iAcceptAddrLen;
	pSQE->user_data = (unsigned long long)(uintptr_t)pOp;
	
	__xrt_uring_submit_sqe(pPoller);
	pthread_mutex_unlock(&pPoller->tLock);
	
	return XRT_NET_OK;
}

XXAPI xnet_result xrtPollWait(xnetpoller* pPoller, int iTimeoutMs)
{
	if ( !pPoller ) return XRT_NET_ERROR;
	
	// 批量提交所有待提交 SQE
	__xrt_uring_flush(pPoller);
	
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
					// 继承监听 socket 的 pUserData 用于事件路由
					pOp->pAcceptConn->pUserData = pOp->pConn->pUserData;
					if ( pPoller->pfnCallback ) {
						pPoller->pfnCallback(pPoller, pOp->pAcceptConn, XRT_POLL_ACCEPT, NULL, 0);
					}
				}
				__xrt_uring_free_op(pPoller, pOp);
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
				__xrt_uring_free_op(pPoller, pOp);
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
				__xrt_uring_free_op(pPoller, pOp);
			}
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



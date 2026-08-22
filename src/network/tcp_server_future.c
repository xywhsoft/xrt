#include "../internal/xrt_tcp_server.h"



#if defined(XRT_FEATURE_NET_TCP_SERVER_FUTURE)

/* 一个等待节点持有 Server 和 Promise，直到唯一终态完成。 */
struct __xrt_net_server_wait {
	__xrt_net_server_wait* Next;
	xnetserver* Server;
	xpromise* Promise;
	xcancelwatch* Watch;
	bool Linked;
};



/* 释放 Future 持有的已接受 Stream 引用。 */
static void __xrtNetServerStreamFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtNetStreamDestroy((xnetstream*)pValue);
}



/* 从 Server 等待链中移除指定节点；调用方持有 AcceptLock。 */
static bool __xrtNetServerWaitRemove(
	xnetserver* pServer,
	__xrt_net_server_wait* pWaiter
)
{
	__xrt_net_server_wait** ppCurrent = &pServer->WaitHead;
	__xrt_net_server_wait* pPrevious = NULL;

	while ( (*ppCurrent != NULL) && (*ppCurrent != pWaiter) ) {
		pPrevious = *ppCurrent;
		ppCurrent = &(*ppCurrent)->Next;
	}
	if ( *ppCurrent == NULL ) {
		return false;
	}
	*ppCurrent = pWaiter->Next;
	if ( pServer->WaitTail == pWaiter ) {
		pServer->WaitTail = pPrevious;
	}
	pWaiter->Next = NULL;
	pWaiter->Linked = false;
	(void)xrtAtomic32FetchSub(
		&pServer->AcceptWaiters,
		1,
		XMEMORY_ACQ_REL
	);
	return true;
}



/* 完成一个 Server 等待节点并释放全部持有资源。 */
static void __xrtNetServerWaitFinish(
	__xrt_net_server_wait* pWaiter,
	xnetstream* pStream,
	bool bCancelled
)
{
	xnetserver* pServer = pWaiter->Server;
	xpromise* pPromise = pWaiter->Promise;

	xrtCancelUnwatch(pWaiter->Watch);
	pWaiter->Watch = NULL;
	pWaiter->Promise = NULL;
	xrtFree(pWaiter);
	xrtNetServerDestroy(pServer);
	if ( pStream != NULL ) {
		if ( !xrtPromiseResolveOwned(
			pPromise,
			pStream,
			__xrtNetServerStreamFree,
			NULL
		) ) {
			xrtNetStreamDestroy(pStream);
		}
	} else if ( bCancelled ) {
		(void)xrtPromiseCancel(pPromise);
	} else {
		(void)xrtPromiseClose(pPromise);
	}
	xrtPromiseDestroy(pPromise);
}



/* 在 AcceptLock 内把连接线性化交给最早的异步消费者。 */
bool __xrtNetServerFutureAccept(
	xnetserver* pServer,
	xnetstream* pStream
)
{
	__xrt_net_server_wait* pWaiter = NULL;

	__xrtSpinLock(&pServer->AcceptLock);
	if ( (xrtNetServerState(pServer) == XNET_SERVER_OPEN) &&
		 (pServer->WaitHead != NULL) ) {
		pWaiter = pServer->WaitHead;
		(void)__xrtNetServerWaitRemove(pServer, pWaiter);
	}
	__xrtSpinUnlock(&pServer->AcceptLock);
	if ( pWaiter == NULL ) {
		return false;
	}
	__xrtNetServerWaitFinish(pWaiter, pStream, false);
	return true;
}



/* 取消只移除当前聚合 Accept 等待，不停止 Server。 */
static void __xrtNetServerWaitCancel(ptr pData)
{
	__xrt_net_server_wait* pWaiter =
		(__xrt_net_server_wait*)pData;
	xnetserver* pServer = pWaiter->Server;
	bool bRemoved;

	__xrtSpinLock(&pServer->AcceptLock);
	bRemoved = pWaiter->Linked &&
		__xrtNetServerWaitRemove(pServer, pWaiter);
	__xrtSpinUnlock(&pServer->AcceptLock);
	if ( bRemoved ) {
		__xrtNetServerWaitFinish(pWaiter, NULL, true);
	}
}



/* 按 FIFO 配对排队 Stream 与 Future，并关闭终止后的等待。 */
void __xrtNetServerFutureNotify(xnetserver* pServer)
{
	for ( ;; ) {
		__xrt_net_server_wait* pWaiter = NULL;
		xnetstream* pStream = NULL;

		__xrtSpinLock(&pServer->AcceptLock);
		if ( pServer->WaitHead != NULL ) {
			if ( xrtNetServerState(pServer) != XNET_SERVER_OPEN ) {
				pWaiter = pServer->WaitHead;
				(void)__xrtNetServerWaitRemove(pServer, pWaiter);
			} else if ( pServer->AcceptHead != NULL ) {
				pWaiter = pServer->WaitHead;
				(void)__xrtNetServerWaitRemove(pServer, pWaiter);
				pStream = __xrtNetServerTakeQueued(pServer);
			}
		}
		__xrtSpinUnlock(&pServer->AcceptLock);
		if ( pWaiter == NULL ) {
			break;
		}
		__xrtNetServerWaitFinish(pWaiter, pStream, false);
	}
}



/* 建立一个 Server 拉取 Accept Future。 */
XRT_API xfuture* xrtNetServerAcceptAsync(xnetserver* pServer)
{
	__xrt_net_server_wait* pWaiter;
	xnetserver* pOwned;
	xfuture* pFuture = NULL;
	xcancel* pCancel;
	xerror* pError;
	xnetstream* pStream = NULL;
	bool bClosed = false;

	if ( pServer == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( pServer->Events.Accept != NULL ) {
		__xrtNetSetError(
			XERR_STATE,
			XNET_ERROR_SERVER_ACCEPT,
			"accept-server",
			"TCP server accept Future requires no Accept callback",
			0
		);
		return NULL;
	}
	pOwned = xrtNetServerRef(pServer);
	if ( pOwned == NULL ) {
		return NULL;
	}
	pWaiter = (__xrt_net_server_wait*)xrtCalloc(1, sizeof(*pWaiter));
	if ( pWaiter == NULL ) {
		xrtNetServerDestroy(pOwned);
		return NULL;
	}
	pWaiter->Server = pOwned;
	pWaiter->Promise = xrtPromiseCreate(&pFuture, NULL);
	if ( pWaiter->Promise == NULL ) {
		xrtFree(pWaiter);
		xrtNetServerDestroy(pOwned);
		return NULL;
	}
	pCancel = xrtPromiseCancelToken(pWaiter->Promise);
	if ( pCancel != NULL ) {
		pWaiter->Watch = xrtCancelWatch(
			pCancel,
			__xrtNetServerWaitCancel,
			pWaiter
		);
		xrtCancelDestroy(pCancel);
	}
	if ( pWaiter->Watch == NULL ) {
		pError = xrtTakeError();
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(pWaiter->Promise);
		xrtFree(pWaiter);
		xrtNetServerDestroy(pOwned);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return NULL;
	}
	__xrtSpinLock(&pServer->AcceptLock);
	if ( xrtNetServerState(pServer) != XNET_SERVER_OPEN ) {
		bClosed = true;
	} else {
		if ( pServer->WaitHead == NULL ) {
			pStream = __xrtNetServerTakeQueued(pServer);
		}
		if ( pStream == NULL ) {
			pWaiter->Linked = true;
			if ( pServer->WaitTail != NULL ) {
				pServer->WaitTail->Next = pWaiter;
			} else {
				pServer->WaitHead = pWaiter;
			}
			pServer->WaitTail = pWaiter;
			(void)xrtAtomic32FetchAdd(
				&pServer->AcceptWaiters,
				1,
				XMEMORY_ACQ_REL
			);
		}
	}
	__xrtSpinUnlock(&pServer->AcceptLock);
	if ( (pStream != NULL) || bClosed ) {
		__xrtNetServerWaitFinish(
			pWaiter,
			pStream,
			false
		);
	}
	return pFuture;
}

#endif

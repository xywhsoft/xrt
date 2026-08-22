#include "../internal/xrt_http_server_runtime.h"
#include <xrt/http_server_future.h>



#if defined(XRT_FEATURE_HTTP_SERVER_FUTURE)

/* Server 关闭等待节点独立持有 Server 和 Promise。 */
struct __xrt_http_server_wait {
	__xrt_http_server_wait* Next;
	xhttpserver* Server;
	xpromise* Promise;
	xcancelwatch* Watch;
	bool Linked;
};



/* 从 Server 等待链表中移除指定节点；调用方持有 Server Lock。 */
static bool __xrtHttpServerWaitRemove(
	xhttpserver* pServer,
	__xrt_http_server_wait* pWaiter
)
{
	__xrt_http_server_wait** ppCurrent =
		&pServer->WaitHead;
	__xrt_http_server_wait* pPrevious = NULL;

	while ( (*ppCurrent != NULL) &&
		(*ppCurrent != pWaiter) ) {
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
	return true;
}



/* 完成一个已摘除的关闭等待并释放全部持有资源。 */
static void __xrtHttpServerWaitFinish(
	__xrt_http_server_wait* pWaiter,
	bool bCancelled
)
{
	xpromise* pPromise = pWaiter->Promise;
	xhttpserver* pServer = pWaiter->Server;

	xrtCancelUnwatch(pWaiter->Watch);
	pWaiter->Watch = NULL;
	xrtHttpServerDestroy(pServer);
	xrtFree(pWaiter);
	if ( bCancelled ) {
		(void)xrtPromiseCancel(pPromise);
	} else {
		(void)xrtPromiseResolve(pPromise, NULL);
	}
	xrtPromiseDestroy(pPromise);
}



/* 取消只摘除当前关闭等待，不改变 Server 生命周期。 */
static void __xrtHttpServerWaitCancel(ptr pData)
{
	__xrt_http_server_wait* pWaiter =
		(__xrt_http_server_wait*)pData;
	xhttpserver* pServer = pWaiter->Server;
	bool bRemoved;

	__xrtSpinLock(&pServer->Lock);
	bRemoved = pWaiter->Linked &&
		__xrtHttpServerWaitRemove(pServer, pWaiter);
	__xrtSpinUnlock(&pServer->Lock);
	if ( bRemoved ) {
		__xrtHttpServerWaitFinish(pWaiter, true);
	}
}



/* 摘除并成功完成 Server 的全部关闭等待。 */
void __xrtHttpServerFutureFinish(xhttpserver* pServer)
{
	__xrt_http_server_wait* pWaiter;

	if ( pServer == NULL ) {
		return;
	}
	__xrtSpinLock(&pServer->Lock);
	pWaiter = pServer->WaitHead;
	pServer->WaitHead = NULL;
	pServer->WaitTail = NULL;
	for ( __xrt_http_server_wait* pCurrent = pWaiter;
		pCurrent != NULL;
		pCurrent = pCurrent->Next ) {
		pCurrent->Linked = false;
	}
	__xrtSpinUnlock(&pServer->Lock);
	while ( pWaiter != NULL ) {
		__xrt_http_server_wait* pNext = pWaiter->Next;

		pWaiter->Next = NULL;
		__xrtHttpServerWaitFinish(pWaiter, false);
		pWaiter = pNext;
	}
}



/* 建立一个可独立取消的 Server 关闭 Future。 */
XRT_API xfuture* xrtHttpServerWaitAsync(xhttpserver* pServer)
{
	__xrt_http_server_wait* pWaiter;
	xfuture* pFuture = NULL;
	xcancel* pCancel;
	xerror* pError;
	bool bClosed;

	if ( pServer == NULL ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"wait-http-server",
			"HTTP server is null",
			NULL
		);
		return NULL;
	}
	pWaiter = (__xrt_http_server_wait*)xrtCalloc(
		1,
		sizeof(*pWaiter)
	);
	if ( pWaiter == NULL ) {
		return NULL;
	}
	pWaiter->Server = xrtHttpServerRef(pServer);
	if ( pWaiter->Server == NULL ) {
		xrtFree(pWaiter);
		return NULL;
	}
	pWaiter->Promise = xrtPromiseCreate(&pFuture, NULL);
	if ( pWaiter->Promise == NULL ) {
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(pWaiter->Promise);
		xrtHttpServerDestroy(pWaiter->Server);
		xrtFree(pWaiter);
		return NULL;
	}
	pCancel = xrtPromiseCancelToken(pWaiter->Promise);
	if ( pCancel != NULL ) {
		pWaiter->Watch = xrtCancelWatch(
			pCancel,
			__xrtHttpServerWaitCancel,
			pWaiter
		);
		xrtCancelDestroy(pCancel);
	}
	if ( pWaiter->Watch == NULL ) {
		pError = xrtTakeError();
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(pWaiter->Promise);
		xrtHttpServerDestroy(pWaiter->Server);
		xrtFree(pWaiter);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return NULL;
	}
	__xrtSpinLock(&pServer->Lock);
	bClosed = xrtAtomic32Load(
		&pServer->ShutdownPublished,
		XMEMORY_ACQUIRE
	) != 0;
	if ( !bClosed ) {
		pWaiter->Linked = true;
		if ( pServer->WaitTail != NULL ) {
			pServer->WaitTail->Next = pWaiter;
		} else {
			pServer->WaitHead = pWaiter;
		}
		pServer->WaitTail = pWaiter;
	}
	__xrtSpinUnlock(&pServer->Lock);
	if ( bClosed ) {
		__xrtHttpServerWaitFinish(pWaiter, false);
	}
	return pFuture;
}



/* Future 桥接上下文用独立锁切断 Connection 与任意完成线程的竞态。 */
struct __xrt_http_conn_future {
	volatile int32 References;
	xrt_spinlock Lock;
	xhttpconn* Connection;
	xhttpconn* PostedConnection;
	xfuture* Source;
};



/* 增加 Future 桥接上下文引用。 */
static __xrt_http_conn_future* __xrtHttpConnFutureRef(
	__xrt_http_conn_future* pContext
)
{
	if ( (pContext == NULL) ||
		(xrtRefRetain(&pContext->References) < 0) ) {
		return NULL;
	}
	return pContext;
}



/* 释放 Future 桥接上下文及其独立持有的源 Future。 */
static void __xrtHttpConnFutureDestroy(
	__xrt_http_conn_future* pContext
)
{
	xhttpconn* pConnection;
	xhttpconn* pPosted;

	if ( (pContext == NULL) ||
		(xrtRefRelease(&pContext->References) != 0) ) {
		return;
	}
	__xrtSpinLock(&pContext->Lock);
	pConnection = pContext->Connection;
	pPosted = pContext->PostedConnection;
	pContext->Connection = NULL;
	pContext->PostedConnection = NULL;
	__xrtSpinUnlock(&pContext->Lock);
	xrtHttpConnDestroy(pPosted);
	xrtHttpConnDestroy(pConnection);
	xrtFutureDestroy(pContext->Source);
	__xrtSpinUnit(&pContext->Lock);
	memset(pContext, 0, sizeof(*pContext));
	xrtFree(pContext);
}



/* 让 Future Owned 延续释放其桥接上下文引用。 */
static void __xrtHttpConnFutureFree(ptr pValue, ptr pData)
{
	(void)pData;
	__xrtHttpConnFutureDestroy(
		(__xrt_http_conn_future*)pValue
	);
}



/* 从 Future 终态选择稳定的应用错误响应。 */
static void __xrtHttpConnFutureFail(
	xhttpconn* pConnection,
	const xfutureresult* pResult,
	xerrkind Kind,
	cstr sMessage
)
{
	const xerror* pCause = pResult != NULL ?
		pResult->Error : NULL;
	uint16 iStatus = 500;

	if ( Kind == XERR_TIMEOUT ) {
		iStatus = 504;
	} else if ( (Kind == XERR_CANCELLED) ||
		(Kind == XERR_CLOSED) ) {
		iStatus = 503;
	}
	__xrtHttpConnRememberError(
		pConnection,
		Kind,
		XHTTP_SERVER_ERROR_CALLBACK,
		"complete-http-response-future",
		sMessage,
		pCause
	);
	__xrtHttpConnEmitError(pConnection);
	pConnection->ForceClose = true;
	if ( xrtHttpConnReply(
		pConnection,
		iStatus,
		XRT_STR_LITERAL("text/plain; charset=utf-8"),
		iStatus == 504 ?
			XRT_BYTES_LITERAL("Gateway Timeout") :
			(iStatus == 503 ?
			 XRT_BYTES_LITERAL("Service Unavailable") :
			 XRT_BYTES_LITERAL("Internal Server Error"))
	) != XNET_RESULT_OK ) {
		(void)xrtHttpConnAbort(pConnection);
	}
}



/* 在 Connection Worker 上消费 Future 唯一终态。 */
static void __xrtHttpConnFutureTask(
	xnetworker* pWorker,
	ptr pData
)
{
	__xrt_http_conn_future* pContext =
		(__xrt_http_conn_future*)pData;
	xhttpconn* pConnection;
	xfutureresult Result;

	__xrtSpinLock(&pContext->Lock);
	pConnection = pContext->PostedConnection;
	pContext->PostedConnection = NULL;
	__xrtSpinUnlock(&pContext->Lock);
	if ( pConnection == NULL ) {
		__xrtHttpConnFutureDestroy(pContext);
		return;
	}
	(void)pWorker;
	if ( pConnection->Future != pContext ) {
		goto Finish;
	}
	__xrtHttpConnFutureDetach(pConnection, false);
	if ( (xrtHttpConnState(pConnection) ==
		 XHTTP_CONN_CLOSING) ||
		(xrtHttpConnState(pConnection) ==
		 XHTTP_CONN_CLOSED) ) {
		goto Finish;
	}
	if ( !xrtFutureResult(pContext->Source, &Result) ) {
		__xrtHttpConnFutureFail(
			pConnection,
			NULL,
			XERR_INTERNAL,
			"HTTP response Future did not publish a terminal result"
		);
		goto Finish;
	}
	if ( Result.State == XFUTURE_RESOLVED ) {
		xerror* pCause;

		if ( Result.Value == NULL ) {
			__xrtHttpConnFutureFail(
				pConnection,
				&Result,
				XERR_VALUE,
				"HTTP response Future resolved without a Reply"
			);
			goto Finish;
		}
		if ( xrtHttpConnRespond(
			pConnection,
			(const xhttpreply*)Result.Value
		) == XNET_RESULT_OK ) {
			goto Finish;
		}
		pCause = xrtTakeError();
		__xrtHttpConnRememberError(
			pConnection,
			__xrtHttpServerCauseKind(
				pCause,
				XERR_PROTOCOL
			),
			XHTTP_SERVER_ERROR_RESPONSE,
			"complete-http-response-future",
			"HTTP response Future produced an invalid Reply",
			pCause
		);
		xrtErrorFree(pCause);
		__xrtHttpConnEmitError(pConnection);
		pConnection->ForceClose = true;
		if ( xrtHttpConnReply(
			pConnection,
			500,
			XRT_STR_LITERAL("text/plain; charset=utf-8"),
			XRT_BYTES_LITERAL("Internal Server Error")
		) != XNET_RESULT_OK ) {
			(void)xrtHttpConnAbort(pConnection);
		}
		goto Finish;
	}
	if ( Result.State == XFUTURE_FAILED ) {
		__xrtHttpConnFutureFail(
			pConnection,
			&Result,
			Result.Error != NULL ?
				xrtErrorKind(Result.Error) :
				XERR_INTERNAL,
			"HTTP response Future failed"
		);
	} else if ( Result.State == XFUTURE_CANCELLED ) {
		__xrtHttpConnFutureFail(
			pConnection,
			&Result,
			XERR_CANCELLED,
			"HTTP response Future was cancelled"
		);
	} else {
		__xrtHttpConnFutureFail(
			pConnection,
			&Result,
			XERR_CLOSED,
			"HTTP response Future closed without a result"
		);
	}

Finish:
	xrtHttpConnDestroy(pConnection);
	__xrtHttpConnFutureDestroy(pContext);
}



/* Future 可从任意线程完成；这里只保留引用并投递回所属 Worker。 */
static void __xrtHttpConnFutureComplete(
	const xfutureresult* pResult,
	xpromise* pOutput,
	ptr pData
)
{
	__xrt_http_conn_future* pContext =
		(__xrt_http_conn_future*)pData;
	xhttpconn* pConnection = NULL;

	(void)pResult;
	__xrtSpinLock(&pContext->Lock);
	if ( (pContext->Connection != NULL) &&
		(pContext->PostedConnection == NULL) ) {
		pConnection = xrtHttpConnRef(
			pContext->Connection
		);
		if ( (pConnection != NULL) &&
			(__xrtHttpConnFutureRef(pContext) != NULL) ) {
			pContext->PostedConnection = pConnection;
		} else {
			xrtHttpConnDestroy(pConnection);
			pConnection = NULL;
		}
	}
	__xrtSpinUnlock(&pContext->Lock);
	if ( (pConnection != NULL) &&
		!xrtNetEnginePost(
			pConnection->Server->Engine,
			xrtNetWorkerIndex(pConnection->Worker),
			__xrtHttpConnFutureTask,
			pContext
		) ) {
		__xrtSpinLock(&pContext->Lock);
		if ( pContext->PostedConnection ==
			pConnection ) {
			pContext->PostedConnection = NULL;
		}
		__xrtSpinUnlock(&pContext->Lock);
		(void)xrtHttpConnAbort(pConnection);
		xrtHttpConnDestroy(pConnection);
		__xrtHttpConnFutureDestroy(pContext);
	}
	(void)xrtPromiseResolve(pOutput, NULL);
}



/* 解除连接持有的 Future 桥，并可请求生产过程协作取消。 */
void __xrtHttpConnFutureDetach(
	xhttpconn* pConnection,
	bool bCancel
)
{
	__xrt_http_conn_future* pContext;
	xhttpconn* pHeld = NULL;

	if ( pConnection == NULL ) {
		return;
	}
	pContext = pConnection->Future;
	if ( pContext == NULL ) {
		return;
	}
	pConnection->Future = NULL;
	__xrtSpinLock(&pContext->Lock);
	if ( pContext->Connection == pConnection ) {
		pHeld = pContext->Connection;
		pContext->Connection = NULL;
	}
	__xrtSpinUnlock(&pContext->Lock);
	if ( bCancel ) {
		(void)xrtFutureCancel(pContext->Source);
	}
	xrtHttpConnDestroy(pHeld);
	__xrtHttpConnFutureDestroy(pContext);
}



/* 把当前完整请求的唯一最终响应绑定到 Future。 */
XRT_API bool xrtHttpConnRespondFuture(
	xhttpconn* pConnection,
	xfuture* pFuture
)
{
	__xrt_http_conn_future* pContext;
	xfuture* pContinue;

	if ( (pConnection == NULL) || (pFuture == NULL) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"bind-http-response-future",
			"HTTP connection and response Future are required",
			NULL
		);
		return false;
	}
	if ( !xrtNetWorkerIsCurrent(pConnection->Worker) ||
		(xrtHttp1ServerExchangeRequest(
			pConnection->Exchange
		 ) == NULL) ||
		!xrtHttp1ServerExchangeComplete(
			pConnection->Exchange
		) ) {
		__xrtHttpServerSetError(
			XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			"bind-http-response-future",
			"HTTP response Future requires its complete request Worker",
			NULL
		);
		return false;
	}
	if ( (pConnection->Future != NULL) ||
		xrtAtomic32Load(
			&pConnection->FinalGate,
			XMEMORY_ACQUIRE
		) ) {
		__xrtHttpServerSetError(
			XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			"bind-http-response-future",
			"HTTP request already has a Future or final response",
			NULL
		);
		return false;
	}
	pContext = (__xrt_http_conn_future*)xrtCalloc(
		1,
		sizeof(*pContext)
	);
	if ( pContext == NULL ) {
		return false;
	}
	pContext->References = 2;
	__xrtSpinInit(&pContext->Lock);
	pContext->Connection = xrtHttpConnRef(pConnection);
	pContext->Source = xrtFutureRef(pFuture);
	if ( (pContext->Connection == NULL) ||
		(pContext->Source == NULL) ) {
		xrtHttpConnDestroy(pContext->Connection);
		xrtFutureDestroy(pContext->Source);
		__xrtSpinUnit(&pContext->Lock);
		xrtFree(pContext);
		return false;
	}
	pConnection->Future = pContext;
	pContinue = xrtFutureContinueOwned(
		pFuture,
		__xrtHttpConnFutureComplete,
		pContext,
		__xrtHttpConnFutureFree,
		NULL
	);
	if ( pContinue == NULL ) {
		pConnection->Future = NULL;
		__xrtSpinLock(&pContext->Lock);
		pContext->Connection = NULL;
		__xrtSpinUnlock(&pContext->Lock);
		xrtHttpConnDestroy(pConnection);
		__xrtHttpConnFutureDestroy(pContext);
		__xrtHttpConnFutureDestroy(pContext);
		return false;
	}
	xrtFutureDestroy(pContinue);
	return true;
}

#endif

#include "../internal/xrt_http_client_stream.h"
#include <xrt/http_exchange_async.h>



#if defined(XRT_FEATURE_HTTP_CLIENT_STREAM_ASYNC)

/* Future 通知释放后在所属 Worker 上消费终态。 */
static void __xrtHttp1CallAsyncReadyTask(
	xnetworker* pWorker,
	ptr pData
);



/* 用专用内嵌节点提交不受公开队列容量影响的内部命令。 */
static void __xrtHttp1CallAsyncPost(xhttp1call* pCall)
{
	__xrtNetEnginePostInternal(
		pCall->Worker,
		&pCall->OutputCommand,
		__xrtHttp1CallAsyncReadyTask,
		pCall
	);
}



/* 释放 waiter 引用，或在完成通知退出后把它转移给 Worker 命令。 */
static void __xrtHttp1CallAsyncRelease(ptr pData)
{
	xhttp1call* pCall = (xhttp1call*)pData;

	if ( pCall->OutputNotified ) {
		pCall->OutputNotified = false;
		__xrtHttp1CallAsyncPost(pCall);
		return;
	}
	xrtHttp1CallDestroy(pCall);
}



/* 把非成功 Future 终态转换为可保留的结构化原因。 */
static xerror* __xrtHttp1CallAsyncError(
	const xfutureresult* pResult
)
{
	if ( pResult->State == XFUTURE_FAILED ) {
		return xrtErrorRef(pResult->Error);
	}
	if ( pResult->State == XFUTURE_CANCELLED ) {
		return xrtErrorCreate(
			XERR_CANCELLED,
			"xrt.future",
			(int32)XFUTURE_CANCELLED,
			"HTTP request body readiness wait was cancelled"
		);
	}
	return xrtErrorCreate(
		XERR_CLOSED,
		"xrt.future",
		(int32)pResult->State,
		"HTTP request body readiness Future closed without a value"
	);
}



/* 在所属 Worker 上消费 Future 终态并恢复或结束调用。 */
static void __xrtHttp1CallAsyncReady(xhttp1call* pCall)
{
	xfutureresult Result;
	xfuture* pFuture = pCall->OutputFuture;
	xerror* pCause = NULL;

	if ( (pFuture == NULL) || !pCall->OutputWaiting ) {
		return;
	}
	pCall->OutputFuture = NULL;
	pCall->OutputWaiting = false;
	if ( !xrtFutureResult(pFuture, &Result) ) {
		pCause = xrtErrorRef(xrtGetError());
	} else if ( Result.State != XFUTURE_RESOLVED ) {
		pCause = __xrtHttp1CallAsyncError(&Result);
	}
	xrtFutureDestroy(pFuture);
	if ( pCause != NULL ) {
		__xrtHttp1CallFail(
			pCall,
			XHTTP1_CALL_ERROR_EXCHANGE,
			xrtErrorKind(pCause),
			"wait-http-request-body",
			"HTTP request body readiness wait did not succeed",
			pCause
		);
		xrtErrorFree(pCause);
		return;
	}
	__xrtHttp1CallDrive(pCall);
}



/* Engine 投递回到所属 Worker 后消费 Future 终态。 */
static void __xrtHttp1CallAsyncReadyTask(
	xnetworker* pWorker,
	ptr pData
)
{
	xhttp1call* pCall = (xhttp1call*)pData;

	(void)pWorker;
	__xrtHttp1CallAsyncReady(pCall);
	xrtHttp1CallDestroy(pCall);
}



/* Future 完成回调只登记通知；Release 在 Calling 清除后提交 Worker 命令。 */
static void __xrtHttp1CallAsyncNotify(ptr pData)
{
	xhttp1call* pCall = (xhttp1call*)pData;

	pCall->OutputNotified = true;
}



/* 订阅一次正文可读性 Future，并覆盖立即完成竞态。 */
bool __xrtHttp1CallAsyncWait(xhttp1call* pCall)
{
	xfuture* pFuture;

	if ( (pCall == NULL) || pCall->OutputWaiting ||
		(pCall->OutputFuture != NULL) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pFuture = xrtHttp1ExchangeOutputWait(pCall->Exchange);
	if ( pFuture == NULL ) {
		return false;
	}
	memset(&pCall->OutputWaiter, 0, sizeof(pCall->OutputWaiter));
	pCall->OutputNotified = false;
	pCall->OutputWaiter.Proc = __xrtHttp1CallAsyncNotify;
	pCall->OutputWaiter.Release = __xrtHttp1CallAsyncRelease;
	pCall->OutputWaiter.Data = pCall;
	pCall->OutputFuture = pFuture;
	pCall->OutputWaiting = true;
	if ( xrtHttp1CallRef(pCall) == NULL ) {
		pCall->OutputFuture = NULL;
		pCall->OutputWaiting = false;
		xrtFutureDestroy(pFuture);
		return false;
	}
	if ( __xrtFutureWaiterAdd(
		pFuture,
		&pCall->OutputWaiter
	) ) {
		return true;
	}
	if ( xrtFutureDone(pFuture) ) {
		__xrtHttp1CallAsyncPost(pCall);
		return true;
	}
	xrtHttp1CallDestroy(pCall);
	pCall->OutputFuture = NULL;
	pCall->OutputWaiting = false;
	xrtFutureDestroy(pFuture);
	return false;
}



/* 停止调用时请求取消来源，并等待并发通知离开内嵌节点。 */
void __xrtHttp1CallAsyncStop(xhttp1call* pCall)
{
	xfuture* pFuture;

	if ( pCall == NULL ) {
		return;
	}
	pFuture = pCall->OutputFuture;
	if ( pFuture == NULL ) {
		pCall->OutputWaiting = false;
		return;
	}
	pCall->OutputFuture = NULL;
	pCall->OutputWaiting = false;
	(void)xrtFutureCancel(pFuture);
	__xrtFutureWaiterRemove(
		pFuture,
		&pCall->OutputWaiter
	);
	xrtFutureDestroy(pFuture);
}

#endif

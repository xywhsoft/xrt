#include "../internal/xrt_http_server_runtime.h"



#if defined(XHTTP_FEATURE_HTTP_SERVER_BODY_ASYNC)

/* 释放 Future 等待节点持有的 Connection 引用。 */
static void __xrtHttpConnBodyRelease(ptr pData)
{
	xrtHttpConnDestroy((xhttpconn*)pData);
}



/* 把非成功 Future 终态转换为可保留的结构化原因。 */
static xerror* __xrtHttpConnBodyError(
	const xfutureresult* pResult
)
{
	xerror* pError;

	if ( pResult->State == XFUTURE_FAILED ) {
		if ( pResult->Error != NULL ) {
			return xrtErrorRef(pResult->Error);
		}
		pError = xrtErrorCreate(
			XERR_INTERNAL,
			"xrt.future",
			(int32)XFUTURE_FAILED,
			"HTTP response body readiness Future failed without an error"
		);
	} else if ( pResult->State == XFUTURE_CANCELLED ) {
		pError = xrtErrorCreate(
			XERR_CANCELLED,
			"xrt.future",
			(int32)XFUTURE_CANCELLED,
			"HTTP response body readiness wait was cancelled"
		);
	} else {
		pError = xrtErrorCreate(
			XERR_CLOSED,
			"xrt.future",
			(int32)pResult->State,
			"HTTP response body readiness Future closed without a value"
		);
	}
	if ( pError == NULL ) {
		pError = xrtErrorRef(xrtGetError());
	}
	return pError;
}



/* 在所属 Worker 上消费 Future 终态并恢复或结束响应。 */
static void __xrtHttpConnBodyReady(
	xhttpconn* pConnection
)
{
	xfutureresult Result;
	xfuture* pFuture = pConnection->BodyFuture;
	xerror* pCause = NULL;

	if ( (pFuture == NULL) ||
		!pConnection->BodyWaiting ) {
		return;
	}
	pConnection->BodyFuture = NULL;
	pConnection->BodyWaiting = false;
	if ( !xrtFutureResult(pFuture, &Result) ) {
		pCause = xrtErrorRef(xrtGetError());
	} else if ( Result.State != XFUTURE_RESOLVED ) {
		pCause = __xrtHttpConnBodyError(&Result);
	}
	xrtFutureDestroy(pFuture);
	if ( (xrtHttpConnState(pConnection) ==
		 XHTTP_CONN_CLOSING) ||
		(xrtHttpConnState(pConnection) ==
		 XHTTP_CONN_CLOSED) ) {
		xrtErrorFree(pCause);
		return;
	}
	if ( pCause != NULL ) {
		__xrtHttpConnRememberError(
			pConnection,
			__xrtHttpServerCauseKind(
				pCause,
				XERR_IO
			),
			XHTTP_SERVER_ERROR_RESPONSE,
			"wait-http-response-body",
			"HTTP response body readiness wait did not succeed",
			pCause
		);
		xrtErrorFree(pCause);
		__xrtHttpConnEmitError(pConnection);
		pConnection->ForceClose = true;
		(void)xrtHttpConnAbort(pConnection);
		return;
	}
	__xrtHttpConnDriveOutput(pConnection);
}



/* Engine 投递回到所属 Worker 后消费 Future 终态。 */
static void __xrtHttpConnBodyReadyTask(
	xnetworker* pWorker,
	ptr pData
)
{
	xhttpconn* pConnection = (xhttpconn*)pData;

	(void)pWorker;
	__xrtHttpConnBodyReady(pConnection);
	xrtHttpConnDestroy(pConnection);
}



/* Future 完成线程只负责把恢复动作送回 Connection Worker。 */
static void __xrtHttpConnBodyNotify(ptr pData)
{
	xhttpconn* pConnection = (xhttpconn*)pData;

	if ( xrtNetWorkerIsCurrent(pConnection->Worker) ) {
		__xrtHttpConnBodyReady(pConnection);
		return;
	}
	if ( xrtHttpConnRef(pConnection) == NULL ) {
		(void)xrtHttpConnAbort(pConnection);
		return;
	}
	if ( !xrtNetEnginePost(
		pConnection->Server->Engine,
		xrtNetWorkerIndex(pConnection->Worker),
		__xrtHttpConnBodyReadyTask,
		pConnection
	) ) {
		xrtHttpConnDestroy(pConnection);
		(void)xrtHttpConnAbort(pConnection);
	}
}



/* 订阅一次正文可读性 Future，并覆盖立即完成竞态。 */
bool __xrtHttpConnBodyWait(xhttpconn* pConnection)
{
	xfuture* pFuture;
	xfuturewatchresult WatchResult;

	if ( (pConnection == NULL) ||
		pConnection->BodyWaiting ||
		(pConnection->BodyFuture != NULL) ||
		(pConnection->Response == NULL) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pFuture = xrtHttp1ServerResponseWait(
		pConnection->Response
	);
	if ( pFuture == NULL ) {
		return false;
	}
	if ( !xrtFutureWatchInit(
		&pConnection->BodyWatch,
		__xrtHttpConnBodyNotify,
		__xrtHttpConnBodyRelease,
		pConnection
	) ) {
		xrtFutureDestroy(pFuture);
		return false;
	}
	pConnection->BodyFuture = pFuture;
	pConnection->BodyWaiting = true;
	if ( xrtHttpConnRef(pConnection) == NULL ) {
		pConnection->BodyFuture = NULL;
		pConnection->BodyWaiting = false;
		xrtFutureDestroy(pFuture);
		return false;
	}
	WatchResult = xrtFutureWatchAdd(
		pFuture,
		&pConnection->BodyWatch
	);
	if ( WatchResult == XFUTURE_WATCH_PENDING ) {
		/*
		 * 正文生产者挂起期间恢复明文 TCP 的有界读取；TLS 继续由
		 * 未消费明文施加背压，二者都不会推进下一条流水线请求。
		 */
		__xrtHttpConnResumeInput(pConnection);
		return true;
	}
	xrtHttpConnDestroy(pConnection);
	if ( WatchResult == XFUTURE_WATCH_READY ) {
		__xrtHttpConnBodyNotify(pConnection);
		return true;
	}
	pConnection->BodyFuture = NULL;
	pConnection->BodyWaiting = false;
	xrtFutureDestroy(pFuture);
	return false;
}



/* 停止连接时请求取消来源，并等待并发通知离开内嵌节点。 */
void __xrtHttpConnBodyStop(
	xhttpconn* pConnection,
	bool bCancel
)
{
	xfuture* pFuture;

	if ( pConnection == NULL ) {
		return;
	}
	pFuture = pConnection->BodyFuture;
	if ( pFuture == NULL ) {
		pConnection->BodyWaiting = false;
		return;
	}
	pConnection->BodyFuture = NULL;
	pConnection->BodyWaiting = false;
	if ( bCancel ) {
		(void)xrtFutureCancel(pFuture);
	}
	xrtFutureWatchRemove(
		pFuture,
		&pConnection->BodyWatch
	);
	xrtFutureDestroy(pFuture);
}

#endif

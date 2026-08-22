#include <stdio.h>
#include <xrt.h>



/* 后台工作项独立持有 Promise 和协作取消令牌。 */
typedef struct example_http_future_work {
	xpromise* Promise;
	xcancel* Cancel;
} example_http_future_work;



/* Future 最后释放成功值时销毁其拥有的 Reply。 */
static void exampleHttpFutureReplyFree(
	ptr pValue,
	ptr pData
)
{
	(void)pData;
	xrtHttpReplyDestroy((xhttpreply*)pValue);
}



/* 在普通工作线程中生成响应，并通过 Promise 发布 Reply。 */
static int32 exampleHttpFutureWorker(ptr pData)
{
	example_http_future_work* pWork =
		(example_http_future_work*)pData;
	xhttpreply* pReply = NULL;

	xrtSleep(100);
	if ( xrtCancelRequested(pWork->Cancel) ) {
		(void)xrtPromiseCancel(pWork->Promise);
	} else {
		pReply = xrtHttpReplyCreate(
			XHTTP_STATUS_ACCEPTED
		);
		if ( (pReply == NULL) ||
			!xrtHttpReplySetBytes(
				pReply,
				XRT_BYTES_LITERAL(
					"{\"state\":\"completed\"}"
				),
				XRT_STR_LITERAL(
					"application/json; charset=utf-8"
				)
			) ||
			!xrtPromiseResolveOwned(
				pWork->Promise,
				pReply,
				exampleHttpFutureReplyFree,
				NULL
			) ) {
			xrtHttpReplyDestroy(pReply);
			(void)xrtPromiseClose(pWork->Promise);
		}
	}
	xrtCancelDestroy(pWork->Cancel);
	xrtPromiseDestroy(pWork->Promise);
	xrtFree(pWork);
	return 0;
}



/* 为当前请求启动后台响应；失败时直接走固定错误响应。 */
static void exampleHttpFutureRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	example_http_future_work* pWork = NULL;
	xpromise* pPromise = NULL;
	xfuture* pFuture = NULL;
	xthread* pThread = NULL;

	(void)pServer;
	(void)pRequest;
	(void)pData;
	pPromise = xrtPromiseCreate(&pFuture, NULL);
	if ( (pPromise == NULL) || (pFuture == NULL) ) {
		goto Failed;
	}
	pWork = (example_http_future_work*)xrtCalloc(
		1,
		sizeof(*pWork)
	);
	if ( pWork == NULL ) {
		goto Failed;
	}
	pWork->Promise = pPromise;
	pWork->Cancel = xrtFutureCancelToken(pFuture);
	if ( pWork->Cancel == NULL ) {
		goto Failed;
	}
	pThread = xrtThreadCreate(
		exampleHttpFutureWorker,
		pWork,
		0
	);
	if ( pThread == NULL ) {
		goto Failed;
	}
	xrtThreadDestroy(pThread);
	pThread = NULL;
	pPromise = NULL;
	pWork = NULL;
	if ( xrtHttpConnRespondFuture(
		pConnection,
		pFuture
	) ) {
		xrtFutureDestroy(pFuture);
		return;
	}
	(void)xrtFutureCancel(pFuture);

Failed:
	xrtThreadDestroy(pThread);
	if ( pWork != NULL ) {
		xrtCancelDestroy(pWork->Cancel);
		xrtFree(pWork);
	}
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);
	(void)xrtHttpConnReply(
		pConnection,
		XHTTP_STATUS_INTERNAL_SERVER_ERROR,
		XRT_STR_LITERAL("text/plain; charset=utf-8"),
		XRT_BYTES_LITERAL("Internal Server Error")
	);
}



/* 输出异步请求的结构化运行时错误。 */
static void exampleHttpFutureError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	(void)pServer;
	(void)pConnection;
	(void)pData;
	fprintf(
		stderr,
		"HTTP error [%s/%s]: %s\n",
		xrtErrorDomain(pError),
		xrtErrorOperation(pError),
		xrtErrorMessage(pError)
	);
}



/* 启动 Future 响应服务，按回车后优雅排空。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents Events;
	xnetengine* pEngine = NULL;
	xhttpserver* pServer = NULL;
	xfuture* pShutdown = NULL;
	xnetaddr Address;
	str sEndpoint = NULL;
	int iResult = 1;

	xrtNetEngineConfigInit(&EngineConfig);
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) ||
		!xrtNetEngineStart(pEngine) ) {
		goto Cleanup;
	}
	xrtHttpServerConfigInit(&ServerConfig);
	if ( !xrtNetAddrLoopback(
		&ServerConfig.Network.Listen.Address,
		XNET_FAMILY_IPV4,
		0
	) ) {
		goto Cleanup;
	}
	xrtHttpServerEventsInit(&Events);
	Events.Request = exampleHttpFutureRequest;
	Events.Error = exampleHttpFutureError;
	pServer = xrtHttpServerStart(
		pEngine,
		&ServerConfig,
		&Events
	);
	if ( (pServer == NULL) ||
		!xrtHttpServerLocal(pServer, 0, &Address) ) {
		goto Cleanup;
	}
	pShutdown = xrtHttpServerWaitAsync(pServer);
	if ( pShutdown == NULL ) {
		goto Cleanup;
	}
	sEndpoint = xrtNetAddrEndpointString(&Address);
	if ( sEndpoint == NULL ) {
		goto Cleanup;
	}
	printf(
		"listening on http://%s/task\n"
		"press Enter to drain\n",
		sEndpoint
	);
	xrtFree(sEndpoint);
	sEndpoint = NULL;
	(void)getchar();
	if ( !xrtHttpServerDrain(pServer) ) {
		goto Cleanup;
	}
	if ( (xrtFutureWaitFor(
		pShutdown,
		UINT64_C(5000000)
	) != XWAIT_OK) ||
		(xrtFutureState(pShutdown) != XFUTURE_RESOLVED) ) {
		goto Cleanup;
	}
	iResult = 0;

Cleanup:
	xrtFree(sEndpoint);
	if ( (pServer != NULL) &&
		(xrtHttpServerState(pServer) !=
		 XHTTP_SERVER_CLOSED) ) {
		if ( pShutdown == NULL ) {
			pShutdown = xrtHttpServerWaitAsync(pServer);
		}
		if ( !xrtHttpServerAbort(pServer) ||
			(pShutdown == NULL) ||
			(xrtFutureWaitFor(
				pShutdown,
				UINT64_C(5000000)
			 ) != XWAIT_OK) ||
			(xrtFutureState(pShutdown) !=
			 XFUTURE_RESOLVED) ) {
			iResult = 2;
		}
	}
	xrtFutureDestroy(pShutdown);
	xrtHttpServerDestroy(pServer);
	if ( (pEngine != NULL) &&
		!xrtNetEngineDestroy(pEngine) &&
		(iResult == 0) ) {
		iResult = 2;
	}
	return iResult;
}

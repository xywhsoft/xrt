#include <stdio.h>
#include <string.h>
#include <xhttp.h>



static const unsigned char ExampleBody[] =
	"body became ready\n";



/* Body 工厂与后台线程共同持有生产状态。 */
typedef struct example_http_body_async {
	xatomic32 References;
	xatomic32 Ready;
	size_t Offset;
} example_http_body_async;



/* 后台线程独立持有 Promise、取消令牌和正文状态。 */
typedef struct example_http_body_async_work {
	example_http_body_async* Body;
	xpromise* Promise;
	xcancel* Cancel;
} example_http_body_async_work;



/* 增加异步正文生产状态的引用。 */
static example_http_body_async* exampleHttpBodyAsyncRef(
	example_http_body_async* pBody
)
{
	if ( pBody != NULL ) {
		(void)xrtAtomic32FetchAdd(
			&pBody->References,
			1,
			XMEMORY_RELAXED
		);
	}
	return pBody;
}



/* 释放正文生产状态的最后一个引用。 */
static void exampleHttpBodyAsyncDestroy(
	example_http_body_async* pBody
)
{
	if ( (pBody != NULL) &&
		(xrtAtomic32FetchSub(
			&pBody->References,
			1,
			XMEMORY_ACQ_REL
		 ) == 1) ) {
		xrtFree(pBody);
	}
}



/* 静态示例正文不需要真实回收。 */
static void exampleHttpBodyAsyncReleaseChunk(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
}



/* 未就绪时返回 AGAIN，就绪后按读取上限发布正文。 */
static xhttpbodystatus exampleHttpBodyAsyncNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	example_http_body_async* pBody =
		(example_http_body_async*)pContext;
	size_t iRemaining;
	size_t iTake;

	if ( xrtAtomic32Load(
		&pBody->Ready,
		XMEMORY_ACQUIRE
	) == 0 ) {
		return XHTTP_BODY_AGAIN;
	}
	if ( pBody->Offset == (sizeof(ExampleBody) - 1u) ) {
		return XHTTP_BODY_EOF;
	}
	iRemaining = (sizeof(ExampleBody) - 1u) -
		pBody->Offset;
	iTake = iRemaining < iMaxBytes ?
		iRemaining : iMaxBytes;
	pChunk->Data = ExampleBody + pBody->Offset;
	pChunk->Size = iTake;
	pChunk->Release =
		exampleHttpBodyAsyncReleaseChunk;
	pBody->Offset += iTake;
	return XHTTP_BODY_DATA;
}



/* 模拟磁盘、队列或上游网络在稍后产生可读数据。 */
static int32 exampleHttpBodyAsyncWorker(ptr pData)
{
	example_http_body_async_work* pWork =
		(example_http_body_async_work*)pData;

	xrtSleep(100);
	if ( xrtCancelRequested(pWork->Cancel) ) {
		(void)xrtPromiseCancel(pWork->Promise);
	} else {
		xrtAtomic32Store(
			&pWork->Body->Ready,
			1,
			XMEMORY_RELEASE
		);
		(void)xrtPromiseResolve(
			pWork->Promise,
			NULL
		);
	}
	xrtCancelDestroy(pWork->Cancel);
	xrtPromiseDestroy(pWork->Promise);
	exampleHttpBodyAsyncDestroy(pWork->Body);
	xrtFree(pWork);
	return 0;
}



/* 为本次 AGAIN 启动生产工作并返回可读性 Future。 */
static xfuture* exampleHttpBodyAsyncWait(ptr pContext)
{
	example_http_body_async* pBody =
		(example_http_body_async*)pContext;
	example_http_body_async_work* pWork = NULL;
	xpromise* pPromise = NULL;
	xfuture* pFuture = NULL;
	xthread* pThread = NULL;

	pPromise = xrtPromiseCreate(&pFuture, NULL);
	if ( pPromise == NULL ) {
		return NULL;
	}
	pWork = (example_http_body_async_work*)xrtCalloc(
		1,
		sizeof(*pWork)
	);
	if ( pWork == NULL ) {
		goto Failed;
	}
	pWork->Body = exampleHttpBodyAsyncRef(pBody);
	pWork->Promise = pPromise;
	pWork->Cancel = xrtFutureCancelToken(pFuture);
	if ( pWork->Cancel == NULL ) {
		goto Failed;
	}
	pThread = xrtThreadCreate(
		exampleHttpBodyAsyncWorker,
		pWork,
		0
	);
	if ( pThread == NULL ) {
		goto Failed;
	}
	xrtThreadDestroy(pThread);
	return pFuture;

Failed:
	xrtThreadDestroy(pThread);
	if ( pWork != NULL ) {
		xrtCancelDestroy(pWork->Cancel);
		exampleHttpBodyAsyncDestroy(pWork->Body);
		xrtFree(pWork);
	}
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);
	return NULL;
}



/* 打开一次性异步正文 Reader。 */
static bool exampleHttpBodyAsyncOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	example_http_body_async* pBody =
		(example_http_body_async*)pFactory;

	pBody->Offset = 0;
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = exampleHttpBodyAsyncNext;
	pOps->Wait = exampleHttpBodyAsyncWait;
	*ppReader = pBody;
	return true;
}



/* Body 对象最后释放时交还工厂引用。 */
static void exampleHttpBodyAsyncFactoryDestroy(
	ptr pFactory
)
{
	exampleHttpBodyAsyncDestroy(
		(example_http_body_async*)pFactory
	);
}



/* 为当前请求提交异步生产的固定长度正文。 */
static void exampleHttpBodyAsyncRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	static const xhttpbodyops Ops = {
		exampleHttpBodyAsyncOpen,
		exampleHttpBodyAsyncFactoryDestroy
	};
	example_http_body_async* pState =
		(example_http_body_async*)xrtCalloc(
			1,
			sizeof(*pState)
		);
	xhttpbody* pBody = NULL;
	xhttpreply* pReply = NULL;

	(void)pServer;
	(void)pRequest;
	(void)pData;
	if ( pState == NULL ) {
		goto Failed;
	}
	xrtAtomic32Init(&pState->References, 1);
	xrtAtomic32Init(&pState->Ready, 0);
	pBody = xrtHttpBodyCreate(
		&Ops,
		pState,
		sizeof(ExampleBody) - 1u,
		XHTTP_BODY_NONE
	);
	if ( pBody == NULL ) {
		exampleHttpBodyAsyncDestroy(pState);
		goto Failed;
	}
	pState = NULL;
	pReply = xrtHttpReplyCreate(XHTTP_STATUS_OK);
	if ( (pReply == NULL) ||
		!xrtHttpReplySetBody(pReply, pBody) ||
		!xrtHttpReplySetHeader(
			pReply,
			XRT_STR_LITERAL("Content-Type"),
			XRT_STR_LITERAL("text/plain; charset=utf-8")
		) ||
		(xrtHttpConnRespond(
			pConnection,
			pReply
		 ) != XNET_RESULT_OK) ) {
		goto Failed;
	}
	xrtHttpReplyDestroy(pReply);
	xrtHttpBodyDestroy(pBody);
	return;

Failed:
	exampleHttpBodyAsyncDestroy(pState);
	xrtHttpReplyDestroy(pReply);
	xrtHttpBodyDestroy(pBody);
	(void)xrtHttpConnReply(
		pConnection,
		XHTTP_STATUS_INTERNAL_SERVER_ERROR,
		XRT_STR_LITERAL("text/plain; charset=utf-8"),
		XRT_BYTES_LITERAL("Internal Server Error")
	);
}



/* 输出异步正文线路的结构化运行时错误。 */
static void exampleHttpBodyAsyncError(
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



/* 启动异步正文 Server，并在标准输入结束后优雅排空。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents Events;
	xnetengine* pEngine = NULL;
	xhttpserver* pServer = NULL;
	xnetaddr Address;
	xdeadline Deadline;
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
	Events.Request = exampleHttpBodyAsyncRequest;
	Events.Error = exampleHttpBodyAsyncError;
	pServer = xrtHttpServerStart(
		pEngine,
		&ServerConfig,
		&Events
	);
	if ( (pServer == NULL) ||
		!xrtHttpServerLocal(pServer, 0, &Address) ) {
		goto Cleanup;
	}
	sEndpoint = xrtNetAddrEndpointString(&Address);
	if ( sEndpoint == NULL ) {
		goto Cleanup;
	}
	printf(
		"listening on http://%s/async-body\n"
		"press Enter to drain\n",
		sEndpoint
	);
	xrtFree(sEndpoint);
	sEndpoint = NULL;
	(void)getchar();
	if ( !xrtHttpServerDrain(pServer) ) {
		goto Cleanup;
	}
	Deadline = xrtDeadlineAfter(UINT64_C(5000000));
	while ( xrtHttpServerState(pServer) !=
		XHTTP_SERVER_CLOSED ) {
		if ( xrtDeadlineExpired(Deadline) ) {
			(void)xrtHttpServerAbort(pServer);
			break;
		}
		xrtThreadYield();
	}
	iResult = 0;

Cleanup:
	xrtFree(sEndpoint);
	if ( (pServer != NULL) &&
		(xrtHttpServerState(pServer) !=
		 XHTTP_SERVER_CLOSED) ) {
		(void)xrtHttpServerAbort(pServer);
		Deadline = xrtDeadlineAfter(
			UINT64_C(5000000)
		);
		while ( xrtHttpServerState(pServer) !=
			XHTTP_SERVER_CLOSED ) {
			if ( xrtDeadlineExpired(Deadline) ) {
				iResult = 2;
				break;
			}
			xrtThreadYield();
		}
	}
	xrtHttpServerDestroy(pServer);
	if ( (pEngine != NULL) &&
		!xrtNetEngineDestroy(pEngine) &&
		(iResult == 0) ) {
		iResult = 2;
	}
	return iResult;
}

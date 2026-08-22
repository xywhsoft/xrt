#include <stdio.h>
#include <string.h>
#include <xhttp.h>



/* 示例上下文只同步唯一终态，不在回调和宿主线程间共享借用视图。 */
typedef struct sse_example {
	xatomic32 Closed;
} sse_example;



/* 输出一段不保证以零字符结尾的 SSE 文本视图。 */
static void printView(cstr sName, xstrview Value)
{
	printf("%s: ", sName);
	if ( Value.Size != 0 ) {
		(void)fwrite(Value.Data, 1, Value.Size, stdout);
	}
	(void)fputc('\n', stdout);
}



/* 报告已经通过状态码和媒体类型校验的响应。 */
static bool onOpen(
	xhttpsseclient* pClient,
	const xhttpresponse* pResponse,
	ptr pData
)
{
	(void)pClient;
	(void)pData;
	printf("open: HTTP %u\n", (unsigned)xrtHttpResponseStatus(pResponse));
	return true;
}



/* 在网络 Worker 回调内消费借用消息，不把 Parser 视图带出回调。 */
static bool onMessage(
	xhttpsseclient* pClient,
	const xhttpssemessage* pMessage,
	ptr pData
)
{
	(void)pClient;
	(void)pData;
	printView("event", pMessage->Type);
	printView("id", pMessage->LastEventId);
	printView("data", pMessage->Data);
	(void)fputc('\n', stdout);
	return true;
}



/* 报告暂态断开；Client 随后按协商后的毫秒延迟自动重连。 */
static void onRetrying(
	xhttpsseclient* pClient,
	size_t iReconnect,
	uint64 iDelay,
	const xerror* pError,
	ptr pData
)
{
	(void)pClient;
	(void)pData;
	fprintf(
		stderr,
		"reconnect %zu after %llu ms: %s\n",
		iReconnect,
		(unsigned long long)iDelay,
		pError != NULL ? xrtErrorMessage(pError) : "stream ended"
	);
}



/* 发布唯一终态并唤醒示例宿主线程。 */
static void onClose(
	xhttpsseclient* pClient,
	xhttpsseclosereason Reason,
	const xerror* pError,
	ptr pData
)
{
	sse_example* pExample = (sse_example*)pData;

	(void)pClient;
	fprintf(
		stderr,
		"closed (%d): %s\n",
		(int)Reason,
		pError != NULL ? xrtErrorMessage(pError) : "normal stop"
	);
	xrtAtomic32Store(&pExample->Closed, 1, XMEMORY_RELEASE);
}



/* 连接一个 SSE URL，打印消息并等待服务端 204 或永久错误终止会话。 */
int main(int argc, char** argv)
{
	xnetengineconfig EngineConfig;
	xhttpclientconfig HttpConfig;
	xhttpsseclientevents Events;
	sse_example Example;
	xnetengine* pEngine = NULL;
	xhttpclient* pHttp = NULL;
	xhttpsseclient* pSse = NULL;
	int iExit = 1;

	if ( argc != 2 ) {
		fprintf(stderr, "usage: sse_client <http-url>\n");
		return 0;
	}
	memset(&Example, 0, sizeof(Example));
	memset(&Events, 0, sizeof(Events));
	Events.Open = onOpen;
	Events.Message = onMessage;
	Events.Retrying = onRetrying;
	Events.Close = onClose;
	Events.Data = &Example;

	xrtNetEngineConfigInit(&EngineConfig);
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		goto cleanup;
	}
	xrtHttpClientConfigInit(&HttpConfig);
	pHttp = xrtHttpClientCreate(pEngine, &HttpConfig);
	if ( pHttp == NULL ) {
		goto cleanup;
	}
	pSse = xrtHttpSseConnect(
		pHttp,
		(xstrview){ argv[1], strlen(argv[1]) },
		NULL,
		&Events
	);
	if ( pSse == NULL ) {
		goto cleanup;
	}

	while ( xrtAtomic32Load(&Example.Closed, XMEMORY_ACQUIRE) == 0 ) {
		xrtSleep(10);
	}
	iExit = xrtHttpSseClientError(pSse) == NULL ? 0 : 1;

cleanup:
	xrtHttpSseClientDestroy(pSse);
	xrtHttpClientDestroy(pHttp);
	if ( pEngine != NULL ) {
		while ( !xrtNetEngineDestroy(pEngine) ) {
			xrtClearError();
			xrtThreadYield();
		}
	}
	return iExit;
}

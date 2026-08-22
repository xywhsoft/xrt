#include <stdio.h>
#include <string.h>
#include <xhttp.h>



/* 保存一次 HTTPS 调用转移给调用方的响应和跨线程终态。 */
typedef struct example_http_https {
	xatomic32 Done;
	xhttpresponse* Response;
	bool Success;
} example_http_https;



/* 输出完整错误原因链，保留 TLS 验证或网络失败的底层来源。 */
static void exampleHttpHttpsError(const xerror* pError)
{
	while ( pError != NULL ) {
		fprintf(
			stderr,
			"%s/%d %s: %s\n",
			xrtErrorDomain(pError),
			(int)xrtErrorCode(pError),
			xrtErrorOperation(pError),
			xrtErrorMessage(pError)
		);
		pError = xrtErrorCause(pError);
	}
}



/* 接管成功响应；失败错误只在回调期间借用。 */
static void exampleHttpHttpsDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	example_http_https* pExample =
		(example_http_https*)pData;

	(void)pCall;
	if ( (pResult != NULL) &&
		(pResult->Result == XNET_RESULT_OK) &&
		(pResult->Response != NULL) &&
		pResult->Info.Secure ) {
		pExample->Response = pResult->Response;
		pExample->Success = true;
	} else if ( pResult != NULL ) {
		exampleHttpHttpsError(pResult->Error);
	}
	xrtAtomic32Store(
		&pExample->Done,
		1,
		XMEMORY_RELEASE
	);
}



/* 在截止时间内等待 Worker 发布终态。 */
static bool exampleHttpHttpsWait(
	example_http_https* pExample,
	xdeadline Deadline
)
{
	while ( xrtAtomic32Load(
		&pExample->Done,
		XMEMORY_ACQUIRE
	) == 0 ) {
		if ( xrtDeadlineExpired(Deadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}



/*
	使用系统信任库执行一次 HTTPS GET。
	默认客户端自动完成 DNS、TCP、TLS 1.3、SNI、证书身份验证和 HTTP/1.1。
*/
int main(int argc, char** argv)
{
	example_http_https Example;
	xnetengineconfig EngineConfig;
	xhttpclientconfig ClientConfig;
	xnetengine* pEngine = NULL;
	xhttpclient* pClient = NULL;
	xhttprequest* pRequest = NULL;
	xhttpcall* pCall = NULL;
	xdeadline Deadline;
	int iResult = 1;

	if ( argc != 2 ) {
		printf("usage: client_https <https-url>\n");
		return 0;
	}
	if ( strncmp(argv[1], "https://", 8u) != 0 ) {
		fprintf(stderr, "an https URL is required\n");
		return 2;
	}
	memset(&Example, 0, sizeof(Example));
	xrtAtomic32Init(&Example.Done, 0);

	/* 默认配置启用系统信任；生产调用不应关闭证书验证。 */
	xrtNetEngineConfigInit(&EngineConfig);
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) ||
		!xrtNetEngineStart(pEngine) ) {
		exampleHttpHttpsError(xrtGetError());
		goto Cleanup;
	}
	xrtHttpClientConfigInit(&ClientConfig);
	pClient = xrtHttpClientCreate(pEngine, &ClientConfig);
	if ( pClient == NULL ) {
		exampleHttpHttpsError(xrtGetError());
		goto Cleanup;
	}

	/* Request 在提交返回后即可销毁，Call 使用其不可变快照。 */
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		(xstrview){ argv[1], strlen(argv[1]) }
	);
	if ( pRequest == NULL ) {
		exampleHttpHttpsError(xrtGetError());
		goto Cleanup;
	}
	pCall = xrtHttpClientDo(
		pClient,
		pRequest,
		NULL,
		exampleHttpHttpsDone,
		&Example
	);
	xrtHttpRequestDestroy(pRequest);
	pRequest = NULL;
	if ( pCall == NULL ) {
		exampleHttpHttpsError(xrtGetError());
		goto Cleanup;
	}

	/* 超时后通过统一 Call 入口协作取消，再等待唯一终态。 */
	Deadline = xrtDeadlineAfter(UINT64_C(35000000));
	if ( !exampleHttpHttpsWait(&Example, Deadline) ) {
		(void)xrtHttpCallCancel(pCall);
		(void)exampleHttpHttpsWait(
			&Example,
			xrtDeadlineAfter(UINT64_C(5000000))
		);
		goto Cleanup;
	}
	if ( Example.Success ) {
		xbytesview Body =
			xrtHttpResponseBody(Example.Response);

		printf(
			"status=%u body=%zu bytes secure=yes\n",
			(unsigned)xrtHttpResponseStatus(
				Example.Response
			),
			Body.Size
		);
		iResult = 0;
	}

Cleanup:
	xrtHttpResponseDestroy(Example.Response);
	xrtHttpCallDestroy(pCall);
	xrtHttpRequestDestroy(pRequest);
	xrtHttpClientDestroy(pClient);
	if ( pEngine != NULL ) {
		while ( !xrtNetEngineDestroy(pEngine) ) {
			xrtClearError();
			xrtThreadYield();
		}
	}
	return iResult;
}

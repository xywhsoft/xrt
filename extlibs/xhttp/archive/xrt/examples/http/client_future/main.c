#include <stdio.h>
#include <string.h>
#include <xrt.h>



/* 以通用 Future 执行一次 GET，并在宿主线程读取拥有型 HTTP 结果。 */
int main(int argc, char** argv)
{
	xnetengineconfig EngineConfig;
	xhttpclientconfig ClientConfig;
	xnetengine* pEngine;
	xhttpclient* pClient;
	xhttprequest* pRequest;
	xfuture* pFuture;
	xfuture* pClosed;
	xhttpresult* pResult;
	const xhttpresponse* pResponse;
	int iExit = 1;

	if ( argc != 2 ) {
		fprintf(stderr, "usage: client_future <http-url>\n");
		return 0;
	}
	xrtNetEngineConfigInit(&EngineConfig);
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		goto cleanup_engine;
	}
	xrtHttpClientConfigInit(&ClientConfig);
	pClient = xrtHttpClientCreate(pEngine, &ClientConfig);
	if ( pClient == NULL ) {
		goto cleanup_engine;
	}
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		(xstrview){ argv[1], strlen(argv[1]) }
	);
	if ( pRequest == NULL ) {
		goto cleanup_client;
	}
	pFuture = xrtHttpClientDoAsync(
		pClient,
		pRequest,
		NULL
	);
	xrtHttpRequestDestroy(pRequest);
	if ( (pFuture == NULL) ||
		(xrtFutureWait(pFuture) != XWAIT_OK) ||
		(xrtFutureState(pFuture) != XFUTURE_RESOLVED) ) {
		xrtFutureDestroy(pFuture);
		goto cleanup_client;
	}
	pResult = (xhttpresult*)xrtFutureValue(pFuture);
	pResponse = xrtHttpResultResponse(pResult);
	if ( pResponse != NULL ) {
		xbytesview Body = xrtHttpResponseBody(pResponse);

		printf("status: %u\n", (unsigned)xrtHttpResponseStatus(pResponse));
		if ( Body.Size != 0 ) {
			(void)fwrite(Body.Data, 1, Body.Size, stdout);
			(void)fputc('\n', stdout);
		}
		iExit = 0;
	}
	xrtFutureDestroy(pFuture);

cleanup_client:
	pClosed = xrtHttpClientWaitAsync(pClient);
	if ( iExit == 0 ) {
		(void)xrtHttpClientDrain(pClient);
	} else {
		(void)xrtHttpClientAbort(pClient);
	}
	xrtHttpClientDestroy(pClient);
	if ( pClosed != NULL ) {
		(void)xrtFutureWait(pClosed);
		xrtFutureDestroy(pClosed);
	}

cleanup_engine:
	if ( pEngine != NULL ) {
		while ( !xrtNetEngineDestroy(pEngine) ) {
			xrtClearError();
			xrtThreadYield();
		}
	}
	return iExit;
}

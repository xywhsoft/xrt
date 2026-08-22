#include <stdio.h>
#include <string.h>
#include <xhttp.h>



/* 用一行 GET 便利入口执行请求并输出响应状态与正文。 */
int main(int argc, char** argv)
{
	xnetengineconfig EngineConfig;
	xhttpclientconfig ClientConfig;
	xnetengine* pEngine;
	xhttpclient* pClient;
	xhttpresult* pResult;
	const xhttpresponse* pResponse;
	int iExit = 1;

	if ( argc != 2 ) {
		fprintf(stderr, "usage: client_easy <http-url>\n");
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
	pResult = xrtHttpClientGetSync(
		pClient,
		(xstrview){ argv[1], strlen(argv[1]) },
		NULL
	);
	if ( pResult == NULL ) {
		goto cleanup_client;
	}
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
	xrtHttpResultDestroy(pResult);

cleanup_client:
	xrtHttpClientDestroy(pClient);

cleanup_engine:
	if ( pEngine != NULL ) {
		while ( !xrtNetEngineDestroy(pEngine) ) {
			xrtClearError();
			xrtThreadYield();
		}
	}
	return iExit;
}



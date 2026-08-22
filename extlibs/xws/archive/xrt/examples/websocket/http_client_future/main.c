#include <stdio.h>
#include <string.h>

#include <xrt.h>



/* 在宿主线程等待连接 Future，并明确取走 Connection 与 101 Response。 */
int main(int argc, char** argv)
{
	xnetengineconfig EngineConfig;
	xhttpclientconfig ClientConfig;
	xwsclientconfig WsConfig;
	xnetengine* pEngine = NULL;
	xhttpclient* pClient = NULL;
	xfuture* pFuture = NULL;
	xwsopenresult* pResult;
	xwsconn* pConnection = NULL;
	xhttpresponse* pResponse = NULL;
	int iExit = 1;

	if ( argc != 2 ) {
		fprintf(stderr, "usage: http_client_future <ws-url>\n");
		return 0;
	}
	xrtNetEngineConfigInit(&EngineConfig);
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) ||
		!xrtNetEngineStart(pEngine) ) {
		goto Cleanup;
	}
	xrtHttpClientConfigInit(&ClientConfig);
	pClient = xrtHttpClientCreate(
		pEngine,
		&ClientConfig
	);
	if ( pClient == NULL ) {
		goto Cleanup;
	}
	xrtWsClientConfigInit(&WsConfig);
	pFuture = xrtWsConnectAsync(
		pClient,
		(xstrview) {
			argv[1],
			strlen(argv[1])
		},
		&WsConfig,
		NULL,
		NULL
	);
	if ( (pFuture == NULL) ||
		(xrtFutureWait(pFuture) != XWAIT_OK) ||
		(xrtFutureState(pFuture) !=
		 XFUTURE_RESOLVED) ) {
		goto Cleanup;
	}
	pResult = (xwsopenresult*)xrtFutureValue(
		pFuture
	);
	pConnection = xrtWsOpenResultTakeConnection(
		pResult
	);
	pResponse = xrtWsOpenResultTakeResponse(
		pResult
	);
	if ( (pConnection == NULL) ||
		(pResponse == NULL) ) {
		goto Cleanup;
	}
	printf(
		"status: %u, protocol: %.*s\n",
		(unsigned)xrtHttpResponseStatus(pResponse),
		(int)xrtWsConnProtocol(pConnection).Size,
		xrtWsConnProtocol(pConnection).Data
	);
	iExit = 0;

Cleanup:
	if ( pConnection != NULL ) {
		(void)xrtWsConnAbort(pConnection);
		xrtWsConnDestroy(pConnection);
	}
	xrtHttpResponseDestroy(pResponse);
	xrtFutureDestroy(pFuture);
	xrtHttpClientDestroy(pClient);
	if ( pEngine != NULL ) {
		while ( !xrtNetEngineDestroy(pEngine) ) {
			xrtClearError();
			xrtThreadYield();
		}
	}
	return iExit;
}

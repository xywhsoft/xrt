/*
 * XRT Example - HTTPD Server Async
 * XRT 范例 - XHTTPD 异步服务
 *
 * Description / 说明:
 *   EN: Demonstrates using xhttpd OnRequestAsync with a background thread future and returning a delayed HTTP response.
 *   CN: 演示使用 xhttpd 的 OnRequestAsync，通过后台线程 future 延迟返回 HTTP 响应。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/xhttpd_server_async.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/xhttpd_server_async -lm -lpthread
 */

#include "../../../xrt.c"
#include "../../example_common.h"
#include "../../example_wait.h"
#include <stdio.h>

typedef struct {
	volatile long iOpenCount;
	volatile long iRequestCount;
	volatile long iAsyncCount;
	volatile long iCloseCount;
	char aLastPath[128];
	char aLastBody[128];
} ex_httpd_async_ctx;

typedef struct {
	ex_httpd_async_ctx* pCtx;
	char aBody[128];
} ex_httpd_async_task;


static void procCopyText(char* sOut, size_t iCap, const char* sText, size_t iLen)
{
	size_t iCopy = 0u;

	if ( !sOut || iCap == 0u ) return;
	memset(sOut, 0, iCap);
	if ( !sText ) return;

	iCopy = iLen;
	if ( iCopy + 1u > iCap ) iCopy = iCap - 1u;
	memcpy(sOut, sText, iCopy);
	sOut[iCopy] = '\0';
}


static void procOnOpen(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn)
{
	ex_httpd_async_ctx* pCtx = (ex_httpd_async_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	if ( !pCtx ) return;
	++pCtx->iOpenCount;
}


static int32 procAsyncWorker(ptr pArg, xfuture_result* pOut)
{
	ex_httpd_async_task* pTask = (ex_httpd_async_task*)pArg;
	xhttpdresponse* pResp = NULL;

	if ( !pTask || !pOut ) {
		xrtFree(pTask);
		return XRT_NET_ERROR;
	}

	xrtSleep(40u);
	pResp = xrtHttpdResponseCreate();
	if ( !pResp ) {
		xrtFree(pTask);
		return XRT_NET_ERROR;
	}

	xrtHttpdResponseSetStatus(pResp, 202u, NULL);
	(void)xrtHttpdResponseSetHeader(pResp, "X-Async", "thread");
	(void)xrtHttpdResponseSetBodyCopy(pResp, pTask->aBody, strlen(pTask->aBody), "text/plain");
	pOut->pValue = pResp;
	++pTask->pCtx->iAsyncCount;
	xrtFree(pTask);
	return XRT_NET_OK;
}


static xfuture* procOnRequestAsync(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq)
{
	ex_httpd_async_ctx* pCtx = (ex_httpd_async_ctx*)pOwner;
	ex_httpd_async_task* pTask;
	(void)pServer;
	(void)pConn;

	if ( !pCtx || !pReq ) return NULL;
	if ( strcmp(pReq->sPath, "/async") != 0 ) return NULL;

	++pCtx->iRequestCount;
	procCopyText(pCtx->aLastPath, sizeof(pCtx->aLastPath), pReq->sPath, strlen(pReq->sPath));
	procCopyText(pCtx->aLastBody, sizeof(pCtx->aLastBody), pReq->pBody ? pReq->pBody : "", pReq->iBodyLen);

	pTask = (ex_httpd_async_task*)xrtCalloc(1u, sizeof(*pTask));
	if ( !pTask ) return NULL;

	pTask->pCtx = pCtx;
	snprintf(pTask->aBody, sizeof(pTask->aBody), "future:%s", pReq->pBody ? pReq->pBody : "");
	return xTaskRunThread(procAsyncWorker, pTask, 0u);
}


static void procOnClose(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, xnet_result iReason)
{
	ex_httpd_async_ctx* pCtx = (ex_httpd_async_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	(void)iReason;
	if ( !pCtx ) return;
	++pCtx->iCloseCount;
}


int main(void)
{
	ex_httpd_async_ctx tCtx;
	xnetengineconfig tEngineCfg;
	xhttpdconfig tSrvCfg;
	xhttpdevents tEvents;
	xnetengine* pEngine = NULL;
	xhttpdserver* pServer = NULL;
	xhttprequest tReq;
	xhttpresponse* pResp = NULL;
	xnet_result iStatus = XRT_NET_ERROR;
	char sURL[256];
	int iRet = 0;

	xrtInit();
	memset(&tCtx, 0, sizeof(tCtx));
	memset(&tEngineCfg, 0, sizeof(tEngineCfg));
	memset(&tSrvCfg, 0, sizeof(tSrvCfg));
	xrtHttpdEventsInit(&tEvents);
	memset(&tReq, 0, sizeof(tReq));
	memset(sURL, 0, sizeof(sURL));

	tEvents.OnOpen = procOnOpen;
	tEvents.OnRequestAsync = procOnRequestAsync;
	tEvents.OnClose = procOnClose;

	xrtNetEngineConfigInit(&tEngineCfg);
	tEngineCfg.iWorkerCount = 1u;
	xrtHttpdConfigInit(&tSrvCfg);
	(void)xrtNetAddrParse(&tSrvCfg.tBindAddr, "127.0.0.1", 0u);

	pEngine = xrtNetEngineCreate(&tEngineCfg);
	if ( !pEngine || xrtNetEngineStart(pEngine) != XRT_NET_OK ) {
		printf("engine = failed\n");
		iRet = 1;
		goto cleanup;
	}
	pServer = xrtHttpdCreate(pEngine, &tSrvCfg, &tEvents, &tCtx);
	if ( !pServer || xrtHttpdStart(pServer) != XRT_NET_OK ) {
		printf("server = failed\n");
		iRet = 1;
		goto cleanup;
	}

	snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/async", (unsigned)xrtHttpdBoundPort(pServer));
	xrtHttpRequestInit(&tReq);
	(void)xrtHttpRequestSetMethod(&tReq, "POST");
	(void)xrtHttpRequestSetURL(&tReq, sURL);
	(void)xrtHttpRequestSetBodyCopy(&tReq, "hello-async", 11u, "text/plain");
	pResp = xrtHttpExecuteSync(NULL, &tReq, &iStatus);
	xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
	(void)exWaitLongMin(&tCtx.iCloseCount, 1, 1000u);

	printf("server_url = %s\n", sURL);
	printf("client_status = %d\n", iStatus);
	printf("response_code = %u\n", pResp ? (unsigned)pResp->iStatusCode : 0u);
	printf("response_body = %s\n", pResp && pResp->pBody ? pResp->pBody : "");
	printf("async_header = %s\n", pResp ? xrtHttpResponseHeader(pResp, "X-Async") : "");
	printf("open_count = %ld\n", tCtx.iOpenCount);
	printf("request_count = %ld\n", tCtx.iRequestCount);
	printf("async_count = %ld\n", tCtx.iAsyncCount);
	printf("close_count = %ld\n", tCtx.iCloseCount);
	printf("last_path = %s\n", tCtx.aLastPath);
	printf("last_body = %s\n", tCtx.aLastBody);

cleanup:
	if ( pResp ) xrtHttpResponseDestroy(pResp);
	xrtHttpRequestUnit(&tReq);
	xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
	xrtNetSyncShutdownHiddenEngine();
	if ( pServer ) {
		xrtHttpdStop(pServer);
		xrtHttpdDestroy(pServer);
	}
	if ( pEngine ) {
		xrtNetEngineStop(pEngine);
		xrtNetEngineDestroy(pEngine);
	}
	xrtUnit();
	return iRet;
}

/*
 * XRT Example - HTTPD Server Basic
 * XRT 范例 - XHTTPD 基础服务
 *
 * Description / 说明:
 *   EN: Demonstrates starting a local xhttpd server, handling a request, and validating the reply with a local HTTP client request.
 *   CN: 演示启动本地 xhttpd 服务、处理请求，并通过本地 HTTP 客户端请求验证响应。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/xhttpd_server_basic.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/xhttpd_server_basic -lm -lpthread
 */

#include "../../../xrt.c"
#include "../../example_common.h"
#include "../../example_wait.h"
#include <stdio.h>

typedef struct {
	volatile long iOpenCount;
	volatile long iRequestCount;
	volatile long iCloseCount;
	char aLastMethod[16];
	char aLastPath[128];
	char aLastQuery[128];
	char aLastBody[128];
} ex_httpd_basic_ctx;


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
	ex_httpd_basic_ctx* pCtx = (ex_httpd_basic_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	if ( !pCtx ) return;
	++pCtx->iOpenCount;
}


static bool procOnRequest(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq, xhttpdresponse* pResp)
{
	ex_httpd_basic_ctx* pCtx = (ex_httpd_basic_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	if ( !pCtx || !pReq || !pResp ) return false;

	++pCtx->iRequestCount;
	procCopyText(pCtx->aLastMethod, sizeof(pCtx->aLastMethod), pReq->sMethod, strlen(pReq->sMethod));
	procCopyText(pCtx->aLastPath, sizeof(pCtx->aLastPath), pReq->sPath, strlen(pReq->sPath));
	procCopyText(pCtx->aLastQuery, sizeof(pCtx->aLastQuery), pReq->sQuery ? pReq->sQuery : "", pReq->sQuery ? strlen(pReq->sQuery) : 0u);
	procCopyText(pCtx->aLastBody, sizeof(pCtx->aLastBody), pReq->pBody ? pReq->pBody : "", pReq->iBodyLen);

	if ( strcmp(pReq->sPath, "/echo") == 0 ) {
		xrtHttpdResponseSetStatus(pResp, 201u, NULL);
		(void)xrtHttpdResponseSetHeader(pResp, "X-Example", "server_basic");
		(void)xrtHttpdResponseSetBodyCopy(pResp, pReq->pBody ? pReq->pBody : "", pReq->iBodyLen, "text/plain");
		return true;
	}

	xrtHttpdResponseSetStatus(pResp, 404u, NULL);
	(void)xrtHttpdResponseSetBodyCopy(pResp, "not found", 9u, "text/plain");
	return true;
}


static void procOnClose(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, xnet_result iReason)
{
	ex_httpd_basic_ctx* pCtx = (ex_httpd_basic_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	(void)iReason;
	if ( !pCtx ) return;
	++pCtx->iCloseCount;
}


int main(void)
{
	ex_httpd_basic_ctx tCtx;
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
	tEvents.OnRequest = procOnRequest;
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

	snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/echo?mode=basic", (unsigned)xrtHttpdBoundPort(pServer));
	xrtHttpRequestInit(&tReq);
	(void)xrtHttpRequestSetMethod(&tReq, "POST");
	(void)xrtHttpRequestSetURL(&tReq, sURL);
	(void)xrtHttpRequestSetBodyCopy(&tReq, "hello-httpd", 11u, "text/plain");
	pResp = xrtHttpExecuteSync(NULL, &tReq, &iStatus);
	xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
	(void)exWaitLongMin(&tCtx.iCloseCount, 1, 1000u);

	printf("server_url = %s\n", sURL);
	printf("client_status = %d\n", iStatus);
	printf("response_code = %u\n", pResp ? (unsigned)pResp->iStatusCode : 0u);
	printf("response_body = %s\n", pResp && pResp->pBody ? pResp->pBody : "");
	printf("open_count = %ld\n", tCtx.iOpenCount);
	printf("request_count = %ld\n", tCtx.iRequestCount);
	printf("close_count = %ld\n", tCtx.iCloseCount);
	printf("last_method = %s\n", tCtx.aLastMethod);
	printf("last_path = %s\n", tCtx.aLastPath);
	printf("last_query = %s\n", tCtx.aLastQuery);
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

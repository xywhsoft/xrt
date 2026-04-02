/*
 * XRT Example - HTTP Server
 * XRT 范例 - HTTP 服务器
 *
 * Description / 说明:
 *   EN: Demonstrates serving a local HTML file with xrtHttpd and validating it with a local client request.
 *   CN: 演示使用 xrtHttpd 提供本地 HTML 文件，并通过本地客户端请求验证。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/tools_http_server.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/tools_http_server -lm -lpthread
 */
#include "../../../xrt.c"
#include "../../example_common.h"

typedef struct {
	str sIndexPath;
	volatile long iRequestCount;
} http_server_ctx;


static bool OnHttpRequest(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq, xhttpdresponse* pResp)
{
	http_server_ctx* pCtx = (http_server_ctx*)pOwner;
	str sBody = NULL;
	size_t iSize = 0;

	(void)pServer;
	(void)pConn;
	if ( !pCtx || !pReq || !pResp ) return FALSE;

	pCtx->iRequestCount++;

	if ( strcmp(pReq->sPath, "/") == 0 ) {
		sBody = xrtFileReadAll(pCtx->sIndexPath, XRT_CP_UTF8, &iSize);
		xrtHttpdResponseSetStatus(pResp, 200u, NULL);
		(void)xrtHttpdResponseSetHeader(pResp, "X-Server", "xrt-http-server");
		(void)xrtHttpdResponseSetBodyCopy(pResp, sBody, iSize, "text/html; charset=utf-8");
		xrtFree(sBody);
		return TRUE;
	}

	xrtHttpdResponseSetStatus(pResp, 404u, NULL);
	(void)xrtHttpdResponseSetBodyCopy(pResp, "not found", 9, "text/plain");
	return TRUE;
}



int main()
{
	http_server_ctx tCtx = { 0 };
	xnetengineconfig tCfg;
	xhttpdconfig tSrvCfg;
	xhttpdevents tEvents;
	xnetengine* pEngine = NULL;
	xhttpdserver* pServer = NULL;
	xhttprequest tReq;
	xhttpresponse* pResp = NULL;
	xnet_result iStatus = XRT_NET_ERROR;
	char sURL[256];

	xrtInit();

	tCtx.sIndexPath = exMakeAppFilePath("http_server_demo_index.html");
	xrtFileWriteAll(tCtx.sIndexPath, "<html><body><h1>XRT HTTP Server</h1></body></html>", 50, XRT_CP_UTF8);

	memset(&tEvents, 0, sizeof(tEvents));
	tEvents.OnRequest = OnHttpRequest;

	xrtNetEngineConfigInit(&tCfg);
	tCfg.iWorkerCount = 1;
	xrtHttpdConfigInit(&tSrvCfg);
	(void)xrtNetAddrParse(&tSrvCfg.tBindAddr, "127.0.0.1", 0);

	pEngine = xrtNetEngineCreate(&tCfg);
	xrtNetEngineStart(pEngine);
	pServer = xrtHttpdCreate(pEngine, &tSrvCfg, &tEvents, &tCtx);
	xrtHttpdStart(pServer);

	snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/", (unsigned)xrtHttpdBoundPort(pServer));
	xrtHttpRequestInit(&tReq);
	(void)xrtHttpRequestSetURL(&tReq, sURL);
	pResp = xrtHttpExecuteSync(NULL, &tReq, &iStatus);

	printf("server_url = %s\n", sURL);
	printf("status = %d\n", iStatus);
	printf("request_count = %ld\n", tCtx.iRequestCount);
	if ( pResp ) {
		printf("code = %u\n", (unsigned)pResp->iStatusCode);
		printf("body = %s\n", pResp->pBody ? pResp->pBody : "");
		xrtHttpResponseDestroy(pResp);
	}

	xrtHttpRequestUnit(&tReq);
	xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
	xrtNetSyncShutdownHiddenEngine();
	xrtHttpdStop(pServer);
	xrtHttpdDestroy(pServer);
	xrtNetEngineStop(pEngine);
	xrtNetEngineDestroy(pEngine);
	xrtFileDelete(tCtx.sIndexPath);
	xrtFree(tCtx.sIndexPath);
	xrtUnit();
	return 0;
}

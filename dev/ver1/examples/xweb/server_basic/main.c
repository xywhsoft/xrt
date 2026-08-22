/*
 * XRT Example - XWeb Server Basic
 * XRT 范例 - XWeb 基础 HTTP 服务
 *
 * Description / 说明:
 *   EN: Demonstrates route parameters, query values, POST body, ANY routes, and static files with xweb.
 *   CN: 演示使用 xweb 处理路由变量、query、POST 请求体、ANY 路由和静态文件。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/xweb_server_basic.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/xweb_server_basic -lm -lpthread
 */
#include "../../../xrt.c"
#include "../../example_common.h"

typedef struct {
	int iGetCount;
	int iPostCount;
	int iAnyCount;
} xweb_demo_ctx;


static xwebaction OnUserRoute(xwebrequest* pReq, xwebresponse* pResp, ptr pUserData)
{
	xweb_demo_ctx* pCtx = (xweb_demo_ctx*)pUserData;
	char sId[64];
	char sName[64];
	bool bOk;

	if ( pCtx ) { pCtx->iGetCount++; }

	bOk = xrtWebRequestParam(pReq, "id", sId, sizeof(sId), NULL) &&
		xrtWebRequestQueryValue(pReq, "name", sName, sizeof(sName), NULL) &&
		strcmp(sId, "42") == 0 &&
		strcmp(sName, "xlang") == 0;

	return xrtWebResponseText(pResp, bOk ? "route-get-ok" : "route-get-bad", "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
}


static xwebaction OnEchoRoute(xwebrequest* pReq, xwebresponse* pResp, ptr pUserData)
{
	xweb_demo_ctx* pCtx = (xweb_demo_ctx*)pUserData;
	const char* sBody;
	size_t iBodyLen = 0u;

	if ( pCtx ) { pCtx->iPostCount++; }

	sBody = (const char*)xrtWebRequestBody(pReq, &iBodyLen);
	if ( sBody && iBodyLen == 4u && memcmp(sBody, "ping", 4u) == 0 ) {
		return xrtWebResponseText(pResp, "xlang-http-ok", "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
	}
	return xrtWebResponseText(pResp, "xlang-http-bad", "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
}


static xwebaction OnAnyRoute(xwebrequest* pReq, xwebresponse* pResp, ptr pUserData)
{
	xweb_demo_ctx* pCtx = (xweb_demo_ctx*)pUserData;
	if ( pCtx ) { pCtx->iAnyCount++; }
	return xrtWebResponseText(pResp, xrtWebRequestMethod(pReq), "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
}


static xhttpresponse* DemoRequest(const char* sURL, const char* sMethod, const char* sBody)
{
	xhttprequest tReq;
	xhttpresponse* pResp;
	xnet_result iStatus = XRT_NET_ERROR;

	xrtHttpRequestInit(&tReq);
	(void)xrtHttpRequestSetURL(&tReq, sURL);
	if ( sMethod ) { (void)xrtHttpRequestSetMethod(&tReq, sMethod); }
	if ( sBody ) { (void)xrtHttpRequestSetBodyCopy(&tReq, sBody, strlen(sBody), "text/plain"); }
	pResp = xrtHttpExecuteSync(NULL, &tReq, &iStatus);
	xrtHttpRequestUnit(&tReq);

	if ( iStatus != XRT_NET_OK ) {
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		return NULL;
	}
	return pResp;
}


static void DemoPrintResponse(const char* sName, xhttpresponse* pResp)
{
	printf("%s_code = %u\n", sName, pResp ? (unsigned)pResp->iStatusCode : 0u);
	printf("%s_body = %s\n", sName, pResp && pResp->pBody ? pResp->pBody : "");
	if ( pResp ) { xrtHttpResponseDestroy(pResp); }
}


int main()
{
	xweb_demo_ctx tCtx;
	xwebconfig tCfg;
	xwebstaticconfig tStaticCfg;
	xwebserver* pServer = NULL;
	char sURL[512];
	str sStaticRoot = NULL;
	str sStaticFile = NULL;
	uint16 iPort = 0u;

	memset(&tCtx, 0, sizeof(tCtx));
	xrtInit();

	sStaticRoot = exMakeAppFilePath((str)"xweb_static");
	sStaticFile = xrtPathJoin(2u, sStaticRoot, (str)"index.txt");
	(void)xrtDirCreateAll((str)sStaticRoot);
	(void)xrtFileWriteAll((str)sStaticFile, (str)"static-ok", 9u, XRT_CP_UTF8);

	xrtWebConfigInit(&tCfg);
	(void)xrtNetAddrParse(&tCfg.tBindAddr, "127.0.0.1", 0u);
	pServer = xrtWebServerCreate(NULL, &tCfg);
	if ( !pServer ) {
		printf("server_create = FAIL\n");
		goto cleanup;
	}

	printf("route_get = %s\n", exBoolText(xrtWebServerGet(pServer, "/users/{id}", OnUserRoute, &tCtx)));
	printf("route_post = %s\n", exBoolText(xrtWebServerPost(pServer, "/demo", OnEchoRoute, &tCtx)));
	printf("route_any = %s\n", exBoolText(xrtWebServerAny(pServer, "/any", OnAnyRoute, &tCtx)));
	xrtWebStaticConfigInit(&tStaticCfg);
	printf("static_mount = %s\n", exBoolText(xrtWebServerStatic(pServer, "/public", (const char*)sStaticRoot, &tStaticCfg)));
	printf("server_start = %s\n", exBoolText(xrtWebServerStart(pServer) == XRT_NET_OK));

	iPort = xrtWebServerPort(pServer);
	printf("port = %u\n", (unsigned)iPort);

	snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/users/42?name=xlang", (unsigned)iPort);
	DemoPrintResponse("get", DemoRequest(sURL, "GET", NULL));

	snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/demo", (unsigned)iPort);
	DemoPrintResponse("post", DemoRequest(sURL, "POST", "ping"));

	snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/any", (unsigned)iPort);
	DemoPrintResponse("any", DemoRequest(sURL, "PUT", NULL));

	snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/public/index.txt", (unsigned)iPort);
	DemoPrintResponse("static", DemoRequest(sURL, "GET", NULL));

	printf("counts = %d,%d,%d\n", tCtx.iGetCount, tCtx.iPostCount, tCtx.iAnyCount);

cleanup:
	xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
	xrtNetSyncShutdownHiddenEngine();
	if ( pServer ) { xrtWebServerDestroy(pServer); }
	if ( sStaticFile ) { (void)xrtFileDelete(sStaticFile); }
	if ( sStaticRoot ) { (void)xrtDirDelete(sStaticRoot); }
	xrtFree(sStaticFile);
	xrtFree(sStaticRoot);
	xrtUnit();
	return 0;
}

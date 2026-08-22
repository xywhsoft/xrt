/*
 * XRT Example - XWeb App Reload And VHost
 * XRT 范例 - xweb app 热替换与虚拟主机
 */
#include "../../../xrt.c"

static xwebaction OnText(xwebrequest* pReq, xwebresponse* pResp, ptr pUserData)
{
	(void)pReq;
	return xrtWebResponseText(pResp, (const char*)pUserData, "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
}


static xwebaction OnError(xwebrequest* pReq, xwebresponse* pResp, uint32 iStatusCode, const char* sMessage, ptr pUserData)
{
	char sBody[128];
	(void)pReq;
	(void)pUserData;
	snprintf(sBody, sizeof(sBody), "custom error %u: %s", (unsigned)iStatusCode, sMessage ? sMessage : "");
	xrtWebResponseStatus(pResp, iStatusCode, NULL);
	return xrtWebResponseText(pResp, sBody, "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
}


static xhttpresponse* Request(const char* sURL, const char* sHost)
{
	xhttprequest tReq;
	xhttpresponse* pResp;
	xnet_result iStatus = XRT_NET_ERROR;

	xrtHttpRequestInit(&tReq);
	(void)xrtHttpRequestSetURL(&tReq, sURL);
	if ( sHost ) { (void)xrtHttpRequestSetHeader(&tReq, "Host", sHost); }
	pResp = xrtHttpExecuteSync(NULL, &tReq, &iStatus);
	xrtHttpRequestUnit(&tReq);

	if ( iStatus != XRT_NET_OK ) {
		if ( pResp ) { xrtHttpResponseDestroy(pResp); }
		return NULL;
	}
	return pResp;
}


static void PrintBody(const char* sName, xhttpresponse* pResp)
{
	printf("%s = %s\n", sName, pResp && pResp->pBody ? pResp->pBody : "");
	if ( pResp ) { xrtHttpResponseDestroy(pResp); }
}


int main(void)
{
	xwebconfig tCfg;
	xwebserver* pServer = NULL;
	xwebapp* pApp1 = NULL;
	xwebapp* pApp2 = NULL;
	xwebapp* pHostApp = NULL;
	char sURL[256];
	uint16 iPort;

	xrtInit();
	xrtWebConfigInit(&tCfg);
	(void)xrtNetAddrParse(&tCfg.tBindAddr, "127.0.0.1", 0u);

	pServer = xrtWebServerCreate(NULL, &tCfg);
	pApp1 = xrtWebAppCreate();
	pApp2 = xrtWebAppCreate();
	pHostApp = xrtWebAppCreate();
	if ( !pServer || !pApp1 || !pApp2 || !pHostApp ) { goto cleanup; }

	(void)xrtWebAppGet(pApp1, "/", OnText, "first app");
	(void)xrtWebAppGet(pApp2, "/", OnText, "second app");
	(void)xrtWebAppError(pApp2, OnError, NULL, NULL);
	(void)xrtWebAppGet(pHostApp, "/", OnText, "host app");

	(void)xrtWebServerSetApp(pServer, pApp1);
	(void)xrtWebServerHost(pServer, "x.local", pHostApp);
	(void)xrtWebServerReloadApp(pServer, pApp2);

	xrtWebAppDestroy(pApp1);
	xrtWebAppDestroy(pApp2);
	xrtWebAppDestroy(pHostApp);
	pApp1 = NULL;
	pApp2 = NULL;
	pHostApp = NULL;

	if ( xrtWebServerStart(pServer) != XRT_NET_OK ) { goto cleanup; }
	iPort = xrtWebServerPort(pServer);
	snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/", (unsigned)iPort);

	PrintBody("default", Request(sURL, NULL));
	snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/missing", (unsigned)iPort);
	PrintBody("error", Request(sURL, NULL));
	snprintf(sURL, sizeof(sURL), "http://127.0.0.1:%u/", (unsigned)iPort);
	PrintBody("vhost", Request(sURL, "x.local"));
	(void)xrtWebServerRemoveHost(pServer, "x.local");
	PrintBody("vhost_removed", Request(sURL, "x.local"));

cleanup:
	xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
	xrtNetSyncShutdownHiddenEngine();
	if ( pServer ) { xrtWebServerDestroy(pServer); }
	if ( pApp1 ) { xrtWebAppDestroy(pApp1); }
	if ( pApp2 ) { xrtWebAppDestroy(pApp2); }
	if ( pHostApp ) { xrtWebAppDestroy(pHostApp); }
	xrtUnit();
	return 0;
}

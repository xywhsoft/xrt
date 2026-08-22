/*
 * XRT Example - HTTP Advanced
 * XRT 范例 - HTTP 高级请求
 *
 * Description / 说明:
 *   EN: Demonstrates the current low-level HTTP request configuration API.
 *   CN: 演示当前低层 HTTP 请求配置 API。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/http_http_advanced.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/http_http_advanced -lm -lpthread
 */
#include "../../../xrt.c"


int main()
{
	xhttprequest tReq;
	xhttpresponse* pResp = NULL;
	xnet_result iStatus = XRT_NET_ERROR;

	xrtInit();

	xrtHttpRequestInit(&tReq);
	(void)xrtHttpRequestSetMethod(&tReq, "POST");
	(void)xrtHttpRequestSetURL(&tReq, "https://httpbin.org/anything");
	(void)xrtHttpRequestSetHeader(&tReq, "X-Demo", "xrt");
	(void)xrtHttpRequestSetBodyCopy(&tReq, "{\"kind\":\"advanced\"}", 19, "application/json");
	xrtHttpRequestSetTimeout(&tReq, 15000u);
	xrtHttpRequestSetVerifyPeer(&tReq, TRUE);
	pResp = xrtHttpExecuteSync(NULL, &tReq, &iStatus);

	printf("status = %d\n", iStatus);
	if ( pResp ) {
		printf("code = %u\n", (unsigned)pResp->iStatusCode);
		printf("server = %s\n", xrtHttpResponseHeader(pResp, "Server"));
		printf("body = %s\n", pResp->pBody ? pResp->pBody : "");
		xrtHttpResponseDestroy(pResp);
	}

	xrtHttpRequestUnit(&tReq);
	xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
	xrtNetSyncShutdownHiddenEngine();
	xrtUnit();
	return 0;
}

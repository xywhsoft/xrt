/*
 * XRT Example - HTTP GET
 * XRT 范例 - HTTP GET 请求
 *
 * Description / 说明:
 *   EN: Demonstrates a blocking HTTP GET request using the current XNet HTTP client.
 *   CN: 演示使用当前 XNet HTTP 客户端执行阻塞式 GET 请求。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/http_http_get.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/http_http_get -lm -lpthread
 */
#include "../../../xrt.c"


int main()
{
	xhttprequest tReq;
	xhttpresponse* pResp = NULL;
	xnet_result iStatus = XRT_NET_ERROR;

	xrtInit();

	xrtHttpRequestInit(&tReq);
	(void)xrtHttpRequestSetURL(&tReq, "https://httpbin.org/get");
	pResp = xrtHttpExecuteSync(NULL, &tReq, &iStatus);

	printf("status = %d\n", iStatus);
	if ( pResp ) {
		printf("code = %u\n", (unsigned)pResp->iStatusCode);
		printf("body_len = %zu\n", pResp->iBodyLen);
		printf("body = %s\n", pResp->pBody ? pResp->pBody : "");
		xrtHttpResponseDestroy(pResp);
	}

	xrtHttpRequestUnit(&tReq);
	xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
	xrtNetSyncShutdownHiddenEngine();
	xrtUnit();
	return 0;
}

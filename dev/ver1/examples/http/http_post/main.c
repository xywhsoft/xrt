/*
 * XRT Example - HTTP POST
 * XRT 范例 - HTTP POST 请求
 *
 * Description / 说明:
 *   EN: Demonstrates form-style HTTP POST requests with the current request object API.
 *   CN: 演示基于当前请求对象 API 的表单式 POST 请求。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/http_http_post.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/http_http_post -lm -lpthread
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
	(void)xrtHttpRequestSetURL(&tReq, "https://httpbin.org/post");
	(void)xrtHttpRequestSetBodyCopy(&tReq, "name=xrt&mode=post", 18, "application/x-www-form-urlencoded");
	pResp = xrtHttpExecuteSync(NULL, &tReq, &iStatus);

	printf("status = %d\n", iStatus);
	if ( pResp ) {
		printf("code = %u\n", (unsigned)pResp->iStatusCode);
		printf("body = %s\n", pResp->pBody ? pResp->pBody : "");
		xrtHttpResponseDestroy(pResp);
	}

	xrtHttpRequestUnit(&tReq);
	xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
	xrtNetSyncShutdownHiddenEngine();
	xrtUnit();
	return 0;
}

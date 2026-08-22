/*
 * XRT Example - HTTP Upload
 * XRT 范例 - HTTP 上传
 *
 * Description / 说明:
 *   EN: Demonstrates multipart upload by manually composing the request body.
 *   CN: 演示手工构造 multipart 请求体实现上传。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/http_http_upload.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/http_http_upload -lm -lpthread
 */
#include "../../../xrt.c"


int main()
{
	const char* sBoundary = "----XRTExampleBoundary";
	const char* sBody =
		"------XRTExampleBoundary\r\n"
		"Content-Disposition: form-data; name=\"name\"\r\n\r\n"
		"xrt\r\n"
		"------XRTExampleBoundary\r\n"
		"Content-Disposition: form-data; name=\"file\"; filename=\"demo.txt\"\r\n"
		"Content-Type: text/plain\r\n\r\n"
		"upload from xrt example\r\n"
		"------XRTExampleBoundary--\r\n";
	char sContentType[128];
	xhttprequest tReq;
	xhttpresponse* pResp = NULL;
	xnet_result iStatus = XRT_NET_ERROR;

	xrtInit();

	snprintf(sContentType, sizeof(sContentType), "multipart/form-data; boundary=%s", sBoundary);

	xrtHttpRequestInit(&tReq);
	(void)xrtHttpRequestSetMethod(&tReq, "POST");
	(void)xrtHttpRequestSetURL(&tReq, "https://httpbin.org/post");
	(void)xrtHttpRequestSetBodyCopy(&tReq, sBody, strlen(sBody), sContentType);
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

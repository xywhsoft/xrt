/*
 * XRT Example - HTTP Download
 * XRT 范例 - HTTP 下载
 *
 * Description / 说明:
 *   EN: Demonstrates fetching a remote body and writing it into a local file.
 *   CN: 演示抓取远程响应正文并写入本地文件。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/http_http_download.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/http_http_download -lm -lpthread
 */
#include "../../../xrt.c"
#include "../../example_common.h"


int main()
{
	str sPath = NULL;
	xhttprequest tReq;
	xhttpresponse* pResp = NULL;
	xnet_result iStatus = XRT_NET_ERROR;

	xrtInit();

	sPath = exMakeAppFilePath("http_download_demo.json");
	xrtHttpRequestInit(&tReq);
	(void)xrtHttpRequestSetURL(&tReq, "https://httpbin.org/json");
	pResp = xrtHttpExecuteSync(NULL, &tReq, &iStatus);

	printf("status = %d\n", iStatus);
	if ( pResp ) {
		xrtFileWriteAll(sPath, pResp->pBody, pResp->iBodyLen, XRT_CP_UTF8);
		printf("saved_file = %s\n", sPath);
		printf("saved_bytes = %zu\n", pResp->iBodyLen);
		xrtHttpResponseDestroy(pResp);
	}

	if ( xrtPathExists(sPath) ) {
		xrtFileDelete(sPath);
	}
	xrtFree(sPath);
	xrtHttpRequestUnit(&tReq);
	xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
	xrtNetSyncShutdownHiddenEngine();
	xrtUnit();
	return 0;
}

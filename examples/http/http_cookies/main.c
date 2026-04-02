/*
 * XRT Example - HTTP Cookies
 * XRT 范例 - HTTP Cookie 管理
 *
 * Description / 说明:
 *   EN: Demonstrates sending Cookie headers and parsing Set-Cookie responses.
 *   CN: 演示发送 Cookie 头以及解析 Set-Cookie 响应。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/http_http_cookies.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/http_http_cookies -lm -lpthread
 */
#include "../../../xrt.c"


int main()
{
	xhttprequest tReq;
	xhttpresponse* pResp = NULL;
	xnet_result iStatus = XRT_NET_ERROR;
	const char* sSetCookie = NULL;
	xrtsetcookieview tCookie;

	xrtInit();

	xrtHttpRequestInit(&tReq);
	(void)xrtHttpRequestSetURL(&tReq, "https://httpbin.org/response-headers?Set-Cookie=session%3Dxrt-demo");
	(void)xrtHttpRequestSetHeader(&tReq, "Cookie", "theme=light; session_id=local-demo");
	pResp = xrtHttpExecuteSync(NULL, &tReq, &iStatus);

	printf("status = %d\n", iStatus);
	if ( pResp ) {
		sSetCookie = xrtHttpResponseHeader(pResp, "Set-Cookie");
		printf("request_cookie = theme=light; session_id=local-demo\n");
		printf("response_set_cookie = %s\n", sSetCookie ? sSetCookie : "");
		if ( sSetCookie && xrtSetCookieParse(sSetCookie, &tCookie) ) {
			printf("parsed_cookie_name = %.*s\n", (int)tCookie.tName.iLen, tCookie.tName.sPtr);
			printf("parsed_cookie_value = %.*s\n", (int)tCookie.tValue.iLen, tCookie.tValue.sPtr);
		}
		xrtHttpResponseDestroy(pResp);
	}

	xrtHttpRequestUnit(&tReq);
	xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
	xrtNetSyncShutdownHiddenEngine();
	xrtUnit();
	return 0;
}

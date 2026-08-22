#define XHTTP_IMPLEMENTATION
#define XHTTP_MODULE_HTTP_SSE_HTTP
#include "../../single/xhttp.h"

#include <stdio.h>



/* 验证单头文件同时带入 SSE、Header 与 MIME 的完整依赖闭包。 */
int main(void)
{
	xhttpheaders* pHeaders = xrtHttpHeadersCreate(NULL);
	bool bPass;

	bPass = (pHeaders != NULL) &&
		xrtHttpSseRequestHeaders(
			pHeaders, XRT_STR_LITERAL("resume-8")
		) &&
		xrtHttpSseResponseHeaders(pHeaders) &&
		(xrtHttpSseResponseCheck(200, pHeaders) ==
		 XHTTP_SSE_RESPONSE_OPEN);
	xrtHttpHeadersDestroy(pHeaders);
	printf("%s single-http-sse-http\n", bPass ? "[PASS]" : "[FAIL]");
	return bPass ? 0 : 1;
}

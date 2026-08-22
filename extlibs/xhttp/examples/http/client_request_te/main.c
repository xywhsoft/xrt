#include <stdio.h>

#include <xhttp.h>



/* 演示一步声明 HTTP/1 响应 Trailer 接收能力。 */
int main(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/events")
	);

	if ( (pRequest == NULL) ||
		!xrtHttp1RequestAcceptTrailers(pRequest) ) {
		xrtHttpRequestDestroy(pRequest);
		return 1;
	}
	printf("headers=%zu\n", xrtHttpRequestHeaderCount(pRequest));
	xrtHttpRequestDestroy(pRequest);
	return 0;
}



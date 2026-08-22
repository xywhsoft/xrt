#include <xrt/http_sse.h>

#include <stdio.h>



/* 展示不绑定客户端或服务器对象的 SSE HTTP 字段适配。 */
int main(void)
{
	xhttpheaders* pRequest = xrtHttpHeadersCreate(NULL);
	xhttpheaders* pResponse = xrtHttpHeadersCreate(NULL);
	str sRequest;
	str sResponse;

	if ( (pRequest == NULL) || (pResponse == NULL) ||
		!xrtHttpSseRequestHeaders(
			pRequest, XRT_STR_LITERAL("job-42:3")
		) || !xrtHttpSseResponseHeaders(pResponse) ||
		(xrtHttpSseResponseCheck(200, pResponse) !=
		 XHTTP_SSE_RESPONSE_OPEN) ) {
		xrtHttpHeadersDestroy(pResponse);
		xrtHttpHeadersDestroy(pRequest);
		return 1;
	}
	sRequest = xrtHttpHeadersBuild(pRequest, NULL);
	sResponse = xrtHttpHeadersBuild(pResponse, NULL);
	if ( (sRequest == NULL) || (sResponse == NULL) ) {
		xrtFree(sResponse);
		xrtFree(sRequest);
		xrtHttpHeadersDestroy(pResponse);
		xrtHttpHeadersDestroy(pRequest);
		return 1;
	}
	printf("request:\n%sresponse:\n%s", sRequest, sResponse);
	xrtFree(sResponse);
	xrtFree(sRequest);
	xrtHttpHeadersDestroy(pResponse);
	xrtHttpHeadersDestroy(pRequest);
	return 0;
}

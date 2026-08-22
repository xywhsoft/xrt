#include <xhttp.h>
#include <stdio.h>



/* 生成可以直接交给 TCP 或 TLS 发送的 HTTP/1.1 请求计划。 */
int main(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("https://example.com/api")
	);
	xhttp1requestplan* pPlan;
	xbytesview Head;

	if ( (pRequest == NULL) ||
		!xrtHttpRequestSetBytes(
			pRequest,
			(xbytesview){ (cbytes)"{\"ok\":true}", 11 },
			XRT_STR_LITERAL(
				"application/json; charset=utf-8"
			)
		) ) {
		xrtHttpRequestDestroy(pRequest);
		return 1;
	}
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	xrtHttpRequestDestroy(pRequest);
	if ( pPlan == NULL ) {
		return 2;
	}
	Head = xrtHttp1RequestPlanHead(pPlan);
	fwrite(Head.Data, 1, Head.Size, stdout);
	xrtHttp1RequestPlanDestroy(pPlan);
	return 0;
}


#include <stdio.h>
#include <xhttp.h>



/* 构建一条可交给 HTTP 客户端执行的 JSON 请求。 */
int main(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("https://api.example.test/items?dry_run=1")
	);
	const xurl* pUrl;
	uint16 iPort;

	if ( (pRequest == NULL) ||
		!xrtHttpRequestSetHeader(
			pRequest,
			XRT_STR_LITERAL("Accept"),
			XRT_STR_LITERAL("application/json")
		) ||
		!xrtHttpRequestSetBytes(
			pRequest,
			(xbytesview){
				(cbytes)"{\"name\":\"xrt\"}",
				14
			},
			XRT_STR_LITERAL("application/json; charset=utf-8")
		) ) {
		xrtHttpRequestDestroy(pRequest);
		return 1;
	}
	pUrl = xrtHttpRequestUrl(pRequest);
	if ( !xrtUrlPort(pUrl, &iPort) ) {
		xrtHttpRequestDestroy(pRequest);
		return 1;
	}
	printf(
		"%.*s %.*s:%u\n",
		(int)xrtHttpRequestMethod(pRequest).Size,
		xrtHttpRequestMethod(pRequest).Data,
		(int)pUrl->Host.Size,
		pUrl->Host.Data,
		(unsigned)iPort
	);
	xrtHttpRequestDestroy(pRequest);
	return 0;
}


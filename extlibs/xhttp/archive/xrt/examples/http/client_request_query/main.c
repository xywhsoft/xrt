#include <stdio.h>
#include <xrt.h>



/* 用拥有型 QueryParams 替换请求 URL 的查询组件。 */
int main(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/search")
	);
	xqueryparams* pParams = xrtQueryParamsCreate(NULL);
	xstrview Url;

	if ( (pRequest == NULL) || (pParams == NULL) ||
		!xrtQueryParamsAppend(
			pParams,
			XRT_STR_LITERAL("q"),
			XRT_STR_LITERAL("xrt runtime")
		) || !xrtHttpRequestSetQueryParams(
			pRequest,
			pParams
		) ) {
		xrtQueryParamsDestroy(pParams);
		xrtHttpRequestDestroy(pRequest);
		return 1;
	}
	Url = xrtHttpRequestUrlText(pRequest);
	printf("%.*s\n", (int)Url.Size, Url.Data);
	xrtQueryParamsDestroy(pParams);
	xrtHttpRequestDestroy(pRequest);
	return 0;
}

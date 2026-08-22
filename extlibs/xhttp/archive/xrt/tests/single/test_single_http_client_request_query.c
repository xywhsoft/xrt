#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件发布请求 QueryParams 适配。 */
int main(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/?old=1")
	);
	xqueryparams* pParams = xrtQueryParamsCreate(NULL);
	bool bPass = (pRequest != NULL) && (pParams != NULL) &&
		xrtQueryParamsAppend(
			pParams,
			XRT_STR_LITERAL("q"),
			XRT_STR_LITERAL("x y")
		) && xrtHttpRequestSetQueryParams(pRequest, pParams) &&
		(xrtHttpRequestUrlText(pRequest).Size == 27u) &&
		(memcmp(
			xrtHttpRequestUrlText(pRequest).Data,
			"https://example.test/?q=x+y",
			27u
		) == 0);

	xrtQueryParamsDestroy(pParams);
	xrtHttpRequestDestroy(pRequest);
	return bPass ? 0 : 1;
}

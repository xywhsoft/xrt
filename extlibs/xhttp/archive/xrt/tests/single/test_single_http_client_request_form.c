#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件发布请求 urlencoded Form 适配。 */
int main(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("https://example.test/form")
	);
	xqueryparams* pParams = xrtQueryParamsCreate(NULL);
	xbytesview Body;
	bool bPass = (pRequest != NULL) && (pParams != NULL) &&
		xrtQueryParamsAppend(
			pParams,
			XRT_STR_LITERAL("name"),
			XRT_STR_LITERAL("xrt")
		) && xrtHttpRequestSetForm(pRequest, pParams) &&
		xrtHttpBodyView(xrtHttpRequestBody(pRequest), &Body) &&
		(Body.Size == 8u) &&
		(memcmp(Body.Data, "name=xrt", 8u) == 0);

	xrtQueryParamsDestroy(pParams);
	xrtHttpRequestDestroy(pRequest);
	return bPass ? 0 : 1;
}

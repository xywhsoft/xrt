#include <xrt.h>



/* 用 QueryParams 一步设置标准 urlencoded 请求正文。 */
int main(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("https://example.test/login")
	);
	xqueryparams* pParams = xrtQueryParamsCreate(NULL);
	bool bPass = (pRequest != NULL) && (pParams != NULL) &&
		xrtQueryParamsAppend(
			pParams,
			XRT_STR_LITERAL("user"),
			XRT_STR_LITERAL("xrt")
		) && xrtHttpRequestSetForm(pRequest, pParams);

	xrtQueryParamsDestroy(pParams);
	xrtHttpRequestDestroy(pRequest);
	return bPass ? 0 : 1;
}

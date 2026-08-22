#include <xrt.h>



/* 一步设置客户端 Basic 凭据。 */
int main(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/private")
	);
	bool bResult = (pRequest != NULL) && xrtHttpRequestSetBasicAuth(
		pRequest,
		XRT_STR_LITERAL("Aladdin"),
		XRT_STR_LITERAL("open sesame")
	);

	xrtHttpRequestDestroy(pRequest);
	return bResult ? 0 : 1;
}

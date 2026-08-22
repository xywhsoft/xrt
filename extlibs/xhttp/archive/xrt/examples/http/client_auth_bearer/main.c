#include <xrt.h>



/* 一步设置客户端 Bearer token。 */
int main(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/private")
	);
	bool bResult = (pRequest != NULL) && xrtHttpRequestSetBearerAuth(
		pRequest,
		XRT_STR_LITERAL("mF_9.B5f-4.1JqM")
	);

	xrtHttpRequestDestroy(pRequest);
	return bResult ? 0 : 1;
}

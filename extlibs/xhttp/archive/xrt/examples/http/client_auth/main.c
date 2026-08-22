#include <xrt.h>



/* 使用自定义认证方案构建客户端请求。 */
int main(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/private")
	);
	bool bResult = (pRequest != NULL) && xrtHttpRequestSetAuth(
		pRequest,
		XRT_STR_LITERAL("Custom"),
		XRT_STR_LITERAL("token")
	);

	xrtHttpRequestDestroy(pRequest);
	return bResult ? 0 : 1;
}

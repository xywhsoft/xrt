#include "../test.h"



/* 验证客户端 Bearer Helper 校验并设置源站和代理 token。 */
int main(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/private")
	);
	const xhttpfield* pField;
	xstrview Token;

	testRequire((pRequest != NULL) && xrtHttpRequestSetBearerAuth(
		pRequest,
		XRT_STR_LITERAL("mF_9.B5f-4.1JqM")
	), "HTTP client Bearer authentication setup failed");
	pField = xrtHttpRequestHeader(
		pRequest,
		XRT_STR_LITERAL("Authorization")
	);
	testRequire((pField != NULL) &&
		xrtHttpBearerRead(pField->Value, &Token) &&
		(Token.Size == 15u),
		"HTTP client Bearer authentication field mismatch");
	testRequire(xrtHttpRequestSetProxyBearerAuth(
		pRequest,
		XRT_STR_LITERAL("proxy-token")
	), "HTTP client proxy Bearer authentication failed");
	xrtHttpRequestDestroy(pRequest);
	puts("[PASS] HTTP client request Bearer authentication");
	return 0;
}

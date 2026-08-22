#include "../test.h"



/* 验证客户端 Basic Helper 生成可解码且唯一的认证字段。 */
int main(void)
{
	char Decoded[64];
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/private")
	);
	const xhttpfield* pField;
	xhttpbasicauth Basic;
	size_t iSize;

	testRequire((pRequest != NULL) && xrtHttpRequestSetBasicAuth(
		pRequest,
		XRT_STR_LITERAL("Aladdin"),
		XRT_STR_LITERAL("open sesame")
	), "HTTP client Basic authentication setup failed");
	pField = xrtHttpRequestHeader(
		pRequest,
		XRT_STR_LITERAL("Authorization")
	);
	testRequire((pField != NULL) && xrtHttpBasicRead(
		pField->Value,
		Decoded,
		sizeof(Decoded),
		&iSize,
		&Basic
	) && (Basic.User.Size == 7u) &&
		(memcmp(Basic.User.Data, "Aladdin", 7u) == 0),
		"HTTP client Basic authentication field mismatch");
	testRequire(xrtHttpRequestSetProxyBasicAuth(
		pRequest,
		XRT_STR_LITERAL("proxy"),
		XRT_STR_LITERAL("secret")
	), "HTTP client proxy Basic authentication failed");
	xrtHttpRequestDestroy(pRequest);
	puts("[PASS] HTTP client request Basic authentication");
	return 0;
}

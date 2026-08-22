#include "../test.h"



/* 验证通用认证设置折叠重复字段并可独立清理源站和代理凭据。 */
int main(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/private")
	);
	const xhttpfield* pField;
	xhttpauth Auth;

	testRequire((pRequest != NULL) &&
		xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Authorization"),
			XRT_STR_LITERAL("Old one")
		) && xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("authorization"),
			XRT_STR_LITERAL("Old two")
		) && xrtHttpRequestSetAuth(
			pRequest,
			XRT_STR_LITERAL("Custom"),
			XRT_STR_LITERAL("token")
		), "HTTP client generic authentication setup failed");
	pField = xrtHttpRequestHeader(
		pRequest,
		XRT_STR_LITERAL("Authorization")
	);
	testRequire((pField != NULL) &&
		(xrtHttpHeadersCountName(
			xrtHttpRequestHeaders(pRequest),
			XRT_STR_LITERAL("Authorization")
		) == 1u) &&
		xrtHttpAuthParse(pField->Value, &Auth) &&
		xrtHttpTokenEqual(Auth.Scheme, XRT_STR_LITERAL("Custom")),
		"HTTP client generic authentication field mismatch");
	testRequire(xrtHttpRequestSetProxyAuth(
		pRequest,
		XRT_STR_LITERAL("Custom"),
		XRT_STR_LITERAL("proxy")
	) && (xrtHttpRequestClearAuth(pRequest) == 1u) &&
		(xrtHttpRequestClearProxyAuth(pRequest) == 1u),
		"HTTP client authentication clearing mismatch");
	xrtHttpRequestDestroy(pRequest);
	puts("[PASS] HTTP client request authentication");
	return 0;
}

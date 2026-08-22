#include "../test.h"



/* 验证客户端请求原子设置源站和代理 Digest 凭据。 */
int main(void)
{
	xhttpdigestauth Input = {
		XHTTP_DIGEST_AUTH_ALGORITHM_EXPLICIT,
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XHTTP_DIGEST_QOP_AUTH,
		1u,
		XRT_STR_INIT("user"),
		{ NULL, 0 },
		XRT_STR_INIT("api"),
		XRT_STR_INIT("nonce"),
		XRT_STR_INIT("/private"),
		XRT_STR_INIT("client"),
		XRT_STR_INIT(
			"0123456789abcdef0123456789abcdef"
			"0123456789abcdef0123456789abcdef"
		),
		{ NULL, 0 },
		{ NULL, 0 }
	};
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/private")
	);
	const xhttpfield* pField;
	xhttpdigestauth Parsed;
	char Output[128];
	size_t iSize;

	testRequire((pRequest != NULL) &&
		xrtHttpRequestSetDigestAuth(pRequest, &Input),
		"HTTP client Digest authentication setup failed");
	pField = xrtHttpRequestHeader(
		pRequest, XRT_STR_LITERAL("Authorization")
	);
	testRequire((pField != NULL) && xrtHttpDigestAuthRead(
		pField->Value, Output, sizeof(Output), &iSize, &Parsed
	) && (Parsed.Algorithm == XHTTP_DIGEST_ALGORITHM_SHA256) &&
		(Parsed.NonceCount == 1u),
		"HTTP client Digest Authorization mismatch");
	testRequire(xrtHttpRequestSetProxyDigestAuth(
		pRequest, &Input
	), "HTTP client proxy Digest authentication failed");
	pField = xrtHttpRequestHeader(
		pRequest, XRT_STR_LITERAL("Proxy-Authorization")
	);
	testRequire((pField != NULL) && xrtHttpDigestAuthRead(
		pField->Value, Output, sizeof(Output), &iSize, &Parsed
	), "HTTP client Digest Proxy-Authorization mismatch");
	xrtHttpRequestDestroy(pRequest);
	puts("[PASS] HTTP client request Digest authentication");
	return 0;
}

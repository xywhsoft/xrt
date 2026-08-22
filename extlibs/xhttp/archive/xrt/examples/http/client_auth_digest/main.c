#include <stdio.h>

#include <xrt/http_client.h>



/* 为客户端请求设置一条规范的 Digest Authorization。 */
int main(void)
{
	xhttpdigestauth Digest = {
		XHTTP_DIGEST_AUTH_ALGORITHM_EXPLICIT,
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XHTTP_DIGEST_QOP_AUTH,
		1u,
		XRT_STR_INIT("user"),
		{ NULL, 0 },
		XRT_STR_INIT("api"),
		XRT_STR_INIT("server-nonce"),
		XRT_STR_INIT("/private"),
		XRT_STR_INIT("client-nonce"),
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
	int iResult = 1;

	if ( (pRequest == NULL) ||
		!xrtHttpRequestSetDigestAuth(pRequest, &Digest) ) {
		goto Cleanup;
	}
	pField = xrtHttpRequestHeader(
		pRequest,
		XRT_STR_LITERAL("Authorization")
	);
	if ( pField == NULL ) {
		goto Cleanup;
	}
	printf(
		"Authorization: %.*s\n",
		(int)pField->Value.Size,
		pField->Value.Data
	);
	iResult = 0;

Cleanup:
	xrtHttpRequestDestroy(pRequest);
	return iResult;
}

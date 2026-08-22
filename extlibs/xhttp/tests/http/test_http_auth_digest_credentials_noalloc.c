#include "../test_allocator.h"

#include <xrt/http_auth.h>



/* Digest 凭据的普通与 username* 路径都不得依赖堆分配。 */
int main(void)
{
	xhttpdigestauth Input = {
		XHTTP_DIGEST_AUTH_USERNAME_EXTENDED |
		XHTTP_DIGEST_AUTH_HAS_USERHASH |
		XHTTP_DIGEST_AUTH_ALGORITHM_EXPLICIT,
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XHTTP_DIGEST_QOP_AUTH,
		1u,
		XRT_STR_INIT("J\xC3\xA4s\xC3\xB8n Doe"),
		{ NULL, 0 },
		XRT_STR_INIT("api"),
		XRT_STR_INIT("nonce"),
		XRT_STR_INIT("/"),
		XRT_STR_INIT("client"),
		XRT_STR_INIT(
			"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
		),
		{ NULL, 0 },
		{ NULL, 0 }
	};
	xhttpdigestauth Result;
	char Value[384];
	char Decoded[160];
	size_t iSize;
	size_t iDecoded;

	testRequire(testInstallFailAllocator(),
		"HTTP Digest credentials failure allocator install failed");
	testRequire(xrtHttpDigestAuthWrite(
		&Input, Value, sizeof(Value), &iSize
	), "HTTP Digest credentials writer allocated");
	testRequire(xrtHttpDigestAuthRead(
		(xstrview){ Value, iSize },
		NULL, 0, &iDecoded, &Result
	), "HTTP Digest credentials query allocated");
	testRequire(xrtHttpDigestAuthRead(
		(xstrview){ Value, iSize },
		Decoded, sizeof(Decoded), &iDecoded, &Result
	), "HTTP Digest credentials reader allocated");
	puts("[PASS] HTTP Digest credentials no allocation");
	return 0;
}

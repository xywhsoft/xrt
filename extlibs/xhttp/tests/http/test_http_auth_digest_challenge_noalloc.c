#include "../test_allocator.h"

#include <xrt/http_auth.h>



/* Digest challenge 查询、解析和写出不得依赖堆分配。 */
int main(void)
{
	xhttpdigestchallenge Input = {
		XHTTP_DIGEST_CHALLENGE_HAS_OPAQUE |
		XHTTP_DIGEST_CHALLENGE_UTF8 |
		XHTTP_DIGEST_CHALLENGE_QOP_AUTH |
		XHTTP_DIGEST_CHALLENGE_ALGORITHM_EXPLICIT,
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XRT_STR_INIT("api"),
		{ NULL, 0 },
		XRT_STR_INIT("nonce"),
		XRT_STR_INIT("opaque"),
		{ NULL, 0 }
	};
	xhttpdigestchallenge Result;
	char Value[160];
	char Decoded[64];
	size_t iSize;
	size_t iDecoded;

	testRequire(testInstallFailAllocator(),
		"HTTP Digest challenge failure allocator install failed");
	testRequire(xrtHttpDigestChallengeWrite(
		&Input, Value, sizeof(Value), &iSize
	), "HTTP Digest challenge writer allocated");
	testRequire(xrtHttpDigestChallengeRead(
		(xstrview){ Value, iSize },
		NULL, 0, &iDecoded, &Result
	), "HTTP Digest challenge query allocated");
	testRequire(xrtHttpDigestChallengeRead(
		(xstrview){ Value, iSize },
		Decoded, sizeof(Decoded), &iDecoded, &Result
	), "HTTP Digest challenge reader allocated");
	puts("[PASS] HTTP Digest challenge no allocation");
	return 0;
}

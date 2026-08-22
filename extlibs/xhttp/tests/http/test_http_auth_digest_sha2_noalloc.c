#include "../test_allocator.h"

#include <xrt/http_auth.h>



/* Digest SHA-2 的实体散列、secret 和证明计算不得分配内存。 */
int main(void)
{
	char Secret[XHTTP_DIGEST_MAX_TEXT_SIZE];
	char Digest[XHTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iSize;
	xhttpdigestproof Proof = {
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XHTTP_DIGEST_QOP_AUTH,
		1u,
		{ Secret, 64u },
		XRT_STR_INIT("nonce"),
		XRT_STR_INIT("client"),
		XRT_STR_INIT("/"),
		{ NULL, 0 }
	};

	testRequire(testInstallFailAllocator(),
		"HTTP Digest SHA-2 failure allocator install failed");
	testRequire(xrtHttpDigestSecret(
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XRT_STR_LITERAL("user"),
		XRT_STR_LITERAL("realm"),
		XRT_STR_LITERAL("password"),
		Secret, sizeof(Secret), &iSize
	), "HTTP Digest secret allocated");
	testRequire(xrtHttpDigestRequest(
		&Proof, XRT_STR_LITERAL("GET"),
		Digest, sizeof(Digest), &iSize
	), "HTTP Digest request calculation allocated");
	testRequire(xrtHttpDigestRspAuth(
		&Proof, Digest, sizeof(Digest), &iSize
	), "HTTP Digest rspauth calculation allocated");
	puts("[PASS] HTTP Digest SHA-2 no allocation");
	return 0;
}

#include "../test_allocator.h"

#include <xrt/http_auth.h>



/* 确定性 nonce 的查询、写出与验证不得依赖堆分配。 */
int main(void)
{
	uint8 KeyData[XHTTP_DIGEST_NONCE_KEY_MIN];
	uint8 Salt[XHTTP_DIGEST_NONCE_SALT_SIZE];
	char Nonce[XHTTP_DIGEST_NONCE_TEXT_SIZE];
	xbytesview Key = { KeyData, sizeof(KeyData) };
	xbytesview Context = XRT_BYTES_LITERAL("api");
	size_t iSize;

	memset(KeyData, 0x11, sizeof(KeyData));
	memset(Salt, 0x22, sizeof(Salt));
	testRequire(testInstallFailAllocator(),
		"HTTP Digest nonce failure allocator install failed");
	testRequire(xrtHttpDigestNonceWrite(
		Key, Context, INT64_C(1700000000),
		Salt, NULL, 0, &iSize
	), "HTTP Digest nonce query allocated");
	testRequire(xrtHttpDigestNonceWrite(
		Key, Context, INT64_C(1700000000),
		Salt, Nonce, sizeof(Nonce), &iSize
	), "HTTP Digest nonce writer allocated");
	testRequire(xrtHttpDigestNonceVerify(
		(xstrview){ Nonce, iSize }, Key, Context,
		INT64_C(1700000030), 60, 5, NULL
	) == XHTTP_DIGEST_NONCE_VALID,
		"HTTP Digest nonce verifier allocated");
	puts("[PASS] HTTP Digest nonce no allocation");
	return 0;
}

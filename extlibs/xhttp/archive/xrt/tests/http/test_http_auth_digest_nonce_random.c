#include "../test.h"

#include <xrt/http_auth.h>



/* 验证随机便利层只改变 salt，并保持同一验证契约。 */
int main(void)
{
	static const uint8 KeyData[32] = {
		0x10u, 0x11u, 0x12u, 0x13u, 0x14u, 0x15u, 0x16u, 0x17u,
		0x18u, 0x19u, 0x1Au, 0x1Bu, 0x1Cu, 0x1Du, 0x1Eu, 0x1Fu,
		0x20u, 0x21u, 0x22u, 0x23u, 0x24u, 0x25u, 0x26u, 0x27u,
		0x28u, 0x29u, 0x2Au, 0x2Bu, 0x2Cu, 0x2Du, 0x2Eu, 0x2Fu
	};
	xbytesview Key = { KeyData, sizeof(KeyData) };
	xbytesview Context = XRT_BYTES_LITERAL("api@example.test");
	char First[XRT_HTTP_DIGEST_NONCE_TEXT_SIZE];
	char Second[XRT_HTTP_DIGEST_NONCE_TEXT_SIZE];
	char Before[XRT_HTTP_DIGEST_NONCE_TEXT_SIZE];
	size_t iSize;
	int64 iIssued;

	testRequire(xrtHttpDigestNonceCreate(
		Key, Context, INT64_C(1700000000), NULL, 0, &iSize
	) && (iSize == sizeof(First)),
		"HTTP Digest random nonce query failed");
	testRequire(xrtHttpDigestNonceCreate(
		Key, Context, INT64_C(1700000000),
		First, sizeof(First), &iSize
	) && xrtHttpDigestNonceCreate(
		Key, Context, INT64_C(1700000000),
		Second, sizeof(Second), &iSize
	) && (memcmp(First, Second, sizeof(First)) != 0),
		"HTTP Digest random nonce salt did not vary");
	testRequire((xrtHttpDigestNonceVerify(
		(xstrview){ First, sizeof(First) }, Key, Context,
		INT64_C(1700000030), 60, 5, &iIssued
	) == XHTTP_DIGEST_NONCE_VALID) &&
		(iIssued == INT64_C(1700000000)),
		"HTTP Digest random nonce verification failed");

	memset(First, 0x5A, sizeof(First));
	memcpy(Before, First, sizeof(Before));
	testRequire(!xrtHttpDigestNonceCreate(
		Key, Context, INT64_C(1700000000),
		First, sizeof(First) - 1u, &iSize
	) && (iSize == sizeof(First)) &&
		(memcmp(First, Before, sizeof(First)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTTP Digest random nonce short output was not atomic");
	xrtClearError();
	puts("[PASS] HTTP Digest random nonce");
	return 0;
}

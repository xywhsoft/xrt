#include "../test_allocator.h"

#include <xrt/http_auth.h>



/* 完整 Digest 证明与 nonce 组合验证不得依赖堆分配。 */
int main(void)
{
	uint8 KeyData[XHTTP_DIGEST_NONCE_KEY_MIN];
	uint8 Salt[XHTTP_DIGEST_NONCE_SALT_SIZE];
	char Nonce[XHTTP_DIGEST_NONCE_TEXT_SIZE];
	char Secret[XHTTP_DIGEST_MAX_TEXT_SIZE];
	char Response[XHTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iNonceSize;
	size_t iSecretSize;
	size_t iResponseSize;
	int64 iIssued;
	xhttpdigestproof Proof;
	xhttpdigestchallenge Challenge;
	xhttpdigestauth Auth;
	xhttpdigestverification Verification;

	memset(KeyData, 0x11, sizeof(KeyData));
	memset(Salt, 0x22, sizeof(Salt));
	testRequire(xrtHttpDigestNonceWrite(
		(xbytesview){ KeyData, sizeof(KeyData) },
		XRT_BYTES_LITERAL("api"),
		INT64_C(1700000000),
		Salt,
		Nonce,
		sizeof(Nonce),
		&iNonceSize
	) && xrtHttpDigestSecret(
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XRT_STR_LITERAL("user"),
		XRT_STR_LITERAL("api"),
		XRT_STR_LITERAL("password"),
		Secret,
		sizeof(Secret),
		&iSecretSize
	), "HTTP Digest verify no-allocation fixture failed");
	Proof = (xhttpdigestproof){
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XHTTP_DIGEST_QOP_AUTH,
		1u,
		{ Secret, iSecretSize },
		{ Nonce, iNonceSize },
		XRT_STR_INIT("client"),
		XRT_STR_INIT("/"),
		{ NULL, 0 }
	};
	testRequire(xrtHttpDigestRequest(
		&Proof,
		XRT_STR_LITERAL("GET"),
		Response,
		sizeof(Response),
		&iResponseSize
	), "HTTP Digest verify no-allocation response failed");
	Challenge = (xhttpdigestchallenge){
		XHTTP_DIGEST_CHALLENGE_QOP_AUTH |
		XHTTP_DIGEST_CHALLENGE_ALGORITHM_EXPLICIT,
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XRT_STR_INIT("api"),
		{ NULL, 0 },
		{ Nonce, iNonceSize },
		{ NULL, 0 },
		{ NULL, 0 }
	};
	Auth = (xhttpdigestauth){
		XHTTP_DIGEST_AUTH_ALGORITHM_EXPLICIT,
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XHTTP_DIGEST_QOP_AUTH,
		1u,
		XRT_STR_INIT("user"),
		{ NULL, 0 },
		XRT_STR_INIT("api"),
		{ Nonce, iNonceSize },
		XRT_STR_INIT("/"),
		XRT_STR_INIT("client"),
		{ Response, iResponseSize },
		{ NULL, 0 },
		{ NULL, 0 }
	};
	Verification = (xhttpdigestverification){
		0,
		&Auth,
		&Challenge,
		{ Secret, iSecretSize },
		XRT_STR_INIT("GET"),
		XRT_STR_INIT("/"),
		{ NULL, 0 }
	};
	testRequire(testInstallFailAllocator(),
		"HTTP Digest verify failure allocator install failed");
	testRequire(xrtHttpDigestProofVerify(
		&Verification
	) == XHTTP_DIGEST_VERIFY_VALID,
		"HTTP Digest proof verifier allocated");
	testRequire(xrtHttpDigestVerify(
		&Verification,
		(xbytesview){ KeyData, sizeof(KeyData) },
		XRT_BYTES_LITERAL("api"),
		INT64_C(1700000030),
		60,
		5,
		&iIssued
	) == XHTTP_DIGEST_VERIFY_VALID &&
		(iIssued == INT64_C(1700000000)),
		"HTTP Digest combined verifier allocated");
	puts("[PASS] HTTP Digest verification no allocation");
	return 0;
}

#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留 Digest 证明与无状态 nonce 的组合验证。 */
int main(void)
{
	uint8 Key[XRT_HTTP_DIGEST_NONCE_KEY_MIN];
	uint8 Salt[XRT_HTTP_DIGEST_NONCE_SALT_SIZE];
	char Nonce[XRT_HTTP_DIGEST_NONCE_TEXT_SIZE];
	char Secret[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
	char Response[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iNonceSize;
	size_t iSecretSize;
	size_t iResponseSize;
	xhttpdigestproof Proof;
	xhttpdigestchallenge Challenge;
	xhttpdigestauth Auth;
	xhttpdigestverification Verification;

	memset(Key, 0x11, sizeof(Key));
	memset(Salt, 0x22, sizeof(Salt));
	if ( !xrtHttpDigestNonceWrite(
		(xbytesview){ Key, sizeof(Key) },
		XRT_BYTES_LITERAL("api"),
		1000,
		Salt,
		Nonce,
		sizeof(Nonce),
		&iNonceSize
	) || !xrtHttpDigestSecret(
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XRT_STR_LITERAL("user"),
		XRT_STR_LITERAL("api"),
		XRT_STR_LITERAL("password"),
		Secret,
		sizeof(Secret),
		&iSecretSize
	) ) {
		return 1;
	}
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
	if ( !xrtHttpDigestRequest(
		&Proof,
		XRT_STR_LITERAL("GET"),
		Response,
		sizeof(Response),
		&iResponseSize
	) ) {
		return 2;
	}
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
	return xrtHttpDigestVerify(
		&Verification,
		(xbytesview){ Key, sizeof(Key) },
		XRT_BYTES_LITERAL("api"),
		1030,
		60,
		5,
		NULL
	) == XHTTP_DIGEST_VERIFY_VALID ? 0 : 3;
}

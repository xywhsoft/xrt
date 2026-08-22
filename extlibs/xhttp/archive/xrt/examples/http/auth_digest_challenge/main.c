#include <stdio.h>

#include <xrt.h>



/* 构建支持 SHA-256 和 UTF-8 用户名的 Digest challenge。 */
int main(void)
{
	xhttpdigestchallenge Challenge = {
		XHTTP_DIGEST_CHALLENGE_UTF8 |
		XHTTP_DIGEST_CHALLENGE_HAS_USERHASH |
		XHTTP_DIGEST_CHALLENGE_USERHASH |
		XHTTP_DIGEST_CHALLENGE_QOP_AUTH |
		XHTTP_DIGEST_CHALLENGE_ALGORITHM_EXPLICIT,
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XRT_STR_INIT("api@example.org"),
		{ NULL, 0 },
		XRT_STR_INIT("server-nonce"),
		{ NULL, 0 },
		{ NULL, 0 }
	};
	char Value[192];
	size_t iSize;

	if ( !xrtHttpDigestChallengeWrite(
		&Challenge, Value, sizeof(Value), &iSize
	) ) {
		return 1;
	}
	printf("%.*s\n", (int)iSize, Value);
	return 0;
}

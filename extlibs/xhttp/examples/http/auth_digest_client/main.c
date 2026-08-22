#include <xrt/http_auth.h>
#include <xrt/memory.h>

#include <stdio.h>



/* 展示无状态客户端协商和 Authorization 凭据构建。 */
int main(void)
{
	xhttpdigestchallenge Challenge = {
		XHTTP_DIGEST_CHALLENGE_QOP_AUTH |
		XHTTP_DIGEST_CHALLENGE_ALGORITHM_EXPLICIT,
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XRT_STR_INIT("api"),
		{ NULL, 0 },
		XRT_STR_INIT("server-nonce"),
		{ NULL, 0 },
		{ NULL, 0 }
	};
	xhttpdigestchoice Choice;
	xhttpdigestclientauth Input;
	xhttpdigestauth Auth;
	char Secret[XHTTP_DIGEST_MAX_TEXT_SIZE];
	char Work[XHTTP_DIGEST_MAX_TEXT_SIZE * 2u];
	char Header[512];
	size_t iSecretSize;
	size_t iWorkSize;
	size_t iHeaderSize;

	if ( (xrtHttpDigestChallengeChoose(
		&Challenge, NULL, &Choice
	) != XHTTP_DIGEST_CHOOSE_ACCEPTED) || !xrtHttpDigestSecret(
		Choice.Algorithm,
		XRT_STR_LITERAL("user"),
		Challenge.Realm,
		XRT_STR_LITERAL("password"),
		Secret,
		sizeof(Secret),
		&iSecretSize
	) ) {
		return 1;
	}
	Input = (xhttpdigestclientauth){
		0,
		&Challenge,
		&Choice,
		XRT_STR_INIT("user"),
		{ NULL, 0 },
		{ Secret, iSecretSize },
		XRT_STR_INIT("GET"),
		XRT_STR_INIT("/private"),
		XRT_STR_INIT("client-nonce"),
		{ NULL, 0 },
		1u
	};
	if ( !xrtHttpDigestClientAuth(
		&Input, Work, sizeof(Work), &iWorkSize, &Auth
	) || !xrtHttpDigestAuthWrite(
		&Auth, Header, sizeof(Header), &iHeaderSize
	) ) {
		xrtSecureZero(Secret, sizeof(Secret));
		return 2;
	}
	printf("Authorization: %.*s\n", (int)iHeaderSize, Header);
	xrtSecureZero(Secret, sizeof(Secret));
	xrtSecureZero(Work, iWorkSize);
	return 0;
}

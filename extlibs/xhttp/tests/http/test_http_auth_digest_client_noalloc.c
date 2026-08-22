#include "../test_allocator.h"

#include <xrt/http_auth.h>



/* 客户端 challenge 协商、凭据构建和 rspauth 验证不得分配堆内存。 */
int main(void)
{
	xhttpdigestchallenge Challenge = {
		XHTTP_DIGEST_CHALLENGE_QOP_AUTH |
		XHTTP_DIGEST_CHALLENGE_ALGORITHM_EXPLICIT,
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XRT_STR_INIT("api"),
		{ NULL, 0 },
		XRT_STR_INIT("nonce"),
		{ NULL, 0 },
		{ NULL, 0 }
	};
	xhttpdigestchoice Choice;
	xhttpdigestclientauth Input;
	xhttpdigestauth Auth;
	xhttpdigestproof Proof;
	xhttpdigestinfo Info;
	xhttpdigestinfoverification Verification;
	char Secret[XHTTP_DIGEST_MAX_TEXT_SIZE];
	char Output[XHTTP_DIGEST_MAX_TEXT_SIZE];
	char Response[XHTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iSecretSize;
	size_t iOutputSize;
	size_t iResponseSize;

	testRequire(xrtHttpDigestSecret(
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XRT_STR_LITERAL("user"),
		XRT_STR_LITERAL("api"),
		XRT_STR_LITERAL("password"),
		Secret,
		sizeof(Secret),
		&iSecretSize
	), "HTTP Digest client no-allocation fixture failed");
	testRequire(testInstallFailAllocator(),
		"HTTP Digest client failure allocator install failed");
	testRequire(xrtHttpDigestChallengeChoose(
		&Challenge, NULL, &Choice
	) == XHTTP_DIGEST_CHOOSE_ACCEPTED,
		"HTTP Digest challenge choice allocated");
	Input = (xhttpdigestclientauth){
		0,
		&Challenge,
		&Choice,
		XRT_STR_INIT("user"),
		{ NULL, 0 },
		{ Secret, iSecretSize },
		XRT_STR_INIT("GET"),
		XRT_STR_INIT("/"),
		XRT_STR_INIT("client"),
		{ NULL, 0 },
		1u
	};
	testRequire(xrtHttpDigestClientAuth(
		&Input, Output, sizeof(Output), &iOutputSize, &Auth
	), "HTTP Digest client auth allocated");
	Proof = (xhttpdigestproof){
		Choice.Algorithm,
		Choice.Qop,
		1u,
		{ Secret, iSecretSize },
		Challenge.Nonce,
		Input.Cnonce,
		Input.RequestTarget,
		{ NULL, 0 }
	};
	testRequire(xrtHttpDigestRspAuth(
		&Proof, Response, sizeof(Response), &iResponseSize
	), "HTTP Digest client rspauth fixture allocated");
	Info = (xhttpdigestinfo){
		XHTTP_DIGEST_INFO_HAS_RESPONSE,
		Choice.Algorithm,
		Choice.Qop,
		1u,
		{ NULL, 0 },
		{ Response, iResponseSize },
		Input.Cnonce
	};
	Verification = (xhttpdigestinfoverification){
		&Info,
		&Proof,
		{ NULL, 0 }
	};
	testRequire(xrtHttpDigestInfoVerify(
		&Verification
	) == XHTTP_DIGEST_INFO_VALID,
		"HTTP Digest info verification allocated");
	puts("[PASS] HTTP Digest client protocol no allocation");
	return 0;
}

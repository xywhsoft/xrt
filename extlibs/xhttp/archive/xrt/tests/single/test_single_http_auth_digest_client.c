#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证 Digest 客户端协议层可由单头文件独立提供。 */
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

	return xrtHttpDigestChallengeChoose(
		&Challenge, NULL, &Choice
	) == XHTTP_DIGEST_CHOOSE_ACCEPTED ? 0 : 1;
}

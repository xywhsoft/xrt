#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证 Digest 状态层可由单头文件独立裁剪并使用。 */
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
	xhttpdigestchoice Choice = {
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XHTTP_DIGEST_QOP_AUTH,
		false
	};
	xhttpdigestsessionconfig Config;
	xhttpdigestsession* pSession;
	xhttpdigestexchange* pExchange;
	char Secret[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iSize;

	if ( !xrtHttpDigestSecret(
		Choice.Algorithm,
		XRT_STR_LITERAL("user"),
		Challenge.Realm,
		XRT_STR_LITERAL("password"),
		Secret,
		sizeof(Secret),
		&iSize
	) ) {
		return 1;
	}
	Config = (xhttpdigestsessionconfig){
		0,
		&Challenge,
		&Choice,
		XRT_STR_INIT("user"),
		{ NULL, 0 },
		{ Secret, iSize },
		XRT_STR_INIT("client")
	};
	pSession = xrtHttpDigestSessionCreate(&Config);
	pExchange = xrtHttpDigestSessionAuthorize(
		pSession,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/"),
		(xstrview){ NULL, 0 }
	);
	if ( (pSession == NULL) || (pExchange == NULL) ) {
		xrtHttpDigestSessionRelease(pSession);
		return 2;
	}
	xrtHttpDigestExchangeRelease(pExchange);
	xrtHttpDigestSessionRelease(pSession);
	return 0;
}

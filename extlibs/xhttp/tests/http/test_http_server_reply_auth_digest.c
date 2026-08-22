#include "../test.h"



/* 验证 Reply 追加源站与代理 Digest challenge。 */
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
	xhttpreply* pReply = xrtHttpReplyCreate(XHTTP_STATUS_UNAUTHORIZED);
	const xhttpfield* pField;
	xhttpdigestchallenge Parsed;
	char Output[32];
	size_t iSize;

	testRequire((pReply != NULL) &&
		xrtHttpReplyAddDigestChallenge(pReply, &Challenge) &&
		xrtHttpReplyAddProxyDigestChallenge(pReply, &Challenge),
		"HTTP server Digest challenge setup failed");
	pField = xrtHttpReplyHeader(
		pReply, XRT_STR_LITERAL("WWW-Authenticate")
	);
	testRequire((pField != NULL) && xrtHttpDigestChallengeRead(
		pField->Value, Output, sizeof(Output), &iSize, &Parsed
	) && (Parsed.Algorithm == XHTTP_DIGEST_ALGORITHM_SHA256),
		"HTTP server Digest source challenge mismatch");
	pField = xrtHttpReplyHeader(
		pReply, XRT_STR_LITERAL("Proxy-Authenticate")
	);
	testRequire((pField != NULL) && xrtHttpDigestChallengeRead(
		pField->Value, Output, sizeof(Output), &iSize, &Parsed
	), "HTTP server Digest proxy challenge mismatch");
	xrtHttpReplyDestroy(pReply);
	puts("[PASS] HTTP server reply Digest challenge");
	return 0;
}

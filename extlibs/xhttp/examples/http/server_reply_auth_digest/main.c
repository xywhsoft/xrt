#include <stdio.h>

#include <xrt/http_server.h>



/* 构建带 Digest challenge 的 401 Reply。 */
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
	xhttpreply* pReply = xrtHttpReplyCreate(
		XHTTP_STATUS_UNAUTHORIZED
	);
	const xhttpfield* pField;
	int iResult = 1;

	if ( (pReply == NULL) ||
		!xrtHttpReplyAddDigestChallenge(pReply, &Challenge) ) {
		goto Cleanup;
	}
	pField = xrtHttpReplyHeader(
		pReply,
		XRT_STR_LITERAL("WWW-Authenticate")
	);
	if ( pField == NULL ) {
		goto Cleanup;
	}
	printf("WWW-Authenticate: %.*s\n", (int)pField->Value.Size, pField->Value.Data);
	iResult = 0;

Cleanup:
	xrtHttpReplyDestroy(pReply);
	return iResult;
}

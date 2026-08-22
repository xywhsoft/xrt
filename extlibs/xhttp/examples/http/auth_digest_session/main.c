#include <xrt/http_auth.h>

#include <stdio.h>



/* 展示并发 HTTP 客户端可长期持有的 Digest 会话和单请求 Exchange。 */
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
	xhttpdigestchoice Choice = {
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XHTTP_DIGEST_QOP_AUTH,
		false
	};
	xhttpdigestsessionconfig Config;
	xhttpdigestsession* pSession;
	xhttpdigestexchange* pExchange;
	const xhttpdigestauth* pAuth;
	char Secret[XHTTP_DIGEST_MAX_TEXT_SIZE];
	char Header[512];
	size_t iSecretSize;
	size_t iHeaderSize;

	if ( !xrtHttpDigestSecret(
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
	Config = (xhttpdigestsessionconfig){
		0,
		&Challenge,
		&Choice,
		XRT_STR_INIT("user"),
		{ NULL, 0 },
		{ Secret, iSecretSize },
		XRT_STR_INIT("client-nonce")
	};
	pSession = xrtHttpDigestSessionCreate(&Config);
	pExchange = xrtHttpDigestSessionAuthorize(
		pSession,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/private"),
		(xstrview){ NULL, 0 }
	);
	pAuth = xrtHttpDigestExchangeAuth(pExchange);
	if ( (pSession == NULL) || (pExchange == NULL) ||
		(pAuth == NULL) || !xrtHttpDigestAuthWrite(
			pAuth, Header, sizeof(Header), &iHeaderSize
		) ) {
		xrtHttpDigestExchangeRelease(pExchange);
		xrtHttpDigestSessionRelease(pSession);
		return 2;
	}
	printf("Authorization: %.*s\n", (int)iHeaderSize, Header);
	xrtHttpDigestExchangeRelease(pExchange);
	xrtHttpDigestSessionRelease(pSession);
	return 0;
}

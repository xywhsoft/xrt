#include <xrt/http_client.h>

#include <stdio.h>



/* 使用最终 HTTP/1 request-target 一次完成 Digest 授权和请求冻结。 */
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
	xhttpdigestexchange* pExchange = NULL;
	xhttprequest* pRequest;
	xhttp1requestplan* pPlan;
	xbytesview Head;
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
		XRT_STR_INIT("client-nonce")
	};
	pSession = xrtHttpDigestSessionCreate(&Config);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/private")
	);
	pPlan = xrtHttp1RequestPrepareDigest(
		pRequest,
		NULL,
		pSession,
		(xstrview){ NULL, 0 },
		&pExchange
	);
	if ( (pPlan == NULL) || (pExchange == NULL) ) {
		xrtHttpRequestDestroy(pRequest);
		xrtHttpDigestSessionRelease(pSession);
		return 2;
	}
	Head = xrtHttp1RequestPlanHead(pPlan);
	printf("%.*s", (int)Head.Size, (const char*)Head.Data);
	xrtHttp1RequestPlanDestroy(pPlan);
	xrtHttpDigestExchangeRelease(pExchange);
	xrtHttpRequestDestroy(pRequest);
	xrtHttpDigestSessionRelease(pSession);
	return 0;
}

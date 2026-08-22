#ifndef TEST_HTTP_DIGEST_RESPONSE_SESSION_FIXTURE_H
#define TEST_HTTP_DIGEST_RESPONSE_SESSION_FIXTURE_H

#include "http_client_response_fixture.h"



/* 创建响应会话测试使用的固定 Digest 状态。 */
static inline xhttpdigestsession* testHttpDigestResponseSession(void)
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
	char Secret[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iSecretSize;

	testRequire(xrtHttpDigestSecret(
		Choice.Algorithm,
		XRT_STR_LITERAL("user"),
		Challenge.Realm,
		XRT_STR_LITERAL("password"),
		Secret,
		sizeof(Secret),
		&iSecretSize
	), "HTTP Digest response session secret failed");
	Config = (xhttpdigestsessionconfig){
		0,
		&Challenge,
		&Choice,
		XRT_STR_INIT("user"),
		{ NULL, 0 },
		{ Secret, iSecretSize },
		XRT_STR_INIT("client")
	};
	return xrtHttpDigestSessionCreate(&Config);
}



/* 为指定 Exchange 写出一份可选择破坏的响应证明。 */
static inline size_t testHttpDigestResponseValue(
	const xhttpdigestexchange* pExchange,
	xstrview NextNonce,
	bool bValid,
	char* pOutput,
	size_t iCapacity
)
{
	const xhttpdigestproof* pProof =
		xrtHttpDigestExchangeProof(pExchange);
	xhttpdigestinfo Info;
	char Response[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iResponseSize;
	size_t iSize;

	testRequire((pProof != NULL) && xrtHttpDigestRspAuth(
		pProof,
		Response,
		sizeof(Response),
		&iResponseSize
	), "HTTP Digest response session rspauth failed");
	if ( !bValid ) {
		Response[0] = Response[0] == '0' ? '1' : '0';
	}
	Info = (xhttpdigestinfo){
		XHTTP_DIGEST_INFO_HAS_NEXT_NONCE |
		XHTTP_DIGEST_INFO_HAS_RESPONSE,
		pProof->Algorithm,
		pProof->Qop,
		pProof->NonceCount,
		NextNonce,
		{ Response, iResponseSize },
		pProof->Cnonce
	};
	testRequire(xrtHttpDigestInfoWrite(
		&Info, pOutput, iCapacity, &iSize
	), "HTTP Digest response session info write failed");
	return iSize;
}



/* 从一份动态 Authentication-Info 值创建拥有型响应。 */
static inline xhttpresponse* testHttpDigestResponseCreate(
	xstrview Name,
	xstrview Value
)
{
	xhttpfield Field = { Name, Value };

	return testHttpResponseFixtureCreate(&Field, 1u);
}

#endif

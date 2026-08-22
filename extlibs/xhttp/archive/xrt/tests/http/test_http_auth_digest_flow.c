#include "http_client_response_fixture.h"
#include "http_server_request_fixture.h"



/* 比较 Digest 往返中的借用文本与零结尾常量。 */
static bool testHttpDigestFlowText(xstrview Text, cstr sExpected)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, sExpected, iSize) == 0);
}



/*
	验证 challenge、客户端凭据、服务端校验和 rspauth 回执的对象层闭环。
	网络运行时只负责传输这些已经独立压实的对象，因此这里不建立套接字。
*/
int main(void)
{
	xhttpdigestchallenge Challenge = {
		XHTTP_DIGEST_CHALLENGE_QOP_AUTH |
		XHTTP_DIGEST_CHALLENGE_ALGORITHM_EXPLICIT,
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XRT_STR_INIT("api@example.test"),
		{ NULL, 0 },
		{ NULL, 0 },
		{ NULL, 0 },
		{ NULL, 0 }
	};
	uint8 NonceKey[XRT_HTTP_DIGEST_NONCE_KEY_MIN];
	uint8 NonceSalt[XRT_HTTP_DIGEST_NONCE_SALT_SIZE];
	xhttpdigestproof Proof;
	xhttpdigestauth Auth;
	xhttpdigestauth ServerAuth;
	xhttpdigestinfo Info;
	xhttpdigestinfo ClientInfo;
	xhttpdigestverification Verification;
	xhttpauthcursor Cursor;
	xhttpdigestreplayconfig ReplayConfig = { 1u, 16u, 60 };
	xhttpdigestreplay* pReplay;
	xhttpreply* pReply;
	xhttpresponse* pResponse;
	xhttprequest* pRequest;
	xhttpserverrequest* pServerRequest;
	const xhttpfield* pField;
	xhttpfield Field;
	char ChallengeOutput[128];
	char Nonce[XRT_HTTP_DIGEST_NONCE_TEXT_SIZE];
	char ServerOutput[256];
	char InfoOutput[128];
	char RequestHead[1024];
	char Secret[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
	char RequestDigest[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
	char ResponseDigest[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iSize;
	int iHeadSize;
	int64 iIssued;

	/* 服务端构建 challenge，客户端通过响应对象选择并解码它。 */
	memset(NonceKey, 0x11, sizeof(NonceKey));
	memset(NonceSalt, 0x22, sizeof(NonceSalt));
	testRequire(xrtHttpDigestNonceWrite(
		(xbytesview){ NonceKey, sizeof(NonceKey) },
		XRT_BYTES_LITERAL("api@example.test"),
		INT64_C(1700000000),
		NonceSalt,
		Nonce,
		sizeof(Nonce),
		&iSize
	), "HTTP Digest flow nonce setup failed");
	Challenge.Nonce = (xstrview){ Nonce, iSize };
	pReply = xrtHttpReplyCreate(XHTTP_STATUS_UNAUTHORIZED);
	testRequire((pReply != NULL) &&
		xrtHttpReplyAddDigestChallenge(pReply, &Challenge),
		"HTTP Digest flow challenge setup failed");
	pField = xrtHttpReplyHeader(
		pReply, XRT_STR_LITERAL("WWW-Authenticate")
	);
	testRequire(pField != NULL,
		"HTTP Digest flow challenge field missing");
	Field = *pField;
	pResponse = testHttpResponseFixtureCreate(&Field, 1u);
	xrtHttpReplyDestroy(pReply);
	xrtHttpAuthCursorInit(&Cursor);
	testRequire((xrtHttpResponseDigestChallengeNext(
		pResponse, &Cursor,
		ChallengeOutput, sizeof(ChallengeOutput), &iSize, &Challenge
	) == XHTTP_NEXT_ITEM) &&
		(Challenge.Algorithm == XHTTP_DIGEST_ALGORITHM_SHA256) &&
		((Challenge.Flags & XHTTP_DIGEST_CHALLENGE_QOP_AUTH) != 0) &&
		testHttpDigestFlowText(Challenge.Realm, "api@example.test") &&
		(Challenge.Nonce.Size == XRT_HTTP_DIGEST_NONCE_TEXT_SIZE),
		"HTTP Digest flow challenge decode failed");

	/* 客户端从密码派生可持久化 Secret，并生成唯一 Authorization。 */
	testRequire(xrtHttpDigestSecret(
		Challenge.Algorithm,
		XRT_STR_LITERAL("Mufasa"),
		Challenge.Realm,
		XRT_STR_LITERAL("Circle Of Life"),
		Secret, sizeof(Secret), &iSize
	), "HTTP Digest flow secret derivation failed");
	Proof = (xhttpdigestproof){
		Challenge.Algorithm,
		XHTTP_DIGEST_QOP_AUTH,
		1u,
		{ Secret, iSize },
		Challenge.Nonce,
		XRT_STR_LITERAL("client-nonce"),
		XRT_STR_LITERAL("/private"),
		{ NULL, 0 }
	};
	testRequire(xrtHttpDigestRequest(
		&Proof, XRT_STR_LITERAL("GET"),
		RequestDigest, sizeof(RequestDigest), &iSize
	), "HTTP Digest flow request proof failed");
	Auth = (xhttpdigestauth){
		XHTTP_DIGEST_AUTH_ALGORITHM_EXPLICIT,
		Proof.Algorithm,
		Proof.Qop,
		Proof.NonceCount,
		XRT_STR_INIT("Mufasa"),
		{ NULL, 0 },
		Challenge.Realm,
		Proof.Nonce,
		Proof.Uri,
		Proof.Cnonce,
		{ RequestDigest, iSize },
		{ NULL, 0 },
		{ NULL, 0 }
	};
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://example.test/private")
	);
	testRequire((pRequest != NULL) &&
		xrtHttpRequestSetDigestAuth(pRequest, &Auth),
		"HTTP Digest flow client Authorization failed");
	pField = xrtHttpRequestHeader(
		pRequest, XRT_STR_LITERAL("Authorization")
	);
	testRequire((pField != NULL) && (pField->Value.Size <= INT_MAX),
		"HTTP Digest flow client field missing");
	iHeadSize = snprintf(
		RequestHead,
		sizeof(RequestHead),
		"GET /private HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Authorization: %.*s\r\n\r\n",
		(int)pField->Value.Size,
		pField->Value.Data
	);
	testRequire((iHeadSize > 0) &&
		((size_t)iHeadSize < sizeof(RequestHead)),
		"HTTP Digest flow request fixture overflow");
	pServerRequest = testHttpServerRequestFixtureCreate(
		RequestHead,
		(xbytesview){ NULL, 0 },
		XHTTP_SERVER_REQUEST_NONE
	);
	xrtHttpRequestDestroy(pRequest);
	xrtHttpResponseDestroy(pResponse);

	/* 服务端只需要持久化 Secret，即可验证 challenge、nonce 和客户端证明。 */
	testRequire((xrtHttpServerRequestDigestAuth(
		pServerRequest,
		ServerOutput, sizeof(ServerOutput), &iSize, &ServerAuth
	) == XHTTP_NEXT_ITEM) &&
		testHttpDigestFlowText(ServerAuth.Username, "Mufasa") &&
		testHttpDigestFlowText(ServerAuth.Realm, "api@example.test") &&
		testHttpDigestFlowText(ServerAuth.Uri, "/private"),
		"HTTP Digest flow server credential decode failed");
	Verification = (xhttpdigestverification){
		0,
		&ServerAuth,
		&Challenge,
		{ Secret, xrtHttpDigestSize(ServerAuth.Algorithm) * 2u },
		xrtHttpServerRequestMethod(pServerRequest),
		xrtHttpServerRequestTarget(pServerRequest),
		{ NULL, 0 }
	};
	testRequire((xrtHttpDigestVerify(
		&Verification,
		(xbytesview){ NonceKey, sizeof(NonceKey) },
		XRT_BYTES_LITERAL("api@example.test"),
		INT64_C(1700000030),
		60,
		5,
		&iIssued
	) == XHTTP_DIGEST_VERIFY_VALID) &&
		(iIssued == INT64_C(1700000000)),
		"HTTP Digest flow server proof verification failed");
	Proof = (xhttpdigestproof){
		ServerAuth.Algorithm,
		ServerAuth.Qop,
		ServerAuth.NonceCount,
		{ Secret, xrtHttpDigestSize(ServerAuth.Algorithm) * 2u },
		ServerAuth.Nonce,
		ServerAuth.Cnonce,
		ServerAuth.Uri,
		{ NULL, 0 }
	};
	pReplay = xrtHttpDigestReplayCreate(&ReplayConfig);
	testRequire((pReplay != NULL) &&
		(xrtHttpDigestReplayCheck(
			pReplay,
			ServerAuth.Username,
			ServerAuth.Nonce,
			ServerAuth.Cnonce,
			ServerAuth.NonceCount,
			iIssued,
			INT64_C(1700000030)
		) == XHTTP_DIGEST_REPLAY_ACCEPTED) &&
		(xrtHttpDigestReplayCheck(
			pReplay,
			ServerAuth.Username,
			ServerAuth.Nonce,
			ServerAuth.Cnonce,
			ServerAuth.NonceCount,
			iIssued,
			INT64_C(1700000030)
		) == XHTTP_DIGEST_REPLAY_REPLAY),
		"HTTP Digest flow replay commit failed");
	testRequire(xrtHttpDigestRspAuth(
		&Proof, ResponseDigest, sizeof(ResponseDigest), &iSize
	), "HTTP Digest flow rspauth calculation failed");

	/* 服务端设置 Authentication-Info，客户端解析并验证 rspauth。 */
	Info = (xhttpdigestinfo){
		XHTTP_DIGEST_INFO_HAS_NEXT_NONCE |
		XHTTP_DIGEST_INFO_HAS_RESPONSE,
		Proof.Algorithm,
		Proof.Qop,
		Proof.NonceCount,
		XRT_STR_INIT("next-server-nonce"),
		{ ResponseDigest, iSize },
		Proof.Cnonce
	};
	pReply = xrtHttpReplyCreate(XHTTP_STATUS_OK);
	testRequire((pReply != NULL) &&
		xrtHttpReplySetDigestInfo(pReply, &Info),
		"HTTP Digest flow Authentication-Info setup failed");
	pField = xrtHttpReplyHeader(
		pReply, XRT_STR_LITERAL("Authentication-Info")
	);
	testRequire(pField != NULL,
		"HTTP Digest flow Authentication-Info field missing");
	Field = *pField;
	pResponse = testHttpResponseFixtureCreate(&Field, 1u);
	xrtHttpReplyDestroy(pReply);
	testRequire((xrtHttpResponseDigestInfo(
		pResponse,
		Proof.Algorithm,
		InfoOutput, sizeof(InfoOutput), &iSize, &ClientInfo
	) == XHTTP_NEXT_ITEM) &&
		((ClientInfo.Flags & XHTTP_DIGEST_INFO_HAS_RESPONSE) != 0) &&
		(ClientInfo.NonceCount == Proof.NonceCount) &&
		testHttpDigestFlowText(
			ClientInfo.NextNonce, "next-server-nonce"
		) && xrtHttpDigestEqual(
			ClientInfo.Response, Info.Response
		), "HTTP Digest flow client rspauth verification failed");

	xrtHttpResponseDestroy(pResponse);
	xrtHttpServerRequestDestroy(pServerRequest);
	xrtHttpDigestReplayDestroy(pReplay);
	xrtSecureZero(Secret, sizeof(Secret));
	xrtSecureZero(RequestDigest, sizeof(RequestDigest));
	xrtSecureZero(ResponseDigest, sizeof(ResponseDigest));
	puts("[PASS] HTTP Digest object flow");
	return 0;
}

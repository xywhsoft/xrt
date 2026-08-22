#include "http_client_response_fixture.h"



/* 验证客户端跨字段过滤、查询和解码 Digest challenge。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("WWW-Authenticate"),
			XRT_STR_INIT(
				"Basic realm=skip, Digest realm=\"api\", "
				"nonce=\"n\", algorithm=SHA-256, qop=\"auth\""
			)
		},
		{
			XRT_STR_INIT("Proxy-Authenticate"),
			XRT_STR_INIT(
				"Digest realm=\"proxy\", nonce=\"p\", qop=\"auth\""
			)
		}
	};
	xhttpresponse* pResponse = testHttpResponseFixtureCreate(
		Fields, sizeof(Fields) / sizeof(Fields[0])
	);
	xhttpauthcursor Cursor;
	xhttpauthcursor Saved;
	xhttpdigestchallenge Challenge;
	char Output[32];
	size_t iSize;

	xrtHttpAuthCursorInit(&Cursor);
	Saved = Cursor;
	testRequire((xrtHttpResponseDigestChallengeNext(
		pResponse, &Cursor, NULL, 0, &iSize, &Challenge
	) == XHTTP_NEXT_ITEM) && (iSize == 4u) &&
		(Challenge.Algorithm == XHTTP_DIGEST_ALGORITHM_SHA256) &&
		(memcmp(&Cursor, &Saved, sizeof(Cursor)) == 0),
		"HTTP response Digest challenge query mismatch");
	testRequire((xrtHttpResponseDigestChallengeNext(
		pResponse, &Cursor, Output, sizeof(Output), &iSize, &Challenge
	) == XHTTP_NEXT_ITEM) &&
		testHttpResponseFixtureText(Challenge.Realm, "api") &&
		testHttpResponseFixtureText(Challenge.Nonce, "n"),
		"HTTP response Digest challenge decode mismatch");
	xrtHttpAuthCursorInit(&Cursor);
	testRequire((xrtHttpResponseProxyDigestChallengeNext(
		pResponse, &Cursor, Output, sizeof(Output), &iSize, &Challenge
	) == XHTTP_NEXT_ITEM) &&
		testHttpResponseFixtureText(Challenge.Realm, "proxy"),
		"HTTP response proxy Digest challenge mismatch");
	xrtHttpResponseDestroy(pResponse);
	puts("[PASS] HTTP client response Digest challenge");
	return 0;
}

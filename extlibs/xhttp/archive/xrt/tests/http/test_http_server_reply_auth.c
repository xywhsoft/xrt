#include "../test.h"



/* 验证回复保留多条源站 challenge 并独立写入代理 challenge。 */
int main(void)
{
	xhttpreply* pReply = xrtHttpReplyCreate(XHTTP_STATUS_UNAUTHORIZED);
	const xhttpheaders* pHeaders;
	xhttpauthcursor Cursor;
	xhttpauth Auth;

	testRequire((pReply != NULL) && xrtHttpReplyAddChallenge(
		pReply,
		XRT_STR_LITERAL("Digest"),
		XRT_STR_LITERAL("realm=\"api\"")
	) && xrtHttpReplyAddChallenge(
		pReply,
		XRT_STR_LITERAL("Bearer"),
		XRT_STR_LITERAL("token")
	) && xrtHttpReplyAddProxyChallenge(
		pReply,
		XRT_STR_LITERAL("Basic"),
		XRT_STR_LITERAL("abc==")
	), "HTTP server reply authentication setup failed");
	pHeaders = xrtHttpReplyHeaders(pReply);
	xrtHttpAuthCursorInit(&Cursor);
	testRequire((xrtHttpFieldChallengeNext(
		xrtHttpHeadersData(pHeaders),
		xrtHttpHeadersCount(pHeaders),
		XRT_STR_LITERAL("WWW-Authenticate"),
		&Cursor,
		&Auth
	) == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(Auth.Scheme, XRT_STR_LITERAL("Digest")) &&
		(xrtHttpFieldChallengeNext(
			xrtHttpHeadersData(pHeaders),
			xrtHttpHeadersCount(pHeaders),
			XRT_STR_LITERAL("WWW-Authenticate"),
			&Cursor,
			&Auth
		) == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(Auth.Scheme, XRT_STR_LITERAL("Bearer")),
		"HTTP server reply challenge order mismatch");
	testRequire(!xrtHttpReplyAddChallenge(
		pReply,
		XRT_STR_LITERAL("Bad Scheme"),
		XRT_STR_LITERAL("token")
	) && (xrtHttpHeadersCountName(
		pHeaders,
		XRT_STR_LITERAL("WWW-Authenticate")
	) == 2u), "HTTP server invalid challenge changed Reply");
	xrtClearError();
	xrtHttpReplyDestroy(pReply);
	puts("[PASS] HTTP server reply authentication");
	return 0;
}

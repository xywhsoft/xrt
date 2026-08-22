#include "../test.h"



/* 按字节比较 challenge 的借用文本。 */
static bool testHttpServerBearerTextEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 验证 Bearer challenge 通过共享事务路径追加到源站和代理字段。 */
static void testHttpServerReplyBearerRoundTrip(void)
{
	xhttpbearerchallenge Source = {
		XHTTP_BEARER_HAS_REALM |
		XHTTP_BEARER_HAS_SCOPE |
		XHTTP_BEARER_HAS_ERROR |
		XHTTP_BEARER_HAS_ERROR_DESCRIPTION |
		XHTTP_BEARER_HAS_ERROR_URI,
		XRT_STR_INIT("api"),
		XRT_STR_INIT("read write"),
		XRT_STR_INIT("invalid_token"),
		XRT_STR_INIT("The access token expired"),
		XRT_STR_INIT("https://example.com/help")
	};
	xhttpbearerchallenge Proxy = {
		XHTTP_BEARER_HAS_REALM,
		XRT_STR_INIT("proxy"),
		{ NULL, 0 }, { NULL, 0 }, { NULL, 0 }, { NULL, 0 }
	};
	xhttpbearerchallenge Parsed;
	xhttpreply* pReply = xrtHttpReplyCreate(
		XHTTP_STATUS_UNAUTHORIZED
	);
	const xhttpfield* pField;
	char Output[128];
	size_t iSize;

	testRequire((pReply != NULL) &&
		xrtHttpReplyAddBearerChallenge(pReply, &Source) &&
		xrtHttpReplyAddProxyBearerChallenge(pReply, &Proxy),
		"HTTP server Bearer challenge setup failed");
	pField = xrtHttpReplyHeader(
		pReply, XRT_STR_LITERAL("WWW-Authenticate")
	);
	testRequire((pField != NULL) && xrtHttpBearerChallengeRead(
		pField->Value,
		Output,
		sizeof(Output),
		&iSize,
		&Parsed
	) && (Parsed.Flags == Source.Flags) &&
		testHttpServerBearerTextEqual(Parsed.Scope, Source.Scope) &&
		testHttpServerBearerTextEqual(
			Parsed.ErrorUri, Source.ErrorUri
		), "HTTP server Bearer source challenge mismatch");
	pField = xrtHttpReplyHeader(
		pReply, XRT_STR_LITERAL("Proxy-Authenticate")
	);
	testRequire((pField != NULL) && xrtHttpBearerChallengeRead(
		pField->Value,
		Output,
		sizeof(Output),
		&iSize,
		&Parsed
	) && (Parsed.Flags == XHTTP_BEARER_HAS_REALM) &&
		testHttpServerBearerTextEqual(Parsed.Realm, Proxy.Realm),
		"HTTP server Bearer proxy challenge mismatch");
	xrtHttpReplyDestroy(pReply);
}



/* 验证非法描述符和空参数集不会提交部分 Header。 */
static void testHttpServerReplyBearerAtomicFailure(void)
{
	xhttpbearerchallenge Empty = { 0 };
	xhttpreply* pReply = xrtHttpReplyCreate(
		XHTTP_STATUS_UNAUTHORIZED
	);

	testRequire(pReply != NULL,
		"HTTP server Bearer failure setup failed");
	testRequire(!xrtHttpReplyAddBearerChallenge(
		pReply, &Empty
	) && (xrtHttpReplyHeaderCount(pReply) == 0u),
		"HTTP server empty Bearer challenge changed Reply");
	xrtClearError();
	testRequire(!xrtHttpReplyAddProxyBearerChallenge(
		pReply, NULL
	) && (xrtHttpReplyHeaderCount(pReply) == 0u),
		"HTTP server null Bearer challenge changed Reply");
	xrtClearError();
	xrtHttpReplyDestroy(pReply);
}



int main(void)
{
	testHttpServerReplyBearerRoundTrip();
	testHttpServerReplyBearerAtomicFailure();
	puts("[PASS] HTTP server reply Bearer authentication");
	return 0;
}

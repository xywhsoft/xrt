#include "../test.h"



/* 验证 Basic challenge 统一转义 realm 并保留 UTF-8 声明。 */
int main(void)
{
	xhttpreply* pReply = xrtHttpReplyCreate(XHTTP_STATUS_UNAUTHORIZED);
	const xhttpfield* pField;

	testRequire((pReply != NULL) && xrtHttpReplyAddBasicChallenge(
		pReply,
		XRT_STR_LITERAL("a\"b\\c"),
		true
	) && xrtHttpReplyAddProxyBasicChallenge(
		pReply,
		XRT_STR_LITERAL("proxy"),
		false
	), "HTTP server Basic challenge setup failed");
	pField = xrtHttpReplyHeader(
		pReply,
		XRT_STR_LITERAL("WWW-Authenticate")
	);
	testRequire((pField != NULL) &&
		(pField->Value.Size == 38u) &&
		(memcmp(
			pField->Value.Data,
			"Basic realm=\"a\\\"b\\\\c\", charset=\"UTF-8\"",
			38u
		) == 0), "HTTP server Basic challenge mismatch");
	xrtHttpReplyDestroy(pReply);
	puts("[PASS] HTTP server reply Basic authentication");
	return 0;
}

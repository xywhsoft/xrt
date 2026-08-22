#include "../test.h"



/* 验证 Reply 原子设置并替换唯一 Digest 认证信息字段。 */
int main(void)
{
	xhttpdigestinfo Info = {
		XHTTP_DIGEST_INFO_HAS_NEXT_NONCE,
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XHTTP_DIGEST_QOP_NONE,
		0u,
		XRT_STR_INIT("first"),
		{ NULL, 0 },
		{ NULL, 0 }
	};
	xhttpreply* pReply = xrtHttpReplyCreate(XHTTP_STATUS_OK);
	const xhttpfield* pField;
	xhttpdigestinfo Parsed;
	char Output[32];
	size_t iSize;

	testRequire((pReply != NULL) &&
		xrtHttpReplySetDigestInfo(pReply, &Info),
		"HTTP server Digest info setup failed");
	Info.NextNonce = XRT_STR_LITERAL("second");
	testRequire(xrtHttpReplySetDigestInfo(pReply, &Info) &&
		xrtHttpReplySetProxyDigestInfo(pReply, &Info) &&
		(xrtHttpReplyHeaderCount(pReply) == 2u),
		"HTTP server Digest info replacement failed");
	pField = xrtHttpReplyHeader(
		pReply, XRT_STR_LITERAL("Authentication-Info")
	);
	testRequire((pField != NULL) && xrtHttpDigestInfoRead(
		pField->Value, XHTTP_DIGEST_ALGORITHM_SHA256,
		Output, sizeof(Output), &iSize, &Parsed
	) && (Parsed.NextNonce.Size == 6u) &&
		(memcmp(Parsed.NextNonce.Data, "second", 6u) == 0),
		"HTTP server Digest info field mismatch");
	xrtHttpReplyDestroy(pReply);
	puts("[PASS] HTTP server reply Digest info");
	return 0;
}

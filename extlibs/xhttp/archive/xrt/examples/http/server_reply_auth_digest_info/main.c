#include <stdio.h>

#include <xrt/http_server.h>



/* 构建带 nextnonce 的成功 Digest 回执。 */
int main(void)
{
	xhttpdigestinfo Info = {
		XHTTP_DIGEST_INFO_HAS_NEXT_NONCE,
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XHTTP_DIGEST_QOP_NONE,
		0u,
		XRT_STR_INIT("next-server-nonce"),
		{ NULL, 0 },
		{ NULL, 0 }
	};
	xhttpreply* pReply = xrtHttpReplyCreate(XHTTP_STATUS_OK);
	const xhttpfield* pField;
	int iResult = 1;

	if ( (pReply == NULL) ||
		!xrtHttpReplySetDigestInfo(pReply, &Info) ) {
		goto Cleanup;
	}
	pField = xrtHttpReplyHeader(
		pReply,
		XRT_STR_LITERAL("Authentication-Info")
	);
	if ( pField == NULL ) {
		goto Cleanup;
	}
	printf("Authentication-Info: %.*s\n", (int)pField->Value.Size, pField->Value.Data);
	iResult = 0;

Cleanup:
	xrtHttpReplyDestroy(pReply);
	return iResult;
}

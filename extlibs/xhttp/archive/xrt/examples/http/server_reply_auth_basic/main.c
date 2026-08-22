#include <xrt.h>



/* 构建带 Basic challenge 的 401 回复。 */
int main(void)
{
	xhttpreply* pReply = xrtHttpReplyCreate(XHTTP_STATUS_UNAUTHORIZED);
	bool bResult = (pReply != NULL) && xrtHttpReplyAddBasicChallenge(
		pReply,
		XRT_STR_LITERAL("private"),
		true
	);

	xrtHttpReplyDestroy(pReply);
	return bResult ? 0 : 1;
}

#include <xrt.h>



/* 构建包含多个认证方案的 401 回复。 */
int main(void)
{
	xhttpreply* pReply = xrtHttpReplyCreate(XHTTP_STATUS_UNAUTHORIZED);
	bool bResult = (pReply != NULL) && xrtHttpReplyAddChallenge(
		pReply,
		XRT_STR_LITERAL("Bearer"),
		XRT_STR_LITERAL("realm=\"api\"")
	);

	xrtHttpReplyDestroy(pReply);
	return bResult ? 0 : 1;
}

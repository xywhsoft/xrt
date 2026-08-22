#include <xrt.h>



/* 构建带结构化 Bearer challenge 的 401 回复。 */
int main(void)
{
	xhttpbearerchallenge Challenge = {
		XHTTP_BEARER_HAS_REALM |
		XHTTP_BEARER_HAS_SCOPE,
		XRT_STR_INIT("private"),
		XRT_STR_INIT("profile read"),
		{ NULL, 0 }, { NULL, 0 }, { NULL, 0 }
	};
	xhttpreply* pReply = xrtHttpReplyCreate(
		XHTTP_STATUS_UNAUTHORIZED
	);
	bool bResult = (pReply != NULL) &&
		xrtHttpReplyAddBearerChallenge(pReply, &Challenge);

	xrtHttpReplyDestroy(pReply);
	return bResult ? 0 : 1;
}

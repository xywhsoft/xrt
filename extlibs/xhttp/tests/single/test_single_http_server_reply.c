#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_SERVER_REPLY
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头 Reply 的按需字段和正文入口。 */
int main(void)
{
	xhttpreply* pReply = xrtHttpReplyCreate(
		XHTTP_STATUS_OK
	);
	bool bResult = (pReply != NULL) &&
		xrtHttpReplySetBytes(
			pReply,
			(xbytesview){ (cbytes)"ok", 2 },
			XRT_STR_LITERAL("text/plain")
		) &&
		(xrtHttpReplyHeaderCount(pReply) == 1) &&
		(xrtHttpBodyLength(
			xrtHttpReplyBody(pReply)
		) == 2);

	xrtHttpReplyDestroy(pReply);
	return bResult ? 0 : 1;
}


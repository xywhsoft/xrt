#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件保留纯静态响应到 Reply 的桥接契约。 */
int main(void)
{
	xhttpstaticresponse Response;
	xhttpbody* pBody;
	xhttpreply* pReply;
	bool bPass;

	memset(&Response, 0, sizeof(Response));
	Response.Status = XHTTP_STATUS_OK;
	Response.SendBody = true;
	Response.BodyLength = 3;
	Response.Fields[0].Name =
		XRT_STR_LITERAL("Content-Type");
	Response.Fields[0].Value =
		XRT_STR_LITERAL("text/plain");
	Response.Fields[1].Name =
		XRT_STR_LITERAL("Content-Length");
	Response.Fields[1].Value =
		XRT_STR_LITERAL("3");
	Response.FieldCount = 2;
	pBody = xrtHttpBodyCopy(
		XRT_BYTES_LITERAL("xrt")
	);
	if ( pBody == NULL ) {
		return 1;
	}
	pReply = xrtHttpReplyFromStatic(
		&Response,
		pBody
	);
	xrtHttpBodyDestroy(pBody);
	bPass = (pReply != NULL) &&
		(xrtHttpReplyStatus(pReply) ==
		 XHTTP_STATUS_OK) &&
		(xrtHttpBodyLength(
			xrtHttpReplyBody(pReply)
		 ) == 3);
	xrtHttpReplyDestroy(pReply);
	return bPass ? 0 : 1;
}

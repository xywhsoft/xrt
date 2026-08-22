#include <stdio.h>
#include <xhttp.h>



/* 构建一条无需网络对象的固定 JSON 服务端响应。 */
int main(void)
{
	xhttpreply* pReply = xrtHttpReplyCreate(
		XHTTP_STATUS_OK
	);
	const xhttpfield* pType;

	if ( (pReply == NULL) ||
		!xrtHttpReplySetBytes(
			pReply,
			(xbytesview){
				(cbytes)"{\"code\":200,\"msg\":\"OK\"}",
				23
			},
			XRT_STR_LITERAL(
				"application/json; charset=utf-8"
			)
		) ) {
		xrtHttpReplyDestroy(pReply);
		return 1;
	}
	pType = xrtHttpReplyHeader(
		pReply, XRT_STR_LITERAL("Content-Type")
	);
	printf(
		"%u %.*s, %llu bytes\n",
		(unsigned)xrtHttpReplyStatus(pReply),
		(int)pType->Value.Size,
		pType->Value.Data,
		(unsigned long long)xrtHttpBodyLength(
			xrtHttpReplyBody(pReply)
		)
	);
	xrtHttpReplyDestroy(pReply);
	return 0;
}


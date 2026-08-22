#include <stdio.h>

#include <xrt/http_compress.h>



/* 根据请求的 Accept-Encoding 为固定 Reply 选择压缩表示。 */
static xhttpreply* exampleHttpServerCompress(
	const xhttpserverrequest* pRequest
)
{
	static const char Body[] =
		"{\"code\":200,\"message\":\"compression example\"}";
	xhttpreplycompressconfig Config;
	xhttpreply* pReply = xrtHttpReplyCreate(XHTTP_STATUS_OK);
	xhttpreply* pOutput = NULL;
	xhttpreplycompressstatus Status;

	if ( (pReply == NULL) || !xrtHttpReplySetBytes(
		pReply,
		XRT_BYTES_LITERAL(Body),
		XRT_STR_LITERAL("application/json; charset=utf-8")
	) ) {
		xrtHttpReplyDestroy(pReply);
		return NULL;
	}
	xrtHttpReplyCompressConfigInit(&Config);
	Config.MinimumSize = 0;
	Status = xrtHttpServerReplyCompress(
		pRequest,
		pReply,
		&Config,
		&pOutput
	);
	xrtHttpReplyDestroy(pReply);
	if ( (Status != XHTTP_REPLY_COMPRESS_IDENTITY) &&
		(Status != XHTTP_REPLY_COMPRESS_APPLIED) ) {
		xrtHttpReplyDestroy(pOutput);
		return NULL;
	}
	return pOutput;
}



/* Request 由真实 HTTP Server 请求回调提供。 */
int main(void)
{
	(void)exampleHttpServerCompress;
	puts("server compression helper ready");
	return 0;
}

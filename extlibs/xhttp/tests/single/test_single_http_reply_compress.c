#define XHTTP_MODULE_HTTP_REPLY_COMPRESS
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件能够完成固定 JSON Reply 的 gzip 变换。 */
int main(void)
{
	static uint8 Input[4096];
	xhttpacceptencoding Accept;
	xhttpreply* pReply;
	xhttpreply* pOutput = NULL;
	xhttpreplycompressstatus Status;

	memset(Input, 'j', sizeof(Input));
	xrtHttpAcceptEncodingInit(&Accept);
	pReply = xrtHttpReplyCreate(XHTTP_STATUS_OK);
	if ( !xrtHttpAcceptEncodingAdd(
		&Accept, XRT_STR_LITERAL("gzip")
	) || (pReply == NULL) ||
		!xrtHttpReplySetBytes(
			pReply,
			(xbytesview){ Input, sizeof(Input) },
			XRT_STR_LITERAL("application/json")
		) ) {
		xrtHttpReplyDestroy(pReply);
		return 1;
	}
	Status = xrtHttpReplyCompress(
		&Accept,
		XRT_STR_LITERAL("GET"),
		pReply,
		NULL,
		&pOutput
	);
	if ( (Status != XHTTP_REPLY_COMPRESS_APPLIED) ||
		(pOutput == NULL) ) {
		xrtHttpReplyDestroy(pOutput);
		xrtHttpReplyDestroy(pReply);
		return 1;
	}
	printf("encoding=%.*s\n",
		(int)xrtHttpReplyHeader(
			pOutput,
			XRT_STR_LITERAL("Content-Encoding")
		)->Value.Size,
		xrtHttpReplyHeader(
			pOutput,
			XRT_STR_LITERAL("Content-Encoding")
		)->Value.Data);
	xrtHttpReplyDestroy(pOutput);
	xrtHttpReplyDestroy(pReply);
	return 0;
}

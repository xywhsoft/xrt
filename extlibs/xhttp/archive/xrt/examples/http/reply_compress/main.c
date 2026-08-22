#include <stdio.h>
#include <string.h>

#include <xrt.h>



/* 构建常见 JSON Reply，并按请求能力选择 gzip 或 identity。 */
int main(void)
{
	static const char Json[] =
		"{\"code\":200,\"message\":\"OK\","
		"\"data\":\"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\"}";
	xhttpacceptencoding Accept;
	xhttpreplycompressconfig Config;
	xhttpreply* pReply;
	xhttpreply* pOutput = NULL;
	xhttpreplycompressstatus Status;

	xrtHttpAcceptEncodingInit(&Accept);
	if ( !xrtHttpAcceptEncodingAdd(
		&Accept,
		XRT_STR_LITERAL("gzip, identity;q=0.5")
	) ) {
		return 1;
	}
	pReply = xrtHttpReplyCreate(XHTTP_STATUS_OK);
	if ( (pReply == NULL) ||
		!xrtHttpReplySetBytes(
			pReply,
			(xbytesview){
				(cbytes)Json,
				sizeof(Json) - 1u
			},
			XRT_STR_LITERAL(
				"application/json; charset=utf-8"
			)
		) ) {
		xrtHttpReplyDestroy(pReply);
		return 1;
	}
	xrtHttpReplyCompressConfigInit(&Config);
	Config.MinimumSize = 0;
	Status = xrtHttpReplyCompress(
		&Accept,
		XRT_STR_LITERAL("GET"),
		pReply,
		&Config,
		&pOutput
	);
	if ( Status == XHTTP_REPLY_COMPRESS_ERROR ) {
		xrtHttpReplyDestroy(pReply);
		return 1;
	}
	if ( Status == XHTTP_REPLY_COMPRESS_NOT_ACCEPTABLE ) {
		printf("406 Not Acceptable\n");
	} else if ( Status == XHTTP_REPLY_COMPRESS_SKIP ) {
		printf("use original Reply\n");
	} else {
		const xhttpfield* pEncoding = xrtHttpReplyHeader(
			pOutput,
			XRT_STR_LITERAL("Content-Encoding")
		);

		printf("encoding=%.*s\n",
			pEncoding != NULL ?
				(int)pEncoding->Value.Size : 8,
			pEncoding != NULL ?
				pEncoding->Value.Data : "identity");
	}
	xrtHttpReplyDestroy(pOutput);
	xrtHttpReplyDestroy(pReply);
	return 0;
}

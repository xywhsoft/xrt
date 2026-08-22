#define XHTTP_IMPLEMENTATION
#define XHTTP_MODULE_HTTP_SSE_SERVER
#include "../../single/xhttp.h"



/* 验证单头文件发布 SSE Reply 与默认事件发送能力。 */
int main(void)
{
	xhttpbodystream* pStream = NULL;
	xhttpreply* pReply = xrtHttpSseReplyCreate(NULL, &pStream);
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;
	bool bPass;

	if ( (pReply == NULL) || (pStream == NULL) ||
		(xrtHttpSseSend(
			pStream, XRT_STR_LITERAL("single")
		) != XHTTP_BODY_STREAM_OK) ) {
		xrtHttpBodyStreamDestroy(pStream);
		xrtHttpReplyDestroy(pReply);
		return 1;
	}
	xrtHttpBodyStreamDestroy(pStream);
	pReader = xrtHttpBodyOpen(xrtHttpReplyBody(pReply));
	Status = pReader != NULL ?
		xrtHttpBodyNext(pReader, 32, &Chunk) : XHTTP_BODY_ERROR;
	bPass = (Status == XHTTP_BODY_DATA) &&
		(Chunk.Size == 14u) &&
		(memcmp(Chunk.Data, "data: single\n\n", 14u) == 0);
	if ( Status == XHTTP_BODY_DATA ) {
		xrtHttpBodyChunkRelease(&Chunk);
	}
	if ( bPass ) {
		bPass = xrtHttpBodyNext(
			pReader, 32, &Chunk
		) == XHTTP_BODY_EOF;
	}
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpReplyDestroy(pReply);
	return bPass ? 0 : 1;
}

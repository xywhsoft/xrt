#define XRT_IMPLEMENTATION
#define XRT_MODULE_HTTP_BODY_STREAM
#include "../../single/xrt.h"



/* 验证单头文件执行 Body Stream 的写入、异步读取和 EOF 路径。 */
int main(void)
{
	xhttpbodystream* pStream = NULL;
	xhttpbody* pBody = xrtHttpBodyStreamCreate(
		NULL, &pStream
	);
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	bool bPass = false;

	if ( (pBody == NULL) || (pStream == NULL) ) {
		return 1;
	}
	pReader = xrtHttpBodyOpen(pBody);
	if ( (pReader != NULL) &&
		(xrtHttpBodyStreamWrite(
			pStream,
			XRT_BYTES_LITERAL("stream")
		 ) == XHTTP_BODY_STREAM_OK) &&
		xrtHttpBodyStreamClose(pStream) &&
		(xrtHttpBodyNext(
			pReader, 8, &Chunk
		 ) == XHTTP_BODY_DATA) ) {
		bPass = (Chunk.Size == 6) &&
			(memcmp(Chunk.Data, "stream", 6) == 0);
		xrtHttpBodyChunkRelease(&Chunk);
		bPass = bPass &&
			(xrtHttpBodyNext(
				pReader, 8, &Chunk
			 ) == XHTTP_BODY_EOF);
	}
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyDestroy(pBody);
	xrtHttpBodyStreamDestroy(pStream);
	return bPass ? 0 : 1;
}

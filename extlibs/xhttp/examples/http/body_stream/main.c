#include <stdio.h>
#include <xhttp.h>



/* 读取完整异步正文；AGAIN 只通过公开 Future 契约继续。 */
static bool exampleReadBody(xhttpbody* pBody)
{
	xhttpbodyreader* pReader = xrtHttpBodyOpen(pBody);
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;

	if ( pReader == NULL ) {
		return false;
	}
	for ( ;; ) {
		Status = xrtHttpBodyNext(pReader, 4, &Chunk);
		if ( Status == XHTTP_BODY_DATA ) {
			(void)fwrite(Chunk.Data, 1, Chunk.Size, stdout);
			xrtHttpBodyChunkRelease(&Chunk);
			continue;
		}
		if ( Status == XHTTP_BODY_AGAIN ) {
			xfuture* pReady = xrtHttpBodyReaderWait(pReader);

			if ( (pReady == NULL) ||
				(xrtFutureWait(pReady) != XWAIT_OK) ||
				(xrtFutureState(pReady) != XFUTURE_RESOLVED) ) {
				xrtFutureDestroy(pReady);
				xrtHttpBodyReaderDestroy(pReader);
				return false;
			}
			xrtFutureDestroy(pReady);
			continue;
		}
		break;
	}
	xrtHttpBodyReaderDestroy(pReader);
	return Status == XHTTP_BODY_EOF;
}



/* 创建有界生产流，写入两段数据并由最后一个生产端发布 EOF。 */
int main(void)
{
	xhttpbodystreamconfig Config;
	xhttpbodystream* pStream = NULL;
	xhttpbody* pBody;
	bool bResult;

	xrtHttpBodyStreamConfigInit(&Config);
	Config.MaxBytes = 1024;
	Config.MaxChunks = 16;
	pBody = xrtHttpBodyStreamCreate(&Config, &pStream);
	if ( (pBody == NULL) ||
		(xrtHttpBodyStreamWrite(
			pStream, XRT_BYTES_LITERAL("first chunk\n")
		) != XHTTP_BODY_STREAM_OK) ||
		(xrtHttpBodyStreamWrite(
			pStream, XRT_BYTES_LITERAL("second chunk\n")
		) != XHTTP_BODY_STREAM_OK) ) {
		xrtHttpBodyStreamDestroy(pStream);
		xrtHttpBodyDestroy(pBody);
		return 1;
	}
	xrtHttpBodyStreamDestroy(pStream);
	bResult = exampleReadBody(pBody);
	xrtHttpBodyDestroy(pBody);
	return bResult ? 0 : 1;
}

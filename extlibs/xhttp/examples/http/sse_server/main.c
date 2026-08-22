#include <stdio.h>
#include <string.h>
#include <xhttp.h>



/* 读取并打印 Reply 的完整异步正文。 */
static bool exampleReadReply(xhttpreply* pReply)
{
	xhttpbodyreader* pReader = xrtHttpBodyOpen(
		xrtHttpReplyBody(pReply)
	);
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;

	if ( pReader == NULL ) {
		return false;
	}
	for ( ;; ) {
		Status = xrtHttpBodyNext(pReader, 8, &Chunk);
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



/* 创建 SSE Reply，发送结构化事件、心跳和预封装字节。 */
int main(void)
{
	xhttpbodystreamconfig Config;
	xhttpbodystream* pStream = NULL;
	xhttpreply* pReply;
	xhttpsseevent Event;
	bool bResult;

	xrtHttpBodyStreamConfigInit(&Config);
	Config.MaxBytes = 4096;
	Config.MaxChunks = 32;
	pReply = xrtHttpSseReplyCreate(&Config, &pStream);
	if ( (pReply == NULL) || (pStream == NULL) ) {
		xrtHttpBodyStreamDestroy(pStream);
		xrtHttpReplyDestroy(pReply);
		return 1;
	}

	memset(&Event, 0, sizeof(Event));
	Event.Type = XRT_STR_LITERAL("progress");
	Event.Data = XRT_STR_LITERAL("{\"percent\":75}");
	Event.Id = XRT_STR_LITERAL("job-42:3");
	Event.Retry = 2000;
	Event.Flags = XHTTP_SSE_EVENT_TYPE |
		XHTTP_SSE_EVENT_DATA |
		XHTTP_SSE_EVENT_ID |
		XHTTP_SSE_EVENT_RETRY;
	if ( (xrtHttpSseSendEvent(
			pStream, &Event
		) != XHTTP_BODY_STREAM_OK) ||
		(xrtHttpSseSendComment(
			pStream, XRT_STR_LITERAL("heartbeat")
		) != XHTTP_BODY_STREAM_OK) ||
		(xrtHttpBodyStreamWrite(
			pStream, XRT_BYTES_LITERAL("data: ready\n\n")
		) != XHTTP_BODY_STREAM_OK) ) {
		xrtHttpBodyStreamDestroy(pStream);
		xrtHttpReplyDestroy(pReply);
		return 1;
	}

	xrtHttpBodyStreamDestroy(pStream);
	bResult = exampleReadReply(pReply);
	xrtHttpReplyDestroy(pReply);
	return bResult ? 0 : 1;
}

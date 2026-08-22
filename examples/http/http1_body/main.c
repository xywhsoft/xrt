#include <stdio.h>

#include <xrt.h>



/* 解析响应分帧并以零拷贝视图逐段读取 chunked 正文。 */
static bool readResponseBody(void)
{
	static const uint8 Message[] =
		"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
		"7\r\nchunked\r\n5\r\n body\r\n0\r\nDigest: ok\r\n\r\n";
	xhttpfield Fields[8];
	xhttpfield Trailers[4];
	xhttp1bodylimits Limits;
	xhttp1bodyplan Plan;
	xhttp1head Head;
	xhttp1body Body;
	size_t iOffset;

	xrtHttp1HeadInit(&Head, Fields, 8);
	if ( xrtHttp1ResponseParse(
		(xbytesview){ Message, sizeof(Message) - 1u },
		&Head, NULL, NULL
	) != XHTTP1_READY ) {
		return false;
	}
	if ( !xrtHttp1ResponseBodyPlan(
		&Head, XRT_STR_LITERAL("GET"), &Plan
	) ) {
		return false;
	}
	xrtHttp1BodyLimitsInit(&Limits);
	Limits.MaxBody = 1024;
	if ( !xrtHttp1BodyInit(
		&Body, &Plan, Trailers, 4, &Limits
	) ) {
		return false;
	}
	iOffset = Head.Bytes;
	while ( !xrtHttp1BodyDone(&Body) ) {
		xbytesview Data;
		size_t iConsumed;
		xhttp1bodystatus Status = xrtHttp1BodyRead(
			&Body,
			(xbytesview){ Message + iOffset, sizeof(Message) - 1u - iOffset },
			false, &iConsumed, &Data, NULL
		);

		if ( Status == XHTTP1_BODY_DATA ) {
			fwrite(Data.Data, 1, Data.Size, stdout);
		} else if ( Status != XHTTP1_BODY_DONE ) {
			return false;
		}
		iOffset += iConsumed;
	}
	putchar('\n');
	return true;
}



/* 直接生成完整 chunk，也可只生成 size 行后配合向量发送正文。 */
static bool writeResponseBody(void)
{
	static const xhttpfield Trailers[] = {
		{ XRT_STR_INIT("Digest"), XRT_STR_INIT("sha-256=:demo:") }
	};
	uint8 Output[128];
	size_t iChunk;
	size_t iEnd;

	if ( !xrtHttp1ChunkWrite(
		(xbytesview){ (cbytes)"hello", 5 },
		Output, sizeof(Output), &iChunk
	) || !xrtHttp1ChunkEndWrite(
		Trailers, 1, Output + iChunk,
		sizeof(Output) - iChunk, &iEnd
	) ) {
		return false;
	}
	fwrite(Output, 1, iChunk + iEnd, stdout);
	return true;
}



/* 展示 HTTP/1 Body Plan、流式读取和分块写出。 */
int main(void)
{
	if ( !readResponseBody() || !writeResponseBody() ) {
		return 1;
	}
	return 0;
}

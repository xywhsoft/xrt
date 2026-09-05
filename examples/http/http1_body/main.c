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



/*
 * 范例：http/http1_body —— chunked 正文：Plan 流式读 + 分块写出
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHttp1ResponseBodyPlan   由头推导正文分帧方式（chunked/定长）
 *   xrtHttp1BodyLimitsInit     上限防御（防超大正文）
 *   xrtHttp1BodyInit/Read/Done 流式逐段读取（含 trailer 收集）
 *   xrtHttp1ChunkWrite / ChunkEndWrite   生成 chunk 与终止块
 * 模块宏：XRT_MODULE_HTTP1
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c ${BS}
 *       examples/http/http1_body/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   chunked body
 *   5
 *   hello
 *   0
 *
 * Plan 的意义：先由头确定"怎么读"（chunked/CL/无正文），
 *   再按 Plan 初始化读取器——读侧与写侧共用同一分帧状态机，
 *   响应方法参与语义（HEAD/204 无正文）。
 * 写侧两板斧：ChunkWrite 生成"size 行 + 数据"，ChunkEndWrite
 *   生成"0 终止块 + trailer"；大正文可边生成边向量发送。
 */


/* 展示 HTTP/1 Body Plan、流式读取和分块写出。 */
int main(void)
{
	if ( !readResponseBody() || !writeResponseBody() ) {
		return 1;
	}
	return 0;
}

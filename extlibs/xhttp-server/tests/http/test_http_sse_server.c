#include "../test.h"



/* 分片读取 SSE Reply 的异步正文并复制到连续测试输出。 */
static size_t testHttpSseServerRead(
	xhttpbody* pBody,
	void* pOutput,
	size_t iCapacity
)
{
	xhttpbodyreader* pReader = xrtHttpBodyOpen(pBody);
	xhttpbodychunk Chunk;
	bytes pBytes = (bytes)pOutput;
	size_t iOutput = 0;

	testRequire(pReader != NULL,
		"SSE server Body open failed");
	for ( ;; ) {
		xhttpbodystatus Status = xrtHttpBodyNext(
			pReader, 3, &Chunk
		);

		if ( Status == XHTTP_BODY_DATA ) {
			testRequire(
				(iOutput <= iCapacity) &&
				(Chunk.Size <= (iCapacity - iOutput)),
				"SSE server test output overflow"
			);
			memcpy(pBytes + iOutput, Chunk.Data, Chunk.Size);
			iOutput += Chunk.Size;
			xrtHttpBodyChunkRelease(&Chunk);
			continue;
		}
		if ( Status == XHTTP_BODY_AGAIN ) {
			xfuture* pReady = xrtHttpBodyReaderWait(pReader);

			testRequire(
				(pReady != NULL) &&
				(xrtFutureWait(pReady) == XWAIT_OK) &&
				(xrtFutureState(pReady) == XFUTURE_RESOLVED),
				"SSE server Body wait failed"
			);
			xrtFutureDestroy(pReady);
			continue;
		}
		testRequire(Status == XHTTP_BODY_EOF,
			"SSE server Body read failed");
		break;
	}
	xrtHttpBodyReaderDestroy(pReader);
	return iOutput;
}



/* 验证 Reply 组合、三种发送入口、原始逃生路径和规范线路字节。 */
static void testHttpSseServerFlow(void)
{
	static const char Expected[] =
		"data: simple\n\n"
		"id: 7\n"
		"event: update\n"
		"retry: 1500\n"
		"data: one\n"
		"data: two\n"
		"\n"
		": ping\n"
		": raw\n";
	xhttpbodystreamconfig Config = { 256, 8 };
	xhttpbodystream* pStream = NULL;
	xhttpreply* pReply = xrtHttpSseReplyCreate(
		&Config, &pStream
	);
	xhttpbody* pBody;
	const xhttpfield* pType;
	xhttpsseevent Event;
	char Output[256];
	size_t iOutput;

	testRequire((pReply != NULL) && (pStream != NULL) &&
		(xrtHttpReplyStatus(pReply) == 200),
		"SSE server Reply create failed");
	pType = xrtHttpReplyHeader(
		pReply, XRT_STR_LITERAL("Content-Type")
	);
	pBody = xrtHttpReplyBody(pReply);
	testRequire(
		(pType != NULL) &&
		(pType->Value.Size == sizeof(XHTTP_SSE_MEDIA_TYPE) - 1u) &&
		(memcmp(
			pType->Value.Data,
			XHTTP_SSE_MEDIA_TYPE,
			pType->Value.Size
		) == 0) &&
		(pBody != NULL) &&
		(xrtHttpBodyLength(pBody) == XHTTP_BODY_UNKNOWN) &&
		!xrtHttpBodyReplayable(pBody),
		"SSE server Reply contract mismatch"
	);
	testRequire(
		xrtHttpSseSend(
			pStream, XRT_STR_LITERAL("simple")
		) == XHTTP_BODY_STREAM_OK,
		"SSE server simple event failed"
	);
	memset(&Event, 0, sizeof(Event));
	Event.Id = XRT_STR_LITERAL("7");
	Event.Type = XRT_STR_LITERAL("update");
	Event.Data = XRT_STR_LITERAL("one\ntwo");
	Event.Retry = 1500;
	Event.Flags = XHTTP_SSE_EVENT_ID |
		XHTTP_SSE_EVENT_TYPE |
		XHTTP_SSE_EVENT_DATA |
		XHTTP_SSE_EVENT_RETRY;
	testRequire(
		xrtHttpSseSendEvent(
			pStream, &Event
		) == XHTTP_BODY_STREAM_OK,
		"SSE server complete event failed"
	);
	testRequire(
		xrtHttpSseSendComment(
			pStream, XRT_STR_LITERAL("ping")
		) == XHTTP_BODY_STREAM_OK,
		"SSE server comment failed"
	);
	testRequire(
		xrtHttpBodyStreamWrite(
			pStream, XRT_BYTES_LITERAL(": raw\n")
		) == XHTTP_BODY_STREAM_OK,
		"SSE server raw escape path failed"
	);
	xrtHttpBodyStreamDestroy(pStream);
	pStream = NULL;
	iOutput = testHttpSseServerRead(
		pBody, Output, sizeof(Output)
	);
	testRequire(
		(iOutput == sizeof(Expected) - 1u) &&
		(memcmp(Output, Expected, iOutput) == 0),
		"SSE server wire output mismatch"
	);
	xrtHttpReplyDestroy(pReply);
}



/* 验证无效事件和超预算事件不会向 Stream 提交部分节点。 */
static void testHttpSseServerFailure(void)
{
	xhttpbodystreamconfig Config = { 8, 1 };
	xhttpbodystream* pStream = NULL;
	xhttpreply* pReply = xrtHttpSseReplyCreate(
		&Config, &pStream
	);
	xhttpbodystreaminfo Info;
	xhttpsseevent Event;

	testRequire((pReply != NULL) && (pStream != NULL),
		"SSE server failure setup failed");
	memset(&Event, 0, sizeof(Event));
	Event.Type = XRT_STR_LITERAL("bad\ntype");
	Event.Flags = XHTTP_SSE_EVENT_TYPE;
	testRequire(
		(xrtHttpSseSendEvent(
			pStream, &Event
		) == XHTTP_BODY_STREAM_ERROR) &&
		xrtHttpBodyStreamInfo(pStream, &Info) &&
		(Info.PendingBytes == 0) &&
		(Info.PendingChunks == 0),
		"SSE server invalid event changed Stream"
	);
	xrtClearError();
	testRequire(
		(xrtHttpSseSend(
			pStream, XRT_STR_LITERAL("too large")
		) == XHTTP_BODY_STREAM_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		xrtHttpBodyStreamInfo(pStream, &Info) &&
		(Info.PendingBytes == 0) &&
		(Info.PendingChunks == 0),
		"SSE server oversized event changed Stream"
	);
	xrtClearError();
	xrtHttpBodyStreamDestroy(pStream);
	xrtHttpReplyDestroy(pReply);

	testRequire(
		(xrtHttpSseReplyCreate(NULL, NULL) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SSE server accepted null Stream output"
	);
	xrtClearError();
}



/* 验证 Reply 配置和生产端输出的固定值内存契约。 */
static void testHttpSseServerMemoryContracts(void)
{
	union test_http_sse_server_overlap {
		xhttpbodystreamconfig Config;
		xhttpbodystream* Stream;
		uint8 Bytes[sizeof(xhttpbodystreamconfig)];
	} Overlap;
	uint8 ConfigStorage[sizeof(xhttpbodystreamconfig) + 2u];
	uint8 StreamStorage[sizeof(xhttpbodystream*) + 2u];
	xhttpbodystreamconfig Config = { 64, 2 };
	xhttpbodystream* pStream;
	xhttpreply* pReply;

	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	memcpy(ConfigStorage + 1u, &Config, sizeof(Config));
	memset(StreamStorage, 0xA5, sizeof(StreamStorage));
	pReply = xrtHttpSseReplyCreate(
		(const xhttpbodystreamconfig*)(const void*)(ConfigStorage + 1u),
		(xhttpbodystream**)(void*)(StreamStorage + 1u)
	);
	memcpy(&pStream, StreamStorage + 1u, sizeof(pStream));
	testRequire(
		(pReply != NULL) && (pStream != NULL) &&
		(ConfigStorage[0] == 0xA5) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == 0xA5) &&
		(StreamStorage[0] == 0xA5) &&
		(StreamStorage[sizeof(StreamStorage) - 1u] == 0xA5),
		"SSE server rejected unaligned config or output storage"
	);
	memset(ConfigStorage + 1u, 0, sizeof(Config));
	testRequire(
		xrtHttpSseSend(
			pStream, XRT_STR_LITERAL("snapshot")
		) == XHTTP_BODY_STREAM_OK,
		"SSE server retained caller config storage"
	);
	xrtHttpBodyStreamDestroy(pStream);
	xrtHttpReplyDestroy(pReply);

	testRequire(
		xrtHttpSseReplyCreate(
			NULL,
			(xhttpbodystream**)(uintptr_t)(UINTPTR_MAX - 1u)
		) == NULL &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SSE server accepted a wrapping Stream output"
	);
	xrtClearError();
	pStream = (xhttpbodystream*)(uintptr_t)1u;
	testRequire(
		xrtHttpSseReplyCreate(
			(const xhttpbodystreamconfig*)(uintptr_t)(UINTPTR_MAX - 1u),
			&pStream
		) == NULL &&
		(pStream == (xhttpbodystream*)(uintptr_t)1u) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SSE server accepted a wrapping config or modified output"
	);
	xrtClearError();
	Overlap.Config = Config;
	testRequire(
		xrtHttpSseReplyCreate(
			&Overlap.Config,
			(xhttpbodystream**)(void*)Overlap.Bytes
		) == NULL &&
		(memcmp(&Overlap.Config, &Config, sizeof(Config)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SSE server accepted overlapping config and output"
	);
	xrtClearError();
}



/* 验证 SSE 薄层原样传播硬背压、可写代际和正常关闭。 */
static void testHttpSseServerBackpressure(void)
{
	xhttpbodystreamconfig Config = { 64, 1 };
	xhttpbodystream* pStream = NULL;
	xhttpreply* pReply = xrtHttpSseReplyCreate(
		&Config, &pStream
	);
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xfuture* pWritable;
	xerror* pMarker;

	testRequire((pReply != NULL) && (pStream != NULL),
		"SSE server backpressure setup failed");
	pReader = xrtHttpBodyOpen(xrtHttpReplyBody(pReply));
	testRequire(pReader != NULL,
		"SSE server backpressure Body open failed");
	testRequire(
		xrtHttpSseSend(
			pStream, XRT_STR_LITERAL("first")
		) == XHTTP_BODY_STREAM_OK,
		"SSE server first backpressure event failed"
	);
	pMarker = xrtErrorCreate(
		XERR_STATE,
		"test.http.sse.server",
		1,
		"backpressure marker"
	);
	testRequire(pMarker != NULL,
		"SSE server marker allocation failed");
	xrtSetError(pMarker);
	testRequire(
		(xrtHttpSseSendComment(
			pStream, XRT_STR_LITERAL("wait")
		) == XHTTP_BODY_STREAM_AGAIN) &&
		(xrtGetError() == pMarker),
		"SSE server AGAIN changed the old error"
	);
	pWritable = xrtHttpBodyStreamWaitWritable(pStream);
	testRequire(
		(pWritable != NULL) &&
		(xrtFutureState(pWritable) == XFUTURE_PENDING),
		"SSE server writable Future was not pending"
	);
	testRequire(
		(xrtHttpBodyNext(pReader, 64, &Chunk) == XHTTP_BODY_DATA) &&
		(Chunk.Size == sizeof("data: first\n\n") - 1u),
		"SSE server first backpressure Chunk mismatch"
	);
	xrtHttpBodyChunkRelease(&Chunk);
	testRequire(
		(xrtFutureWait(pWritable) == XWAIT_OK) &&
		(xrtFutureState(pWritable) == XFUTURE_RESOLVED),
		"SSE server writable Future did not resolve"
	);
	xrtFutureDestroy(pWritable);
	testRequire(
		xrtHttpSseSendComment(
			pStream, XRT_STR_LITERAL("wait")
		) == XHTTP_BODY_STREAM_OK,
		"SSE server did not recover after backpressure"
	);
	testRequire(xrtHttpBodyStreamClose(pStream),
		"SSE server Stream close failed");
	xrtSetError(pMarker);
	testRequire(
		(xrtHttpSseSend(
			pStream, XRT_STR_LITERAL("late")
		) == XHTTP_BODY_STREAM_CLOSED) &&
		(xrtGetError() == pMarker),
		"SSE server CLOSED changed the old error"
	);
	testRequire(
		(xrtHttpBodyNext(pReader, 64, &Chunk) == XHTTP_BODY_DATA) &&
		(Chunk.Size == sizeof(": wait\n") - 1u),
		"SSE server comment Chunk mismatch"
	);
	xrtHttpBodyChunkRelease(&Chunk);
	testRequire(
		xrtHttpBodyNext(pReader, 64, &Chunk) == XHTTP_BODY_EOF,
		"SSE server close did not publish EOF"
	);
	xrtClearError();
	xrtErrorFree(pMarker);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpBodyStreamDestroy(pStream);
	xrtHttpReplyDestroy(pReply);
}



/* 运行 SSE 服务端 Reply、编码、反压和失败原子性回归。 */
int main(void)
{
	testHttpSseServerFlow();
	testHttpSseServerFailure();
	testHttpSseServerMemoryContracts();
	testHttpSseServerBackpressure();
	printf("[PASS] HTTP SSE server\n");
	return 0;
}

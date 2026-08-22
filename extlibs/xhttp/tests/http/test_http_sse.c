#include "../test.h"

#include <xrt/http_sse.h>



/* 验证完整事件的规范字段顺序、多行 Data 和零结尾便利层。 */
static void testHttpSseEvent(void)
{
	static const cstr Expected =
		"id: 42\n"
		"event: update\n"
		"retry: 1500\n"
		"data: first\n"
		"data: second\n"
		"data:\n"
		"\n";
	xhttpsseevent Event;
	char Output[128];
	str sBuilt;
	size_t iSize = 0;
	size_t iBuilt = 0;

	memset(&Event, 0, sizeof(Event));
	Event.Type = XRT_STR_LITERAL("update");
	Event.Data = XRT_STR_LITERAL("first\nsecond\n");
	Event.Id = XRT_STR_LITERAL("42");
	Event.Retry = 1500;
	Event.Flags = XHTTP_SSE_EVENT_TYPE |
		XHTTP_SSE_EVENT_DATA |
		XHTTP_SSE_EVENT_ID |
		XHTTP_SSE_EVENT_RETRY;
	testRequire(
		xrtHttpSseEventValid(&Event) &&
		xrtHttpSseEventSize(&Event, &iSize) &&
		(iSize == strlen(Expected)) &&
		xrtHttpSseEventWrite(
			&Event, Output, sizeof(Output), &iSize
		) &&
		(iSize == strlen(Expected)) &&
		(memcmp(Output, Expected, iSize) == 0),
		"SSE complete event encoding mismatch"
	);
	sBuilt = xrtHttpSseEventBuild(&Event, &iBuilt);
	testRequire(
		(sBuilt != NULL) &&
		(iBuilt == strlen(Expected)) &&
		(strcmp(sBuilt, Expected) == 0),
		"SSE event Build mismatch"
	);
	xrtFree(sBuilt);
}



/* 验证省略字段、显式空 data 和注释心跳保持不同语义。 */
static void testHttpSseEmptyAndComment(void)
{
	xhttpsseevent Event;
	char Output[64];
	size_t iSize;

	memset(&Event, 0, sizeof(Event));
	Event.Flags = XHTTP_SSE_EVENT_DATA;
	testRequire(
		xrtHttpSseEventWrite(
			&Event, Output, sizeof(Output), &iSize
		) &&
		(iSize == 7u) &&
		(memcmp(Output, "data:\n\n", 7u) == 0),
		"SSE empty data did not encode a dispatchable event"
	);
	memset(&Event, 0, sizeof(Event));
	testRequire(
		xrtHttpSseEventWrite(
			&Event, Output, sizeof(Output), &iSize
		) &&
		(iSize == 1u) && (Output[0] == '\n'),
		"SSE fieldless block mismatch"
	);
	testRequire(
		xrtHttpSseCommentWrite(
			XRT_STR_LITERAL("keep\n"),
			Output,
			sizeof(Output),
			&iSize
		) &&
		(iSize == 9u) &&
		(memcmp(Output, ": keep\n:\n", 9u) == 0),
		"SSE comment heartbeat encoding mismatch"
	);
}



/* 验证无效 UTF-8、CR、Header ID 和输出容量均在写入前失败。 */
static void testHttpSseInvalid(void)
{
	static const char InvalidUtf8[] = { (char)0xC0, (char)0x80 };
	static const char NullId[] = { 'a', 0, 'b' };
	xhttpsseevent Event;
	char Output[16];
	char Before[16];
	size_t iSize = 99;

	memset(&Event, 0, sizeof(Event));
	Event.Data = (xstrview){ InvalidUtf8, sizeof(InvalidUtf8) };
	Event.Flags = XHTTP_SSE_EVENT_DATA;
	testRequire(
		!xrtHttpSseEventValid(&Event),
		"SSE writer accepted invalid UTF-8"
	);
	Event.Data = XRT_STR_LITERAL("a\rb");
	testRequire(
		!xrtHttpSseEventValid(&Event),
		"SSE writer accepted CR in Data"
	);
	Event.Data = XRT_STR_LITERAL("ok");
	Event.Id = (xstrview){ NullId, sizeof(NullId) };
	Event.Flags |= XHTTP_SSE_EVENT_ID;
	testRequire(
		!xrtHttpSseEventValid(&Event) &&
		!xrtHttpSseLastEventIdValid(Event.Id),
		"SSE writer accepted unsafe Last-Event-ID"
	);
	Event.Id = XRT_STR_LITERAL("7");
	memset(Output, 0x5A, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	testRequire(
		!xrtHttpSseEventWrite(
			&Event, Output, 2u, &iSize
		) &&
		(iSize > 2u) &&
		(memcmp(Output, Before, sizeof(Output)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"SSE short output was not failure atomic"
	);
	xrtClearError();
}



/* 验证借用字段、输出和长度槽重叠时不会破坏任何输入。 */
static void testHttpSseOverlap(void)
{
	union test_http_sse_overlap {
		size_t Size;
		char Text[sizeof(size_t)];
	} Shared;
	xhttpsseevent Event;
	char Output[32];
	char Before[32];
	size_t iSize = 83u;

	memset(&Event, 0, sizeof(Event));
	memset(Output, 0x31, sizeof(Output));
	memcpy(Output, "ok", 2u);
	memcpy(Before, Output, sizeof(Output));
	Event.Data = (xstrview){ Output, 2u };
	Event.Flags = XHTTP_SSE_EVENT_DATA;
	testRequire(
		!xrtHttpSseEventWrite(
			&Event, Output, sizeof(Output), &iSize
		) && (iSize == 83u) &&
		(memcmp(Output, Before, sizeof(Output)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SSE event output overlap was not failure atomic"
	);
	xrtClearError();

	memset(&Shared, 0, sizeof(Shared));
	memcpy(Shared.Text, "ok", 2u);
	Event.Data = (xstrview){ Shared.Text, 2u };
	testRequire(
		!xrtHttpSseEventWrite(
			&Event, NULL, 0, &Shared.Size
		) && (memcmp(Shared.Text, "ok", 2u) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SSE event size overlap modified borrowed data"
	);
	xrtClearError();

	memset(Output, 0x42, sizeof(Output));
	memcpy(Output, "ping", 4u);
	memcpy(Before, Output, sizeof(Output));
	iSize = 89u;
	testRequire(
		!xrtHttpSseCommentWrite(
			(xstrview){ Output, 4u },
			Output,
			sizeof(Output),
			&iSize
		) && (iSize == 89u) &&
		(memcmp(Output, Before, sizeof(Output)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SSE comment output overlap was not failure atomic"
	);
	xrtClearError();
}



/* 验证 SSE 固定描述符支持未对齐存储并拒绝地址回绕。 */
static void testHttpSseMemoryContracts(void)
{
	static const char Expected[] = "id: 7\ndata: payload\n\n";
	uint8 EventStorage[sizeof(xhttpsseevent) + 2u];
	uint8 SizeStorage[sizeof(size_t) + 2u];
	xhttpsseevent Event;
	char Output[64];
	str sBuilt;
	size_t iSize;

	memset(&Event, 0, sizeof(Event));
	Event.Data = XRT_STR_LITERAL("payload");
	Event.Id = XRT_STR_LITERAL("7");
	Event.Flags = XHTTP_SSE_EVENT_DATA | XHTTP_SSE_EVENT_ID;
	memset(EventStorage, 0xA5, sizeof(EventStorage));
	memset(SizeStorage, 0xA5, sizeof(SizeStorage));
	memcpy(EventStorage + 1u, &Event, sizeof(Event));
	testRequire(xrtHttpSseEventValid(
		(const xhttpsseevent*)(const void*)(EventStorage + 1u)
	), "SSE rejected an unaligned event descriptor");
	testRequire(xrtHttpSseEventSize(
		(const xhttpsseevent*)(const void*)(EventStorage + 1u),
		(size_t*)(void*)(SizeStorage + 1u)
	), "SSE size rejected unaligned storage");
	memcpy(&iSize, SizeStorage + 1u, sizeof(iSize));
	testRequire((iSize == (sizeof(Expected) - 1u)) &&
		xrtHttpSseEventWrite(
			(const xhttpsseevent*)(const void*)(EventStorage + 1u),
			Output,
			sizeof(Output),
			(size_t*)(void*)(SizeStorage + 1u)
		) && (memcmp(Output, Expected, iSize) == 0),
		"SSE writer rejected unaligned storage"
	);
	sBuilt = xrtHttpSseEventBuild(
		(const xhttpsseevent*)(const void*)(EventStorage + 1u),
		(size_t*)(void*)(SizeStorage + 1u)
	);
	testRequire((sBuilt != NULL) && (strcmp(sBuilt, Expected) == 0),
		"SSE Build rejected unaligned storage");
	xrtFree(sBuilt);
	testRequire(xrtHttpSseCommentWrite(
		XRT_STR_LITERAL("ping"),
		Output,
		sizeof(Output),
		(size_t*)(void*)(SizeStorage + 1u)
	), "SSE comment writer rejected an unaligned size output");
	sBuilt = xrtHttpSseCommentBuild(
		XRT_STR_LITERAL("ping"),
		(size_t*)(void*)(SizeStorage + 1u)
	);
	testRequire((sBuilt != NULL) && (strcmp(sBuilt, ": ping\n") == 0),
		"SSE comment Build rejected an unaligned size output");
	xrtFree(sBuilt);
	testRequire(
		(EventStorage[0] == 0xA5) &&
		(EventStorage[sizeof(EventStorage) - 1u] == 0xA5) &&
		(SizeStorage[0] == 0xA5) &&
		(SizeStorage[sizeof(SizeStorage) - 1u] == 0xA5),
		"SSE wrote outside unaligned storage"
	);

	testRequire(!xrtHttpSseEventValid(
		(const xhttpsseevent*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "SSE accepted a wrapping event descriptor");
	testRequire(!xrtHttpSseEventSize(
		&Event,
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "SSE size accepted a wrapping output");
	xrtClearError();
	testRequire(!xrtHttpSseEventWrite(
		&Event,
		(void*)(uintptr_t)(UINTPTR_MAX - 1u),
		sizeof(Output),
		&iSize
	), "SSE writer accepted a wrapping output");
	xrtClearError();
	testRequire(!xrtHttpSseCommentSize(
		XRT_STR_LITERAL("ping"),
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
	), "SSE comment size accepted a wrapping output");
	xrtClearError();
	testRequire(!xrtHttpSseCommentWrite(
		XRT_STR_LITERAL("ping"),
		(void*)(uintptr_t)(UINTPTR_MAX - 1u),
		sizeof(Output),
		&iSize
	), "SSE comment writer accepted a wrapping output");
	xrtClearError();
}



/* 运行无状态 SSE Writer 回归。 */
int main(void)
{
	testHttpSseEvent();
	testHttpSseEmptyAndComment();
	testHttpSseInvalid();
	testHttpSseOverlap();
	testHttpSseMemoryContracts();
	printf("[PASS] HTTP SSE writer\n");
	return 0;
}

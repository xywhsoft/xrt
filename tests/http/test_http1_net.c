#include "../test.h"



/* 跨块 Header 应按实际长度连续化，并完整保留 Upgrade 后帧余量。 */
static void testHttp1NetFragmented(void)
{
	static const char First[] =
		"GET /socket HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Upgrade: web";
	static const char Second[] =
		"socket\r\nConnection: Upgrade\r\n\r\n";
	static const uint8 Frame[] = {
		UINT8_C(0x81), UINT8_C(0x02), 'o', 'k'
	};
	xnetbuf Buffer;
	xhttpfield Fields[8];
	xhttp1head Head;
	uint8 Remaining[sizeof(Frame)];
	size_t iBefore;

	testRequire(
		xrtNetBufInit(&Buffer, NULL) &&
		xrtNetBufAppendBorrow(
			&Buffer,
			First,
			sizeof(First) - 1u
		) && xrtNetBufAppendBorrow(
			&Buffer,
			Second,
			sizeof(Second) - 1u
		) && xrtNetBufAppendBorrow(
			&Buffer,
			Frame,
			sizeof(Frame)
		) && (xrtNetBufSpanCount(&Buffer) == 3u),
		"HTTP/1 network fragmented fixture failed"
	);
	iBefore = xrtNetBufSize(&Buffer);
	xrtHttp1HeadInit(
		&Head,
		Fields,
		sizeof(Fields) / sizeof(Fields[0])
	);
	testRequire(
		(xrtHttp1RequestParseBuffer(
			&Buffer,
			&Head,
			NULL,
			NULL
		) == XHTTP1_READY) &&
		(Head.Bytes ==
		 (sizeof(First) - 1u + sizeof(Second) - 1u)) &&
		(xrtNetBufSize(&Buffer) == iBefore) &&
		(xrtNetBufConsume(&Buffer, Head.Bytes) == Head.Bytes) &&
		(xrtNetBufPeek(
			&Buffer,
			0,
			Remaining,
			sizeof(Remaining)
		) == sizeof(Remaining)) &&
		(memcmp(Remaining, Frame, sizeof(Frame)) == 0),
		"HTTP/1 network parser lost Upgrade suffix bytes"
	);
	xrtNetBufClear(&Buffer);
}



/* 单块增量输入不应触发额外合并，完整响应仍按原解析器语义发布。 */
static void testHttp1NetIncremental(void)
{
	static const char Partial[] = "HTTP/1.1 101 Switching Protocols\r\n";
	xnetbuf Buffer;
	xhttpfield Fields[8];
	xhttp1head Head;

	testRequire(
		xrtNetBufInit(&Buffer, NULL) &&
		xrtNetBufAppendBorrow(
			&Buffer,
			Partial,
			sizeof(Partial) - 1u
		),
		"HTTP/1 network incremental fixture failed"
	);
	xrtHttp1HeadInit(
		&Head,
		Fields,
		sizeof(Fields) / sizeof(Fields[0])
	);
	testRequire(
		xrtHttp1ResponseParseBuffer(
			&Buffer,
			&Head,
			NULL,
			NULL
		) == XHTTP1_MORE,
		"HTTP/1 network incremental status mismatch"
	);
	xrtNetBufClear(&Buffer);
}



int main(void)
{
	testHttp1NetFragmented();
	testHttp1NetIncremental();
	return 0;
}

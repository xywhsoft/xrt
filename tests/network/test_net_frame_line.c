#include "../test.h"



/* 验证默认 LF framing 的增量搜索、payload 读取和消费。 */
static void testNetLineIncremental(void)
{
	xnetlineconfig Config;
	xnetlineframer Framer;
	xnetframe Frame;
	xnetbuf Buffer;
	char sOutput[8] = { 0 };

	xrtNetLineConfigInit(&Config);
	testRequire(xrtNetLineInit(&Framer, &Config),
		"line framer init failed");
	testRequire(xrtNetBufInit(&Buffer, NULL) &&
		xrtNetBufAppend(&Buffer, "hel", 3),
		"line incremental prefix setup failed");
	testRequire(xrtNetLineNext(&Framer, &Buffer, &Frame) ==
		XNET_FRAME_MORE, "line prefix did not request more input");
	testRequire(xrtNetBufAppend(&Buffer, "lo\nrest", 7),
		"line incremental suffix append failed");
	testRequire(xrtNetLineNext(&Framer, &Buffer, &Frame) ==
		XNET_FRAME_READY && (Frame.PayloadSize == 5) &&
		(Frame.FrameSize == 6) && (Frame.Declared == 5),
		"line incremental frame mismatch");
	testRequire(xrtNetFrameCopy(
			&Buffer, &Frame, sOutput, sizeof(sOutput)
		) == 5 && (memcmp(sOutput, "hello", 5) == 0),
		"line incremental payload mismatch");
	testRequire(xrtNetFrameConsume(&Buffer, &Frame) &&
		(xrtNetBufSize(&Buffer) == 4),
		"line incremental consume mismatch");
	xrtNetBufClear(&Buffer);
}



/* 验证大量单字节块逐次追加时游标始终从旧块尾继续。 */
static void testNetLineFragmentedIncremental(void)
{
	static const uint8 iByte = 'a';
	static const char sDelimiter[] = "\r\n";
	xnetlineconfig Config;
	xnetlineframer Framer;
	xnetframe Frame;
	xnetbuf Buffer;
	size_t i;

	xrtNetLineConfigInit(&Config);
	Config.Delimiter.Data = (cbytes)sDelimiter;
	Config.Delimiter.Size = sizeof(sDelimiter) - 1u;
	Config.MaxPayload = 4096;
	testRequire(xrtNetLineInit(&Framer, &Config) &&
		xrtNetBufInit(&Buffer, NULL),
		"line fragmented setup failed");
	for ( i = 0; i < Config.MaxPayload; i++ ) {
		testRequire(xrtNetBufAppendBorrow(&Buffer, &iByte, 1) &&
			xrtNetLineNext(&Framer, &Buffer, &Frame) ==
			XNET_FRAME_MORE && (Framer.Search == (i + 1u)) &&
			(Framer.Cursor == Buffer.Tail),
			"line fragmented cursor did not advance incrementally");
	}
	testRequire(xrtNetBufAppendBorrow(&Buffer, sDelimiter, 1) &&
		xrtNetLineNext(&Framer, &Buffer, &Frame) == XNET_FRAME_MORE &&
		xrtNetBufAppendBorrow(&Buffer, sDelimiter + 1, 1) &&
		xrtNetLineNext(&Framer, &Buffer, &Frame) == XNET_FRAME_READY &&
		(Frame.PayloadSize == Config.MaxPayload),
		"line fragmented delimiter mismatch");
	xrtNetBufClear(&Buffer);
}



/* 验证任意长度借用分隔符、跨块匹配、自重叠和包含分隔符。 */
static void testNetLineDelimiter(void)
{
	static const char sLongDelimiter[] = "--delimiter-over-16--";
	xnetlineconfig Config;
	xnetlineframer Framer;
	xnetframe Frame;
	xnetbuf Buffer;

	testRequire(xrtNetBufInit(&Buffer, NULL),
		"line delimiter buffer init failed");
	xrtNetLineConfigInit(&Config);
	Config.Delimiter.Data = (cbytes)"\r\n";
	Config.Delimiter.Size = 2;
	testRequire(xrtNetLineInit(&Framer, &Config) &&
		xrtNetBufAppendBorrow(&Buffer, "alpha\r", 6) &&
		xrtNetBufAppendBorrow(&Buffer, "\nbeta", 5),
		"line cross-block setup failed");
	testRequire(xrtNetLineNext(&Framer, &Buffer, &Frame) ==
		XNET_FRAME_READY && (Frame.PayloadSize == 5) &&
		(Frame.FrameSize == 7),
		"line cross-block delimiter mismatch");
	xrtNetBufClear(&Buffer);

	Config.Delimiter.Data = (cbytes)"abab";
	Config.Delimiter.Size = 4;
	Config.IncludeDelimiter = true;
	testRequire(xrtNetLineInit(&Framer, &Config) &&
		xrtNetBufAppend(&Buffer, "aabab-tail", 10),
		"line overlap delimiter setup failed");
	testRequire(xrtNetLineNext(&Framer, &Buffer, &Frame) ==
		XNET_FRAME_READY && (Frame.PayloadSize == 5) &&
		(Frame.FrameSize == 5) && (Frame.Declared == 1),
		"line overlapping delimiter mismatch");
	xrtNetBufClear(&Buffer);

	Config.Delimiter.Data = (cbytes)sLongDelimiter;
	Config.Delimiter.Size = sizeof(sLongDelimiter) - 1u;
	Config.IncludeDelimiter = false;
	testRequire(xrtNetLineInit(&Framer, &Config) &&
		xrtNetBufAppend(&Buffer, "x", 1) &&
		xrtNetBufAppendBorrow(
			&Buffer, sLongDelimiter, sizeof(sLongDelimiter) - 1u
		), "line long delimiter setup failed");
	testRequire(xrtNetLineNext(&Framer, &Buffer, &Frame) ==
		XNET_FRAME_READY && (Frame.PayloadSize == 1),
		"line framer retained a fixed delimiter cap");
	xrtNetBufClear(&Buffer);
}



/* 验证 payload 上限在分隔符位置而非总缓冲末端执行。 */
static void testNetLineLimit(void)
{
	xnetlineconfig Config;
	xnetlineframer Framer;
	xnetframe Frame;
	xnetbuf Buffer;

	xrtNetLineConfigInit(&Config);
	Config.MaxPayload = 3;
	Config.Delimiter.Data = (cbytes)"\r\n";
	Config.Delimiter.Size = 2;
	testRequire(xrtNetLineInit(&Framer, &Config) &&
		xrtNetBufInit(&Buffer, NULL) &&
		xrtNetBufAppend(&Buffer, "abc\r", 4),
		"line limit partial setup failed");
	testRequire(xrtNetLineNext(&Framer, &Buffer, &Frame) ==
		XNET_FRAME_MORE, "line limit rejected a partial legal delimiter");
	testRequire(xrtNetBufAppend(&Buffer, "\n", 1) &&
		xrtNetLineNext(&Framer, &Buffer, &Frame) == XNET_FRAME_READY,
		"line limit rejected delimiter at exact payload limit");
	xrtNetBufClear(&Buffer);
	testRequire(xrtNetLineReset(&Framer) &&
		xrtNetBufAppend(&Buffer, "abcd\r\n", 6),
		"line over-limit setup failed");
	xrtClearError();
	testRequire(xrtNetLineNext(&Framer, &Buffer, &Frame) ==
		XNET_FRAME_ERROR &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_FRAME_LIMIT),
		"line framer accepted a delimiter after the payload limit");
	xrtNetBufClear(&Buffer);
}



/* 验证配置和状态失败不产生未定义帧。 */
static void testNetLineInvalid(void)
{
	xnetlineconfig Config;
	xnetlineframer Framer;
	xnetframe Frame;
	xnetbuf Buffer;
	xnetbuf Other;

	memset(&Framer, 0, sizeof(Framer));
	testRequire(xrtNetBufInit(&Buffer, NULL),
		"line invalid buffer init failed");
	xrtClearError();
	testRequire(xrtNetLineNext(&Framer, &Buffer, &Frame) ==
		XNET_FRAME_ERROR &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_FRAME_STATE),
		"line framer accepted invalid state");
	xrtNetLineConfigInit(&Config);
	Config.Delimiter.Data = NULL;
	Config.Delimiter.Size = 1;
	xrtClearError();
	testRequire(!xrtNetLineInit(&Framer, &Config) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_FRAME_CONFIG),
		"line framer accepted invalid delimiter storage");
	Config.Delimiter.Size = 0;
	xrtClearError();
	testRequire(!xrtNetLineInit(&Framer, &Config) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_FRAME_CONFIG),
		"line framer accepted an empty delimiter");
	xrtNetLineConfigInit(&Config);
	testRequire(xrtNetLineInit(&Framer, &Config) &&
		xrtNetBufInit(&Other, NULL) &&
		xrtNetBufAppend(&Buffer, "a", 1) &&
		xrtNetBufAppend(&Other, "b", 1) &&
		xrtNetLineNext(&Framer, &Buffer, &Frame) == XNET_FRAME_MORE,
		"line state contract setup failed");
	xrtClearError();
	testRequire(xrtNetLineNext(&Framer, &Other, &Frame) ==
		XNET_FRAME_ERROR &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_FRAME_STATE),
		"line framer accepted another input before reset");
	testRequire(xrtNetLineReset(&Framer) &&
		xrtNetLineNext(&Framer, &Buffer, &Frame) == XNET_FRAME_MORE &&
		(xrtNetBufConsume(&Buffer, 1) == 1),
		"line shrink contract setup failed");
	xrtClearError();
	testRequire(xrtNetLineNext(&Framer, &Buffer, &Frame) ==
		XNET_FRAME_ERROR &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_FRAME_STATE),
		"line framer accepted a consumed prefix before reset");
	xrtNetBufClear(&Other);
	xrtNetBufClear(&Buffer);
}



/* 执行行 framing 的增量、分隔符、上限和非法状态测试。 */
int main(void)
{
	testNetLineIncremental();
	testNetLineFragmentedIncremental();
	testNetLineDelimiter();
	testNetLineLimit();
	testNetLineInvalid();
	return 0;
}

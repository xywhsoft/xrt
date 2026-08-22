#include "../test.h"



/* 验证默认四字节大端 payload 长度和跨块输入。 */
static void testNetLengthDefault(void)
{
	static const uint8 Prefix[] = { 0, 0, 0, 5 };
	xnetlengthconfig Config;
	xnetlengthframer Framer;
	xnetframe Frame;
	xnetbuf Buffer;
	char sOutput[6] = { 0 };

	xrtNetLengthConfigInit(&Config);
	testRequire(xrtNetLengthInit(&Framer, &Config) &&
		xrtNetBufInit(&Buffer, NULL) &&
		xrtNetBufAppendBorrow(&Buffer, Prefix, sizeof(Prefix)) &&
		xrtNetBufAppendBorrow(&Buffer, "wo", 2),
		"length default setup failed");
	testRequire(xrtNetLengthNext(&Framer, &Buffer, &Frame) ==
		XNET_FRAME_MORE, "length partial body did not request more input");
	testRequire(xrtNetBufAppendBorrow(&Buffer, "rld", 3) &&
		xrtNetLengthNext(&Framer, &Buffer, &Frame) ==
		XNET_FRAME_READY && (Frame.PayloadOffset == 4) &&
		(Frame.PayloadSize == 5) && (Frame.FrameSize == 9) &&
		(Frame.Declared == 5), "length default frame mismatch");
	testRequire(xrtNetFrameCopy(
			&Buffer, &Frame, sOutput, sizeof(sOutput)
		) == 5 && (memcmp(sOutput, "world", 5) == 0),
		"length default payload mismatch");
	xrtNetBufClear(&Buffer);
}



/* 验证字段偏移、小端、声明长度含头部和 strip 组合。 */
static void testNetLengthFlexible(void)
{
	static const uint8 Packet[] = {
		0x7Fu, 0x06u, 0x00u, 'a', 'b', 'c'
	};
	xnetlengthconfig Config;
	xnetlengthframer Framer;
	xnetframe Frame;
	xnetbuf Buffer;

	xrtNetLengthConfigInit(&Config);
	Config.LengthOffset = 1;
	Config.LengthSize = 2;
	Config.Order = XNET_FRAME_LITTLE_ENDIAN;
	Config.Adjustment = -3;
	Config.Strip = 3;
	testRequire(xrtNetLengthInit(&Framer, &Config) &&
		xrtNetBufInit(&Buffer, NULL) &&
		xrtNetBufAppend(&Buffer, Packet, sizeof(Packet)),
		"length flexible setup failed");
	testRequire(xrtNetLengthNext(&Framer, &Buffer, &Frame) ==
		XNET_FRAME_READY && (Frame.Declared == 6) &&
		(Frame.PayloadOffset == 3) && (Frame.PayloadSize == 3) &&
		(Frame.FrameSize == 6),
		"length flexible frame mismatch");
	xrtNetBufClear(&Buffer);
}



/* 验证帧上限、声明溢出、负长度和 strip 边界。 */
static void testNetLengthEdges(void)
{
	static const uint8 Huge[] = {
		0xFFu, 0xFFu, 0xFFu, 0xFFu,
		0xFFu, 0xFFu, 0xFFu, 0xFFu
	};
	static const uint8 Five[] = { 0, 0, 0, 5 };
	xnetlengthconfig Config;
	xnetlengthframer Framer;
	xnetframe Frame;
	xnetbuf Buffer;

	testRequire(xrtNetBufInit(&Buffer, NULL),
		"length edge buffer init failed");
	xrtNetLengthConfigInit(&Config);
	Config.MaxFrame = 8;
	testRequire(xrtNetLengthInit(&Framer, &Config) &&
		xrtNetBufAppend(&Buffer, Five, sizeof(Five)),
		"length limit setup failed");
	xrtClearError();
	testRequire(xrtNetLengthNext(&Framer, &Buffer, &Frame) ==
		XNET_FRAME_ERROR &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_FRAME_LIMIT),
		"length framer accepted a frame beyond its limit");
	xrtNetBufClear(&Buffer);

	xrtNetLengthConfigInit(&Config);
	Config.LengthSize = 8;
	Config.Strip = 8;
	Config.MaxFrame = SIZE_MAX;
	testRequire(xrtNetLengthInit(&Framer, &Config) &&
		xrtNetBufAppend(&Buffer, Huge, sizeof(Huge)),
		"length overflow setup failed");
	xrtClearError();
	testRequire(xrtNetLengthNext(&Framer, &Buffer, &Frame) ==
		XNET_FRAME_ERROR &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_FRAME_LENGTH),
		"length framer accepted uint64 frame overflow");
	xrtNetBufClear(&Buffer);

	xrtNetLengthConfigInit(&Config);
	Config.LengthSize = 1;
	Config.Strip = 0;
	Config.Adjustment = -3;
	testRequire(xrtNetLengthInit(&Framer, &Config) &&
		xrtNetBufAppend(&Buffer, "\x01", 1),
		"length negative setup failed");
	xrtClearError();
	testRequire(xrtNetLengthNext(&Framer, &Buffer, &Frame) ==
		XNET_FRAME_ERROR &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_FRAME_LENGTH),
		"length framer accepted a negative adjusted frame");
	xrtNetBufClear(&Buffer);

	xrtNetLengthConfigInit(&Config);
	Config.Strip = Config.MaxFrame + 1u;
	xrtClearError();
	testRequire(!xrtNetLengthInit(&Framer, &Config) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_FRAME_CONFIG),
		"length framer accepted strip beyond maximum frame");
	Config.Strip = 4;
	Config.LengthSize = 0;
	xrtClearError();
	testRequire(!xrtNetLengthInit(&Framer, &Config) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_FRAME_CONFIG),
		"length framer accepted a zero-width field");
}



/* 执行长度 framing 的默认、灵活布局和溢出边界测试。 */
int main(void)
{
	testNetLengthDefault();
	testNetLengthFlexible();
	testNetLengthEdges();
	return 0;
}

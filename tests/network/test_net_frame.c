#include "../test.h"



/* 验证通用帧复制、精确消费和陈旧边界拒绝。 */
int main(void)
{
	xnetbuf Buffer;
	xnetframe Frame = { 1, 3, 6, 3 };
	char sOutput[4] = { 0 };

	testRequire(xrtNetBufInit(&Buffer, NULL), "frame buffer init failed");
	testRequire(xrtNetBufAppend(&Buffer, "abcde-rest", 10),
		"frame buffer append failed");
	testRequire(xrtNetFrameCopy(
			&Buffer, &Frame, sOutput, sizeof(sOutput)
		) == 3 && (memcmp(sOutput, "bcd", 3) == 0),
		"frame payload copy mismatch");
	testRequire(xrtNetFrameConsume(&Buffer, &Frame) &&
		(xrtNetBufSize(&Buffer) == 4),
		"frame consume did not leave suffix");
	memset(sOutput, 0, sizeof(sOutput));
	testRequire(xrtNetBufPeek(&Buffer, 0, sOutput, sizeof(sOutput)) == 4 &&
		(memcmp(sOutput, "rest", 4) == 0),
		"frame consume suffix mismatch");
	xrtClearError();
	testRequire(!xrtNetFrameConsume(&Buffer, &Frame) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_FRAME_STATE) &&
		(xrtNetBufSize(&Buffer) == 4),
		"stale frame changed input");
	Frame.PayloadOffset = 4;
	Frame.PayloadSize = 2;
	Frame.FrameSize = 5;
	xrtClearError();
	testRequire(xrtNetFrameCopy(
			&Buffer, &Frame, sOutput, sizeof(sOutput)
		) == 0 && (xrtErrorCode(xrtGetError()) == XNET_ERROR_FRAME_STATE),
		"invalid payload range was accepted");
	xrtNetBufClear(&Buffer);
	return 0;
}

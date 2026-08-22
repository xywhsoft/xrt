#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件行 framing 增量入口。 */
int main(void)
{
	xnetlineconfig Config;
	xnetlineframer Framer;
	xnetframe Frame;
	xnetbuf Buffer;

	xrtNetLineConfigInit(&Config);
	if ( !xrtNetLineInit(&Framer, &Config) ||
		 !xrtNetBufInit(&Buffer, NULL) ||
		 !xrtNetBufAppend(&Buffer, "line\nnext", 9) ||
		 (xrtNetLineNext(&Framer, &Buffer, &Frame) !=
			XNET_FRAME_READY) || (Frame.PayloadSize != 4) ||
		 (Frame.FrameSize != 5) ) {
		xrtNetBufClear(&Buffer);
		return 1;
	}
	xrtNetBufClear(&Buffer);
	return 0;
}

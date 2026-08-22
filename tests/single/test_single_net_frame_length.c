#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件长度 framing 入口。 */
int main(void)
{
	static const uint8 Packet[] = { 0, 0, 0, 3, 'a', 'b', 'c' };
	xnetlengthconfig Config;
	xnetlengthframer Framer;
	xnetframe Frame;
	xnetbuf Buffer;

	xrtNetLengthConfigInit(&Config);
	if ( !xrtNetLengthInit(&Framer, &Config) ||
		 !xrtNetBufInit(&Buffer, NULL) ||
		 !xrtNetBufAppend(&Buffer, Packet, sizeof(Packet)) ||
		 (xrtNetLengthNext(&Framer, &Buffer, &Frame) !=
			XNET_FRAME_READY) || (Frame.PayloadSize != 3) ||
		 (Frame.FrameSize != sizeof(Packet)) ) {
		xrtNetBufClear(&Buffer);
		return 1;
	}
	xrtNetBufClear(&Buffer);
	return 0;
}

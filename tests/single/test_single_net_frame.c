#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件通用帧复制与消费入口。 */
int main(void)
{
	xnetbuf Buffer;
	xnetframe Frame = { 1, 2, 4, 2 };
	char sOutput[2];

	if ( !xrtNetBufInit(&Buffer, NULL) ||
		 !xrtNetBufAppend(&Buffer, "abcdx", 5) ||
		 (xrtNetFrameCopy(
			&Buffer, &Frame, sOutput, sizeof(sOutput)
		 ) != 2) || (memcmp(sOutput, "bc", 2) != 0) ||
		 !xrtNetFrameConsume(&Buffer, &Frame) ||
		 (xrtNetBufSize(&Buffer) != 1) ) {
		xrtNetBufClear(&Buffer);
		return 1;
	}
	xrtNetBufClear(&Buffer);
	return 0;
}

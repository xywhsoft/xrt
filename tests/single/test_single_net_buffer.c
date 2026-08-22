#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须提供可直接接收数据的动态网络缓冲。 */
int main(void)
{
	xnetbuf Buffer;
	xnetwspan Write;
	char sOutput[8] = { 0 };

	if ( !xrtNetBufInit(&Buffer, NULL) ||
		!xrtNetBufReserve(&Buffer, 5, &Write) ) {
		return 1;
	}
	memcpy(Write.Data, "hello", 5);
	if ( !xrtNetBufCommit(&Buffer, 5) ||
		!xrtNetBufPrepend(&Buffer, ">", 1) ||
		(xrtNetBufRead(&Buffer, sOutput, sizeof(sOutput)) != 6) ) {
		return 1;
	}
	xrtNetBufClear(&Buffer);
	return memcmp(sOutput, ">hello", 6) == 0 ? 0 : 1;
}

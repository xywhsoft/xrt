#include <stdio.h>
#include <string.h>

#include <xrt.h>



/* 展示后端或协议解析器直接写入可变尺寸网络缓冲。 */
int main(void)
{
	xnetbufpool* pPool = xrtNetBufPoolCreate(NULL);
	xnetbuf Buffer;
	xnetwspan Write;
	xnetspan Read;

	if ( (pPool == NULL) || !xrtNetBufInit(&Buffer, pPool) ||
		!xrtNetBufReserve(&Buffer, 6, &Write) ) {
		return 1;
	}
	memcpy(Write.Data, "packet", 6);
	if ( !xrtNetBufCommit(&Buffer, 6) ||
		!xrtNetBufFront(&Buffer, &Read) ) {
		return 1;
	}
	printf("bytes=%zu spans=%zu data=%.*s\n",
		xrtNetBufSize(&Buffer), xrtNetBufSpanCount(&Buffer),
		(int)Read.Size, (const char*)Read.Data);
	xrtNetBufClear(&Buffer);
	return xrtNetBufPoolDestroy(pPool) ? 0 : 1;
}

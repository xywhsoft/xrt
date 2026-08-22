#include <stdio.h>

#include <xrt.h>



/* 解析四字节网络序长度前缀，并直接读取 payload。 */
int main(void)
{
	static const uint8 Packet[] = {
		0, 0, 0, 5, 'h', 'e', 'l', 'l', 'o'
	};
	xnetlengthconfig Config;
	xnetlengthframer Framer;
	xnetframe Frame;
	xnetbuf Input;
	char sPayload[6] = { 0 };

	xrtNetLengthConfigInit(&Config);
	if ( !xrtNetLengthInit(&Framer, &Config) ||
		 !xrtNetBufInit(&Input, NULL) ||
		 !xrtNetBufAppend(&Input, Packet, sizeof(Packet)) ||
		 (xrtNetLengthNext(&Framer, &Input, &Frame) !=
			XNET_FRAME_READY) ||
		 (xrtNetFrameCopy(
			&Input, &Frame, sPayload, sizeof(sPayload) - 1u
		 ) != 5) ) {
		xrtNetBufClear(&Input);
		return 1;
	}
	printf("%s\n", sPayload);
	xrtNetBufClear(&Input);
	return 0;
}

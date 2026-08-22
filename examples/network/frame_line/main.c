#include <stdio.h>

#include <xrt.h>



/* 从分块输入中逐条取出 CRLF 文本帧。 */
int main(void)
{
	xnetlineconfig Config;
	xnetlineframer Framer;
	xnetframe Frame;
	xnetbuf Input;
	char sLine[32];

	xrtNetLineConfigInit(&Config);
	Config.Delimiter.Data = (cbytes)"\r\n";
	Config.Delimiter.Size = 2;
	if ( !xrtNetLineInit(&Framer, &Config) ||
		 !xrtNetBufInit(&Input, NULL) ||
		 !xrtNetBufAppend(&Input, "first\r", 6) ) {
		return 1;
	}
	if ( xrtNetLineNext(&Framer, &Input, &Frame) != XNET_FRAME_MORE ||
		 !xrtNetBufAppend(&Input, "\nsecond\r\n", 10) ) {
		xrtNetBufClear(&Input);
		return 1;
	}
	while ( xrtNetLineNext(&Framer, &Input, &Frame) ==
		XNET_FRAME_READY ) {
		size_t iSize = xrtNetFrameCopy(
			&Input, &Frame, sLine, sizeof(sLine) - 1u
		);

		sLine[iSize] = 0;
		printf("%s\n", sLine);
		if ( !xrtNetFrameConsume(&Input, &Frame) ) {
			xrtNetBufClear(&Input);
			return 1;
		}
	}
	xrtNetBufClear(&Input);
	return 0;
}

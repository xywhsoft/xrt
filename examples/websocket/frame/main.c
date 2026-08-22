#include <stdio.h>
#include <string.h>

#include <xrt.h>



/* 展示服务端方向的帧头解析和可分片负载解掩码。 */
int main(void)
{
	static const uint8 Mask[XWS_MASK_SIZE] = {
		0x37, 0xFA, 0x21, 0x3D
	};
	xwsframeconfig Config;
	xwsframe Parsed;
	xwsframe Frame;
	xbytesview Input;
	uint8 Head[XWS_FRAME_HEAD_MAX];
	uint8 Payload[] = { 'H', 'e', 'l', 'l', 'o' };
	size_t iHeadSize;

	xrtWsFrameConfigInit(&Config);
	Config.Mask = XWS_MASK_REQUIRED;

	xrtWsFrameInit(&Frame);
	Frame.Flags =
		(uint32)XWS_FRAME_FIN | (uint32)XWS_FRAME_MASKED;
	Frame.Opcode = (uint8)XWS_OPCODE_TEXT;
	Frame.PayloadSize = sizeof(Payload);
	memcpy(Frame.Mask, Mask, sizeof(Mask));

	if ( !xrtWsFrameWrite(
		&Frame, &Config, Head, sizeof(Head), &iHeadSize
	) ) {
		return 1;
	}
	if ( !xrtWsMask(Payload, sizeof(Payload), Mask, 0) ) {
		return 2;
	}

	Input.Data = Head;
	Input.Size = iHeadSize;
	if ( xrtWsFrameParse(
		Input, &Parsed, &Config, NULL
	) != XWS_FRAME_READY ) {
		return 3;
	}
	if ( !xrtWsMask(Payload, 2, Parsed.Mask, 0) ||
		!xrtWsMask(Payload + 2, 3, Parsed.Mask, 2) ) {
		return 4;
	}

	printf(
		"opcode=%u payload=%llu text=%.*s\n",
		(unsigned int)Parsed.Opcode,
		(unsigned long long)Parsed.PayloadSize,
		(int)sizeof(Payload),
		(const char*)Payload
	);
	return 0;
}

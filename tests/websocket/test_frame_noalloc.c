#include "../test_allocator.h"



/* 验证全部成功路径在全局分配器失效时仍然可用。 */
int main(void)
{
	static const uint8 Mask[XWS_MASK_SIZE] = {
		0x12, 0x34, 0x56, 0x78
	};
	xwsframe Parsed;
	xwsframe Frame;
	xbytesview Input;
	uint8 Head[XWS_FRAME_HEAD_MAX];
	uint8 Payload[4096];
	size_t iHeadSize;

	testRequire(
		testInstallFailAllocator(),
		"WebSocket failure allocator install failed"
	);

	xrtWsFrameInit(&Frame);
	Frame.Flags =
		(uint32)XWS_FRAME_FIN | (uint32)XWS_FRAME_MASKED;
	Frame.Opcode = (uint8)XWS_OPCODE_BINARY;
	Frame.PayloadSize = sizeof(Payload);
	memcpy(Frame.Mask, Mask, sizeof(Mask));
	testRequire(
		xrtWsFrameWrite(
			&Frame, NULL, Head, sizeof(Head), &iHeadSize
		),
		"WebSocket frame write unexpectedly allocated"
	);

	Input.Data = Head;
	Input.Size = iHeadSize;
	testRequire(
		(xrtWsFrameParse(Input, &Parsed, NULL, NULL) ==
			XWS_FRAME_READY) &&
		(Parsed.PayloadSize == sizeof(Payload)),
		"WebSocket frame parse unexpectedly allocated"
	);

	memset(Payload, 0xA5, sizeof(Payload));
	testRequire(
		xrtWsMask(Payload, sizeof(Payload), Mask, 0),
		"WebSocket payload mask unexpectedly allocated"
	);
	printf("[PASS] websocket_frame_noalloc\n");
	return 0;
}

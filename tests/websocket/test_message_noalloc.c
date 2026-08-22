#include "../test_allocator.h"



/* 消息状态、分片、控制帧和 UTF-8 校验不得分配堆内存。 */
int main(void)
{
	static const uint8 First[] = { 'A', 0xE4, 0xB8 };
	static const uint8 Last[] = { 0xAD, 'B' };
	xwsmessagestate State;
	xwsmessageinfo Info;
	xwsframe Frame;

	testRequire(
		testInstallFailAllocator(),
		"WebSocket message failure allocator install failed"
	);
	testRequire(
		xrtWsMessageInit(&State, NULL),
		"WebSocket message initialization allocated"
	);

	xrtWsFrameInit(&Frame);
	Frame.Opcode = XWS_OPCODE_TEXT;
	Frame.PayloadSize = sizeof(First);
	testRequire(
		xrtWsMessageFrameBegin(&State, &Frame, &Info, NULL) &&
		xrtWsMessagePayload(
			&State,
			(xbytesview) { First, sizeof(First) },
			NULL
		) &&
		xrtWsMessageFrameEnd(&State, NULL),
		"WebSocket first fragment allocated"
	);

	xrtWsFrameInit(&Frame);
	Frame.Flags = XWS_FRAME_FIN;
	Frame.Opcode = XWS_OPCODE_CONTINUATION;
	Frame.PayloadSize = sizeof(Last);
	testRequire(
		xrtWsMessageFrameBegin(&State, &Frame, &Info, NULL) &&
		xrtWsMessagePayload(
			&State,
			(xbytesview) { Last, sizeof(Last) },
			NULL
		) &&
		xrtWsMessageFrameEnd(&State, NULL),
		"WebSocket final fragment allocated"
	);
	printf("[PASS] websocket_message_noalloc\n");
	return 0;
}

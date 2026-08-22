#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留流式分片消息和 UTF-8 校验。 */
int main(void)
{
	static const uint8 First[] = { 'A', 0xE4, 0xB8 };
	static const uint8 Last[] = { 0xAD, 'B' };
	xwsmessagestate State;
	xwsmessageinfo Info;
	xwsframe Frame;

	if ( !xrtWsMessageInit(&State, NULL) ) {
		return 1;
	}
	xrtWsFrameInit(&Frame);
	Frame.Opcode = XWS_OPCODE_TEXT;
	Frame.PayloadSize = sizeof(First);
	if ( !xrtWsMessageFrameBegin(&State, &Frame, &Info, NULL) ||
		!xrtWsMessagePayload(
			&State,
			(xbytesview) { First, sizeof(First) },
			NULL
		) ||
		!xrtWsMessageFrameEnd(&State, NULL) ) {
		return 2;
	}

	xrtWsFrameInit(&Frame);
	Frame.Flags = XWS_FRAME_FIN;
	Frame.Opcode = XWS_OPCODE_CONTINUATION;
	Frame.PayloadSize = sizeof(Last);
	if ( !xrtWsMessageFrameBegin(&State, &Frame, &Info, NULL) ||
		!xrtWsMessagePayload(
			&State,
			(xbytesview) { Last, sizeof(Last) },
			NULL
		) ||
		!xrtWsMessageFrameEnd(&State, NULL) ) {
		return 3;
	}
	return 0;
}

#include <xrt.h>

#include <stdio.h>



/* 以控制帧可穿插的方式流式接收一个分片文本消息。 */
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
	printf("message complete\n");
	return 0;
}

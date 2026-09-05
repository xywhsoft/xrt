#include <xrt.h>

#include <stdio.h>



/*
 * 范例：websocket/message —— 分片消息重组：控制帧可穿插 + UTF-8 增量校验
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtWsMessageInit          初始化重组状态（可挂消息配置）
 *   xrtWsMessageFrameBegin    每帧开始（提供帧头信息）
 *   xrtWsMessagePayload       喂入该帧负载
 *   xrtWsMessageFrameEnd      帧结束（穿插的控制帧被忽略）
 * 模块宏：XRT_MODULE_WEBSOCKET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/websocket/message/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   message complete
 *
 * 刻意的切分点：First = {A, 0xE4, 0xB8}、Last = {0xAD, B}——
 *   一个汉字（E4 B8 AD）被帧边界劈开！状态机带增量 UTF-8
 *   校验：跨帧拼接后仍合法才放行（本例拼接出 A中B）。
 *   控制帧（Ping/Pong/Close）允许在分片间穿插并被正确旁路。
 */


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

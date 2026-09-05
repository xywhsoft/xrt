#include <xrt.h>

#include <stdio.h>



/*
 * 范例：websocket/close —— Close 控制帧负载：写出与解析往返
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtWsCloseWrite    状态码 + UTF-8 原因 → Close 负载字节
 *   xrtWsCloseParse    负载 → xwsclose（Code + Reason 视图）
 *   XWS_CLOSE_PAYLOAD_MAX   负载上限（码 2 字节 + 原因 ≤123）
 * 模块宏：XRT_MODULE_WEBSOCKET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/websocket/close/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   code=1000 reason=shutdown
 *
 * 协议约束由 API 强制：原因必须是合法 UTF-8、总长不超限、
 *   码不能是保留区间（如 1005/1006/1015 禁止出现在线上）——
 *   写侧越界直接失败，读侧拒绝畸形负载。
 */


/* 写出并解析一个带 UTF-8 原因的 Close 控制帧负载。 */
int main(void)
{
	uint8 Payload[XWS_CLOSE_PAYLOAD_MAX];
	xwsclose Close;
	xbytesview Input;
	size_t iSize;

	if ( !xrtWsCloseWrite(
		XWS_CLOSE_NORMAL,
		XRT_STR_LITERAL("shutdown"),
		Payload,
		sizeof(Payload),
		&iSize
	) ) {
		return 1;
	}
	Input.Data = Payload;
	Input.Size = iSize;
	if ( !xrtWsCloseParse(Input, &Close) ) {
		return 2;
	}
	printf(
		"code=%u reason=%.*s\n",
		(unsigned)Close.Code,
		(int)Close.Reason.Size,
		Close.Reason.Data
	);
	return 0;
}

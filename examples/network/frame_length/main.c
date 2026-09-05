#include <stdio.h>

#include <xrt.h>



/*
 * 范例：network/frame_length —— 长度前缀分帧：解析与读取
 * ----------------------------------------------------------------
 * 演示 API：
 *   长度前缀帧解析器   4 字节网络序长度 + payload
 * 模块宏：XRT_MODULE_NET_FRAME
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/frame_length/main.c -lws2_32 -liphlpapi
 * 预期输出：（解出 payload "hello"）
 *
 * 私有二进制协议最常用的分帧方式（RPC、游戏服）：
 *   解析器处理粘包/半包——流式到达的任意切分都能
 *   正确组帧。同族 frame_line 是文本行分帧（见该范例）。
 */


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

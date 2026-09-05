#include <stdio.h>

#include <xrt.h>



/*
 * 范例：network/frame_line —— 流式行分帧（CRLF/LF 自适应）
 * ----------------------------------------------------------------
 * 演示 API：
 *   行帧解析器   分块输入 → 逐条文本帧（去行尾符）
 * 模块宏：XRT_MODULE_NET_FRAME
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/frame_line/main.c -lws2_32 -liphlpapi
 * 预期输出：（逐行输出输入文本）
 *
 * 与 io/line 分工：io 层面向 Reader（文件等）；
 *   本范例面向网络流收包路径，零中间缓冲。
 *   Redis/SMTP 类文本协议的分帧底座；粘包半包由
 *   解析器状态机吸收。
 */


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

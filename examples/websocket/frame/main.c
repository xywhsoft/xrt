#include <stdio.h>
#include <string.h>

#include <xrt.h>



/*
 * 范例：websocket/frame —— 帧头封包/解析与分段解掩码
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtWsFrameConfigInit + Mask = XWS_MASK_REQUIRED   服务端方向
 *   xrtWsFrameWrite   帧描述 → 线路帧头字节
 *   xrtWsFrameParse   帧头字节 → 借用解析（三态）
 *   xrtWsMask         负载掩码/解掩码（可按段推进）
 * 模块宏：XRT_MODULE_WEBSOCKET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/websocket/frame/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   opcode=1 payload=5 text=Hello
 *
 * 掩码样例 0x37FA213D 是 RFC 6455 第 5.7 节的规范示例——
 *   掩码按"负载下标 mod 4"循环异或；分段解掩码演示：
 *   前段结束时记住偏移（本例 0 与 2），下一段接着算——
 *   网络分片到达时无需等整帧凑齐。
 */


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

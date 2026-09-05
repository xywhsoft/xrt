#include <stdio.h>

#include <xrt.h>



/* 范例输出直接打印一条短文本消息。 */
static bool onText(xbytesview Data, ptr pData)
{
	(void)pData;
	return fwrite(
		Data.Data,
		1,
		Data.Size,
		stdout
	) == Data.Size;
}



/*
 * 范例：websocket/inflater —— 解压 permessage-deflate 消息
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtWsInflaterConfigInit / Create / Destroy
 *   xrtWsInflaterBegin / Write / End   与 Deflater 对称的三段式
 * 模块宏：XRT_MODULE_WEBSOCKET（DEFLATE 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/websocket/inflater/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   Hello permessage-deflate
 *
 * 输入是 deflater 范例产物的线路形态（无同步尾部）——
 *   End 会先补回尾部再收流，两侧互为逆运算验证。
 *   解出的明文分片直达回调（onText），零中间拼接。
 */


/* 解码一条去除同步尾部的 permessage-deflate 负载。 */
int main(void)
{
	static const uint8 Encoded[] = {
		0xF2, 0x48, 0xCD, 0xC9, 0xC9, 0x57, 0x28, 0x48,
		0x2D, 0xCA, 0x4D, 0x2D, 0x2E, 0x4E, 0x4C, 0x4F,
		0xD5, 0x4D, 0x49, 0x4D, 0xCB, 0x49, 0x2C, 0x49,
		0x05, 0x00
	};
	xwsinflaterconfig Config;
	xwsinflater* pInflater;

	xrtWsInflaterConfigInit(&Config);
	Config.NoContextTakeover = true;
	pInflater = xrtWsInflaterCreate(&Config);
	if ( (pInflater == NULL) ||
		!xrtWsInflaterBegin(pInflater, true) ||
		!xrtWsInflaterWrite(
			pInflater,
			(xbytesview){ Encoded, sizeof(Encoded) },
			onText,
			NULL
		) ||
		!xrtWsInflaterEnd(pInflater, onText, NULL) ) {
		xrtWsInflaterDestroy(pInflater);
		return 1;
	}
	xrtWsInflaterDestroy(pInflater);
	fputc('\n', stdout);
	return 0;
}

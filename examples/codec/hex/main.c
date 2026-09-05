/*
 * 范例：codec/hex —— 二进制数据的 HEX 编解码（显式长度，可含零字节）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHexEncodeNew  字节 → 大写/小写十六进制字符串（拥有式）
 *   xrtHexDecodeNew  十六进制视图 → 拥有式字节缓冲
 *   XHEX_UPPER       输出大写字母 A-F（默认小写 xhex_lower）
 * 模块宏：XRT_MODULE_CODEC
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/codec/hex/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   000102FEFF (5 bytes)
 *
 * 与字符串函数的区别：所有入口都以显式长度工作，
 * 缓冲中的 0x00 不会被当作结束符——这是"二进制安全"的含义。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	/* 注意首字节就是 0x00：任何依赖结尾零的 API 都会在这里截断。 */
	static const uint8 arrData[] = { 0, 1, 2, 0xFEu, 0xFFu };

	/* 编码：5 字节 → 10 个大写十六进制字符，产物由 xrtFree 释放。 */
	str sText = xrtHexEncodeNew(arrData, sizeof(arrData), (uint32)XHEX_UPPER);
	size_t iSize;
	bytes pData;

	if ( sText == NULL ) {
		return 1;
	}

	/*
	 * 解码：用视图显式传 10 个字符（sizeof(arrData) * 2），
	 * 末参 Flags=0（默认容错：允许成对空白；严格模式另有开关）。
	 */
	pData = xrtHexDecodeNew((xstrview){ sText, sizeof(arrData) * 2u }, &iSize, 0);
	if ( pData == NULL ) {
		xrtFree(sText);
		return 1;
	}

	/* 打印十六进制串与还原后的字节数（应为 5，含首部零字节）。 */
	printf("%s (%llu bytes)\n", sText, (unsigned long long)iSize);
	xrtFree(pData);
	xrtFree(sText);
	return 0;
}

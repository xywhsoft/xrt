/*
 * 范例：data/buffer_hex —— 十六进制文本一步解码为拥有型缓冲
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtBufferFromHex    HEX 视图 → 堆分配的 xbuffer*
 *   xrtBufferDestroy    释放 From* 系列返回的缓冲指针
 * 模块宏：XRT_MODULE_BUFFER（依赖 CODEC）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/data/buffer_hex/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   Hello
 *
 * 典型场景：调试转储、密钥指纹、二进制协议报文展示——
 *   这些地方拿到的都是 HEX 文本，FromHex 一步还原成
 *   可继续处理的字节缓冲（Size/Data 字段齐备）。
 * Flags=0 为默认容错（允许成对空白）；严格模式见 codec.h。
 */

#include <stdio.h>
#include <xrt.h>



int main(void)
{
	/* "48656c6c6f" 即 "Hello" 的十六进制（H=0x48 e=0x65 ...）。 */
	xbuffer* pBuffer = xrtBufferFromHex(
		XRT_STR_LITERAL("48656c6c6f"),
		0
	);

	if ( pBuffer == NULL ) {
		return 1;
	}
	printf(
		"%.*s\n",
		(int)pBuffer->Size,
		(const char*)pBuffer->Data
	);
	xrtBufferDestroy(pBuffer);
	return 0;
}

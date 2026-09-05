/*
 * 范例：compress/inflate —— 解码 gzip 数据并强制 trailer 校验
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtInflateConfigInit  默认解压配置（含尺寸上限防御）
 *   XINFLATE_GZIP         输入格式选择：gzip 容器（另有 RAW/ZLIB）
 *   xrtInflateAll         一次性解压：视图 + 配置 → 拥有式字节缓冲
 * 模块宏：XRT_MODULE_COMPRESS
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/compress/inflate/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   hello
 *
 * 安全面向不可信输入的设计：
 *   - gzip trailer（CRC32 + 原始长度）会与解压结果核对，不匹配即失败；
 *   - 配置带输出上限，"zip 炸弹"式输入会触顶失败而不是耗尽内存。
 * 流式场景另有 xrtInflateInit/Process/Finish 族。
 */

#include <stdio.h>
#include <string.h>

#include <xrt.h>



int main(void)
{
	/*
	 * 手工固定的合法 gzip 字节（内容 "hello"）：
	 *   1F 8B     魔数；08       deflate 方法；00       标志；
	 *   02        最慢压缩级；FF 未知 OS；
	 *   CB 48 CD C9 C9 07 00     deflate 流（"hello"）；
	 *   86 A6 10 36              CRC32；
	 *   05 00 00 00              原始长度 5。
	 * 用固定字节而非现场压缩，保证范例输出与压缩器版本解耦。
	 */
	static const uint8 Gzip[] = {
		0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x02, 0xFF, 0xCB, 0x48, 0xCD, 0xC9, 0xC9, 0x07,
		0x00, 0x86, 0xA6, 0x10, 0x36, 0x05, 0x00, 0x00,
		0x00
	};
	xinflateconfig Config;
	bytes pText;             /* 拥有式：解压产物 */
	size_t iSize;

	/* 默认配置 + 显式选择 gzip 容器格式。 */
	xrtInflateConfigInit(&Config);
	Config.Format = XINFLATE_GZIP;

	/* 一次性解压：CRC 与长度 trailer 任一不符都会返回 NULL。 */
	pText = xrtInflateAll(
		(xbytesview){ Gzip, sizeof(Gzip) },
		&Config,
		&iSize
	);
	if ( pText == NULL ) {
		return 1;
	}

	/* 二进制缓冲打印用显式长度。 */
	printf("%.*s\n", (int)iSize, (const char*)pText);
	xrtFree(pText);
	return 0;
}

/*
 * 范例：compress/deflate —— 一次性把字节压缩为 gzip 数据
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtDeflateConfigInit  用默认值初始化压缩配置（Level/Gzip 标志等）
 *   xrtDeflateAll         一次性压缩：输入视图 + 配置 → 拥有式 gzip 缓冲
 * 模块宏：XRT_MODULE_COMPRESS
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/compress/deflate/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   plain=34 gzip=30
 *
 * 输出为什么是 30：gzip 容器固定开销约 18 字节
 *   （10 字节头 + 8 字节 CRC32/长度 trailer），
 *   高度重复的 34 字节正文 deflate 后只占约 12 字节。
 * 数据是确定性的：同配置同输入永远产生逐字节相同的产物
 *   ——这对内容寻址/缓存/测试快照都是关键性质。
 * 流式（分块输入）场景另有 xrtDeflateInit/Process/Finish 族。
 */

#include <stdio.h>
#include <string.h>

#include <xrt.h>



int main(void)
{
	/* 高重复文本：deflate 的字典匹配最容易发挥的形态。 */
	static const char Text[] =
		"repeat repeat repeat repeat repeat";
	xdeflateconfig Config;
	bytes pGzip;             /* 拥有式：压缩产物 */
	size_t iSize;

	/* 默认配置即可产出合法 gzip；需要更高速率/更小体积时再调 Level。 */
	xrtDeflateConfigInit(&Config);

	/* 一次性压缩：输入用字面量视图（长度自动 = sizeof - 1）。 */
	pGzip = xrtDeflateAll(
		XRT_BYTES_LITERAL(Text),
		&Config,
		&iSize
	);
	if ( pGzip == NULL ) {
		return 1;
	}

	/* 对比压缩前后字节数。 */
	printf(
		"plain=%u gzip=%u\n",
		(unsigned int)(sizeof(Text) - 1u),
		(unsigned int)iSize
	);
	xrtFree(pGzip);
	return 0;
}

/*
 * 范例：charset/transcode —— UTF-8 到 UTF-16 LE 的带 BOM 封包转换
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtTranscode     任意内置编码间的全向转换（UTF-8/16/32 + Latin-1）
 *   XRT_BYTES_LITERAL 编译期构造字节视图（长度 = sizeof - 1，不含零）
 *   XUTF_STRICT      严格错误策略：非法序列直接失败（另有 REPLACE/IGNORE）
 * 模块宏：XRT_MODULE_CHARSET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/charset/transcode/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   UTF-16 LE bytes with BOM: 14
 *
 * 字节数推算："XRT 你好" 共 6 个码点（X/R/T/空格/你/好），
 *   全部落在 BMP 内，每码点 2 字节 = 12，加 LE BOM（FF FE）2 字节 = 14。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	/* 源文本 "XRT 你好"（UTF-8 字面量，"你好"各占 3 字节）。 */
	xbytesview Source = XRT_BYTES_LITERAL("XRT \xE4\xBD\xA0\xE5\xA5\xBD");
	bytes pUtf16;             /* 拥有式：转换产物 */
	size_t iSize = 0;

	/*
	 * 六个参数：
	 *   源视图 / 源编码 / 目标编码 / 错误策略 / 是否写 BOM / 出参长度。
	 * bBom = true：产物开头带 FF FE（UTF-16 LE 签名），
	 * Windows API（如 WideCharToMultiByte 链路）常要求这种封包。
	 * STRICT 下任何非法 UTF-8 都会返回 NULL 并设置线程错误。
	 */
	pUtf16 = xrtTranscode(Source, XENCODING_UTF8, XENCODING_UTF16_LE,
		XUTF_STRICT, true, &iSize);
	if ( pUtf16 == NULL ) {
		return 1;
	}

	/* 打印产物总字节数（含 BOM），证明转换真实发生。 */
	printf("UTF-16 LE bytes with BOM: %llu\n", (unsigned long long)iSize);

	/* 拥有式产物由 xrtFree 释放。 */
	xrtFree(pUtf16);
	return 0;
}

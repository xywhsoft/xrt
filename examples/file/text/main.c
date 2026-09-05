#include <stdio.h>

#include <xrt.h>



/*
 * 范例：file/text —— 文本文件：指定编码写入与自动探测读回
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtFileWriteText   按编码写文本（含 BOM 控制）
 *   xrtFileReadText    读回 UTF-8（UNKNOWN 自动探测）
 * 模块宏：XRT_MODULE_FILE（依赖 CHARSET）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/file/text/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   Hello, XRT
 *
 * 写 UTF-16 LE + BOM（Windows 工具友好），读侧
 *   XENCODING_UNKNOWN 自动探测并统一转成 UTF-8——
 *   调用方只处理一种编码，跨平台乱码在这层终结。
 */


/* 展示 UTF-8 文本与带 BOM UTF-16 文件之间的一次往返。 */
int main(void)
{
	static const char sPath[] = "xrt-file-text-example.tmp";
	str sText;
	size_t iSize;

	if ( !xrtFileWriteText(sPath, XRT_STR_LITERAL("Hello, XRT"),
		XENCODING_UTF16_LE, XUTF_STRICT, true) ) {
		return 1;
	}
	sText = xrtFileReadText(sPath, XENCODING_UNKNOWN, XUTF_STRICT, &iSize);
	if ( sText == NULL ) {
		return 1;
	}
	printf("%.*s\n", (int)iSize, sText);
	xrtFree(sText);
	return xrtFileDelete(sPath) ? 0 : 1;
}

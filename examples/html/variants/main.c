/*
 * 范例：html/variants —— HTML 转义补遗：长度查询与缓冲写出
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHtmlEscapeSize   严格校验 UTF-8 并返回转义后精确字节数
 *   xrtHtmlEscapeWrite  转义到调用方缓冲（两用查询模式）
 * 模块宏：XRT_MODULE_HTML
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/html/variants/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   size=9
 *   escaped=&lt;a&gt;
 *
 * Size 先行是"分配友好"姿势：查长度→开精确缓冲→写入。
 *   与分配版 xrtHtmlEscape 的分工同 string 族惯例。
 */

#include <stdio.h>
#include <xrt.h>

int main(void)
{
	char Buffer[32];
	size_t iSize = 0;

	/* 查询："<a>" 转义为 "&lt;a&gt;" 共 9 字节（4+1+4，不含零）。 */
	if ( !xrtHtmlEscapeSize(XRT_STR_LITERAL("<a>"),
		XHTML_ESCAPE_TEXT, &iSize) ) {
		return 1;
	}
	printf("size=%zu\n", iSize);

	/* 写入：容量含结尾零；同一模式返回 true。 */
	if ( !xrtHtmlEscapeWrite(XRT_STR_LITERAL("<a>"),
		XHTML_ESCAPE_TEXT, Buffer, sizeof(Buffer), &iSize) ) {
		return 2;
	}
	printf("escaped=%s\n", Buffer);
	return 0;
}

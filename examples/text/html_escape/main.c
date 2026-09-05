/*
 * 范例：text/html_escape —— HTML 文本节点与属性值的双模式转义
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHtmlEscape         按上下文模式转义（拥有式输出，xrtFree）
 *   XHTML_ESCAPE_TEXT     文本节点模式：& < >（含非 ASCII 原样保留）
 *   XHTML_ESCAPE_ATTRIBUTE 属性值模式：额外转义 " 和 '
 * 模块宏：XRT_MODULE_HTML
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/text/html_escape/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   <p title="Tom&#39;s &quot;report&quot;">状态：&lt;ready&gt; &amp; 可用</p>
 *
 * 为什么要分两种模式：HTML 里文本节点与属性值的危险字符集不同——
 *   属性值可能被单/双引号包裹，' 和 " 必须转义；
 *   文本节点不需要，多转义只会让输出更难读。
 * 中文等非 ASCII 字符按 UTF-8 原样保留（不需要实体化）。
 * 第三参数可传自定义映射表覆盖默认实体（本例 NULL 用默认）。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	str sText;
	str sAttribute;

	/* 文本节点：只动 & < >，"状态：" 原样通过。 */
	sText = xrtHtmlEscape(
		XRT_STR_LITERAL("状态：<ready> & 可用"),
		XHTML_ESCAPE_TEXT,
		NULL
	);

	/* 属性值：' 与 " 一并转义（&#39; &quot;）。 */
	sAttribute = xrtHtmlEscape(
		XRT_STR_LITERAL("Tom's \"report\""),
		XHTML_ESCAPE_ATTRIBUTE,
		NULL
	);
	if ( (sText == NULL) || (sAttribute == NULL) ) {
		xrtFree(sAttribute);
		xrtFree(sText);
		return 1;
	}

	/* 拼回一个完整且注入安全的 <p> 元素。 */
	printf("<p title=\"%s\">%s</p>\n", sAttribute, sText);
	xrtFree(sAttribute);
	xrtFree(sText);
	return 0;
}

#include <stdio.h>

#include <xrt.h>



/* 演示分别转义 HTML 文本节点和带引号属性值。 */
int main(void)
{
	str sText;
	str sAttribute;

	sText = xrtHtmlEscape(
		XRT_STR_LITERAL("状态：<ready> & 可用"),
		XHTML_ESCAPE_TEXT,
		NULL
	);
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
	printf("<p title=\"%s\">%s</p>\n", sAttribute, sText);
	xrtFree(sAttribute);
	xrtFree(sText);
	return 0;
}

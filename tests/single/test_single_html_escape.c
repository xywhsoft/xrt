#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件中的 HTML 属性转义主路径。 */
int main(void)
{
	char Buffer[32];
	size_t iSize;

	if ( !xrtHtmlEscapeWrite(
		XRT_STR_LITERAL("A&\"B"), XHTML_ESCAPE_ATTRIBUTE,
		Buffer, sizeof(Buffer), &iSize
	) || (iSize != 13u) ||
		(strcmp(Buffer, "A&amp;&quot;B") != 0) ) {
		return 1;
	}
	printf("[PASS] single-html-escape\n");
	return 0;
}

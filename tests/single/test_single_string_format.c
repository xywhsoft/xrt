#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件中的字符串格式化模块。 */
int main(void)
{
	str sText = xrtFormat("%s-%d", "single", 2);

	if ( (sText == NULL) || (strcmp(sText, "single-2") != 0) ) {
		xrtFree(sText);
		return 1;
	}
	xrtFree(sText);
	printf("[PASS] single-string-format\n");
	return 0;
}

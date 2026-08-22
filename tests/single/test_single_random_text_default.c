#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件中的当前线程随机文本模块。 */
int main(void)
{
	str sText = xrtRandString(12);

	if ( (sText == NULL) || (strlen(sText) != 12) ) {
		xrtFree(sText);
		return 1;
	}
	xrtFree(sText);
	printf("[PASS] single-random-text-default\n");
	return 0;
}

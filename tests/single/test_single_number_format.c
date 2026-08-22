#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件中的数字展示格式层。 */
int main(void)
{
	char sOutput[64];
	size_t iSize;

	if ( !xrtIntFormatTo(INT64_MIN, XRT_STR_LITERAL(",d"),
			sOutput, sizeof(sOutput), &iSize) ||
		 (strcmp(sOutput, "-9,223,372,036,854,775,808") != 0) ) {
		return 1;
	}
	if ( !xrtNumFormatTo(0.125, XRT_STR_LITERAL(".1%"),
			sOutput, sizeof(sOutput), &iSize) ||
		 (strcmp(sOutput, "12.5%") != 0) ) {
		return 2;
	}
	if ( !xrtIntFormatTo(20320, XRT_STR_LITERAL("c"),
			sOutput, sizeof(sOutput), &iSize) ||
		 (strcmp(sOutput, "\xE4\xBD\xA0") != 0) ) {
		return 3;
	}
	printf("[PASS] single-number-format\n");
	return 0;
}

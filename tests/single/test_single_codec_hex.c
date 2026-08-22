#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件中的 HEX codec。 */
int main(void)
{
	char arrText[8];
	size_t iSize;

	if ( !xrtHexEncode("x", 1, arrText, sizeof(arrText), &iSize, 0) ||
		 (strcmp(arrText, "78") != 0) ) {
		return 1;
	}
	printf("[PASS] single-codec-hex\n");
	return 0;
}

#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件中的显式随机文本模块。 */
int main(void)
{
	xrng Rng = XRT_RNG_INITIALIZER;
	str sText = xrtRngString(&Rng, 12);

	if ( (sText == NULL) || (strlen(sText) != 12) ) {
		xrtFree(sText);
		return 1;
	}
	xrtFree(sText);
	printf("[PASS] single-random-text\n");
	return 0;
}

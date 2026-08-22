#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件中的精确浮点写出和解析。 */
int main(void)
{
	char sOutput[64];
	size_t iSize;
	double fValue;

	if ( !xrtNumWrite(
			3.141592653589793,
			sOutput,
			sizeof(sOutput),
			&iSize,
			0
		) ||
		(strcmp(sOutput, "3.141592653589793") != 0) ||
		!xrtNumParse(
			(xstrview){ sOutput, iSize }, 0, &fValue) ||
		(fValue != 3.141592653589793) ) {
		return 1;
	}
	printf("[PASS] single-number-float\n");
	return 0;
}

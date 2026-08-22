#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件中的整数数值层。 */
int main(void)
{
	char sOutput[32];
	size_t iSize;
	uint64 iValue;

	if ( !xrtUIntWrite(UINT64_C(255), 16, sOutput, sizeof(sOutput),
			&iSize, (uint32)XNUMBER_PREFIX) ||
		 (strcmp(sOutput, "0xff") != 0) ||
		 !xrtUIntParse((xstrview){ sOutput, iSize }, 0,
			(uint32)XNUMBER_PARSE_PREFIX, &iValue) ||
		 (iValue != UINT64_C(255)) ) {
		return 1;
	}
	printf("[PASS] single-number-integer\n");
	return 0;
}

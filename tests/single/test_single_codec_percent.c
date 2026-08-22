#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件中的 percent 原地编解码主路径。 */
int main(void)
{
	uint8 Buffer[32];
	uint8 iValue;
	size_t iOffset = 0;
	size_t iSize;

	memcpy(Buffer, "a b", 3);
	if ( !xrtPercentEncode(
		Buffer, 3, XRT_STR_LITERAL(""),
		(char*)Buffer, sizeof(Buffer), &iSize
	) || (strcmp((char*)Buffer, "a%20b") != 0) ||
		!xrtPercentDecode(
			(xstrview){ (cstr)Buffer, iSize }, Buffer, sizeof(Buffer), &iSize
		) || (iSize != 3) || (memcmp(Buffer, "a b", 3) != 0) ) {
		return 1;
	}
	if ( (xrtPercentNext(
		XRT_STR_LITERAL("%2F"), false, &iOffset, &iValue
	) != XPERCENT_NEXT_BYTE) || (iValue != (uint8)'/') ||
		(xrtPercentNext(
			XRT_STR_LITERAL("%2F"), false, &iOffset, &iValue
		) != XPERCENT_NEXT_END) ) {
		return 2;
	}
	if ( !xrtPercentWrite(
		"a b", 3, XRT_STR_LITERAL(""),
		(char*)Buffer, 5, &iSize
	) || (iSize != 5) ||
		(memcmp(Buffer, "a%20b", 5) != 0) ) {
		return 3;
	}
	printf("[PASS] single-codec-percent\n");
	return 0;
}

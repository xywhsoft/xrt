#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件中的字符串核心和构建器。 */
int main(void)
{
	xstrbuf tBuffer;
	char arrFiltered[16];
	size_t iFiltered;
	str sText;

	if ( !xrtStrFilterTo(XRT_STR_LITERAL("a1b2"), XRT_STR_LITERAL("12"),
			arrFiltered, sizeof(arrFiltered), &iFiltered) ||
		 (iFiltered != 2) || (strcmp(arrFiltered, "ab") != 0) ) {
		return 1;
	}
	xrtStrBufInit(&tBuffer);
	if ( !xrtStrBufAppend(&tBuffer, XRT_STR_LITERAL("single")) ||
		 !xrtStrBufAppendByte(&tBuffer, '-') ||
		 !xrtStrBufAppend(&tBuffer, XRT_STR_LITERAL("string")) ) {
		return 1;
	}
	sText = xrtStrBufTake(&tBuffer);
	if ( (sText == NULL) || (strcmp(sText, "single-string") != 0) ) {
		xrtFree(sText);
		return 3;
	}
	xrtFree(sText);
	printf("[PASS] single-string\n");
	return 0;
}

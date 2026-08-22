#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件中的 Unicode 文本模块。 */
int main(void)
{
	str sText = xrtUtf8Reverse(XRT_STR_LITERAL("A你😀"));
	str sFiltered;
	xstrview Range;

	if ( (sText == NULL) || (strcmp(sText, "😀你A") != 0) ) {
		xrtFree(sText);
		return 1;
	}
	xrtFree(sText);
	sFiltered = xrtUtf8Filter(XRT_STR_LITERAL("A你B好"),
		XRT_STR_LITERAL("你"));
	if ( (sFiltered == NULL) || (strcmp(sFiltered, "AB好") != 0) ) {
		xrtFree(sFiltered);
		return 2;
	}
	xrtFree(sFiltered);
	if ( !xrtUtf8Range(XRT_STR_LITERAL("A你😀"), -2, 1, &Range) ||
		 (Range.Size != (sizeof("你") - 1u)) ||
		 (memcmp(Range.Data, "你", Range.Size) != 0) ) {
		return 3;
	}
	sText = xrtUtf8PadLeft(XRT_STR_LITERAL("你"), 3,
		XRT_STR_LITERAL("好"));
	if ( (sText == NULL) || (strcmp(sText, "好好你") != 0) ) {
		xrtFree(sText);
		return 4;
	}
	xrtFree(sText);
	printf("[PASS] single-unicode-text\n");
	return 0;
}

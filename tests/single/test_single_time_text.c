#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供协议时间解析和格式化。 */
int main(void)
{
	char arrText[30];
	xtime iTime;

	if ( !xrtTimeParseHTTPDate(
		XRT_STR_LITERAL("Sun, 06 Nov 1994 08:49:37 GMT"), &iTime) ) {
		return 1;
	}
	return (xrtTimeWriteHTTPDate(arrText, sizeof(arrText), iTime) == 29) &&
		(strcmp(arrText, "Sun, 06 Nov 1994 08:49:37 GMT") == 0) ? 0 : 1;
}

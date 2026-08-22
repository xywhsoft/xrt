#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>



/* 验证单头文件中的 Unicode 编辑距离模块。 */
int main(void)
{
	if ( xrtUtf8Distance(XRT_STR_LITERAL("你好"), XRT_STR_LITERAL("你们"),
		XRT_NPOS) != 1 ) {
		return 1;
	}
	printf("[PASS] single-unicode-distance\n");
	return 0;
}

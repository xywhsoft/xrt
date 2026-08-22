#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>



/* 验证单头文件中的严格 UTF-8 通配模块。 */
int main(void)
{
	if ( !xrtStrGlob(XRT_STR_LITERAL("xrt.h"), XRT_STR_LITERAL("xrt.[ch]"), 0) ) {
		return 1;
	}
	printf("[PASS] single-string-glob\n");
	return 0;
}

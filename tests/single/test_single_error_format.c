#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件中的独立错误格式化模块。 */
int main(void)
{
	xrtSetErrorFormat(XERR_IO, "single.error", 7, "item=%d", 42);
	if (
		(xrtErrorKind(xrtGetError()) != XERR_IO) ||
		(strcmp(xrtErrorMessage(xrtGetError()), "item=42") != 0)
	) {
		return 1;
	}
	xrtClearError();
	printf("[PASS] single-error-format\n");
	return 0;
}

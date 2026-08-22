#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件包含 TLS Dial Future API 与入口参数错误契约。 */
int main(void)
{
	xrtClearError();
	if ( (xrtTlsDialAsync(
		NULL,
		NULL,
		NULL,
		0,
		NULL,
		NULL,
		NULL,
		NULL
	) != NULL) ||
		(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ) {
		return 1;
	}
	return 0;
}

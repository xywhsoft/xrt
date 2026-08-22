#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件发布 HTTP/1 TLS 调用入口。 */
int main(void)
{
	xrtClearError();
	if ( xrtHttp1CallTls(
		NULL,
		NULL,
		NULL,
		NULL
	) || (xrtGetError() == NULL) ||
		(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ) {
		return 1;
	}
	return 0;
}

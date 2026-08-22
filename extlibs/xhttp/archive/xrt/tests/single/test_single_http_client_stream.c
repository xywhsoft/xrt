#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件发布 HTTP/1 TCP 调用配置和错误契约。 */
int main(void)
{
	xhttp1callconfig Config;

	xrtHttp1CallConfigInit(&Config);
	if ( Config.WriteSize != 16384u ) {
		return 1;
	}
	xrtClearError();
	if ( xrtHttp1CallTcp(
		NULL,
		NULL,
		NULL,
		NULL
	) || (xrtGetError() == NULL) ||
		(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ) {
		return 2;
	}
	return 0;
}

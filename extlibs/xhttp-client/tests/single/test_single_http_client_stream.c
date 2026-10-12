#define XHTTP_POLICY_IMPLEMENTATION
#define XHTTP_CLIENT_IMPLEMENTATION
#define XHTTP_IMPLEMENTATION
#include "../../include/xhttp-client/features.h"
#include <xhttp-policy/features.h>
#include <xhttp/features.h>
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xhttp.h"
#include "../../../../single/extlibs/xhttp-policy.h"
#include "../../../../single/extlibs/xhttp-client.h"




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

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

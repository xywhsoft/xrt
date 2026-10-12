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




/* 验证单头文件发布高层 HTTP Client 的默认配置和参数错误。 */
int main(void)
{
	xhttpclientconfig Client;
	xhttpcalloptions Call;
	xhttpcallinfo Info;

	xrtHttpClientConfigInit(&Client);
	xrtHttpCallOptionsInit(&Call);
	if ( (Client.Timeout != XHTTP_CLIENT_TIMEOUT_DEFAULT) ||
		(Client.IdleTimeout !=
		 XHTTP_CLIENT_IDLE_TIMEOUT_DEFAULT) ||
		(Call.Timeout != 0) ||
		(Call.IdleTimeout != 0) ||
		(Call.Cancel != NULL) ) {
		return 1;
	}
	xrtClearError();
	if ( (xrtHttpClientDo(
		NULL,
		NULL,
		NULL,
		NULL,
		NULL
	) != NULL) ||
		(xrtGetError() == NULL) ||
		(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ) {
		return 2;
	}
	xrtClearError();
	if ( xrtHttpCallInfo(NULL, &Info) ||
		(xrtGetError() == NULL) ||
		(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ) {
		return 3;
	}
	return 0;
}

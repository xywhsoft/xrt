#define XHTTP_POLICY_IMPLEMENTATION
#define XHTTP_CLIENT_IMPLEMENTATION
#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#ifndef XHTTP_CLIENT_MODULE_HTTP_CLIENT_EASY_FUTURE
	#define XHTTP_CLIENT_MODULE_HTTP_CLIENT_EASY_FUTURE
#endif
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




/* 验证单头文件发布 Future 与同步便利入口。 */
int main(void)
{
	xrtClearError();
	if ( (xrtHttpClientGetAsync(
		NULL,
		XRT_STR_LITERAL("http://example.test/"),
		NULL
	) != NULL) ||
		(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ) {
		return 1;
	}
	xrtClearError();
	if ( (xrtHttpClientPostSync(
		NULL,
		XRT_STR_LITERAL("http://example.test/"),
		XRT_BYTES_LITERAL("{}"),
		XRT_STR_LITERAL("application/json"),
		NULL
	) != NULL) ||
		(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ) {
		return 2;
	}
	return 0;
}



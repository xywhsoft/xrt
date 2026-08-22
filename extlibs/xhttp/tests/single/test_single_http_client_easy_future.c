#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#ifndef XHTTP_MODULE_HTTP_CLIENT_EASY_FUTURE
	#define XHTTP_MODULE_HTTP_CLIENT_EASY_FUTURE
#endif
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



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



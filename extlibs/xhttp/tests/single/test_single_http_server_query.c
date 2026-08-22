#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_SERVER_QUERY
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头 Query 请求辅助入口和稳定错误域。 */
int main(void)
{
	bool bFailed = xrtHttpServerRequestQueryParams(
		NULL,
		NULL,
		NULL
	) == NULL;
	const xerror* pError = xrtGetError();

	return bFailed && (pError != NULL) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.http.server.request"
		 ) == 0) ? 0 : 1;
}


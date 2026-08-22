#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



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

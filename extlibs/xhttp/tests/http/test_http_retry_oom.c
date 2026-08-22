#include "../test_allocator.h"

#include <xrt/http_retry.h>



/* 分配型 Retry-After Helper 在 OOM 时不得发布长度或部分结果。 */
int main(void)
{
	const xhttpretryafter Retry = {
		XHTTP_RETRY_AFTER_DELAY,
		UINT64_C(3),
		0
	};
	size_t iSize = 77u;

	testRequire(
		testInstallFailAllocator(),
		"HTTP retry OOM allocator install failed"
	);
	testRequire(
		(xrtHttpRetryAfterBuild(&Retry, &iSize) == NULL) &&
		(iSize == 77u) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"Retry-After build OOM was not atomic"
	);
	return 0;
}



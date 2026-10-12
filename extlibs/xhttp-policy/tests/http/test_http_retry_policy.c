#include "../test_allocator.h"
#include <xrt/http_retry_policy.h>

/* 验证默认状态集合和封顶指数退避的精确边界。 */
static void testHttpRetryPolicy(void)
{
	static const uint16 Retry[] = {
		408, 421, 425, 429, 500, 502, 503, 504
	};
	static const uint16 NoRetry[] = {
		200, 301, 400, 401, 404, 409, 501, 505
	};
	uint64 iDelay;
	size_t i;

	for ( i = 0; i < (sizeof(Retry) / sizeof(Retry[0])); i++ ) {
		testRequire(
			xrtHttpRetryStatusDefault(Retry[i]),
			"temporary status was not retryable"
		);
	}
	for ( i = 0; i < (sizeof(NoRetry) / sizeof(NoRetry[0])); i++ ) {
		testRequire(
			!xrtHttpRetryStatusDefault(NoRetry[i]),
			"permanent status was retryable"
		);
	}
	testRequire(
		xrtHttpRetryBackoff(100000, 1600000, 0, &iDelay) &&
		(iDelay == 100000) &&
		xrtHttpRetryBackoff(100000, 1600000, 3, &iDelay) &&
		(iDelay == 800000) &&
		xrtHttpRetryBackoff(100000, 1600000, 4, &iDelay) &&
		(iDelay == 1600000) &&
		xrtHttpRetryBackoff(100000, 1600000, UINT32_MAX, &iDelay) &&
		(iDelay == 1600000) &&
		xrtHttpRetryBackoff(1, UINT64_MAX, UINT32_MAX, &iDelay) &&
		(iDelay == UINT64_MAX) &&
		xrtHttpRetryBackoff(0, 1600000, UINT32_MAX, &iDelay) &&
		(iDelay == 0),
		"HTTP retry backoff mismatch"
	);
	iDelay = UINT64_C(77);
	testRequire(
		!xrtHttpRetryBackoff(2, 1, 0, &iDelay) &&
		(iDelay == UINT64_C(77)) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP retry invalid backoff was not atomic"
	);
	xrtClearError();
}



int main(void) {
	uint64 iDelay;
	testHttpRetryPolicy();
	testRequire(
		!xrtHttpRetryBackoff(
			1,
			2,
			0,
			(uint64*)(uintptr_t)(UINTPTR_MAX - 3u)
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP retry backoff accepted a wrapped output"
	);
	xrtClearError();
	puts("[PASS] HTTP retry policy");
	testRequire(testInstallFailAllocator(), "retry policy allocator install failed");
	testRequire(xrtHttpRetryBackoff(1000, 8000, 3, &iDelay) &&
		iDelay == 8000 && xrtHttpRetryStatusDefault(503), "retry policy allocated memory");
	return 0;
}

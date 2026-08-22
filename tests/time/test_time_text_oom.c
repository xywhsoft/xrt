#include "../test_allocator.h"



/* 分配型时间格式化必须把内存不足原样暴露给调用方。 */
int main(void)
{
	xdatetime tDateTime = {
		2024, 2, 29, 23, 58, 57, 654321, 0, 4, 60, -1
	};

	testRequire(testInstallFailAllocator(), "failure allocator install failed");
	testRequire(xrtDateTimeFormat(
		&tDateTime, XRT_STR_LITERAL("%F %T.%f")) == NULL,
		"date-time format allocation should fail");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"date-time format OOM error mismatch");
	testRequire(xrtTimeFormat(
		0, 0, XRT_STR_LITERAL("%F %T")) == NULL,
		"time format allocation should fail");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"time format OOM error mismatch");
	testRequire(xrtTimeRFC3339(0, 0) == NULL,
		"RFC 3339 allocation should fail");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"RFC 3339 OOM error mismatch");
	testRequire(xrtTimeHTTPDate(0) == NULL,
		"HTTP-date allocation should fail");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP-date OOM error mismatch");
	return 0;
}

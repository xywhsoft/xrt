#include "../test_allocator.h"

#include <xrt/http_forwarded.h>



/* Forwarded Build 在 OOM 下保持可选长度输出不变。 */
int main(void)
{
	static const xhttpforwardedvalue Element = {
		XRT_STR_INIT("192.0.2.43"),
		XRT_STR_INIT(""),
		XRT_STR_INIT(""),
		XRT_STR_INIT(""),
		NULL,
		0,
		XHTTP_FORWARDED_HAS_FOR
	};
	size_t iSize = 77u;

	testRequire(testInstallFailAllocator(),
		"Forwarded build failure allocator install failed");
	testRequire(
		(xrtHttpForwardedBuild(
			&Element, 1u, &iSize
		) == NULL) && (iSize == 77u) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"Forwarded build OOM was not atomic"
	);
	printf("[PASS] http_forwarded_write_oom\n");
	return 0;
}

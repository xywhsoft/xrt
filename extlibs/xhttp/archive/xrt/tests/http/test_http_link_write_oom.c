#include "../test_allocator.h"

#include <xrt/http_link.h>



/* Link Build 必须传播分配失败且不泄漏。 */
int main(void)
{
	static const xhttplinkvalue Link = {
		XRT_STR_INIT("/next"),
		XRT_STR_INIT("next"),
		NULL,
		0
	};
	size_t iSize = 71u;

	testRequire(testInstallFailAllocator(),
		"Link build failure allocator install failed");
	testRequire(
		(xrtHttpLinkBuild(&Link, 1u, &iSize) == NULL) &&
		(iSize == 71u) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"Link Build did not propagate OOM"
	);
	printf("[PASS] http_link_write_oom\n");
	return 0;
}

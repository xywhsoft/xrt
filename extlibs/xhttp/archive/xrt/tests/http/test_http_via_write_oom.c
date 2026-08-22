#include "../test_allocator.h"

#include <xrt/http_via.h>



/* Via Build 必须传播分配失败且不修改长度输出。 */
int main(void)
{
	static const xhttpviavalue Via = {
		XRT_STR_INIT(""),
		XRT_STR_INIT("1.1"),
		XRT_STR_INIT("edge"),
		XRT_STR_INIT(""),
		XRT_STR_INIT(""),
		0
	};
	size_t iSize = 71u;

	testRequire(testInstallFailAllocator(),
		"Via build failure allocator install failed");
	testRequire(
		(xrtHttpViaBuild(&Via, 1u, &iSize) == NULL) &&
		(iSize == 71u) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"Via Build did not propagate OOM atomically"
	);
	printf("[PASS] http_via_write_oom\n");
	return 0;
}

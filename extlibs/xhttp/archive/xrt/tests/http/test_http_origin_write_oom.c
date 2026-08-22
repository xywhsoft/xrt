#include "../test_allocator.h"

#include <xrt/http_origin.h>



/* Origin Build 必须传播分配失败并保持长度输出。 */
int main(void)
{
	xhttporigin Origin;
	size_t iSize = 73u;

	testRequire(
		xrtHttpOriginParse(
			XRT_STR_LITERAL("https://oom.test"), &Origin
		),
		"Origin OOM setup failed"
	);
	testRequire(testInstallFailAllocator(),
		"Origin OOM allocator install failed");
	testRequire(
		(xrtHttpOriginBuild(&Origin, 1u, &iSize) == NULL) &&
		(iSize == 73u) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"Origin Build did not preserve OOM contract"
	);
	printf("[PASS] http_origin_write_oom\n");
	return 0;
}

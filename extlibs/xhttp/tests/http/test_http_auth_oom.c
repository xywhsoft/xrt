#include "../test_allocator.h"

#include <xrt/http_auth.h>



/* 分配型认证构建器失败时不得修改调用方长度输出。 */
int main(void)
{
	size_t iSize = 77u;

	testRequire(testInstallFailAllocator(),
		"HTTP auth failure allocator install failed");
	testRequire((xrtHttpAuthBuild(
		XRT_STR_LITERAL("Bearer"),
		XRT_STR_LITERAL("token"),
		&iSize
	) == NULL) && (iSize == 77u) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP auth builder OOM was not atomic");
	puts("[PASS] HTTP authentication OOM");
	return 0;
}

#include "../test_allocator.h"



/* 分配型参数便捷函数在 OOM 下不得修改长度结果。 */
int main(void)
{
	size_t iSize = 77;

	testRequire(testInstallFailAllocator(),
		"HTTP parameter OOM allocator install failed");
	testRequire((xrtHttpQuotedBuild(
		XRT_STR_LITERAL("value"), &iSize
	) == NULL) && (iSize == 77) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP quoted build OOM was not atomic");
	xrtClearError();
	testRequire((xrtHttpParamBuild(
		XRT_STR_LITERAL("name"), XRT_STR_LITERAL("value"),
		XHTTP_PARAM_HAS_VALUE, &iSize
	) == NULL) && (iSize == 77) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP parameter build OOM was not atomic");
	printf("[PASS] http_param_oom\n");
	return 0;
}

#include "../test_allocator.h"



/* 验证全部 form-urlencoded 分配型便利函数的 OOM 原子性。 */
int main(void)
{
	static const xformfield Field = {
		XRT_BYTES_INIT("a"), XRT_BYTES_INIT("1")
	};
	size_t iSize = 77;

	testRequire(testInstallFailAllocator(),
		"form failure allocator install failed");
	testRequire(xrtFormEncodeNew("a b", 3, &iSize) == NULL &&
		(iSize == 77), "form allocated encode OOM was not atomic");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"form allocated encode OOM error mismatch");
	xrtClearError();
	testRequire(xrtFormDecodeNew(
		XRT_STR_LITERAL("a+b"), &iSize
	) == NULL && (iSize == 77),
		"form allocated decode OOM was not atomic");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"form allocated decode OOM error mismatch");
	xrtClearError();
	testRequire(xrtFormBuild(&Field, 1, &iSize) == NULL &&
		(iSize == 77), "form allocated build OOM was not atomic");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"form allocated build OOM error mismatch");
	printf("[PASS] form_urlencoded_oom\n");
	return 0;
}

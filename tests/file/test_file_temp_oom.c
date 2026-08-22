#include "../test_allocator.h"



/* 临时名称分配失败不得创建对象或保留伪路径。 */
int main(void)
{
	str sPath = (str)(uintptr_t)1u;

	testRequire(testInstallFailAllocator(),
		"failure allocator install failed");
	testRequire(xrtFileTemp(".", "xrt-temp-oom-", ".tmp", &sPath) == NULL,
		"temporary file survived forced OOM");
	testRequire((sPath == NULL) && (xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"temporary-file OOM reported the wrong result");
	return 0;
}

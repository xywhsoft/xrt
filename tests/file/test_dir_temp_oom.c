#include "../test_allocator.h"



/* 临时目录名称分配失败不得创建半成品目录。 */
int main(void)
{
	testRequire(testInstallFailAllocator(),
		"failure allocator install failed");
	testRequire(xrtDirTemp(".", "xrt-dir-temp-oom-", NULL) == NULL,
		"temporary directory survived forced OOM");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"temporary-directory OOM reported the wrong error");
	return 0;
}

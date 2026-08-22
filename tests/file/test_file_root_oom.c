#include "../test_allocator.h"



/* 初始根创建遇到分配失败时必须返回统一内存错误。 */
int main(void)
{
	testRequire(testInstallFailAllocator(),
		"root failure allocator install failed");
	testRequire(xrtRootOpen(".") == NULL,
		"root creation survived forced OOM");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"root OOM reported the wrong error");
	return 0;
}

#include "../test_allocator.h"



/* 验证 OOM 错误报告本身不申请内存。 */
int main(void)
{
	testRequire(testInstallFailAllocator(), "failure allocator install failed");
	testRequire(xrtMalloc(32) == NULL, "allocation should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "allocation failure must report memory error");
	testRequire(strcmp(xrtErrorDomain(xrtGetError()), "xrt.memory") == 0, "OOM domain mismatch");

	xrtClearError();
	testRequire(xrtErrorCreate(XERR_IO, "test", 1, "cannot allocate") == NULL, "error allocation should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "error allocation failure must preserve OOM");
	xrtClearError();
	printf("[PASS] oom\n");
	return 0;
}

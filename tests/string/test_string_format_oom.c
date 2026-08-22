#include "../test_allocator.h"



/* 验证格式化分配失败时报告 OOM。 */
int main(void)
{
	testRequire(testInstallFailAllocator(), "failure allocator install failed");
	testRequire(xrtFormat("value=%d", 42) == NULL, "format should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "format OOM error mismatch");
	xrtClearError();
	printf("[PASS] string-format-oom\n");
	return 0;
}

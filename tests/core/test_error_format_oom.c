#include "../test_allocator.h"



/* 验证长错误消息分配失败时保留无分配 OOM 错误。 */
int main(void)
{
	char sLongMessage[700];

	memset(sLongMessage, 'x', sizeof(sLongMessage) - 1u);
	sLongMessage[sizeof(sLongMessage) - 1u] = '\0';
	testRequire(testInstallFailAllocator(), "failure allocator install failed");
	xrtSetErrorFormat(XERR_IO, "test.format", 1, "%s", sLongMessage);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"error format OOM mismatch");
	xrtClearError();
	printf("[PASS] error-format-oom\n");
	return 0;
}

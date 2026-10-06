#include "../test_allocator.h"



/* 验证长错误消息分配失败时保留无分配 OOM 错误。 */
int main(void)
{
	char sLongMessage[700];

	memset(sLongMessage, 'x', sizeof(sLongMessage) - 1u);
	sLongMessage[sizeof(sLongMessage) - 1u] = '\0';
	testRequire(testInstallFailAllocator(), "failure allocator install failed");
	{
		xerrordescview Desc = { 0 };
		Desc.Kind = XERR_IO;
		Desc.Message = (xstrview){ "a\0suffix", sizeof("a\0suffix") - 1u };
		testRequire(xrtErrorBuildView(&Desc) == NULL &&
			xrtErrorKind(xrtGetError()) == XERR_MEMORY &&
			xrtErrorMessageView(xrtGetError()).Size == sizeof("memory allocation failed") - 1u,
			"exact error OOM must preserve an allocation-free static message");
		xrtClearError();
	}
	xrtSetErrorFormat(XERR_IO, "test.format", 1, "%s", sLongMessage);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"error format OOM mismatch");
	xrtClearError();
	printf("[PASS] error-format-oom\n");
	return 0;
}

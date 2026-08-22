#include "../test_allocator.h"



/* 验证分配便捷层在 OOM 下保持长度输出不变。 */
int main(void)
{
	size_t iSize = 77u;

	testRequire(testInstallFailAllocator(),
		"HTML failure allocator install failed");
	testRequire(
		xrtHtmlEscape(
			XRT_STR_LITERAL("A&B"), XHTML_ESCAPE_TEXT, &iSize
		) == NULL && (iSize == 77u),
		"HTML allocation OOM was not atomic"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"HTML allocation OOM error mismatch"
	);
	printf("[PASS] html_escape_oom\n");
	return 0;
}

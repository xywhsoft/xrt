#include "../test_allocator.h"



/* 验证长度查询和调用方缓冲路径不依赖动态分配。 */
int main(void)
{
	char Buffer[32];
	size_t iSize;

	testRequire(testInstallFailAllocator(),
		"HTML failure allocator install failed");
	testRequire(xrtHtmlEscapeSize(
		XRT_STR_LITERAL("A&B"), XHTML_ESCAPE_TEXT, &iSize
	) && (iSize == 7u), "HTML size query allocated memory");
	testRequire(xrtHtmlEscapeWrite(
		XRT_STR_LITERAL("A&B"), XHTML_ESCAPE_TEXT,
		Buffer, sizeof(Buffer), &iSize
	) && (strcmp(Buffer, "A&amp;B") == 0),
		"HTML buffer write allocated memory");
	printf("[PASS] html_escape_noalloc\n");
	return 0;
}

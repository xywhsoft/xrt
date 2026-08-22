#include "../test_allocator.h"

#include <xrt/http_structured.h>



/* Structured Fields 规范写出和长度查询必须保持零堆分配。 */
int main(void)
{
	xhttpstructureddictionaryentry Entry;
	char arrOutput[64];
	size_t iSize;

	memset(&Entry, 0, sizeof(Entry));
	Entry.Key = XRT_STR_LITERAL("label");
	Entry.Member.Kind = XHTTP_STRUCTURED_MEMBER_ITEM;
	Entry.Member.Item.Bare.Type = XHTTP_STRUCTURED_DISPLAY;
	Entry.Member.Item.Bare.Data = XRT_STR_LITERAL("work");
	testRequire(
		testInstallFailAllocator(),
		"structured writer failure allocator install failed"
	);
	testRequire(
		xrtHttpStructuredDictionaryWrite(
			&Entry, 1, NULL, 0, &iSize
		) && xrtHttpStructuredDictionaryWrite(
			&Entry, 1, arrOutput, sizeof(arrOutput), &iSize
		),
		"structured Dictionary writer allocated"
	);
	printf("[PASS] http_structured_write_noalloc\n");
	return 0;
}

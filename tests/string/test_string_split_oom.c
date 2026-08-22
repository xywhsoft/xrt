#include "../test_allocator.h"



/* 验证零分配迭代器在便捷结果 OOM 后仍可独立使用。 */
int main(void)
{
	xstrsplit tSplit;
	xstrview Item;

	testRequire(testInstallFailAllocator(), "failure allocator install failed");
	testRequire(xrtStrSplit(XRT_STR_LITERAL("a,b"), XRT_STR_LITERAL(",")) == NULL,
		"split list should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "split OOM error mismatch");
	xrtClearError();
	testRequire(xrtStrFields(XRT_STR_LITERAL("a b")) == NULL,
		"field list should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "field OOM error mismatch");
	xrtClearError();

	testRequire(xrtStrSplitInit(&tSplit, XRT_STR_LITERAL("a,b"), XRT_STR_LITERAL(",")),
		"zero-allocation iterator init failed");
	testRequire(xrtStrSplitNext(&tSplit, &Item) && (Item.Size == 1) && (Item.Data[0] == 'a'),
		"zero-allocation iterator failed after OOM");
	printf("[PASS] string-split-oom\n");
	return 0;
}

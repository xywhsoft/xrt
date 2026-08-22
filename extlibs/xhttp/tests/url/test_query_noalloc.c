#include "../test_allocator.h"



/* 验证 Query 迭代、统计和调用方缓冲写出不依赖堆分配。 */
int main(void)
{
	static const xquerypair Pairs[] = {
		{ XQUERY_HAS_VALUE, XRT_STR_INIT("a"), XRT_STR_INIT("1") },
		{ 0, XRT_STR_INIT("b"), { NULL, 0 } }
	};
	xquerypair Pair;
	char Text[16];
	size_t iOffset = 0;
	size_t iSize;

	testRequire(testInstallFailAllocator(),
		"query failure allocator install failed");
	testRequire(xrtQueryNext(
		XRT_STR_LITERAL("a=1&b"), &iOffset, &Pair
	) == XQUERY_NEXT_ITEM, "query iterator unexpectedly allocated");
	testRequire(xrtQueryCount(
		XRT_STR_LITERAL("a=1&b"), &iSize
	) && (iSize == 2), "query count unexpectedly allocated");
	testRequire(xrtQueryValidate(
		XRT_STR_LITERAL("a=1&b"), NULL, &iSize
	) && (iSize == 2), "query validation unexpectedly allocated");
	testRequire(xrtQueryRawWrite(
		Pairs, 2, Text, sizeof(Text), &iSize
	) && (iSize == 5) && (memcmp(Text, "a=1&b", 5) == 0),
		"query write unexpectedly allocated");
	printf("[PASS] query_noalloc\n");
	return 0;
}

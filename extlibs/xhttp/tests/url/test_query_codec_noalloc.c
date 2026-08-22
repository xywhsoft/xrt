#include "../test_allocator.h"



/* 验证调用方缓冲 Query 编码不依赖堆分配。 */
int main(void)
{
	static const xquerypair Pair = {
		XQUERY_HAS_VALUE, XRT_STR_INIT("a b"), XRT_STR_INIT("1&2")
	};
	char Text[32];
	size_t iSize;

	testRequire(testInstallFailAllocator(),
		"query codec failure allocator install failed");
	testRequire(xrtQueryWrite(
		&Pair, 1, Text, sizeof(Text), &iSize
	) && (iSize == 11) && (memcmp(Text, "a%20b=1%262", 11) == 0),
		"query codec direct write unexpectedly allocated");
	printf("[PASS] query_codec_noalloc\n");
	return 0;
}

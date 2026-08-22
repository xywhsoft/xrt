#include "../test_allocator.h"



/* 任何静态多范围测量和写入都不得访问全局分配器。 */
int main(void)
{
	xhttpbyterange Ranges[2] = {
		{ 0, 2 },
		{ 7, 9 }
	};
	char Output[128];
	uint64 iLength;
	size_t iSize;

	testRequire(testInstallFailAllocator(),
		"HTTP range multipart failure allocator install failed");
	testRequire(xrtHttpRangeMultipartLength(
		Ranges,
		2,
		10,
		XRT_STR_LITERAL("text/plain"),
		XRT_STR_LITERAL("noalloc"),
		&iLength
	) && xrtHttpRangeMultipartHeadWrite(
		&Ranges[0],
		10,
		XRT_STR_LITERAL("text/plain"),
		XRT_STR_LITERAL("noalloc"),
		Output,
		sizeof(Output),
		&iSize
	) && xrtHttpRangeMultipartEndWrite(
		Output,
		sizeof(Output),
		&iSize
	) && xrtHttpRangeMultipartCloseWrite(
		XRT_STR_LITERAL("noalloc"),
		Output,
		sizeof(Output),
		&iSize
	) && (iLength != 0),
		"HTTP range multipart allocated");
	printf("[PASS] http_range_multipart_noalloc\n");
	return 0;
}

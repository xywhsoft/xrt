#include "../test_allocator.h"



/* Multipart writer 的查询、Helper 和原始 Part 路径必须保持零堆分配。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_LITERAL("Content-Disposition"),
			XRT_STR_LITERAL("form-data; name=\"raw\"")
		}
	};
	xmultipartboundary Boundary;
	xstrview Filename = XRT_STR_LITERAL("a.txt");
	uint8 Output[512];
	size_t iSize;

	testRequire(testInstallFailAllocator(),
		"Multipart writer failure allocator install failed");
	testRequire(xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("noalloc"), &Boundary
	), "Multipart writer boundary allocated");
	testRequire(xrtMultipartFieldWrite(
		&Boundary,
		XRT_STR_LITERAL("field"),
		(xbytesview){ (const uint8*)"value", 5u },
		Output, sizeof(Output), &iSize
	), "Multipart field writer allocated");
	testRequire(xrtMultipartFormHeadWrite(
		&Boundary,
		XRT_STR_LITERAL("field"),
		&Filename,
		XRT_STR_LITERAL("text/plain"),
		Output,
		sizeof(Output),
		&iSize
	), "Multipart form Head writer allocated");
	testRequire(xrtMultipartFileWrite(
		&Boundary,
		XRT_STR_LITERAL("file"),
		XRT_STR_LITERAL("a.txt"),
		XRT_STR_LITERAL("text/plain"),
		(xbytesview){ (const uint8*)"body", 4u },
		Output, sizeof(Output), &iSize
	), "Multipart file writer allocated");
	testRequire(xrtMultipartPartWrite(
		&Boundary, Fields, 1,
		(xbytesview){ (const uint8*)"body", 4u },
		Output, sizeof(Output), &iSize
	), "Multipart raw Part writer allocated");
	printf("[PASS] multipart_write_noalloc\n");
	return 0;
}

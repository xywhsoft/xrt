#include "../test_allocator.h"



/* 有效 Header 解析与原始封包必须完全不依赖堆分配。 */
int main(void)
{
	static const char Request[] =
		"GET / HTTP/1.1\r\nHost: example.test\r\n\r\n";
	static const xhttpfield ResponseFields[] = {
		{ XRT_STR_INIT("Content-Length"), XRT_STR_INIT("0") }
	};
	xhttpfield Fields[2];
	xhttp1head Head;
	xbytesview Input;
	char Output[128];
	size_t iSize;

	testRequire(testInstallFailAllocator(),
		"HTTP/1 failure allocator install failed");
	Input.Data = (cbytes)Request;
	Input.Size = sizeof(Request) - 1u;
	xrtHttp1HeadInit(&Head, Fields, 2);
	testRequire(xrtHttp1RequestParse(
		Input, &Head, NULL, NULL
	) == XHTTP1_READY, "HTTP/1 valid parse allocated memory");
	testRequire(xrtHttp1ResponseWrite(
		XHTTP_VERSION_1_1, 204, XRT_STR_LITERAL("No Content"),
		ResponseFields, 1, Output, sizeof(Output), &iSize
	), "HTTP/1 valid write allocated memory");
	printf("[PASS] http1_noalloc\n");
	return 0;
}

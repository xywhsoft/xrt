#include "../test_allocator.h"



/* 完整消息扫描与正文复制必须在失败分配器下保持可用。 */
int main(void)
{
	static const uint8 Wire[] =
		"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
		"4\r\nWiki\r\n5\r\npedia\r\n0\r\nDigest: ok\r\n\r\n";
	xhttpfield Fields[2];
	xhttpfield Trailers[1];
	xhttp1errorinfo Error;
	xhttp1message Message;
	uint8 Output[16];
	size_t iSize;

	testRequire(testInstallFailAllocator(),
		"HTTP/1 Message failure allocator install failed");
	xrtHttp1MessageInit(&Message, Fields, 2, Trailers, 1);
	testRequire(xrtHttp1ResponseMessageParse(
		(xbytesview){ Wire, sizeof(Wire) - 1u }, false,
		XRT_STR_LITERAL("GET"),
		&Message, NULL, NULL, &Error
	) == XHTTP1_READY,
		"HTTP/1 Message parse allocated memory");
	testRequire(xrtHttp1MessageBodyCopy(
		&Message, Output, sizeof(Output), &iSize
	) && (iSize == 9) && (memcmp(Output, "Wikipedia", 9) == 0),
		"HTTP/1 Message Body copy allocated memory");
	printf("[PASS] http1_message_noalloc\n");
	return 0;
}

#include "../test_allocator.h"



/* HTTP 借用视图、token-list 和字段查找必须完全不依赖堆分配。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Connection"), XRT_STR_INIT("keep-alive, Upgrade") }
	};
	static const xstrview Tokens[] = {
		XRT_STR_INIT("keep-alive"),
		XRT_STR_INIT("Upgrade")
	};
	xhttpfield Field;
	xhttpfieldtokencursor Cursor;
	char Output[64];
	xstrview Token;
	uint64 iLength;
	size_t iCount;
	size_t iOffset = 0;
	size_t iSize;

	testRequire(testInstallFailAllocator(),
		"HTTP failure allocator install failed");
	testRequire(xrtHttpFieldParse(
		XRT_STR_LITERAL("Content-Type: application/json"), &Field
	), "HTTP field parse allocated memory");
	testRequire(xrtHttpTokenNext(
		Fields[0].Value, &iOffset, &Token
	) == XHTTP_NEXT_ITEM,
		"HTTP token-list iteration allocated memory");
	testRequire(xrtHttpTokenListHas(
		Fields[0].Value, XRT_STR_LITERAL("upgrade")
	), "HTTP token-list lookup allocated memory");
	testRequire(xrtHttpTokenListWrite(
		Tokens, 2u, Output, sizeof(Output), &iSize
	) && (iSize == 19u),
		"HTTP token-list write allocated memory");
	xrtHttpFieldTokenCursorInit(&Cursor);
	testRequire((xrtHttpFieldTokenNext(
		Fields,
		1u,
		XRT_STR_LITERAL("Connection"),
		&Cursor,
		&Token
	) == XHTTP_NEXT_ITEM) && xrtHttpFieldTokenCount(
		Fields, 1u, XRT_STR_LITERAL("Connection"), &iCount
	) && (iCount == 2u),
		"HTTP repeated field token operations allocated memory");
	testRequire(xrtHttpContentLengthParse(
		XRT_STR_LITERAL("7, 7"), &iLength
	) && (iLength == 7),
		"HTTP Content-Length parse allocated memory");
	testRequire(xrtHttpFieldGet(
		Fields, 1, XRT_STR_LITERAL("connection")
	) == &Fields[0], "HTTP field lookup allocated memory");
	testRequire(xrtHttpFieldBlockWrite(
		Fields, 1, Output, sizeof(Output), &iSize
	), "HTTP field write allocated memory");
	printf("[PASS] http_noalloc\n");
	return 0;
}

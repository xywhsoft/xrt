#include "../test_allocator.h"

#include <xrt/http_via.h>



/* Via 验证、迭代和注释解码不得依赖堆分配。 */
int main(void)
{
	xstrview Value = XRT_STR_LITERAL(
		"1.0 first, HTTP/1.1 edge:8080 (west\\))"
	);
	xhttpviacursor Cursor;
	xhttpvia Via;
	char sComment[16];
	size_t iSize;

	testRequire(testInstallFailAllocator(),
		"Via failure allocator install failed");
	testRequire(xrtHttpViaValid(Value),
		"Via validation allocated");
	xrtHttpViaCursorInit(&Cursor);
	testRequire(
		(xrtHttpViaNext(
			Value, &Cursor, &Via
		) == XHTTP_NEXT_ITEM) &&
		(xrtHttpViaNext(
			Value, &Cursor, &Via
		) == XHTTP_NEXT_ITEM) &&
		xrtHttpViaCommentDecode(
			Via.Comment, sComment, sizeof(sComment), &iSize
		) && (iSize == 5u) &&
		(memcmp(sComment, "west)", 5u) == 0),
		"Via direct parser path allocated"
	);
	printf("[PASS] http_via_noalloc\n");
	return 0;
}

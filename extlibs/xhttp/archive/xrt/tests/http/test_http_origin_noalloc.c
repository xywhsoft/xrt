#include "../test_allocator.h"

#include <xrt/http_origin.h>



/* Origin 解析、列表和同源比较必须保持零堆分配。 */
int main(void)
{
	xstrview Value = XRT_STR_LITERAL(
		"https://one.test https://two.test:8443"
	);
	xhttporigincursor Cursor;
	xhttporigin First;
	xhttporigin Second;

	testRequire(testInstallFailAllocator(),
		"Origin failure allocator install failed");
	xrtHttpOriginCursorInit(&Cursor);
	testRequire(
		(xrtHttpOriginNext(
			Value, &Cursor, &First
		) == XHTTP_NEXT_ITEM) &&
		(xrtHttpOriginNext(
			Value, &Cursor, &Second
		) == XHTTP_NEXT_ITEM) &&
		!xrtHttpOriginSame(&First, &Second) &&
		(xrtHttpOriginNext(
			Value, &Cursor, &Second
		) == XHTTP_NEXT_END),
		"Origin parser allocated"
	);
	printf("[PASS] http_origin_noalloc\n");
	return 0;
}

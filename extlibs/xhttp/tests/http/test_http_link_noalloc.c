#include "../test_allocator.h"

#include <xrt/http_link.h>



/* Link 解析和调用方缓冲 Helper 不得依赖堆分配。 */
int main(void)
{
	xstrview Value = XRT_STR_LITERAL(
		"</next>; rel=\"n\\ext alternate\";"
		"anchor=\"#p\\art\";hreflang=\"e\\n\";"
		"type=\"text\\/html\";title=\"fallback\";"
		"title*=UTF-8'en'preferred%20title"
	);
	xhttplinkcursor Cursor;
	xhttplink Link;
	char sOutput[64];
	size_t iSize;

	testRequire(testInstallFailAllocator(),
		"Link failure allocator install failed");
	testRequire(
		xrtHttpLinkValid(Value),
		"Link parser allocated during validation"
	);
	xrtHttpLinkCursorInit(&Cursor);
	testRequire(
		(xrtHttpLinkNext(
			Value, &Cursor, &Link
		) == XHTTP_NEXT_ITEM) &&
		(xrtHttpLinkRelationFind(
			&Link, XRT_STR_LITERAL("alternate")
		) == XHTTP_NEXT_ITEM),
		"Link parser allocated during iteration"
	);
	testRequire(
		xrtHttpLinkAnchorWrite(
			&Link, sOutput, sizeof(sOutput), &iSize
		) && (iSize == 5u) &&
		(memcmp(sOutput, "#part", 5u) == 0),
		"Link anchor helper allocated"
	);
	testRequire(
		xrtHttpLinkTitleWrite(
			&Link, sOutput, sizeof(sOutput), &iSize
		) && (iSize == 15u) &&
		(memcmp(sOutput, "preferred title", 15u) == 0),
		"Link title helper allocated"
	);
	printf("[PASS] http_link_noalloc\n");
	return 0;
}

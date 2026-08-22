#include "../test_allocator.h"

#include <xrt/http_te.h>



/* TE 解析、重复字段迭代和汇总不得依赖堆分配。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("TE"), XRT_STR_INIT("trailers") },
		{
			XRT_STR_INIT("TE"),
			XRT_STR_INIT("gzip;level=\"a,b\";q=0.5")
		}
	};
	xhttptefieldcursor Cursor;
	xhttptecoding Coding;
	xhttpteinfo Info;

	testRequire(testInstallFailAllocator(),
		"HTTP TE failure allocator install failed");
	xrtHttpTeFieldCursorInit(&Cursor);
	testRequire((xrtHttpTeFieldNext(
		Fields, 2u, &Cursor, &Coding
	) == XHTTP_NEXT_ITEM) &&
		xrtHttpTeParse(Fields, 2u, &Info) &&
		((Info.Flags & XHTTP_TE_ACCEPTS_TRAILERS) != 0) &&
		(xrtHttpTeQuality(
			Fields, 2u, XRT_STR_LITERAL("gzip")
		) == 500u),
		"HTTP TE direct path allocated memory");
	puts("[PASS] http_te_noalloc");
	return 0;
}

#include "../test_allocator.h"

#include <xrt/http_accept.h>



/* Accept 迭代、匹配和选择必须保持零堆分配。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Accept"),
			XRT_STR_INIT(
				"application/json;q=0.9;profile=full, "
				"text/*;q=0.5"
			)
		}
	};
	static const xstrview Available[] = {
		XRT_STR_INIT("text/html"),
		XRT_STR_INIT("application/json;profile=full")
	};
	xhttpacceptcursor Cursor;
	xhttpacceptmatch Match;
	xhttpmediarange Range;
	xhttpparam Param;
	size_t iIndex;
	size_t iOffset = 0;

	testRequire(testInstallFailAllocator(),
		"Accept failure allocator install failed");
	xrtHttpAcceptCursorInit(&Cursor);
	testRequire(
		(xrtHttpAcceptNext(
			Fields, 1, &Cursor, &Range
		) == XHTTP_NEXT_ITEM),
		"Accept field iteration allocated"
	);
	testRequire(
		(xrtHttpMediaRangeParamNext(
			&Range, &iOffset, &Param
		) == XHTTP_NEXT_ITEM) &&
		xrtHttpTokenEqual(
			Param.Name, XRT_STR_LITERAL("profile")
		),
		"Accept parameter iteration allocated"
	);
	testRequire(
		xrtHttpAcceptMatch(
			Fields,
			1,
			XRT_STR_LITERAL("application/json;profile=full"),
			&Match
		) && (Match.Quality == 900u),
		"Accept matching allocated"
	);
	testRequire(
		(xrtHttpAcceptSelect(
			Fields,
			1,
			Available,
			2,
			&iIndex
		) == XHTTP_NEXT_ITEM) && (iIndex == 1u),
		"Accept selection allocated"
	);
	printf("[PASS] http_accept_noalloc\n");
	return 0;
}

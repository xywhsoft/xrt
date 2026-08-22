#include "../test_allocator.h"

#include <xrt/http_expect.h>



/* Expect 解析、重复字段迭代和分类不得依赖堆分配。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Expect"), XRT_STR_INIT("100-continue") },
		{ XRT_STR_INIT("Expect"), XRT_STR_INIT("feature=\"a,b\"") }
	};
	xhttpexpectfieldcursor Cursor;
	xhttpexpectation Expectation;

	testRequire(testInstallFailAllocator(),
		"HTTP Expect failure allocator install failed");
	xrtHttpExpectFieldCursorInit(&Cursor);
	testRequire((xrtHttpExpectFieldNext(
		Fields, 2u, &Cursor, &Expectation
	) == XHTTP_NEXT_ITEM) &&
		(xrtHttpExpectFields(
			Fields, 2u
		 ) == XHTTP_EXPECT_UNSUPPORTED),
		"HTTP Expect direct path allocated memory");
	puts("[PASS] http_expect_noalloc");
	return 0;
}

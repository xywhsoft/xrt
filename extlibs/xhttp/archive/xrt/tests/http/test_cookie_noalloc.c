#include "../test_allocator.h"



/* Cookie 借用解析、查找、批量解析和写出均不得依赖堆分配。 */
int main(void)
{
	xstrview Text = XRT_STR_LITERAL("sid=abc123; theme=dark");
	xcookiepair Pairs[2];
	xcookiepair Pair;
	char Output[64];
	size_t iOffset = 0;
	size_t iCount;
	size_t iSize;

	testRequire(testInstallFailAllocator(),
		"cookie failure allocator install failed");
	testRequire(xrtCookieNext(Text, &iOffset, &Pair) ==
		XCOOKIE_NEXT_ITEM, "cookie iteration allocated memory");
	iOffset = 0;
	testRequire(xrtCookieFind(
		Text, XRT_STR_LITERAL("theme"), &iOffset, &Pair
	) == XCOOKIE_NEXT_ITEM, "cookie lookup allocated memory");
	testRequire(xrtCookieParse(
		Text, Pairs, 2, &iCount, NULL
	) && (iCount == 2), "cookie parse allocated memory");
	testRequire(xrtCookieWrite(
		Pairs, 2, Output, sizeof(Output), &iSize
	) && (iSize == Text.Size), "cookie write allocated memory");
	printf("[PASS] cookie_noalloc\n");
	return 0;
}

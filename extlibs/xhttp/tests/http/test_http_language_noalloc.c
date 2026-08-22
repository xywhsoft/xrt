#include "../test_allocator.h"

#include <xrt/http_language.h>



/* 语言范围迭代、匹配、选择和 Lookup 必须保持零堆分配。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Accept-Language"),
			XRT_STR_INIT("zh-Hant;q=0.9, en;q=0.5")
		}
	};
	static const xstrview Available[] = {
		XRT_STR_INIT("en"),
		XRT_STR_INIT("zh"),
		XRT_STR_INIT("zh-Hant")
	};
	xhttplanguagecursor Cursor;
	xhttplanguagematch Match;
	xhttplanguagerange Range;
	size_t iIndex;

	testRequire(testInstallFailAllocator(),
		"language failure allocator install failed");
	xrtHttpLanguageCursorInit(&Cursor);
	testRequire(
		(xrtHttpAcceptLanguageNext(
			Fields, 1, &Cursor, &Range
		) == XHTTP_NEXT_ITEM),
		"Accept-Language iteration allocated"
	);
	testRequire(
		xrtHttpAcceptLanguageMatch(
			Fields, 1, XRT_STR_LITERAL("zh-Hant-CN"), &Match
		) && (Match.Quality == 900u),
		"Accept-Language matching allocated"
	);
	testRequire(
		(xrtHttpAcceptLanguageSelect(
			Fields, 1, Available, 3, &iIndex
		) == XHTTP_NEXT_ITEM) && (iIndex == 2u),
		"Accept-Language selection allocated"
	);
	testRequire(
		(xrtHttpAcceptLanguageLookup(
			Fields, 1, Available, 3, &iIndex
		) == XHTTP_NEXT_ITEM) && (iIndex == 2u),
		"Accept-Language lookup allocated"
	);
	printf("[PASS] http_language_noalloc\n");
	return 0;
}

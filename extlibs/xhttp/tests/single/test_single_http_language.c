#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 单头发布必须保留 Accept-Language Lookup。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Accept-Language"),
			XRT_STR_INIT("zh-Hant-CN, en;q=0.5")
		}
	};
	static const xstrview Available[] = {
		XRT_STR_INIT("en"),
		XRT_STR_INIT("zh-Hant")
	};
	size_t iIndex;

	return (xrtHttpAcceptLanguageLookup(
		Fields, 1, Available, 2, &iIndex
	) == XHTTP_NEXT_ITEM) && (iIndex == 1u) ? 0 : 1;
}

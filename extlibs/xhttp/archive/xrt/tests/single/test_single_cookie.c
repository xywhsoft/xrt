#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <string.h>



/* 单头发布必须保留 Cookie 的借用解析和规范写出。 */
int main(void)
{
	static const xcookiepair Pairs[] = {
		{ XRT_STR_INIT("sid"), XRT_STR_INIT("abc") },
		{ XRT_STR_INIT("theme"), XRT_STR_INIT("dark") }
	};
	xcookiepair Pair;
	char Text[32];
	size_t iOffset = 0;
	size_t iSize;

	if ( !xrtCookieWrite(Pairs, 2, Text, sizeof(Text), &iSize) ||
		(iSize != 19) || (memcmp(Text, "sid=abc; theme=dark", 19) != 0) ||
		(xrtCookieNext(
			(xstrview){ Text, iSize }, &iOffset, &Pair
		) != XCOOKIE_NEXT_ITEM) || (Pair.Name.Size != 3) ) {
		return 1;
	}
	return 0;
}

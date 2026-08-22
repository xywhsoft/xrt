#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_COOKIE
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证 Cookie 字段的无分配扫描路径。 */
int main(void)
{
	xcookiepair Pair;
	size_t iOffset = 0;

	return (xrtCookieNext(
		XRT_STR_LITERAL("sid=abc; theme=dark"),
		&iOffset,
		&Pair
	) == XCOOKIE_NEXT_ITEM) && (Pair.Name.Size == 3u) ? 0 : 1;
}

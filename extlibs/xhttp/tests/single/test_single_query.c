#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_QUERY
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件中的 Query 迭代和原始写出主路径。 */
int main(void)
{
	static const xquerypair Pairs[] = {
		{ XQUERY_HAS_VALUE, XRT_STR_INIT("a"), XRT_STR_INIT("1") },
		{ 0, XRT_STR_INIT("b"), { NULL, 0 } }
	};
	xquerypair Pair;
	char Text[16];
	size_t iOffset = 0;
	size_t iSize;

	if ( (xrtQueryNext(
		XRT_STR_LITERAL("?a=1&b"), &iOffset, &Pair
	) != XQUERY_NEXT_ITEM) || (Pair.Key.Size != 1) ||
		!xrtQueryRawWrite(Pairs, 2, Text, sizeof(Text), &iSize) ||
		(iSize != 5) || (memcmp(Text, "a=1&b", 5) != 0) ) {
		return 1;
	}
	printf("[PASS] single-query\n");
	return 0;
}

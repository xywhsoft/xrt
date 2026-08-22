#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_QUERY_CODEC
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"

#include <stdio.h>
#include <string.h>



/* 验证单头文件中的 RFC 3986 Query 构建主路径。 */
int main(void)
{
	static const xquerypair Pair = {
		XQUERY_HAS_VALUE, XRT_STR_INIT("a b"), XRT_STR_INIT("1&2")
	};
	char Text[32];
	size_t iSize;

	if ( !xrtQueryWrite(
		&Pair, 1, Text, sizeof(Text), &iSize
	) || (iSize != 11) || (memcmp(Text, "a%20b=1%262", 11) != 0) ) {
		return 1;
	}
	printf("[PASS] single-query-codec\n");
	return 0;
}

#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#ifndef XHTTP_MODULE_HTTP_PROXY_ALIAS_WRITE
	#define XHTTP_MODULE_HTTP_PROXY_ALIAS_WRITE
#endif
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 单头发布必须保留 RFC 9532 别名列表写出。 */
int main(void)
{
	static const xstrview Aliases[] = {
		XRT_STR_INIT("comma,name"),
		XRT_STR_INIT("dot\\.label")
	};
	char arrOutput[64];
	size_t iSize;

	return xrtHttpProxyAliasesWrite(
		Aliases, 2u, arrOutput, sizeof(arrOutput), &iSize
	) && (iSize == 25u) &&
		(memcmp(
			arrOutput, "comma%2Cname,dot%5C.label", iSize
		) == 0) ? 0 : 1;
}

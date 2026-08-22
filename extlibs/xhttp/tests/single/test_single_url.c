#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_URL
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 单头发布必须保留 URL 解析、HTTP target 与引用解析。 */
int main(void)
{
	xurl Url;
	char Output[128];
	str sResolved;
	size_t iSize;
	int iResult = 0;

	if ( !xrtUrlParse(
		XRT_STR_LITERAL("https://[::1]:443/a?q=#fragment"), &Url
	) || !xrtUrlTargetWrite(
		&Url, Output, sizeof(Output), &iSize
	) || (iSize != strlen("/a?q=")) ||
		(memcmp(Output, "/a?q=", iSize) != 0) ) {
		iResult = 1;
	}
	sResolved = xrtUrlResolveBuild(
		&Url, XRT_STR_LITERAL("../next"), &iSize
	);
	if ( (sResolved == NULL) ||
		(iSize != strlen("https://[::1]:443/next")) ||
		(memcmp(sResolved, "https://[::1]:443/next", iSize + 1u) != 0) ) {
		iResult = 1;
	}
	xrtFree(sResolved);
	return iResult;
}

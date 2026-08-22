#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_COOKIE_JAR
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证 CookieJar 的存储、URL 匹配和请求字段构建主路径。 */
int main(void)
{
	xcookiejar* pJar = xrtCookieJarCreate(NULL);
	xcookiereject Reject = XCOOKIE_REJECT_NONE;
	str sCookie;
	size_t iSize = 0;
	int iResult = 0;

	if ( (pJar == NULL) || (xrtCookieJarStoreUrl(
		pJar,
		XRT_STR_LITERAL("https://example.com/a"),
		XRT_STR_LITERAL("sid=abc; Path=/; Secure"),
		&Reject
	) != XCOOKIE_STORE_STORED) ) {
		iResult = 1;
	}
	sCookie = xrtCookieJarBuildUrl(
		pJar, XRT_STR_LITERAL("https://example.com/b"), &iSize
	);
	if ( (sCookie == NULL) || (iSize != 7u) ||
		(memcmp(sCookie, "sid=abc", 7u) != 0) ) {
		iResult = 2;
	}
	xrtFree(sCookie);
	xrtCookieJarRelease(pJar);
	return iResult;
}

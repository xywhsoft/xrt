#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_COOKIE_JAR_HEADERS
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证 CookieJar 与拥有型 Header 容器的组合边界。 */
int main(void)
{
	xcookiejar* pJar = xrtCookieJarCreate(NULL);
	xhttpheaders* pHeaders = xrtHttpHeadersCreate(NULL);
	xcookiestorecontext Store;
	xcookierequestcontext Request;
	xcookiestorereport Report;
	int iResult = 0;

	memset(&Store, 0, sizeof(Store));
	memset(&Request, 0, sizeof(Request));
	Store.Flags = XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_SAME_SITE;
	Store.URL = XRT_STR_LITERAL("https://example.com/");
	Request.Flags = XCOOKIE_REQUEST_HTTP_API |
		XCOOKIE_REQUEST_SAME_SITE |
		XCOOKIE_REQUEST_SAFE_METHOD;
	Request.URL = Store.URL;

	if ( (pJar == NULL) || (pHeaders == NULL) ||
		!xrtHttpHeadersAdd(
			pHeaders,
			XRT_STR_LITERAL("Set-Cookie"),
			XRT_STR_LITERAL("sid=abc; Path=/; Secure")
		) || !xrtCookieJarStoreHeaders(
			pJar, &Store, pHeaders, &Report
		) || !xrtCookieJarApply(pJar, &Request, pHeaders) ||
		!xrtHttpHeadersHas(
			pHeaders, XRT_STR_LITERAL("Cookie")
		) ) {
		iResult = 1;
	}
	xrtHttpHeadersDestroy(pHeaders);
	xrtCookieJarRelease(pJar);
	return iResult;
}

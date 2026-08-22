#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留 CookieJar 与动态 Header 的可裁剪适配。 */
int main(void)
{
	xcookiejar* pJar = xrtCookieJarCreate(NULL);
	xhttpheaders* pHeaders = xrtHttpHeadersCreate(NULL);
	xcookierequestcontext Request;

	if ( (pJar == NULL) || (pHeaders == NULL) ||
		(xrtCookieJarStoreUrl(
			pJar, XRT_STR_LITERAL("https://example.com/"),
			XRT_STR_LITERAL("sid=1; Path=/"), NULL
		) != XCOOKIE_STORE_STORED) ) {
		return 1;
	}
	memset(&Request, 0, sizeof(Request));
	Request.Flags = XCOOKIE_REQUEST_HTTP_API |
		XCOOKIE_REQUEST_SAME_SITE;
	Request.URL = XRT_STR_LITERAL("https://example.com/");
	if ( !xrtCookieJarApply(pJar, &Request, pHeaders) ||
		!xrtHttpHeadersHas(pHeaders, XRT_STR_LITERAL("Cookie")) ) {
		return 1;
	}
	xrtHttpHeadersDestroy(pHeaders);
	xrtCookieJarRelease(pJar);
	return 0;
}

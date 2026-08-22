#include <stdio.h>
#include <string.h>

#include <xrt.h>



/* 展示 CookieJar 与动态 Header 的批量适配。 */
int main(void)
{
	xcookiejar* pJar = xrtCookieJarCreate(NULL);
	xhttpheaders* pResponse = xrtHttpHeadersCreate(NULL);
	xhttpheaders* pRequest = xrtHttpHeadersCreate(NULL);
	xcookiestorecontext Store;
	xcookierequestcontext Request;
	const xhttpfield* pCookie;

	if ( (pJar == NULL) || (pResponse == NULL) || (pRequest == NULL) ||
		!xrtHttpHeadersAdd(
			pResponse, XRT_STR_LITERAL("Set-Cookie"),
			XRT_STR_LITERAL("sid=abc123; Path=/; Secure; HttpOnly")
		) ) {
		return 1;
	}
	memset(&Store, 0, sizeof(Store));
	Store.Flags = XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_SAME_SITE;
	Store.URL = XRT_STR_LITERAL("https://api.example.com/login");
	memset(&Request, 0, sizeof(Request));
	Request.Flags = XCOOKIE_REQUEST_HTTP_API |
		XCOOKIE_REQUEST_SAME_SITE;
	Request.URL = XRT_STR_LITERAL("https://api.example.com/data");
	if ( !xrtCookieJarStoreHeaders(pJar, &Store, pResponse, NULL) ||
		!xrtCookieJarApply(pJar, &Request, pRequest) ) {
		return 1;
	}
	pCookie = xrtHttpHeadersGet(pRequest, XRT_STR_LITERAL("Cookie"));
	if ( pCookie != NULL ) {
		printf("Cookie: %.*s\n", (int)pCookie->Value.Size,
			pCookie->Value.Data);
	}
	xrtHttpHeadersDestroy(pRequest);
	xrtHttpHeadersDestroy(pResponse);
	xrtCookieJarRelease(pJar);
	return 0;
}

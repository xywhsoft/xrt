#include "../test.h"



/* 验证 Set-Cookie 独立接收和 Cookie 单字段应用。 */
int main(void)
{
	xcookiejar* pJar = xrtCookieJarCreate(NULL);
	xhttpheaders* pResponse = xrtHttpHeadersCreate(NULL);
	xhttpheaders* pRequest = xrtHttpHeadersCreate(NULL);
	xcookiestorecontext Store;
	xcookierequestcontext Request;
	xcookiestorereport Report;
	const xhttpfield* pField;

	testRequire((pJar != NULL) && (pResponse != NULL) &&
		(pRequest != NULL), "cookie jar headers fixture create failed");
	testRequire(xrtHttpHeadersAdd(
		pResponse, XRT_STR_LITERAL("Set-Cookie"),
		XRT_STR_LITERAL("a=1; Path=/")
	) && xrtHttpHeadersAdd(
		pResponse, XRT_STR_LITERAL("Set-Cookie"),
		XRT_STR_LITERAL("b=2; Path=/")
	) && xrtHttpHeadersAdd(
		pResponse, XRT_STR_LITERAL("Set-Cookie"),
		XRT_STR_LITERAL("=")
	), "cookie jar response headers add failed");
	memset(&Store, 0, sizeof(Store));
	Store.Flags = XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_SAME_SITE;
	Store.URL = XRT_STR_LITERAL("https://example.com/");
	testRequire(xrtCookieJarStoreHeaders(
		pJar, &Store, pResponse, &Report
	) && (Report.Fields == 3) && (Report.Stored == 2) &&
		(Report.Rejected == 1),
		"cookie jar response report mismatch");
	memset(&Request, 0, sizeof(Request));
	Request.Flags = XCOOKIE_REQUEST_HTTP_API |
		XCOOKIE_REQUEST_SAME_SITE;
	Request.URL = XRT_STR_LITERAL("https://example.com/");
	testRequire(xrtHttpHeadersAdd(
		pRequest, XRT_STR_LITERAL("Cookie"),
		XRT_STR_LITERAL("stale=1")
	) && xrtHttpHeadersAdd(
		pRequest, XRT_STR_LITERAL("Cookie"),
		XRT_STR_LITERAL("duplicate=1")
	) && xrtCookieJarApply(pJar, &Request, pRequest),
		"cookie jar request apply failed");
	testRequire(xrtHttpHeadersCountName(
		pRequest, XRT_STR_LITERAL("Cookie")
	) == 1, "cookie jar apply retained duplicate Cookie fields");
	pField = xrtHttpHeadersGet(pRequest, XRT_STR_LITERAL("Cookie"));
	testRequire((pField != NULL) && (pField->Value.Size == 8u) &&
		(memcmp(pField->Value.Data, "a=1; b=2", 8u) == 0),
		"cookie jar applied field mismatch");
	xrtCookieJarClear(pJar);
	testRequire(xrtCookieJarApply(pJar, &Request, pRequest) &&
		!xrtHttpHeadersHas(pRequest, XRT_STR_LITERAL("Cookie")),
		"empty cookie jar did not remove request field");
	xrtHttpHeadersDestroy(pRequest);
	xrtHttpHeadersDestroy(pResponse);
	xrtCookieJarRelease(pJar);
	printf("[PASS] cookie_jar_headers\n");
	return 0;
}


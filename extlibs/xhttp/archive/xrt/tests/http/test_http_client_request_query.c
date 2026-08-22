#include "../test.h"



/* 比较两个不要求零结尾的文本视图。 */
static bool testHttpRequestQueryEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 验证 QueryParams 替换旧查询、保留 fragment 并表达显式空查询。 */
int main(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/path?old=1#part")
	);
	xqueryparams* pParams = xrtQueryParamsCreate(NULL);
	xqueryparams* pEmpty = xrtQueryParamsCreate(NULL);
	xstrview Before;
	const xurl* pUrl;

	testRequire(
		(pRequest != NULL) && (pParams != NULL) &&
		(pEmpty != NULL) &&
		xrtQueryParamsAppend(
			pParams,
			XRT_STR_LITERAL("q"),
			XRT_STR_LITERAL("a b")
		) &&
		xrtQueryParamsAppend(
			pParams,
			XRT_STR_LITERAL("tag"),
			XRT_STR_LITERAL("x/y")
		),
		"HTTP request query setup failed"
	);
	testRequire(
		xrtHttpRequestSetQueryParams(pRequest, pParams) &&
		testHttpRequestQueryEqual(
			xrtHttpRequestUrlText(pRequest),
			XRT_STR_LITERAL(
				"https://example.test/path?q=a+b&tag=x%2Fy#part"
			)
		),
		"HTTP request query replacement mismatch"
	);
	pUrl = xrtHttpRequestUrl(pRequest);
	testRequire(
		((pUrl->Flags & XURL_HAS_QUERY) != 0) &&
		testHttpRequestQueryEqual(
			pUrl->Query,
			XRT_STR_LITERAL("q=a+b&tag=x%2Fy")
		) &&
		testHttpRequestQueryEqual(
			pUrl->Fragment,
			XRT_STR_LITERAL("part")
		),
		"HTTP request parsed query mismatch"
	);

	Before = xrtHttpRequestUrlText(pRequest);
	testRequire(
		!xrtHttpRequestSetQueryParams(pRequest, NULL) &&
		testHttpRequestQueryEqual(
			xrtHttpRequestUrlText(pRequest),
			Before
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP request null QueryParams changed URL"
	);
	xrtClearError();
	testRequire(
		xrtHttpRequestSetQueryParams(pRequest, pEmpty) &&
		testHttpRequestQueryEqual(
			xrtHttpRequestUrlText(pRequest),
			XRT_STR_LITERAL("https://example.test/path?#part")
		),
		"HTTP request empty query mismatch"
	);

	xrtQueryParamsDestroy(pEmpty);
	xrtQueryParamsDestroy(pParams);
	xrtHttpRequestDestroy(pRequest);
	printf("[PASS] HTTP client request QueryParams\n");
	return 0;
}

#include "../test.h"



/* 验证 QueryParams 一步形成拥有型 urlencoded 正文和准确媒体类型。 */
int main(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("https://example.test/form")
	);
	xqueryparams* pParams = xrtQueryParamsCreate(NULL);
	xqueryparams* pEmpty = xrtQueryParamsCreate(NULL);
	xhttpbody* pBefore;
	const xhttpfield* pType;
	xbytesview Body;

	testRequire(
		(pRequest != NULL) && (pParams != NULL) &&
		(pEmpty != NULL) &&
		xrtHttpRequestSetBytes(
			pRequest,
			XRT_BYTES_LITERAL("old"),
			XRT_STR_LITERAL("text/plain")
		) &&
		xrtQueryParamsAppend(
			pParams,
			XRT_STR_LITERAL("name"),
			XRT_STR_LITERAL("xrt runtime")
		) &&
		xrtQueryParamsAppend(
			pParams,
			XRT_STR_LITERAL("mode"),
			XRT_STR_LITERAL("fast")
		),
		"HTTP request form setup failed"
	);
	pBefore = xrtHttpRequestBody(pRequest);
	testRequire(
		!xrtHttpRequestSetForm(pRequest, NULL) &&
		(xrtHttpRequestBody(pRequest) == pBefore) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP request null form changed body"
	);
	xrtClearError();

	testRequire(
		xrtHttpRequestSetForm(pRequest, pParams) &&
		xrtHttpBodyView(xrtHttpRequestBody(pRequest), &Body) &&
		(Body.Size == 26u) &&
		(memcmp(
			Body.Data,
			"name=xrt+runtime&mode=fast",
			26u
		) == 0),
		"HTTP request form body mismatch"
	);
	pType = xrtHttpRequestHeader(
		pRequest,
		XRT_STR_LITERAL("Content-Type")
	);
	testRequire(
		(pType != NULL) &&
		(pType->Value.Size ==
			(sizeof("application/x-www-form-urlencoded") - 1u)) &&
		(memcmp(
			pType->Value.Data,
			"application/x-www-form-urlencoded",
			sizeof("application/x-www-form-urlencoded") - 1u
		) == 0),
		"HTTP request form Content-Type mismatch"
	);

	testRequire(
		xrtHttpRequestSetForm(pRequest, pEmpty) &&
		xrtHttpBodyView(xrtHttpRequestBody(pRequest), &Body) &&
		(Body.Size == 0),
		"HTTP request empty form mismatch"
	);

	xrtQueryParamsDestroy(pEmpty);
	xrtQueryParamsDestroy(pParams);
	xrtHttpRequestDestroy(pRequest);
	printf("[PASS] HTTP client request urlencoded form\n");
	return 0;
}


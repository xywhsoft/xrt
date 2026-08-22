#include "../test.h"

#include <xrt/http_sse.h>



/* 判断唯一同名字段的值，减少事务测试里的重复样板。 */
static bool testHeaderValue(
	const xhttpheaders* pHeaders,
	xstrview Name,
	xstrview Value
)
{
	const xhttpfield* pField = xrtHttpHeadersGet(pHeaders, Name);

	return (xrtHttpHeadersCountName(pHeaders, Name) == 1u) &&
		(pField != NULL) &&
		(pField->Value.Size == Value.Size) &&
		((Value.Size == 0) ||
		 (memcmp(pField->Value.Data, Value.Data, Value.Size) == 0));
}



/* 验证媒体类型参数和大小写语义，同时确保纯判断不覆盖旧错误。 */
static void testHttpSseContentType(void)
{
	xerror* pMarker = xrtErrorCreate(
		XERR_STATE, "test.sse", 7, "marker"
	);

	testRequire(pMarker != NULL, "SSE marker error allocation failed");
	xrtSetError(pMarker);
	testRequire(
		xrtHttpSseContentTypeValid(
			XRT_STR_LITERAL("Text/Event-Stream; charset=utf-8")
		) && (xrtGetError() == pMarker),
		"SSE Content-Type did not accept legal parameters"
	);
	testRequire(
		!xrtHttpSseContentTypeValid(XRT_STR_LITERAL("text/plain")) &&
		(xrtGetError() == pMarker),
		"SSE Content-Type mismatch changed the old error"
	);
	testRequire(
		!xrtHttpSseContentTypeValid(
			XRT_STR_LITERAL("text/event-stream; charset")
		) && (xrtGetError() == pMarker),
		"malformed SSE Content-Type changed the old error"
	);
	xrtClearError();
	xrtErrorFree(pMarker);
}



/* 验证公开容器交换不分配，并完整交换字段所有权。 */
static void testHttpHeadersSwap(void)
{
	xhttpheaders* pLeft = xrtHttpHeadersCreate(NULL);
	xhttpheaders* pRight = xrtHttpHeadersCreate(NULL);

	testRequire(
		(pLeft != NULL) && (pRight != NULL) &&
		xrtHttpHeadersAdd(
			pLeft, XRT_STR_LITERAL("X-Left"), XRT_STR_LITERAL("a")
		) &&
		xrtHttpHeadersAdd(
			pRight, XRT_STR_LITERAL("X-Right"), XRT_STR_LITERAL("b")
		) &&
		xrtHttpHeadersSwap(pLeft, pRight) &&
		testHeaderValue(
			pLeft, XRT_STR_LITERAL("X-Right"), XRT_STR_LITERAL("b")
		) &&
		testHeaderValue(
			pRight, XRT_STR_LITERAL("X-Left"), XRT_STR_LITERAL("a")
		),
		"HTTP Header container swap mismatch"
	);
	testRequire(
		!xrtHttpHeadersSwap(NULL, pRight) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Header swap did not reject null input"
	);
	xrtClearError();
	xrtHttpHeadersDestroy(pRight);
	xrtHttpHeadersDestroy(pLeft);
}



/* 验证请求字段去重、删除和无效 ID 失败原子性。 */
static void testHttpSseRequestHeaders(void)
{
	static const char NullId[] = { 'a', 0, 'b' };
	xhttpheaders* pHeaders = xrtHttpHeadersCreate(NULL);
	str sBefore;
	str sAfter;
	size_t iBefore;
	size_t iAfter;

	testRequire(
		(pHeaders != NULL) &&
		xrtHttpHeadersAdd(
			pHeaders, XRT_STR_LITERAL("Accept"), XRT_STR_LITERAL("old-a")
		) &&
		xrtHttpHeadersAdd(
			pHeaders, XRT_STR_LITERAL("Accept"), XRT_STR_LITERAL("old-b")
		) &&
		xrtHttpHeadersAdd(
			pHeaders,
			XRT_STR_LITERAL("Last-Event-ID"),
			XRT_STR_LITERAL("old-a")
		) &&
		xrtHttpHeadersAdd(
			pHeaders,
			XRT_STR_LITERAL("Last-Event-ID"),
			XRT_STR_LITERAL("old-b")
		) &&
		xrtHttpSseRequestHeaders(
			pHeaders, XRT_STR_LITERAL("42")
		) &&
		testHeaderValue(
			pHeaders,
			XRT_STR_LITERAL("Accept"),
			XRT_STR_LITERAL(XHTTP_SSE_MEDIA_TYPE)
		) &&
		testHeaderValue(
			pHeaders,
			XRT_STR_LITERAL("Last-Event-ID"),
			XRT_STR_LITERAL("42")
		),
		"SSE request fields were not set uniquely"
	);
	sBefore = xrtHttpHeadersBuild(pHeaders, &iBefore);
	testRequire(sBefore != NULL, "SSE request snapshot failed");
	testRequire(
		!xrtHttpSseRequestHeaders(
			pHeaders, (xstrview){ NullId, sizeof(NullId) }
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SSE unsafe Last-Event-ID was accepted"
	);
	xrtClearError();
	sAfter = xrtHttpHeadersBuild(pHeaders, &iAfter);
	testRequire(
		(sAfter != NULL) && (iAfter == iBefore) &&
		(memcmp(sAfter, sBefore, iBefore) == 0),
		"SSE invalid request update changed Header state"
	);
	xrtFree(sAfter);
	xrtFree(sBefore);
	testRequire(
		xrtHttpSseRequestHeaders(pHeaders, XRT_STR_LITERAL("")) &&
		(xrtHttpHeadersCountName(
			pHeaders, XRT_STR_LITERAL("Last-Event-ID")
		) == 0u),
		"SSE empty Last-Event-ID did not remove the field"
	);
	xrtHttpHeadersDestroy(pHeaders);
}



/* 验证响应媒体类型去重以及 200、204 和普通拒绝状态。 */
static void testHttpSseResponse(void)
{
	xhttpheaders* pHeaders = xrtHttpHeadersCreate(NULL);
	xerror* pMarker;

	testRequire(
		(pHeaders != NULL) &&
		xrtHttpHeadersAdd(
			pHeaders,
			XRT_STR_LITERAL("Content-Type"),
			XRT_STR_LITERAL("text/event-stream")
		) &&
		xrtHttpHeadersAdd(
			pHeaders,
			XRT_STR_LITERAL("content-type"),
			XRT_STR_LITERAL("text/event-stream")
		) &&
		(xrtHttpSseResponseCheck(200, pHeaders) ==
		 XHTTP_SSE_RESPONSE_REJECT),
		"SSE duplicate Content-Type was accepted"
	);
	testRequire(
		xrtHttpSseResponseHeaders(pHeaders) &&
		testHeaderValue(
			pHeaders,
			XRT_STR_LITERAL("Content-Type"),
			XRT_STR_LITERAL(XHTTP_SSE_MEDIA_TYPE)
		) &&
		(xrtHttpSseResponseCheck(200, pHeaders) ==
		 XHTTP_SSE_RESPONSE_OPEN) &&
		(xrtHttpSseResponseCheck(204, NULL) ==
		 XHTTP_SSE_RESPONSE_STOP) &&
		(xrtHttpSseResponseCheck(503, NULL) ==
		 XHTTP_SSE_RESPONSE_REJECT),
		"SSE HTTP response classification mismatch"
	);
	pMarker = xrtErrorCreate(
		XERR_STATE, "test.sse", 8, "response marker"
	);
	testRequire(pMarker != NULL, "SSE response marker allocation failed");
	xrtSetError(pMarker);
	testRequire(
		(xrtHttpSseResponseCheck(200, NULL) ==
		 XHTTP_SSE_RESPONSE_REJECT) &&
		(xrtHttpSseResponseCheck(204, NULL) ==
		 XHTTP_SSE_RESPONSE_STOP) &&
		(xrtHttpSseResponseCheck(503, NULL) ==
		 XHTTP_SSE_RESPONSE_REJECT) &&
		(xrtGetError() == pMarker),
		"SSE protocol response classification changed the old error"
	);
	xrtClearError();
	xrtErrorFree(pMarker);
	testRequire(
		(xrtHttpSseResponseCheck(0, pHeaders) ==
		 XHTTP_SSE_RESPONSE_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SSE invalid HTTP status was not an argument error"
	);
	xrtClearError();
	xrtHttpHeadersDestroy(pHeaders);
}



/* 运行 SSE 通用 HTTP 适配层回归。 */
int main(void)
{
	testHttpSseContentType();
	testHttpHeadersSwap();
	testHttpSseRequestHeaders();
	testHttpSseResponse();
	printf("[PASS] HTTP SSE adapter\n");
	return 0;
}

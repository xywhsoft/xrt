#include "../test.h"

#include <xrt/http_sse.h>



/* 创建带重复目标字段的请求容器，使复制和去重路径都参与故障扫描。 */
static xhttpheaders* testHttpSseRequestSource(void)
{
	xhttpheaders* pHeaders = xrtHttpHeadersCreate(NULL);

	if ( (pHeaders == NULL) ||
		!xrtHttpHeadersAdd(
			pHeaders, XRT_STR_LITERAL("X-Keep"), XRT_STR_LITERAL("stable")
		) ||
		!xrtHttpHeadersAdd(
			pHeaders, XRT_STR_LITERAL("Accept"), XRT_STR_LITERAL("old-a")
		) ||
		!xrtHttpHeadersAdd(
			pHeaders, XRT_STR_LITERAL("Accept"), XRT_STR_LITERAL("old-b")
		) ||
		!xrtHttpHeadersAdd(
			pHeaders,
			XRT_STR_LITERAL("Last-Event-ID"),
			XRT_STR_LITERAL("old")
		) ) {
		xrtHttpHeadersDestroy(pHeaders);
		return NULL;
	}
	return pHeaders;
}



/* 创建带重复 Content-Type 的响应容器。 */
static xhttpheaders* testHttpSseResponseSource(void)
{
	xhttpheaders* pHeaders = xrtHttpHeadersCreate(NULL);

	if ( (pHeaders == NULL) ||
		!xrtHttpHeadersAdd(
			pHeaders, XRT_STR_LITERAL("X-Keep"), XRT_STR_LITERAL("stable")
		) ||
		!xrtHttpHeadersAdd(
			pHeaders,
			XRT_STR_LITERAL("Content-Type"),
			XRT_STR_LITERAL("text/plain")
		) ||
		!xrtHttpHeadersAdd(
			pHeaders,
			XRT_STR_LITERAL("content-type"),
			XRT_STR_LITERAL("application/json")
		) ) {
		xrtHttpHeadersDestroy(pHeaders);
		return NULL;
	}
	return pHeaders;
}



/* 比较故障前后的规范字段块。 */
static bool testHttpSseHeadersEqual(
	const xhttpheaders* pHeaders,
	cstr sExpected,
	size_t iExpected
)
{
	str sActual;
	size_t iActual;
	bool bEqual;

	sActual = xrtHttpHeadersBuild(pHeaders, &iActual);
	bEqual = (sActual != NULL) && (iActual == iExpected) &&
		(memcmp(sActual, sExpected, iExpected) == 0);
	xrtFree(sActual);
	return bEqual;
}



/* 在一个逻辑分配序号上执行请求 Header 事务。 */
static bool testHttpSseRequestOomAttempt(size_t iFail)
{
	xhttpheaders* pHeaders = testHttpSseRequestSource();
	str sBefore;
	size_t iBefore;
	bool bResult;
	bool bTriggered;
	bool bMemory;

	testRequire(pHeaders != NULL, "SSE request OOM source failed");
	sBefore = xrtHttpHeadersBuild(pHeaders, &iBefore);
	testRequire(sBefore != NULL, "SSE request OOM snapshot failed");
	testRequire(
		xrtMemDebugFailAfter((uint64)iFail),
		"SSE request OOM fault setup failed"
	);
	bResult = xrtHttpSseRequestHeaders(
		pHeaders, XRT_STR_LITERAL("resume-token-longer-than-old")
	);
	bTriggered = xrtMemDebugFailTriggered();
	bMemory = xrtErrorKind(xrtGetError()) == XERR_MEMORY;
	xrtMemDebugFailClear();
	if ( bResult ) {
		testRequire(
			!bTriggered,
			"SSE request helper ignored a triggered allocation fault"
		);
	} else {
		testRequire(
			bTriggered && bMemory &&
			testHttpSseHeadersEqual(pHeaders, sBefore, iBefore),
			"SSE request helper OOM was not failure atomic"
		);
	}
	xrtClearError();
	xrtFree(sBefore);
	xrtHttpHeadersDestroy(pHeaders);
	testMemoryDebugDrain("SSE request helper OOM leaked storage");
	return bResult;
}



/* 在一个逻辑分配序号上执行响应 Header 事务。 */
static bool testHttpSseResponseOomAttempt(size_t iFail)
{
	xhttpheaders* pHeaders = testHttpSseResponseSource();
	str sBefore;
	size_t iBefore;
	bool bResult;
	bool bTriggered;
	bool bMemory;

	testRequire(pHeaders != NULL, "SSE response OOM source failed");
	sBefore = xrtHttpHeadersBuild(pHeaders, &iBefore);
	testRequire(sBefore != NULL, "SSE response OOM snapshot failed");
	testRequire(
		xrtMemDebugFailAfter((uint64)iFail),
		"SSE response OOM fault setup failed"
	);
	bResult = xrtHttpSseResponseHeaders(pHeaders);
	bTriggered = xrtMemDebugFailTriggered();
	bMemory = xrtErrorKind(xrtGetError()) == XERR_MEMORY;
	xrtMemDebugFailClear();
	if ( bResult ) {
		testRequire(
			!bTriggered,
			"SSE response helper ignored a triggered allocation fault"
		);
	} else {
		testRequire(
			bTriggered && bMemory &&
			testHttpSseHeadersEqual(pHeaders, sBefore, iBefore),
			"SSE response helper OOM was not failure atomic"
		);
	}
	xrtClearError();
	xrtFree(sBefore);
	xrtHttpHeadersDestroy(pHeaders);
	testMemoryDebugDrain("SSE response helper OOM leaked storage");
	return bResult;
}



/* 扫描请求与响应 Helper 的全部动态分配点。 */
static void testHttpSseHttpOom(void)
{
	size_t iFail;
	bool bRequestDone = false;
	bool bResponseDone = false;

	for ( iFail = 0; iFail < 64u; iFail++ ) {
		if ( testHttpSseRequestOomAttempt(iFail) ) {
			bRequestDone = true;
			break;
		}
	}
	testRequire(
		bRequestDone && (iFail != 0),
		"SSE request helper OOM scan did not converge"
	);
	for ( iFail = 0; iFail < 64u; iFail++ ) {
		if ( testHttpSseResponseOomAttempt(iFail) ) {
			bResponseDone = true;
			break;
		}
	}
	testRequire(
		bResponseDone && (iFail != 0),
		"SSE response helper OOM scan did not converge"
	);
}



/* 运行 SSE HTTP 适配层分配故障回归。 */
int main(void)
{
	testHttpSseHttpOom();
	printf("[PASS] HTTP SSE adapter OOM\n");
	return 0;
}

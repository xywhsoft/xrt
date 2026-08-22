#include "../test.h"
#include "../../src/internal/xrt_http_client.h"



/* 可调失败分配器扫描响应创建和增长路径。 */
typedef struct test_http_response_allocator {
	size_t Calls;
	size_t FailAt;
	size_t Live;
} test_http_response_allocator;



/* 在指定分配序号失败。 */
static ptr testHttpResponseAlloc(ptr pContext, size_t iSize)
{
	test_http_response_allocator* pState =
		(test_http_response_allocator*)pContext;
	ptr pMemory;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		return NULL;
	}
	pMemory = malloc(iSize);
	if ( pMemory != NULL ) {
		pState->Live++;
	}
	return pMemory;
}



/* 重分配失败保留原块。 */
static ptr testHttpResponseRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_http_response_allocator* pState =
		(test_http_response_allocator*)pContext;
	ptr pResult;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		return NULL;
	}
	pResult = realloc(pMemory, iSize);
	if ( (pResult != NULL) && (pMemory == NULL) ) {
		pState->Live++;
	}
	return pResult;
}



/* 释放底层分配并维护存活计数。 */
static void testHttpResponseFree(ptr pContext, ptr pMemory)
{
	test_http_response_allocator* pState =
		(test_http_response_allocator*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	testRequire(pState->Live != 0,
		"HTTP response OOM live counter underflow");
	pState->Live--;
	free(pMemory);
}



/* 在一个失败点下执行响应全部主要分配路径。 */
static bool testHttpResponseOomAttempt(void)
{
	char Body[16384];
	xhttpresponse* pResponse;
	str sText = NULL;
	bool bComplete = false;

	memset(Body, 'b', sizeof(Body));
	pResponse = __xrtHttpResponseCreate(
		XHTTP_VERSION_1_1,
		200,
		XRT_STR_LITERAL("A dynamic reason phrase"),
		NULL
	);
	if ( pResponse == NULL ) {
		return false;
	}
	if ( !__xrtHttpResponseAddHeader(
		pResponse,
		XRT_STR_LITERAL("X-Test"),
		(xstrview){ Body, sizeof(Body) }
	) ) {
		goto done;
	}
	if ( !__xrtHttpResponseAddTrailer(
		pResponse,
		NULL,
		XRT_STR_LITERAL("Digest"),
		XRT_STR_LITERAL("sha-256=:AA==:")
	) ) {
		testRequire(pResponse->Trailers == NULL,
			"failed first HTTP trailer exposed a partial container");
		goto done;
	}
	if ( !__xrtHttpResponseAppendBody(
		pResponse,
		(xbytesview){ (cbytes)Body, sizeof(Body) }
	) ) {
		testRequire(xrtHttpResponseBodyBytes(pResponse) == 0,
			"HTTP response body OOM exposed partial append");
		goto done;
	}
	if ( !__xrtHttpResponseSetUrl(
		pResponse,
		XRT_STR_LITERAL("https://example.test/final")
	) ) {
		testRequire(xrtHttpResponseUrl(pResponse).Size == 0,
			"HTTP response URL OOM exposed partial update");
		goto done;
	}
	sText = xrtHttpResponseBodyText(pResponse);
	if ( sText == NULL ) {
		goto done;
	}
	bComplete = true;

done:
	xrtFree(sText);
	xrtHttpResponseDestroy(pResponse);
	xrtClearError();
	return bComplete;
}



/* 扫描失败序号并验证响应路径不泄漏。 */
int main(void)
{
	test_http_response_allocator State = { 0 };
	xallocator Allocator = {
		&State,
		testHttpResponseAlloc,
		testHttpResponseRealloc,
		testHttpResponseFree
	};
	size_t iBaseline;
	size_t iFail;
	size_t iFailures = 0;
	bool bSuccess = false;

	testRequire(xrtSetAllocator(&Allocator),
		"HTTP response OOM allocator install failed");
	testRequire(testHttpResponseOomAttempt(),
		"HTTP response OOM warm-up failed");
	testMemoryDebugDrain(
		"HTTP response OOM memory debug reset failed"
	);
	iBaseline = State.Live;
	for ( iFail = 1; iFail <= 96; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		if ( testHttpResponseOomAttempt() ) {
			bSuccess = true;
		} else {
			iFailures++;
		}
		testMemoryDebugDrain(
			"HTTP response OOM memory debug reset failed"
		);
		testRequire(State.Live == iBaseline,
			"HTTP response OOM attempt leaked storage");
	}
	testRequire((iFailures != 0) && bSuccess,
		"HTTP response OOM sweep missed failure or success");
	printf("[PASS] HTTP client response OOM\n");
	return 0;
}

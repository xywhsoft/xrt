#include "../test.h"



/* 可调失败分配器用于扫描请求准备的全部临时与持久分配。 */
typedef struct test_http_prepare_allocator {
	size_t Calls;
	size_t FailAt;
	size_t Live;
} test_http_prepare_allocator;



/* 在指定调用序号失败。 */
static ptr testHttpPrepareAlloc(ptr pContext, size_t iSize)
{
	test_http_prepare_allocator* pState =
		(test_http_prepare_allocator*)pContext;
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



/* 重分配失败时保留原块和存活计数。 */
static ptr testHttpPrepareRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_http_prepare_allocator* pState =
		(test_http_prepare_allocator*)pContext;
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
static void testHttpPrepareFree(ptr pContext, ptr pMemory)
{
	test_http_prepare_allocator* pState =
		(test_http_prepare_allocator*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	testRequire(pState->Live != 0,
		"HTTP prepare OOM live counter underflow");
	pState->Live--;
	free(pMemory);
}



/* 在一个失败点下执行完整请求构建与 absolute-form 准备。 */
static bool testHttpPrepareOomAttempt(void)
{
	static char Value[16384];
	xhttprequest* pRequest;
	xhttp1requestoptions Options;
	xhttp1requestplan* pPlan = NULL;
	bool bComplete = false;

	memset(Value, 'v', sizeof(Value));
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL(
			"https://example.test/upload?q=1#fragment"
		)
	);
	if ( pRequest == NULL ) {
		return false;
	}
	if ( !xrtHttpRequestAddHeader(
		pRequest,
		XRT_STR_LITERAL("X-Large"),
		(xstrview){ Value, sizeof(Value) }
	) || !xrtHttpRequestSetBytes(
		pRequest,
		(xbytesview){ (cbytes)"payload", 7 },
		XRT_STR_LITERAL("application/octet-stream")
	) ) {
		goto done;
	}
	xrtHttp1RequestOptionsInit(&Options);
	Options.TargetForm = XHTTP1_TARGET_ABSOLUTE;
	pPlan = xrtHttp1RequestPrepare(pRequest, &Options);
	if ( pPlan == NULL ) {
		goto done;
	}
	bComplete = true;

done:
	xrtHttp1RequestPlanDestroy(pPlan);
	xrtHttpRequestDestroy(pRequest);
	xrtClearError();
	return bComplete;
}



/* 扫描分配失败序号并验证请求准备不泄漏。 */
int main(void)
{
	test_http_prepare_allocator State = { 0 };
	xallocator Allocator = {
		&State,
		testHttpPrepareAlloc,
		testHttpPrepareRealloc,
		testHttpPrepareFree
	};
	size_t iBaseline;
	size_t iFail;
	size_t iFailures = 0;
	bool bSuccess = false;

	testRequire(xrtSetAllocator(&Allocator),
		"HTTP prepare OOM allocator install failed");
	testRequire(testHttpPrepareOomAttempt(),
		"HTTP prepare OOM warm-up failed");
	testMemoryDebugDrain(
		"HTTP prepare OOM memory debug reset failed"
	);
	iBaseline = State.Live;
	for ( iFail = 1; iFail <= 128; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		if ( testHttpPrepareOomAttempt() ) {
			bSuccess = true;
		} else {
			iFailures++;
		}
		testMemoryDebugDrain(
			"HTTP prepare OOM memory debug reset failed"
		);
		testRequire(State.Live == iBaseline,
			"HTTP prepare OOM attempt leaked storage");
	}
	testRequire((iFailures != 0) && bSuccess,
		"HTTP prepare OOM sweep missed failure or success");
	printf("[PASS] HTTP client prepare OOM\n");
	return 0;
}

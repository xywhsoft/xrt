#include "../test.h"



/* 可调分配器用于扫描请求创建、修改和克隆失败路径。 */
typedef struct test_http_request_allocator {
	size_t Calls;
	size_t FailAt;
	size_t Live;
} test_http_request_allocator;



/* 在指定分配序号失败，其余分配交给 C 运行库。 */
static ptr testHttpRequestAlloc(ptr pContext, size_t iSize)
{
	test_http_request_allocator* pState =
		(test_http_request_allocator*)pContext;
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



/* 重分配失败必须保留原块，成功创建首块时更新存活计数。 */
static ptr testHttpRequestRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_http_request_allocator* pState =
		(test_http_request_allocator*)pContext;
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



/* 释放底层块并验证计数。 */
static void testHttpRequestFree(ptr pContext, ptr pMemory)
{
	test_http_request_allocator* pState =
		(test_http_request_allocator*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	testRequire(pState->Live != 0,
		"HTTP request OOM live counter underflow");
	pState->Live--;
	free(pMemory);
}



/* 在一个失败点下执行构建器主要分配路径并检查可见状态。 */
static bool testHttpRequestOomAttempt(void)
{
	char LongUrl[8192];
	xhttprequest* pRequest;
	xhttprequest* pClone = NULL;
	xstrview OldUrl;
	size_t i;
	bool bComplete = false;

	for ( i = 0; i < sizeof(LongUrl) - 1; i++ ) {
		LongUrl[i] = i < 21 ?
			"https://example.test/"[i] : 'p';
	}
	LongUrl[sizeof(LongUrl) - 1] = '\0';
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("https://example.test/old")
	);
	if ( pRequest == NULL ) {
		return false;
	}
	if ( !xrtHttpRequestSetBytes(
		pRequest,
		(xbytesview){ (cbytes)"old", 3 },
		XRT_STR_LITERAL("text/plain")
	) ) {
		goto done;
	}
	OldUrl = xrtHttpRequestUrlText(pRequest);
	if ( !xrtHttpRequestSetUrl(
		pRequest,
		(xstrview){ LongUrl, sizeof(LongUrl) - 1 }
	) ) {
		testRequire((xrtHttpRequestUrlText(pRequest).Size ==
			OldUrl.Size) &&
			(memcmp(
				xrtHttpRequestUrlText(pRequest).Data,
				OldUrl.Data,
				OldUrl.Size
			) == 0),
			"HTTP request URL OOM changed visible state");
		goto done;
	}
	if ( !xrtHttpRequestSetBytes(
		pRequest,
		(xbytesview){ (cbytes)LongUrl, sizeof(LongUrl) },
		XRT_STR_LITERAL("application/octet-stream")
	) ) {
		testRequire(xrtHttpBodyLength(
			xrtHttpRequestBody(pRequest)
		) == 3, "HTTP request body OOM changed old body");
		goto done;
	}
	pClone = xrtHttpRequestClone(pRequest);
	if ( pClone == NULL ) {
		testRequire(xrtHttpRequestBody(pRequest) != NULL,
			"HTTP request Clone OOM changed source");
		goto done;
	}
	bComplete = true;

done:
	xrtHttpRequestDestroy(pClone);
	xrtHttpRequestDestroy(pRequest);
	xrtClearError();
	return bComplete;
}



/* 扫描分配失败点并要求回到稳定底层内存基线。 */
int main(void)
{
	test_http_request_allocator State = { 0 };
	xallocator Allocator = {
		&State,
		testHttpRequestAlloc,
		testHttpRequestRealloc,
		testHttpRequestFree
	};
	size_t iBaseline;
	size_t iFail;
	size_t iFailures = 0;
	bool bSuccess = false;

	testRequire(xrtSetAllocator(&Allocator),
		"HTTP request OOM allocator install failed");
	testRequire(testHttpRequestOomAttempt(),
		"HTTP request OOM warm-up failed");
	testMemoryDebugDrain(
		"HTTP request OOM memory debug reset failed"
	);
	iBaseline = State.Live;
	for ( iFail = 1; iFail <= 128; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		if ( testHttpRequestOomAttempt() ) {
			bSuccess = true;
		} else {
			iFailures++;
		}
		testMemoryDebugDrain(
			"HTTP request OOM memory debug reset failed"
		);
		testRequire(State.Live == iBaseline,
			"HTTP request OOM attempt leaked storage");
	}
	testRequire((iFailures != 0) && bSuccess,
		"HTTP request OOM sweep missed failure or success");
	printf("[PASS] HTTP client request OOM\n");
	return 0;
}

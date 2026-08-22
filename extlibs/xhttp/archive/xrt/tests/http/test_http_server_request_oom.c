#include "../test.h"
#include "../../src/internal/xrt_http_server.h"



/* 可调失败分配器扫描请求快照、正文和 Trailer 路径。 */
typedef struct test_http_server_request_allocator {
	size_t Calls;
	size_t FailAt;
	size_t Live;
} test_http_server_request_allocator;



/* 在指定分配序号失败。 */
static ptr testHttpServerRequestAlloc(ptr pContext, size_t iSize)
{
	test_http_server_request_allocator* pState =
		(test_http_server_request_allocator*)pContext;
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
static ptr testHttpServerRequestRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_http_server_request_allocator* pState =
		(test_http_server_request_allocator*)pContext;
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



/* 释放成功分配的底层块。 */
static void testHttpServerRequestFree(
	ptr pContext,
	ptr pMemory
)
{
	test_http_server_request_allocator* pState =
		(test_http_server_request_allocator*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	testRequire(pState->Live != 0,
		"HTTP server request OOM live counter underflow");
	pState->Live--;
	free(pMemory);
}



/* 在一个失败点下推进请求的全部拥有型存储。 */
static bool testHttpServerRequestOomAttempt(void)
{
	static char Input[] =
		"POST /upload HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n";
	static char Large[32768];
	xhttpfield Parsed[8];
	xhttpfield Trailers[] = {
		{
			XRT_STR_INIT("Digest"),
			XRT_STR_INIT("sha-256=:AA==:")
		},
		{
			XRT_STR_INIT("X-Meta"),
			{ Large, sizeof(Large) }
		}
	};
	xhttp1bodyplan Plan;
	xhttp1head Head;
	xhttpserverrequest* pRequest = NULL;
	size_t iOldBody;
	bool bComplete = false;

	memset(Large, 'x', sizeof(Large));
	xrtHttp1HeadInit(
		&Head,
		Parsed,
		sizeof(Parsed) / sizeof(Parsed[0])
	);
	testRequire(xrtHttp1RequestParse(
		(xbytesview){
			(cbytes)Input,
			sizeof(Input) - 1u
		},
		&Head,
		NULL,
		NULL
	) == XHTTP1_READY,
		"HTTP server request OOM parse failed");
	testRequire(xrtHttp1RequestBodyPlan(&Head, &Plan),
		"HTTP server request OOM body plan failed");
	pRequest = __xrtHttpServerRequestCreate(
		&Head, &Plan, XHTTP_SERVER_REQUEST_NONE
	);
	if ( pRequest == NULL ) {
		return false;
	}
	iOldBody = xrtHttpServerRequestBody(pRequest).Size;
	if ( !__xrtHttpServerRequestAppendBody(
		pRequest,
		(xbytesview){ (cbytes)Large, sizeof(Large) }
	) ) {
		testRequire(
			xrtHttpServerRequestBody(pRequest).Size == iOldBody,
			"HTTP server request body OOM changed visible data"
		);
		goto done;
	}
	if ( !__xrtHttpServerRequestSetTrailers(
		pRequest,
		Trailers,
		sizeof(Trailers) / sizeof(Trailers[0])
	) ) {
		testRequire(
			xrtHttpServerRequestTrailerCount(pRequest) == 0,
			"HTTP server request Trailer OOM exposed partial data"
		);
		goto done;
	}
	bComplete = true;

done:
	xrtHttpServerRequestDestroy(pRequest);
	xrtClearError();
	return bComplete;
}



/* 扫描失败序号并要求所有请求资产回到稳定基线。 */
int main(void)
{
	test_http_server_request_allocator State = { 0 };
	xallocator Allocator = {
		&State,
		testHttpServerRequestAlloc,
		testHttpServerRequestRealloc,
		testHttpServerRequestFree
	};
	size_t iBaseline;
	size_t iFail;
	size_t iFailures = 0;
	bool bSuccess = false;

	testRequire(xrtSetAllocator(&Allocator),
		"HTTP server request OOM allocator install failed");
	testRequire(testHttpServerRequestOomAttempt(),
		"HTTP server request OOM warm-up failed");
	testMemoryDebugDrain(
		"HTTP server request OOM memory debug reset failed"
	);
	iBaseline = State.Live;
	for ( iFail = 1; iFail <= 96; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		if ( testHttpServerRequestOomAttempt() ) {
			bSuccess = true;
		} else {
			iFailures++;
		}
		testMemoryDebugDrain(
			"HTTP server request OOM memory debug reset failed"
		);
		testRequire(State.Live == iBaseline,
			"HTTP server request OOM attempt leaked storage");
	}
	testRequire((iFailures != 0) && bSuccess,
		"HTTP server request OOM sweep missed failure or success");
	printf("[PASS] HTTP server request OOM\n");
	return 0;
}

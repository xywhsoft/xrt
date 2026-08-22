#include "../test.h"
#include "../fixtures/http_origin.h"



#define TEST_HTTP_CLIENT_FUTURE_OOM_CLASSES ((size_t)64)
#define TEST_HTTP_CLIENT_FUTURE_OOM_BLOCKS ((size_t)16384)

#ifndef TEST_HTTP_CLIENT_FUTURE_OOM_ALLOW
	#define TEST_HTTP_CLIENT_FUTURE_OOM_ALLOW 0
#endif

#ifndef TEST_HTTP_CLIENT_FUTURE_OOM_NAME
	#define TEST_HTTP_CLIENT_FUTURE_OOM_NAME "waiter"
#endif



/* OOM 夹具在故障门关闭时完整转发系统分配。 */
typedef struct test_http_future_oom {
	xatomic32 Gate;
	xatomic32 Allow;
	xatomic64 Allocations;
	xatomic64 Denied;
	ptr* Held;
	size_t HeldCount;
	size_t HeldCapacity;
} test_http_future_oom;



/* 记录并按故障开关拒绝分配。 */
static ptr testHttpFutureOomAlloc(ptr pData, size_t iSize)
{
	test_http_future_oom* pState =
		(test_http_future_oom*)pData;

	(void)xrtAtomic64FetchAdd(
		&pState->Allocations,
		1,
		XMEMORY_RELAXED
	);
	if ( xrtAtomic32Load(
		&pState->Gate,
		XMEMORY_ACQUIRE
	) != 0 ) {
		if ( xrtAtomic32Load(
			&pState->Allow,
			XMEMORY_ACQUIRE
		) == 0 ) {
			(void)xrtAtomic64FetchAdd(
				&pState->Denied,
				1,
				XMEMORY_RELAXED
			);
			return NULL;
		}
		(void)xrtAtomic32FetchSub(
			&pState->Allow,
			1,
			XMEMORY_ACQ_REL
		);
	}
	return malloc(iSize);
}



/* 记录并按故障开关拒绝扩容。 */
static ptr testHttpFutureOomRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	test_http_future_oom* pState =
		(test_http_future_oom*)pData;

	(void)xrtAtomic64FetchAdd(
		&pState->Allocations,
		1,
		XMEMORY_RELAXED
	);
	if ( xrtAtomic32Load(
		&pState->Gate,
		XMEMORY_ACQUIRE
	) != 0 ) {
		if ( xrtAtomic32Load(
			&pState->Allow,
			XMEMORY_ACQUIRE
		) == 0 ) {
			(void)xrtAtomic64FetchAdd(
				&pState->Denied,
				1,
				XMEMORY_RELAXED
			);
			return NULL;
		}
		(void)xrtAtomic32FetchSub(
			&pState->Allow,
			1,
			XMEMORY_ACQ_REL
		);
	}
	return realloc(pMemory, iSize);
}



/* 释放故障前已经建立的对象。 */
static void testHttpFutureOomFree(ptr pData, ptr pMemory)
{
	(void)pData;
	free(pMemory);
}



/* 耗尽 Heap 各池化尺寸类，确保下一次分配必须经过故障分配器。 */
static void testHttpFutureOomExhaust(test_http_future_oom* pState)
{
	xrtAtomic32Store(&pState->Allow, 0, XMEMORY_RELEASE);
	xrtAtomic32Store(&pState->Gate, 1, XMEMORY_RELEASE);
	for ( size_t i = 1;
		i <= TEST_HTTP_CLIENT_FUTURE_OOM_CLASSES;
		i++ ) {
		for ( ;; ) {
			ptr pMemory = xrtMalloc(i * 16);

			if ( pMemory == NULL ) {
				xrtClearError();
				break;
			}
			testRequire(
				pState->HeldCount < pState->HeldCapacity,
				"HTTP Client Future OOM exhaustion overflowed"
			);
			pState->Held[pState->HeldCount++] = pMemory;
		}
	}
}



/* 关闭故障门并归还尺寸类耗尽期间持有的全部块。 */
static void testHttpFutureOomRestore(test_http_future_oom* pState)
{
	xrtAtomic32Store(&pState->Gate, 0, XMEMORY_RELEASE);
	for ( size_t i = 0; i < pState->HeldCount; i++ ) {
		xrtFree(pState->Held[i]);
	}
	pState->HeldCount = 0;
	xrtClearError();
}



/* 验证桥接首个分配失败无副作用，恢复后仍能完成真实请求。 */
int main(void)
{
	static const char Wire[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 2\r\n"
		"Connection: close\r\n"
		"\r\n"
		"OK";
	test_http_future_oom State;
	testhttporigin Origin;
	xallocator Allocator;
	xnetengineconfig EngineConfig;
	xhttpclientconfig ClientConfig;
	xnetenginestats Before;
	xnetenginestats After;
	xnetengine* pEngine;
	xhttpclient* pClient;
	xhttprequest* pRequest;
	xfuture* pFuture;
	xhttpresult* pResult;
	char Url[256];
	int iLength;
	xdeadline Deadline;

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.Gate, 0);
	xrtAtomic32Init(&State.Allow, 0);
	xrtAtomic64Init(&State.Allocations, 0);
	xrtAtomic64Init(&State.Denied, 0);
	State.HeldCapacity = TEST_HTTP_CLIENT_FUTURE_OOM_BLOCKS;
	State.Held = (ptr*)malloc(State.HeldCapacity * sizeof(ptr));
	testRequire(
		State.Held != NULL,
		"HTTP Client Future OOM hold table failed"
	);
	Allocator.Context = &State;
	Allocator.Alloc = testHttpFutureOomAlloc;
	Allocator.Realloc = testHttpFutureOomRealloc;
	Allocator.Free = testHttpFutureOomFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"HTTP Future OOM allocator install failed"
	);

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) &&
		xrtNetEngineStart(pEngine),
		"HTTP Future OOM engine start failed"
	);
	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Dial.Family = XNET_FAMILY_IPV4;
	ClientConfig.Dial.MaxAttempts = 1;
	pClient = xrtHttpClientCreate(
		pEngine,
		&ClientConfig
	);
	testRequire(
		pClient != NULL,
		"HTTP Future OOM client create failed"
	);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://127.0.0.1:1/oom")
	);
	testRequire(
		pRequest != NULL,
		"HTTP Future OOM request create failed"
	);
	testRequire(
		xrtNetEngineStats(pEngine, &Before),
		"HTTP Future OOM baseline stats failed"
	);

	testHttpFutureOomExhaust(&State);
	xrtAtomic32Store(
		&State.Allow,
		TEST_HTTP_CLIENT_FUTURE_OOM_ALLOW,
		XMEMORY_RELEASE
	);
	pFuture = xrtHttpClientWaitAsync(pClient);
	testRequire(
		(pFuture == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtHttpClientState(pClient) == XHTTP_CLIENT_RUNNING),
		"HTTP Client wait OOM changed lifecycle"
	);
	xrtClearError();
	pFuture = xrtHttpClientDoAsync(
		pClient,
		pRequest,
		NULL
	);
	testRequire(
		(pFuture == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP Future bridge OOM mismatch"
	);
	xrtClearError();
	testHttpFutureOomRestore(&State);
	testRequire(
		xrtNetEngineStats(pEngine, &After) &&
		(After.LiveObjects == Before.LiveObjects) &&
		(After.ActiveTimers == Before.ActiveTimers),
		"HTTP Future bridge OOM changed Engine ownership"
	);
	testHttpOriginStart(
		&Origin,
		pEngine,
		Wire,
		sizeof(Wire) - 1u
	);
	xrtHttpRequestDestroy(pRequest);
	iLength = snprintf(
		Url,
		sizeof(Url),
		"http://127.0.0.1:%u/oom",
		(unsigned)testHttpOriginPort(&Origin)
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Url)),
		"HTTP Future OOM URL overflowed"
	);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		(xstrview){ Url, (size_t)iLength }
	);
	testRequire(
		pRequest != NULL,
		"HTTP Future OOM recovery request create failed"
	);

	pFuture = xrtHttpClientDoAsync(
		pClient,
		pRequest,
		NULL
	);
	testRequire(
		(pFuture != NULL) &&
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(10000000)
		) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED),
		"HTTP Future did not recover after OOM"
	);
	pResult = (xhttpresult*)xrtFutureValue(pFuture);
	testRequire(
		(pResult != NULL) &&
		(xrtHttpResponseStatus(
			xrtHttpResultResponse(pResult)
		) == 200),
		"HTTP Future OOM recovery response mismatch"
	);
	xrtFutureDestroy(pFuture);
	xrtHttpRequestDestroy(pRequest);
	xrtHttpClientDestroy(pClient);
	testHttpOriginStop(&Origin);

	Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	while ( !xrtNetEngineDestroy(pEngine) ) {
		xrtClearError();
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP Future OOM retained an Engine object"
		);
		xrtThreadYield();
	}
	testRequire(
		xrtAtomic64Load(
			&State.Allocations,
			XMEMORY_ACQUIRE
		) != 0,
		"HTTP Future OOM allocator was not exercised"
	);
	testRequire(
		xrtAtomic64Load(
			&State.Denied,
			XMEMORY_ACQUIRE
		) > TEST_HTTP_CLIENT_FUTURE_OOM_CLASSES,
		"HTTP Future OOM allocator denied no allocations"
	);
	free(State.Held);
	printf(
		"[PASS] high-level HTTP Future OOM recovery (%s)\n",
		TEST_HTTP_CLIENT_FUTURE_OOM_NAME
	);
	return 0;
}

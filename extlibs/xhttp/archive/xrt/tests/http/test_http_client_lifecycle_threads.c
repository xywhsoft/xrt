#include "../test.h"
#include "../fixtures/http_origin.h"



#define TEST_HTTP_CLIENT_WAIT_THREADS 64u



/* 每个线程独占一个关闭 Future，并与 Client Abort 竞争取消或等待。 */
typedef struct test_http_client_wait_thread {
	xfuture* Future;
	xatomic32* Ready;
	xatomic32* Release;
	xatomic32 State;
	bool Cancel;
} test_http_client_wait_thread;



/* 在统一屏障后并发取消或等待各自的 Client 关闭 Future。 */
static int32 testHttpClientWaitThread(ptr pData)
{
	test_http_client_wait_thread* pContext =
		(test_http_client_wait_thread*)pData;
	xfuturestate State = XFUTURE_PENDING;

	(void)xrtAtomic32FetchAdd(
		pContext->Ready,
		1,
		XMEMORY_RELEASE
	);
	while ( xrtAtomic32Load(
		pContext->Release,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
	if ( pContext->Cancel ) {
		(void)xrtFutureCancel(pContext->Future);
	}
	if ( xrtFutureWaitFor(
		pContext->Future,
		UINT64_C(5000000)
	) == XWAIT_OK ) {
		State = xrtFutureState(pContext->Future);
	}
	xrtAtomic32Store(
		&pContext->State,
		(uint32)State,
		XMEMORY_RELEASE
	);
	return 0;
}



/* 压测等待取消、活动 Call 取消和 Client 关闭发布的并发边界。 */
int main(void)
{
	testhttporigin Origin;
	xatomic32 Ready;
	xatomic32 Release;
	xnetengineconfig EngineConfig;
	xhttpclientconfig ClientConfig;
	xnetengine* pEngine;
	xhttpclient* pClient;
	xhttprequest* pRequest;
	xfuture* pCall;
	xfuture* pClosed;
	test_http_client_wait_thread Contexts[
		TEST_HTTP_CLIENT_WAIT_THREADS
	];
	xthread* Threads[TEST_HTTP_CLIENT_WAIT_THREADS];
	xdeadline Deadline;
	char Url[256];
	int iLength;

	memset(Contexts, 0, sizeof(Contexts));
	memset(Threads, 0, sizeof(Threads));
	xrtAtomic32Init(&Ready, 0);
	xrtAtomic32Init(&Release, 0);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) && xrtNetEngineStart(pEngine),
		"HTTP Client wait thread engine start failed"
	);
	testHttpOriginStart(&Origin, pEngine, NULL, 0);
	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Dial.Family = XNET_FAMILY_IPV4;
	ClientConfig.Dial.MaxAttempts = 1;
	ClientConfig.Timeout = UINT64_C(5000000);
	pClient = xrtHttpClientCreate(pEngine, &ClientConfig);
	testRequire(
		pClient != NULL,
		"HTTP Client wait thread Client create failed"
	);
	iLength = snprintf(
		Url,
		sizeof(Url),
		"http://127.0.0.1:%u/race",
		(unsigned)testHttpOriginPort(&Origin)
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Url)),
		"HTTP Client wait thread URL overflowed"
	);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		(xstrview) { Url, (size_t)iLength }
	);
	testRequire(
		pRequest != NULL,
		"HTTP Client wait thread request create failed"
	);
	pCall = xrtHttpClientDoAsync(pClient, pRequest, NULL);
	testRequire(
		pCall != NULL,
		"HTTP Client wait thread Call submission failed"
	);
	testHttpOriginWait(
		&Origin.Requests,
		1,
		"HTTP Client wait thread request did not reach Origin"
	);
	pClosed = xrtHttpClientWaitAsync(pClient);
	testRequire(
		pClosed != NULL,
		"HTTP Client final close wait creation failed"
	);

	for ( size_t i = 0;
		i < TEST_HTTP_CLIENT_WAIT_THREADS;
		i++ ) {
		Contexts[i].Future = xrtHttpClientWaitAsync(pClient);
		Contexts[i].Ready = &Ready;
		Contexts[i].Release = &Release;
		Contexts[i].Cancel = (i & 1u) == 0;
		xrtAtomic32Init(
			&Contexts[i].State,
			XFUTURE_PENDING
		);
		Threads[i] = xrtThreadCreate(
			testHttpClientWaitThread,
			&Contexts[i],
			0
		);
		testRequire(
			(Contexts[i].Future != NULL) &&
			(Threads[i] != NULL),
			"HTTP Client concurrent wait setup failed"
		);
	}
	Deadline = xrtDeadlineAfter(UINT64_C(5000000));
	while ( xrtAtomic32Load(
		&Ready,
		XMEMORY_ACQUIRE
	) != TEST_HTTP_CLIENT_WAIT_THREADS ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP Client wait threads did not reach barrier"
		);
		xrtThreadYield();
	}
	xrtAtomic32Store(&Release, 1, XMEMORY_RELEASE);
	testRequire(
		xrtHttpClientAbort(pClient),
		"HTTP Client concurrent wait abort failed"
	);

	for ( size_t i = 0;
		i < TEST_HTTP_CLIENT_WAIT_THREADS;
		i++ ) {
		xfuturestate State;

		testRequire(
			xrtThreadWaitFor(
				Threads[i],
				UINT64_C(5000000)
			) == XWAIT_OK,
			"HTTP Client wait thread did not finish"
		);
		State = (xfuturestate)xrtAtomic32Load(
			&Contexts[i].State,
			XMEMORY_ACQUIRE
		);
		testRequire(
			(State == XFUTURE_RESOLVED) ||
			(State == XFUTURE_CANCELLED),
			"HTTP Client wait race produced an invalid terminal state"
		);
		xrtThreadDestroy(Threads[i]);
		xrtFutureDestroy(Contexts[i].Future);
	}
	testRequire(
		(xrtFutureWaitFor(
			pCall,
			UINT64_C(5000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pCall) == XFUTURE_CANCELLED) &&
		(xrtFutureWaitFor(
			pClosed,
			UINT64_C(5000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pClosed) == XFUTURE_RESOLVED) &&
		(xrtHttpClientState(pClient) == XHTTP_CLIENT_CLOSED),
		"HTTP Client final concurrent close did not resolve"
	);

	xrtFutureDestroy(pCall);
	xrtFutureDestroy(pClosed);
	xrtHttpRequestDestroy(pRequest);
	xrtHttpClientDestroy(pClient);
	testHttpOriginStop(&Origin);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"HTTP Client wait threads retained Engine ownership"
	);
	printf(
		"[PASS] HTTP Client close Future thread race (%u)\n",
		(unsigned int)TEST_HTTP_CLIENT_WAIT_THREADS
	);
	return 0;
}

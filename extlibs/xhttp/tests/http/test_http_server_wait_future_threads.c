#include "../test.h"



#define TEST_HTTP_SERVER_WAIT_THREADS 64u



/* 每个线程独占一个 Future，并与 Server 关闭竞争取消或等待。 */
typedef struct test_http_server_wait_thread {
	xfuture* Future;
	xatomic32* Ready;
	xatomic32* Release;
	xatomic32 State;
	bool Cancel;
} test_http_server_wait_thread;



/* 在统一屏障后并发取消或等待各自的关闭 Future。 */
static int32 testHttpServerWaitThread(ptr pData)
{
	test_http_server_wait_thread* pContext =
		(test_http_server_wait_thread*)pData;
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



/* 压测取消回调、关闭摘链和 Server 引用释放的并发边界。 */
int main(void)
{
	xatomic32 Ready;
	xatomic32 Release;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xnetengine* pEngine;
	xhttpserver* pServer;
	xfuture* pClosed;
	test_http_server_wait_thread Contexts[
		TEST_HTTP_SERVER_WAIT_THREADS
	];
	xthread* Threads[TEST_HTTP_SERVER_WAIT_THREADS];
	xdeadline Deadline;

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
		"HTTP server wait thread engine start failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP server wait thread address setup failed"
	);
	pServer = xrtHttpServerStart(
		pEngine,
		&ServerConfig,
		NULL
	);
	testRequire(
		pServer != NULL,
		"HTTP server wait thread start failed"
	);
	pClosed = xrtHttpServerWaitAsync(pServer);
	testRequire(
		pClosed != NULL,
		"HTTP server final close wait creation failed"
	);

	for ( size_t i = 0;
		i < TEST_HTTP_SERVER_WAIT_THREADS;
		i++ ) {
		Contexts[i].Future = xrtHttpServerWaitAsync(pServer);
		Contexts[i].Ready = &Ready;
		Contexts[i].Release = &Release;
		Contexts[i].Cancel = (i & 1u) == 0;
		xrtAtomic32Init(
			&Contexts[i].State,
			XFUTURE_PENDING
		);
		Threads[i] = xrtThreadCreate(
			testHttpServerWaitThread,
			&Contexts[i],
			0
		);
		testRequire(
			(Contexts[i].Future != NULL) &&
			(Threads[i] != NULL),
			"HTTP server concurrent wait setup failed"
		);
	}
	Deadline = xrtDeadlineAfter(UINT64_C(5000000));
	while ( xrtAtomic32Load(
		&Ready,
		XMEMORY_ACQUIRE
	) != TEST_HTTP_SERVER_WAIT_THREADS ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP server wait threads did not reach barrier"
		);
		xrtThreadYield();
	}
	xrtAtomic32Store(&Release, 1, XMEMORY_RELEASE);
	testRequire(
		xrtHttpServerDrain(pServer),
		"HTTP server concurrent wait drain failed"
	);

	for ( size_t i = 0;
		i < TEST_HTTP_SERVER_WAIT_THREADS;
		i++ ) {
		xfuturestate State;

		testRequire(
			xrtThreadWaitFor(
				Threads[i],
				UINT64_C(5000000)
			) == XWAIT_OK,
			"HTTP server wait thread did not finish"
		);
		State = (xfuturestate)xrtAtomic32Load(
			&Contexts[i].State,
			XMEMORY_ACQUIRE
		);
		testRequire(
			(State == XFUTURE_RESOLVED) ||
			(State == XFUTURE_CANCELLED),
			"HTTP server wait race produced an invalid terminal state"
		);
		xrtThreadDestroy(Threads[i]);
		xrtFutureDestroy(Contexts[i].Future);
	}
	testRequire(
		(xrtFutureWaitFor(
			pClosed,
			UINT64_C(5000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pClosed) == XFUTURE_RESOLVED) &&
		(xrtHttpServerState(pServer) == XHTTP_SERVER_CLOSED),
		"HTTP server final close wait did not resolve"
	);

	xrtFutureDestroy(pClosed);
	xrtHttpServerDestroy(pServer);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"HTTP server wait threads retained Engine ownership"
	);
	printf(
		"[PASS] HTTP server close Future thread race (%u)\n",
		(unsigned int)TEST_HTTP_SERVER_WAIT_THREADS
	);
	return 0;
}

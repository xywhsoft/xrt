#include <xrt/detail/wait.h>
#include "../test.h"



#ifndef TEST_HTTP_SERVER_WAIT_BACKEND
	#define TEST_HTTP_SERVER_WAIT_BACKEND XNET_PORT_SELECT
#endif

#ifndef TEST_HTTP_SERVER_WAIT_BACKEND_NAME
	#define TEST_HTTP_SERVER_WAIT_BACKEND_NAME "select"
#endif



/* 测试状态记录 Shutdown 内注册的等待和回调发布顺序。 */
typedef struct test_http_server_wait {
	xatomic32 Shutdown;
	xatomic32 FirstHeld;
	xatomic32 ReleaseFirst;
	xatomic32 WatchReleased;
	xhttpserver* Server;
	xfuture* Pending;
	xfuturewatch FirstWatch;
	xfuture* CallbackWait;
} test_http_server_wait;



/* Shutdown 回调验证 CLOSED，并建立一个只能在回调返回后完成的等待。 */
static void testHttpServerWaitShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_http_server_wait* pState =
		(test_http_server_wait*)pData;

	testRequire(
		xrtHttpServerState(pServer) == XHTTP_SERVER_CLOSED,
		"HTTP server wait shutdown state mismatch"
	);
	pState->CallbackWait = xrtHttpServerWaitAsync(pServer);
	testRequire(
		(pState->CallbackWait != NULL) &&
		(xrtFutureWaitFor(
			pState->CallbackWait,
			0
		 ) == XWAIT_TIMEOUT),
		"HTTP server wait completed inside Shutdown callback"
	);
	xrtAtomic32Store(
		&pState->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



/* Hold the real first Promise publication before the producer reaches the
 * callback-registered waiter. Different waiters are independent publications. */
static void testHttpServerWaitHoldFirst(ptr pData)
{
	test_http_server_wait* pState = (test_http_server_wait*)pData;
	testRequire(xrtFutureState(pState->Pending) == XFUTURE_RESOLVED,
		"HTTP first waiter notification preceded publication");
	xrtAtomic32Store(&pState->FirstHeld, 1, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(&pState->ReleaseFirst, XMEMORY_ACQUIRE) == 0 ) {
		xrtThreadYield();
	}
}



static void testHttpServerWaitReleaseWatch(ptr pData)
{
	test_http_server_wait* pState = (test_http_server_wait*)pData;
	(void)xrtAtomic32FetchAdd(&pState->WatchReleased, 1, XMEMORY_RELEASE);
}



/* Drain may finish on its caller; keep the controller free to release the gate. */
static int32 testHttpServerWaitDrain(ptr pData)
{
	test_http_server_wait* pState = (test_http_server_wait*)pData;
	return xrtHttpServerDrain(pState->Server) ? 0 : 1;
}



/* 验证多等待者、独立取消、关闭完成与迟注册语义。 */
int main(void)
{
	test_http_server_wait State;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents Events;
	xnetengine* pEngine;
	xhttpserver* pServer;
	xfuture* pCancelled;
	xfuture* pPending;
	xfuture* pLate;
	xthread* pDrain;
	double Deadline;

	testRequire(
		(xrtHttpServerWaitAsync(NULL) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP_SERVER_ERROR_ARGUMENT) &&
		(strcmp(
			xrtErrorOperation(xrtGetError()),
			"wait-http-server"
		 ) == 0),
		"HTTP server null wait error mismatch"
	);
	xrtClearError();
	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.Shutdown, 0);
	xrtAtomic32Init(&State.FirstHeld, 0);
	xrtAtomic32Init(&State.ReleaseFirst, 0);
	xrtAtomic32Init(&State.WatchReleased, 0);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_HTTP_SERVER_WAIT_BACKEND;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) && xrtNetEngineStart(pEngine),
		"HTTP server wait engine start failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP server wait address setup failed"
	);
	xrtHttpServerEventsInit(&Events);
	Events.Shutdown = testHttpServerWaitShutdown;
	Events.Data = &State;
	pServer = xrtHttpServerStart(
		pEngine,
		&ServerConfig,
		&Events
	);
	testRequire(
		(pServer != NULL) &&
		(xrtHttpServerState(pServer) == XHTTP_SERVER_RUNNING),
		"HTTP server wait start failed"
	);

	pCancelled = xrtHttpServerWaitAsync(pServer);
	pPending = xrtHttpServerWaitAsync(pServer);
	testRequire(
		(pCancelled != NULL) &&
		(pPending != NULL) &&
		(xrtFutureWaitFor(pCancelled, 0) == XWAIT_TIMEOUT) &&
		(xrtFutureWaitFor(pPending, 0) == XWAIT_TIMEOUT),
		"HTTP server waits did not begin pending"
	);
	testRequire(
		xrtFutureCancel(pCancelled) &&
		(xrtFutureWaitFor(
			pCancelled,
			INT64_C(5000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pCancelled) == XFUTURE_CANCELLED) &&
		(xrtHttpServerState(pServer) == XHTTP_SERVER_RUNNING) &&
		(xrtFutureWaitFor(pPending, 0) == XWAIT_TIMEOUT),
		"HTTP server wait cancellation affected another owner"
	);
	State.Server = pServer;
	State.Pending = pPending;
	testRequire(xrtFutureWatchInit(&State.FirstWatch, testHttpServerWaitHoldFirst,
		testHttpServerWaitReleaseWatch, &State) &&
		(xrtFutureWatchAdd(pPending, &State.FirstWatch) == XFUTURE_WATCH_PENDING),
		"HTTP first-waiter publication gate setup failed");
	pDrain = xrtThreadCreate(testHttpServerWaitDrain, &State, 0);
	testRequire(pDrain != NULL, "HTTP server wait drain thread failed");
	Deadline = __xrtWaitAfter(5000);
	while ( xrtAtomic32Load(&State.FirstHeld, XMEMORY_ACQUIRE) == 0 ) {
		testRequire(!__xrtWaitExpired(Deadline), "HTTP first-waiter gate was not reached");
		xrtThreadYield();
	}
	testRequire(
		(xrtFutureWaitFor(
			pPending,
			INT64_C(5000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pPending) == XFUTURE_RESOLVED) &&
		(xrtHttpServerState(pServer) == XHTTP_SERVER_CLOSED) &&
		(xrtAtomic32Load(
			&State.Shutdown,
			XMEMORY_ACQUIRE
		 ) == 1) &&
		(State.CallbackWait != NULL) &&
		(xrtFutureWaitFor(
			State.CallbackWait,
			0
		 ) == XWAIT_TIMEOUT) &&
		(xrtFutureState(State.CallbackWait) == XFUTURE_PENDING),
		"HTTP callback waiter escaped the held first publication"
	);
	xrtAtomic32Store(&State.ReleaseFirst, 1, XMEMORY_RELEASE);
	xrtFutureWatchRemove(pPending, &State.FirstWatch);
	testRequire((xrtFutureWaitFor(State.CallbackWait, INT64_C(5000)) == XWAIT_OK) &&
		(xrtFutureState(State.CallbackWait) == XFUTURE_RESOLVED) &&
		(xrtAtomic32Load(&State.WatchReleased, XMEMORY_ACQUIRE) == 1) &&
		(xrtThreadWaitFor(pDrain, INT64_C(5000)) == XWAIT_OK) &&
		(xrtThreadExitCode(pDrain) == 0),
		"HTTP independent waiter did not finish after publication gate release");
	xrtThreadDestroy(pDrain);
	pLate = xrtHttpServerWaitAsync(pServer);
	testRequire(
		(pLate != NULL) &&
		(xrtFutureWaitFor(pLate, 0) == XWAIT_OK) &&
		(xrtFutureState(pLate) == XFUTURE_RESOLVED),
		"HTTP server late close wait was not immediately ready"
	);

	xrtFutureDestroy(pCancelled);
	xrtFutureDestroy(pPending);
	xrtFutureDestroy(pLate);
	xrtFutureDestroy(State.CallbackWait);
	xrtHttpServerDestroy(pServer);
	testRequire(
		testHttpServerEngineDestroy(pEngine),
		"HTTP server wait retained Engine ownership"
	);
	printf(
		"[PASS] HTTP server close Future (%s)\n",
		TEST_HTTP_SERVER_WAIT_BACKEND_NAME
	);
	return 0;
}

#include "../test.h"



#ifndef TEST_HTTP_SERVER_FUTURE_OOM_ALLOW
	#define TEST_HTTP_SERVER_FUTURE_OOM_ALLOW 0
#endif

#ifndef TEST_HTTP_SERVER_FUTURE_OOM_NAME
	#define TEST_HTTP_SERVER_FUTURE_OOM_NAME "context"
#endif

#define TEST_HTTP_SERVER_FUTURE_OOM_CLASSES ((size_t)64)
#define TEST_HTTP_SERVER_FUTURE_OOM_BLOCKS ((size_t)16384)



/* 可切换的底层分配器只在精确 OOM 窗口内拒绝新的 Heap span。 */
typedef struct test_http_server_future_oom_allocator {
	xatomic32 Gate;
	xatomic32 Allow;
	xatomic64 Calls;
	xatomic64 Denied;
} test_http_server_future_oom_allocator;



/* 测试状态保存失败请求的 Future 端点和服务端终态。 */
typedef struct test_http_server_future_oom {
	test_http_server_future_oom_allocator* Allocator;
	ptr* Held;
	size_t HeldCount;
	size_t HeldCapacity;
	xpromise* FailedPromise;
	xfuture* FailedFuture;
	xcancel* FailedCancel;
	xatomic32 Requests;
	xatomic32 Closed;
	xatomic32 Shutdown;
	xatomic32 BarrierStarted;
	xatomic32 BarrierRelease;
	xatomic32 BarrierDone;
} test_http_server_future_oom;



/* 在门关闭时转发系统分配，在门开启时只放行指定次数。 */
static ptr testHttpServerFutureOomAlloc(
	ptr pData,
	size_t iSize
)
{
	test_http_server_future_oom_allocator* pAllocator =
		(test_http_server_future_oom_allocator*)pData;

	(void)xrtAtomic64FetchAdd(
		&pAllocator->Calls,
		1,
		XMEMORY_RELAXED
	);
	if ( xrtAtomic32Load(
		&pAllocator->Gate,
		XMEMORY_ACQUIRE
	) != 0 ) {
		uint32 iAllow = xrtAtomic32Load(
			&pAllocator->Allow,
			XMEMORY_ACQUIRE
		);

		if ( iAllow == 0 ) {
			(void)xrtAtomic64FetchAdd(
				&pAllocator->Denied,
				1,
				XMEMORY_RELAXED
			);
			return NULL;
		}
		(void)xrtAtomic32FetchSub(
			&pAllocator->Allow,
			1,
			XMEMORY_ACQ_REL
		);
	}
	return malloc(iSize);
}



/* 重分配服从与初始分配相同的精确故障门。 */
static ptr testHttpServerFutureOomRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	test_http_server_future_oom_allocator* pAllocator =
		(test_http_server_future_oom_allocator*)pData;

	(void)xrtAtomic64FetchAdd(
		&pAllocator->Calls,
		1,
		XMEMORY_RELAXED
	);
	if ( xrtAtomic32Load(
		&pAllocator->Gate,
		XMEMORY_ACQUIRE
	) != 0 ) {
		uint32 iAllow = xrtAtomic32Load(
			&pAllocator->Allow,
			XMEMORY_ACQUIRE
		);

		if ( iAllow == 0 ) {
			(void)xrtAtomic64FetchAdd(
				&pAllocator->Denied,
				1,
				XMEMORY_RELAXED
			);
			return NULL;
		}
		(void)xrtAtomic32FetchSub(
			&pAllocator->Allow,
			1,
			XMEMORY_ACQ_REL
		);
	}
	return realloc(pMemory, iSize);
}



/* 释放所有由测试分配器成功取得的底层块。 */
static void testHttpServerFutureOomFree(
	ptr pData,
	ptr pMemory
)
{
	(void)pData;
	free(pMemory);
}



/* 耗尽 Heap 每个池化尺寸类已有的空闲块，不申请新的 span。 */
static void testHttpServerFutureOomExhaust(
	test_http_server_future_oom* pState
)
{
	xrtAtomic32Store(
		&pState->Allocator->Allow,
		0,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pState->Allocator->Gate,
		1,
		XMEMORY_RELEASE
	);
	for ( size_t i = 1;
		i <= TEST_HTTP_SERVER_FUTURE_OOM_CLASSES;
		i++ ) {
		for ( ;; ) {
			ptr pMemory = xrtMalloc(i * 16);

			if ( pMemory == NULL ) {
				xrtClearError();
				break;
			}
			testRequire(
				pState->HeldCount <
				pState->HeldCapacity,
				"HTTP server Future OOM exhaustion overflowed"
			);
			pState->Held[pState->HeldCount++] =
				pMemory;
		}
	}
}



/* 关闭故障门并归还为尺寸类耗尽而暂时持有的块。 */
static void testHttpServerFutureOomRestore(
	test_http_server_future_oom* pState
)
{
	xrtAtomic32Store(
		&pState->Allocator->Gate,
		0,
		XMEMORY_RELEASE
	);
	for ( size_t i = 0; i < pState->HeldCount; i++ ) {
		xrtFree(pState->Held[i]);
	}
	pState->HeldCount = 0;
	xrtClearError();
}



/* 暂停唯一 Worker，避免全局故障门误伤 Listener 事件循环。 */
static void testHttpServerFutureOomBlock(
	xnetworker* pWorker,
	ptr pData
)
{
	test_http_server_future_oom* pState =
		(test_http_server_future_oom*)pData;

	(void)pWorker;
	xrtAtomic32Store(
		&pState->BarrierStarted,
		1,
		XMEMORY_RELEASE
	);
	while ( xrtAtomic32Load(
		&pState->BarrierRelease,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
	xrtAtomic32Store(
		&pState->BarrierDone,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证 Server 关闭等待创建 OOM 原子回滚，并可在恢复后重新建立。 */
static void testHttpServerFutureOomWaitCreate(
	test_http_server_future_oom* pState,
	xnetengine* pEngine,
	xhttpserver* pServer
)
{
	xdeadline Deadline;
	xfuture* pFuture;

	testRequire(
		xrtNetEnginePost(
			pEngine,
			0,
			testHttpServerFutureOomBlock,
			pState
		),
		"HTTP server close wait OOM barrier post failed"
	);
	Deadline = xrtDeadlineAfter(UINT64_C(5000000));
	while ( xrtAtomic32Load(
		&pState->BarrierStarted,
		XMEMORY_ACQUIRE
	) == 0 ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP server close wait OOM barrier did not start"
		);
		xrtThreadYield();
	}
	testHttpServerFutureOomExhaust(pState);
	xrtAtomic32Store(
		&pState->Allocator->Allow,
		TEST_HTTP_SERVER_FUTURE_OOM_ALLOW,
		XMEMORY_RELEASE
	);
	pFuture = xrtHttpServerWaitAsync(pServer);
	testRequire(
		(pFuture == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtHttpServerState(pServer) == XHTTP_SERVER_RUNNING),
		"HTTP server close wait OOM rollback mismatch"
	);
	testHttpServerFutureOomRestore(pState);
	xrtAtomic32Store(
		&pState->BarrierRelease,
		1,
		XMEMORY_RELEASE
	);
	Deadline = xrtDeadlineAfter(UINT64_C(5000000));
	while ( xrtAtomic32Load(
		&pState->BarrierDone,
		XMEMORY_ACQUIRE
	) == 0 ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP server close wait OOM barrier did not finish"
		);
		xrtThreadYield();
	}
	pFuture = xrtHttpServerWaitAsync(pServer);
	testRequire(
		(pFuture != NULL) &&
		xrtFutureCancel(pFuture) &&
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(5000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_CANCELLED) &&
		(xrtHttpServerState(pServer) == XHTTP_SERVER_RUNNING),
		"HTTP server close wait did not recover after OOM"
	);
	xrtFutureDestroy(pFuture);
}



/* Future 最后释放时销毁其拥有的 Reply。 */
static void testHttpServerFutureOomReplyFree(
	ptr pValue,
	ptr pData
)
{
	(void)pData;
	xrtHttpReplyDestroy((xhttpreply*)pValue);
}



/* 第一条请求验证 OOM 回滚，第二条请求验证后续完成路径。 */
static void testHttpServerFutureOomRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_http_server_future_oom* pState =
		(test_http_server_future_oom*)pData;
	uint32 iRequest = xrtAtomic32FetchAdd(
		&pState->Requests,
		1,
		XMEMORY_ACQ_REL
	) + 1;
	xpromise* pPromise;
	xfuture* pFuture = NULL;
	xhttpreply* pReply;

	(void)pServer;
	testRequire(
		xrtHttpServerRequestTarget(pRequest).Size != 0,
		"HTTP server Future OOM request target is empty"
	);
	pPromise = xrtPromiseCreate(&pFuture, NULL);
	testRequire(
		(pPromise != NULL) &&
		(pFuture != NULL),
		"HTTP server Future OOM pair creation failed"
	);
	if ( iRequest == 1 ) {
		pState->FailedPromise = pPromise;
		pState->FailedFuture = pFuture;
		pState->FailedCancel = xrtFutureCancelToken(
			pFuture
		);
		testRequire(
			pState->FailedCancel != NULL,
			"HTTP server Future OOM cancel token failed"
		);
		testHttpServerFutureOomExhaust(pState);
		xrtAtomic32Store(
			&pState->Allocator->Allow,
			TEST_HTTP_SERVER_FUTURE_OOM_ALLOW,
			XMEMORY_RELEASE
		);
		testRequire(
			!xrtHttpConnRespondFuture(
				pConnection,
				pFuture
			) &&
			(xrtErrorKind(xrtGetError()) ==
			 XERR_MEMORY),
			"HTTP server Future OOM rollback mismatch"
		);
		testHttpServerFutureOomRestore(pState);
		testRequire(
			xrtHttpConnRespondFuture(
				pConnection,
				pFuture
			),
			"HTTP server Future did not recover after OOM"
		);
		testRequire(
			xrtHttpConnReply(
				pConnection,
				503,
				XRT_STR_LITERAL("text/plain"),
				XRT_BYTES_LITERAL("recovered")
			) == XNET_RESULT_OK,
			"HTTP server Future OOM fallback response failed"
		);
		return;
	}
	pReply = xrtHttpReplyCreate(200);
	testRequire(
		(pReply != NULL) &&
		xrtHttpReplySetBytes(
			pReply,
			XRT_BYTES_LITERAL("ready"),
			XRT_STR_LITERAL("text/plain")
		) &&
		xrtPromiseResolveOwned(
			pPromise,
			pReply,
			testHttpServerFutureOomReplyFree,
			NULL
		) &&
		xrtHttpConnRespondFuture(
			pConnection,
			pFuture
		),
		"HTTP server Future OOM completion recovery failed"
	);
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);
}



/* 记录每条测试连接的唯一终态。 */
static void testHttpServerFutureOomClose(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_future_oom* pState =
		(test_http_server_future_oom*)pData;

	(void)pServer;
	(void)pConnection;
	(void)Result;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pState->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 Server 已排空全部连接。 */
static void testHttpServerFutureOomShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_http_server_future_oom* pState =
		(test_http_server_future_oom*)pData;

	testRequire(
		xrtHttpServerState(pServer) ==
		XHTTP_SERVER_CLOSED,
		"HTTP server Future OOM shutdown state mismatch"
	);
	xrtAtomic32Store(
		&pState->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



/* 在截止时间前等待 Worker 发布指定计数。 */
static void testHttpServerFutureOomWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(
		UINT64_C(10000000)
	);

	while ( xrtAtomic32Load(
		pValue,
		XMEMORY_ACQUIRE
	) < iExpected ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			sMessage
		);
		xrtThreadYield();
	}
}



/* 完整发送一条短请求。 */
static void testHttpServerFutureOomSend(
	xnetsocket Socket,
	cbytes pData,
	size_t iSize
)
{
	size_t iOffset = 0;

	while ( iOffset < iSize ) {
		size_t iSent = 0;

		testRequire(
			(xrtNetSocketSend(
				Socket,
				pData + iOffset,
				iSize - iOffset,
				&iSent
			 ) == XNET_RESULT_OK) &&
			(iSent != 0),
			"HTTP server Future OOM request send failed"
		);
		iOffset += iSent;
	}
}



/* 发起一次关闭连接的请求并收取完整响应。 */
static size_t testHttpServerFutureOomExchange(
	const xnetaddr* pAddress,
	cstr sPath,
	char* pOutput,
	size_t iCapacity
)
{
	char Request[256];
	xnetsocket Socket = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		0
	);
	size_t iOffset = 0;
	int iSize;

	testRequire(
		(Socket != NULL) &&
		(xrtNetSocketConnect(
			Socket,
			pAddress
		 ) == XNET_RESULT_OK),
		"HTTP server Future OOM client connect failed"
	);
	iSize = snprintf(
		Request,
		sizeof(Request),
		"GET %s HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"Connection: close\r\n"
		"\r\n",
		sPath
	);
	testRequire(
		(iSize > 0) &&
		((size_t)iSize < sizeof(Request)),
		"HTTP server Future OOM request format failed"
	);
	testHttpServerFutureOomSend(
		Socket,
		(cbytes)Request,
		(size_t)iSize
	);
	while ( iOffset < (iCapacity - 1) ) {
		size_t iRead = 0;
		xnetresult Result = xrtNetSocketRecv(
			Socket,
			pOutput + iOffset,
			iCapacity - iOffset - 1,
			&iRead
		);

		if ( Result == XNET_RESULT_CLOSED ) {
			break;
		}
		testRequire(
			(Result == XNET_RESULT_OK) &&
			(iRead != 0),
			"HTTP server Future OOM response receive failed"
		);
		iOffset += iRead;
	}
	pOutput[iOffset] = '\0';
	testRequire(
		xrtNetSocketClose(Socket),
		"HTTP server Future OOM client close failed"
	);
	return iOffset;
}



/* 验证 Future 桥两个构造阶段的 OOM 原子性和恢复能力。 */
int main(void)
{
	test_http_server_future_oom_allocator AllocatorState;
	test_http_server_future_oom State;
	xallocator Allocator;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents Events;
	xhttpserverstats Stats;
	xnetengine* pEngine;
	xhttpserver* pServer;
	xnetaddr Address;
	char Response[2048];

	memset(&AllocatorState, 0, sizeof(AllocatorState));
	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&AllocatorState.Gate, 0);
	xrtAtomic32Init(&AllocatorState.Allow, 0);
	xrtAtomic64Init(&AllocatorState.Calls, 0);
	xrtAtomic64Init(&AllocatorState.Denied, 0);
	xrtAtomic32Init(&State.Requests, 0);
	xrtAtomic32Init(&State.Closed, 0);
	xrtAtomic32Init(&State.Shutdown, 0);
	xrtAtomic32Init(&State.BarrierStarted, 0);
	xrtAtomic32Init(&State.BarrierRelease, 0);
	xrtAtomic32Init(&State.BarrierDone, 0);
	State.Allocator = &AllocatorState;
	State.HeldCapacity =
		TEST_HTTP_SERVER_FUTURE_OOM_BLOCKS;
	State.Held = (ptr*)malloc(
		State.HeldCapacity * sizeof(ptr)
	);
	testRequire(
		State.Held != NULL,
		"HTTP server Future OOM hold table failed"
	);
	Allocator.Context = &AllocatorState;
	Allocator.Alloc = testHttpServerFutureOomAlloc;
	Allocator.Realloc = testHttpServerFutureOomRealloc;
	Allocator.Free = testHttpServerFutureOomFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"HTTP server Future OOM allocator install failed"
	);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) &&
		xrtNetEngineStart(pEngine),
		"HTTP server Future OOM engine start failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP server Future OOM address setup failed"
	);
	xrtHttpServerEventsInit(&Events);
	Events.Request = testHttpServerFutureOomRequest;
	Events.Close = testHttpServerFutureOomClose;
	Events.Shutdown = testHttpServerFutureOomShutdown;
	Events.Data = &State;
	pServer = xrtHttpServerStart(
		pEngine,
		&ServerConfig,
		&Events
	);
	testRequire(
		(pServer != NULL) &&
		xrtHttpServerLocal(pServer, 0, &Address),
		"HTTP server Future OOM start failed"
	);
	testHttpServerFutureOomWaitCreate(
		&State,
		pEngine,
		pServer
	);

	testRequire(
		(testHttpServerFutureOomExchange(
			&Address,
			"/oom",
			Response,
			sizeof(Response)
		 ) != 0) &&
		(strstr(
			Response,
			"HTTP/1.1 503 Service Unavailable\r\n"
		 ) != NULL) &&
		(strstr(
			Response,
			"\r\n\r\nrecovered"
		 ) != NULL),
		"HTTP server Future OOM fallback response mismatch"
	);
	testHttpServerFutureOomWait(
		&State.Closed,
		1,
		"HTTP server Future OOM connection did not close"
	);
	testRequire(
		xrtCancelRequested(State.FailedCancel),
		"HTTP server Future OOM replacement did not cancel source"
	);
	testRequire(
		xrtPromiseCancel(State.FailedPromise),
		"HTTP server Future OOM source cancellation failed"
	);
	xrtCancelDestroy(State.FailedCancel);
	xrtPromiseDestroy(State.FailedPromise);
	xrtFutureDestroy(State.FailedFuture);

	testRequire(
		(testHttpServerFutureOomExchange(
			&Address,
			"/ready",
			Response,
			sizeof(Response)
		 ) != 0) &&
		(strstr(
			Response,
			"HTTP/1.1 200 OK\r\n"
		 ) != NULL) &&
		(strstr(Response, "\r\n\r\nready") != NULL),
		"HTTP server Future OOM completion response mismatch"
	);
	testHttpServerFutureOomWait(
		&State.Closed,
		2,
		"HTTP server Future OOM recovery connection did not close"
	);
	testRequire(
		xrtHttpServerDrain(pServer),
		"HTTP server Future OOM drain failed"
	);
	testHttpServerFutureOomWait(
		&State.Shutdown,
		1,
		"HTTP server Future OOM shutdown missing"
	);
	testRequire(
		xrtHttpServerStats(pServer, &Stats) &&
		(Stats.Accepted == 2) &&
		(Stats.Requests == 2) &&
		(Stats.Responses == 2) &&
		(Stats.ProtocolErrors == 0) &&
		(Stats.Timeouts == 0) &&
		(Stats.Connections == 0),
		"HTTP server Future OOM statistics mismatch"
	);
	xrtHttpServerDestroy(pServer);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"HTTP server Future OOM retained Engine ownership"
	);
	testRequire(
		(xrtAtomic64Load(
			&AllocatorState.Denied,
			XMEMORY_ACQUIRE
		 ) > TEST_HTTP_SERVER_FUTURE_OOM_CLASSES) &&
		(xrtAtomic64Load(
			&AllocatorState.Calls,
			XMEMORY_ACQUIRE
		 ) != 0),
		"HTTP server Future OOM allocator was not exercised"
	);
	free(State.Held);
	printf(
		"[PASS] HTTP server Future OOM rollback (%s)\n",
		TEST_HTTP_SERVER_FUTURE_OOM_NAME
	);
	return 0;
}

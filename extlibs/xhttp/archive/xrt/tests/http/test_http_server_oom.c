#include "../test.h"
#include "../../src/internal/xrt_http_server_runtime.h"



#define TEST_HTTP_SERVER_OOM_CASES ((uint32)12)
#define TEST_HTTP_SERVER_OOM_STABLE_ROUNDS ((uint32)2)
#define TEST_HTTP_SERVER_OOM_STABILIZE_LIMIT ((uint32)8)



/* 线程安全失败分配器用于扫描 HTTP 运行时的真实异步分配点。 */
typedef struct test_http_server_oom_allocator {
	xatomic64 Calls;
	xatomic64 FailAt;
	xatomic64 Live;
} test_http_server_oom_allocator;



/* 一次完整运行共享 Server、Client 与故障观测状态。 */
typedef struct test_http_server_oom {
	test_http_server_oom_allocator* Allocator;
	xnetengine* Engine;
	xhttpserver* Server;
	xnetstream* Client;
	xatomic32 Requests;
	xatomic32 Failures;
	xatomic32 PrepareFailures;
	xatomic32 QueueFailures;
	xatomic32 Successes;
	xatomic32 ConnectionClosed;
	xatomic32 ClientClosed;
	xatomic32 Shutdown;
	xatomic32 Errors;
	char LargeHeader[1536];
	char Request[2048];
	size_t RequestSize;
	bool Inject;
} test_http_server_oom;



/* 在指定全局分配序号精确失败一次。 */
static ptr testHttpServerOomAlloc(
	ptr pContext,
	size_t iSize
)
{
	test_http_server_oom_allocator* pState =
		(test_http_server_oom_allocator*)pContext;
	uint64 iCall = xrtAtomic64FetchAdd(
		&pState->Calls,
		1,
		XMEMORY_RELAXED
	) + 1;
	ptr pMemory;

	if ( iCall == xrtAtomic64Load(
		&pState->FailAt,
		XMEMORY_ACQUIRE
	) ) {
		return NULL;
	}
	pMemory = malloc(iSize);
	if ( pMemory != NULL ) {
		(void)xrtAtomic64FetchAdd(
			&pState->Live,
			1,
			XMEMORY_RELAXED
		);
	}
	return pMemory;
}



/* 重分配失败时保留原块，成功扩展空指针时增加存活计数。 */
static ptr testHttpServerOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_http_server_oom_allocator* pState =
		(test_http_server_oom_allocator*)pContext;
	uint64 iCall = xrtAtomic64FetchAdd(
		&pState->Calls,
		1,
		XMEMORY_RELAXED
	) + 1;
	ptr pResult;

	if ( iCall == xrtAtomic64Load(
		&pState->FailAt,
		XMEMORY_ACQUIRE
	) ) {
		return NULL;
	}
	pResult = realloc(pMemory, iSize);
	if ( (pResult != NULL) && (pMemory == NULL) ) {
		(void)xrtAtomic64FetchAdd(
			&pState->Live,
			1,
			XMEMORY_RELAXED
		);
	}
	return pResult;
}



/* 释放成功取得的底层块并维护存活计数。 */
static void testHttpServerOomFree(
	ptr pContext,
	ptr pMemory
)
{
	test_http_server_oom_allocator* pState =
		(test_http_server_oom_allocator*)pContext;
	uint64 iPrevious;

	if ( pMemory == NULL ) {
		return;
	}
	iPrevious = xrtAtomic64FetchSub(
		&pState->Live,
		1,
		XMEMORY_RELAXED
	);
	testRequire(
		iPrevious != 0,
		"HTTP server OOM live counter underflow"
	);
	free(pMemory);
}



/* 在截止时间前等待原子计数达到目标。 */
static void testHttpServerOomWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(10000000u);

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



/*
	持有目标尺寸类的全部空闲块，并在下一次 span 扩展时失败。
	返回的块必须在目标调用结束后统一释放。
*/
static size_t testHttpServerOomExhaust(
	test_http_server_oom_allocator* pAllocator,
	size_t iSize,
	ptr* pHeld,
	size_t iCapacity
)
{
	size_t iCount = 0;

	xrtAtomic64Store(
		&pAllocator->FailAt,
		xrtAtomic64Load(
			&pAllocator->Calls,
			XMEMORY_ACQUIRE
		) + 1,
		XMEMORY_RELEASE
	);
	while ( iCount < iCapacity ) {
		pHeld[iCount] = xrtMalloc(iSize);
		if ( pHeld[iCount] == NULL ) {
			break;
		}
		iCount++;
	}
	xrtAtomic64Store(
		&pAllocator->FailAt,
		0,
		XMEMORY_RELEASE
	);
	testRequire(
		(iCount != 0) &&
		(iCount < iCapacity) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP server OOM could not exhaust a size class"
	);
	xrtClearError();
	return iCount;
}



/* 释放尺寸类耗尽门持有的全部块。 */
static void testHttpServerOomReleaseHeld(
	ptr* pHeld,
	size_t iCount
)
{
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		xrtFree(pHeld[i]);
	}
}



/* 构建一组位于同一 keep-alive 连接上的流水线请求。 */
static void testHttpServerOomRequests(
	test_http_server_oom* pState
)
{
	uint32 i;

	pState->RequestSize = 0;
	for ( i = 0; i < TEST_HTTP_SERVER_OOM_CASES; i++ ) {
		int iWritten = snprintf(
			pState->Request + pState->RequestSize,
			sizeof(pState->Request) -
				pState->RequestSize,
			"GET /oom/%u HTTP/1.1\r\n"
			"Host: oom.test\r\n"
			"Connection: %s\r\n"
			"\r\n",
			(unsigned)i,
			i + 1 == TEST_HTTP_SERVER_OOM_CASES ?
				"close" : "keep-alive"
		);

		testRequire(
			(iWritten > 0) &&
			((size_t)iWritten <
			 (sizeof(pState->Request) -
			  pState->RequestSize)),
			"HTTP server OOM request fixture overflow"
		);
		pState->RequestSize += (size_t)iWritten;
	}
}



/* 每个请求扫描一个信息响应内部失败偏移并验证最终响应恢复。 */
static void testHttpServerOomRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_http_server_oom* pState =
		(test_http_server_oom*)pData;
	xhttpreply* pFirst = xrtHttpReplyCreate(103);
	xhttpreply* pSecond = xrtHttpReplyCreate(102);
	xhttp1serverresponse* pPrepared;
	ptr Held[4096];
	size_t iHeld = 0;
	uint32 iRequest = xrtAtomic32FetchAdd(
		&pState->Requests,
		1,
		XMEMORY_ACQ_REL
	) + 1;
	xnetresult Result;

	(void)pServer;
	testRequire(
		(pRequest != NULL) &&
		(pFirst != NULL) &&
		(pSecond != NULL) &&
		xrtHttpReplyAddHeader(
			pFirst,
			XRT_STR_LITERAL("Link"),
			XRT_STR_LITERAL("</warm.css>; rel=preload")
		) &&
		xrtHttpReplyAddHeader(
			pSecond,
			XRT_STR_LITERAL("X-OOM"),
			iRequest == 1 ?
				(xstrview){
					pState->LargeHeader,
					sizeof(pState->LargeHeader)
				} :
				XRT_STR_LITERAL("information")
		),
		"HTTP server OOM information fixture failed"
	);
	testRequire(
		xrtHttpConnInform(
			pConnection,
			pFirst
		) == XNET_RESULT_OK,
		"HTTP server OOM first information failed"
	);
	if ( pState->Inject && (iRequest == 1) ) {
		xrtAtomic64Store(
			&pState->Allocator->FailAt,
			xrtAtomic64Load(
				&pState->Allocator->Calls,
				XMEMORY_ACQUIRE
			) + 1,
			XMEMORY_RELEASE
		);
	} else if ( pState->Inject && (iRequest == 2) ) {
		pPrepared = xrtHttp1ServerResponseInform(
			XHTTP_VERSION_1_1,
			pSecond
		);
		testRequire(
			pPrepared != NULL,
			"HTTP server OOM queue class warm-up failed"
		);
		xrtHttp1ServerResponseDestroy(pPrepared);
		iHeld = testHttpServerOomExhaust(
			pState->Allocator,
			sizeof(__xrt_http_response_queue),
			Held,
			sizeof(Held) / sizeof(Held[0])
		);
		xrtAtomic64Store(
			&pState->Allocator->FailAt,
			xrtAtomic64Load(
				&pState->Allocator->Calls,
				XMEMORY_ACQUIRE
			) + 1,
			XMEMORY_RELEASE
		);
	}
	Result = xrtHttpConnInform(pConnection, pSecond);
	xrtAtomic64Store(
		&pState->Allocator->FailAt,
		0,
		XMEMORY_RELEASE
	);
	if ( Result == XNET_RESULT_OK ) {
		(void)xrtAtomic32FetchAdd(
			&pState->Successes,
			1,
			XMEMORY_RELEASE
		);
	} else {
		const xerror* pError = xrtGetError();

		testRequire(
			pState->Inject &&
			(pError != NULL) &&
			(xrtErrorKind(pError) == XERR_MEMORY) &&
			(strcmp(
				xrtErrorDomain(pError),
				"xrt.http.server"
			 ) == 0),
			"HTTP server information OOM error mismatch"
		);
		if ( strcmp(
			xrtErrorOperation(pError),
			"queue-http-information"
		) == 0 ) {
			(void)xrtAtomic32FetchAdd(
				&pState->QueueFailures,
				1,
				XMEMORY_RELEASE
			);
		}
		(void)xrtAtomic32FetchAdd(
			&pState->Failures,
			1,
			XMEMORY_RELEASE
		);
		if ( strcmp(
			xrtErrorOperation(pError),
			"inform-http-connection"
		) == 0 ) {
			(void)xrtAtomic32FetchAdd(
				&pState->PrepareFailures,
				1,
				XMEMORY_RELEASE
			);
		}
		xrtClearError();
	}
	testHttpServerOomReleaseHeld(Held, iHeld);
	testRequire(
		xrtHttpConnReply(
			pConnection,
			200,
			XRT_STR_LITERAL("text/plain"),
			XRT_BYTES_LITERAL("recovered")
		) == XNET_RESULT_OK,
		"HTTP server did not recover after information OOM"
	);
	xrtHttpReplyDestroy(pSecond);
	xrtHttpReplyDestroy(pFirst);
}



/* 运行时 OOM 不应提升为稳定连接错误事件。 */
static void testHttpServerOomError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_oom* pState =
		(test_http_server_oom*)pData;

	(void)pServer;
	(void)pConnection;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pState->Errors,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 HTTP 连接唯一关闭事件。 */
static void testHttpServerOomConnectionClose(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_oom* pState =
		(test_http_server_oom*)pData;

	(void)pServer;
	(void)pConnection;
	testRequire(
		(Result == XNET_RESULT_OK) &&
		(pError == NULL),
		"HTTP server OOM connection close mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->ConnectionClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 Server 排空终态。 */
static void testHttpServerOomShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_http_server_oom* pState =
		(test_http_server_oom*)pData;

	testRequire(
		xrtHttpServerState(pServer) ==
			XHTTP_SERVER_CLOSED,
		"HTTP server OOM shutdown state mismatch"
	);
	xrtAtomic32Store(
		&pState->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



/* Client 打开后一次提交全部流水线请求。 */
static void testHttpServerOomClientOpen(
	xnetstream* pStream,
	ptr pData
)
{
	test_http_server_oom* pState =
		(test_http_server_oom*)pData;

	testRequire(
		xrtNetStreamSend(
			pStream,
			pState->Request,
			pState->RequestSize
		) == XNET_RESULT_OK,
		"HTTP server OOM client request failed"
	);
}



/* Client 及时消费全部响应，避免测试本身制造背压。 */
static void testHttpServerOomClientRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	(void)pStream;
	(void)pData;
	(void)xrtNetBufConsume(
		pBuffer,
		xrtNetBufSize(pBuffer)
	);
}



/* HTTP 对端关闭写方向后结束 Client。 */
static void testHttpServerOomClientEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	(void)xrtNetStreamClose(pStream);
}



/* 记录 Client 唯一关闭终态。 */
static void testHttpServerOomClientClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_oom* pState =
		(test_http_server_oom*)pData;

	(void)pStream;
	testRequire(
		(Result == XNET_RESULT_OK) &&
		(pError == NULL),
		"HTTP server OOM client close mismatch"
	);
	xrtAtomic32Store(
		&pState->ClientClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 完成一次正常或注入失败的真实 TCP Server 生命周期。 */
static void testHttpServerOomAttempt(
	test_http_server_oom_allocator* pAllocator,
	bool bInject
)
{
	test_http_server_oom State;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents ServerEvents;
	xnetstreamconfig StreamConfig;
	xnetstreamevents StreamEvents;
	xhttpserverstats Stats;
	xnetaddr Address;

	memset(&State, 0, sizeof(State));
	State.Allocator = pAllocator;
	State.Inject = bInject;
	xrtAtomic32Init(&State.Requests, 0);
	xrtAtomic32Init(&State.Failures, 0);
	xrtAtomic32Init(&State.PrepareFailures, 0);
	xrtAtomic32Init(&State.QueueFailures, 0);
	xrtAtomic32Init(&State.Successes, 0);
	xrtAtomic32Init(&State.ConnectionClosed, 0);
	xrtAtomic32Init(&State.ClientClosed, 0);
	xrtAtomic32Init(&State.Shutdown, 0);
	xrtAtomic32Init(&State.Errors, 0);
	memset(
		State.LargeHeader,
		'x',
		sizeof(State.LargeHeader)
	);
	testHttpServerOomRequests(&State);

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"HTTP server OOM engine start failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP server OOM loopback address failed"
	);
	ServerConfig.Network.Listen.AcceptConcurrency = 1;
	ServerConfig.HeaderTimeout = UINT64_C(10000000);
	ServerConfig.RequestTimeout = UINT64_C(10000000);
	ServerConfig.IdleTimeout = UINT64_C(10000000);
	xrtHttpServerEventsInit(&ServerEvents);
	ServerEvents.Request = testHttpServerOomRequest;
	ServerEvents.Close = testHttpServerOomConnectionClose;
	ServerEvents.Error = testHttpServerOomError;
	ServerEvents.Shutdown = testHttpServerOomShutdown;
	ServerEvents.Data = &State;
	if ( bInject ) {
		ptr Held[4096];
		size_t iHeld = testHttpServerOomExhaust(
			pAllocator,
			sizeof(xhttpserver) + sizeof(xnetaddr),
			Held,
			sizeof(Held) / sizeof(Held[0])
		);

		xrtAtomic64Store(
			&pAllocator->FailAt,
			xrtAtomic64Load(
				&pAllocator->Calls,
				XMEMORY_ACQUIRE
			) + 1,
			XMEMORY_RELEASE
		);
		testRequire(
			xrtHttpServerStart(
				State.Engine,
				&ServerConfig,
				&ServerEvents
			) == NULL,
			"HTTP server start survived forced OOM"
		);
		xrtAtomic64Store(
			&pAllocator->FailAt,
			0,
			XMEMORY_RELEASE
		);
		testHttpServerOomReleaseHeld(Held, iHeld);
		testRequire(
			xrtErrorKind(xrtGetError()) == XERR_MEMORY,
			"HTTP server start OOM error mismatch"
		);
		xrtClearError();
	}
	State.Server = xrtHttpServerStart(
		State.Engine,
		&ServerConfig,
		&ServerEvents
	);
	testRequire(
		(State.Server != NULL) &&
		xrtHttpServerLocal(State.Server, 0, &Address),
		"HTTP server OOM recovery start failed"
	);

	memset(&StreamEvents, 0, sizeof(StreamEvents));
	StreamEvents.Open = testHttpServerOomClientOpen;
	StreamEvents.Read = testHttpServerOomClientRead;
	StreamEvents.End = testHttpServerOomClientEnd;
	StreamEvents.Close = testHttpServerOomClientClose;
	xrtNetStreamConfigInit(&StreamConfig);
	State.Client = xrtNetStreamConnect(
		State.Engine,
		&Address,
		0,
		&StreamConfig,
		&StreamEvents,
		&State
	);
	testRequire(
		State.Client != NULL,
		"HTTP server OOM client connect failed"
	);
	testHttpServerOomWait(
		&State.Requests,
		TEST_HTTP_SERVER_OOM_CASES,
		"HTTP server OOM requests did not complete"
	);
	testHttpServerOomWait(
		&State.ConnectionClosed,
		1,
		"HTTP server OOM connection did not close"
	);
	testHttpServerOomWait(
		&State.ClientClosed,
		1,
		"HTTP server OOM client did not close"
	);
	testRequire(
		xrtHttpServerStats(State.Server, &Stats) &&
		(Stats.Requests ==
		 TEST_HTTP_SERVER_OOM_CASES) &&
		(Stats.Responses ==
		 TEST_HTTP_SERVER_OOM_CASES) &&
		(Stats.Informations ==
		 TEST_HTTP_SERVER_OOM_CASES +
		 xrtAtomic32Load(
			&State.Successes,
			XMEMORY_ACQUIRE
		 )) &&
		(Stats.Connections == 0) &&
		(xrtAtomic32Load(
			&State.Errors,
			XMEMORY_ACQUIRE
		 ) == 0),
		"HTTP server OOM terminal stats mismatch"
	);
	if ( bInject ) {
		testRequire(
			(xrtAtomic32Load(
				&State.Failures,
				XMEMORY_ACQUIRE
			 ) != 0) &&
			(xrtAtomic32Load(
				&State.Successes,
				XMEMORY_ACQUIRE
			 ) != 0) &&
			(xrtAtomic32Load(
				&State.PrepareFailures,
				XMEMORY_ACQUIRE
			 ) == 1) &&
			(xrtAtomic32Load(
				&State.QueueFailures,
				XMEMORY_ACQUIRE
			 ) == 1),
			"HTTP server OOM sweep missed a runtime boundary"
		);
	}

	testRequire(
		xrtHttpServerDrain(State.Server),
		"HTTP server OOM drain failed"
	);
	testHttpServerOomWait(
		&State.Shutdown,
		1,
		"HTTP server OOM shutdown did not finish"
	);
	xrtNetStreamDestroy(State.Client);
	xrtHttpServerDestroy(State.Server);
	testRequire(
		xrtNetEngineDestroy(State.Engine),
		"HTTP server OOM engine destroy failed"
	);
	xrtClearError();
}



/* 有限热身后要求连续故障运行停留在同一底层容量高水位。 */
int main(void)
{
	static test_http_server_oom_allocator State;
	xallocator Allocator;
	uint64 iBaseline;
	uint64 iLive;
	uint32 iStable = 0;
	uint32 i;

	xrtAtomic64Init(&State.Calls, 0);
	xrtAtomic64Init(&State.FailAt, 0);
	xrtAtomic64Init(&State.Live, 0);
	Allocator.Context = &State;
	Allocator.Alloc = testHttpServerOomAlloc;
	Allocator.Realloc = testHttpServerOomRealloc;
	Allocator.Free = testHttpServerOomFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"HTTP server OOM allocator install failed"
	);

	testHttpServerOomAttempt(&State, false);
	testMemoryDebugDrain(
		"HTTP server OOM warm-up debug reset failed"
	);
	iBaseline = xrtAtomic64Load(
		&State.Live,
		XMEMORY_ACQUIRE
	);

	/*
		池化堆会保留新尺寸类 span，底层块数可能在首轮故障扫描后增长。
		真正泄漏会持续增长，连续两轮稳定则表明容量已经收敛。
	*/
	iLive = iBaseline;
	for ( i = 0; i < TEST_HTTP_SERVER_OOM_STABILIZE_LIMIT; i++ ) {
		testHttpServerOomAttempt(&State, true);
		testMemoryDebugDrain(
			"HTTP server OOM injected debug reset failed"
		);
		iLive = xrtAtomic64Load(
			&State.Live,
			XMEMORY_ACQUIRE
		);
		if ( iLive == iBaseline ) {
			iStable++;
			if ( iStable == TEST_HTTP_SERVER_OOM_STABLE_ROUNDS ) {
				break;
			}
		} else {
			iBaseline = iLive;
			iStable = 0;
		}
	}
	if ( iStable != TEST_HTTP_SERVER_OOM_STABLE_ROUNDS ) {
		fprintf(
			stderr,
			"[DIAG] HTTP server OOM live baseline=%llu actual=%llu\n",
			(unsigned long long)iBaseline,
			(unsigned long long)iLive
		);
	}
	testRequire(
		iStable == TEST_HTTP_SERVER_OOM_STABLE_ROUNDS,
		"HTTP server runtime OOM leaked storage"
	);
	printf("[PASS] HTTP server runtime OOM\n");
	return 0;
}

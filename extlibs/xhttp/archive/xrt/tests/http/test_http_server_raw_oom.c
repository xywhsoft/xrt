#include "../test.h"



#ifndef TEST_HTTP_SERVER_RAW_OOM_PLAN
	#define TEST_HTTP_SERVER_RAW_OOM_PLAN 0
#endif

#ifndef TEST_HTTP_SERVER_RAW_OOM_NAME
	#define TEST_HTTP_SERVER_RAW_OOM_NAME "copy"
#endif

#ifndef TEST_HTTP_SERVER_RAW_OOM_REFS
	#define TEST_HTTP_SERVER_RAW_OOM_REFS 0
#endif

#define TEST_HTTP_SERVER_RAW_OOM_CLASSES ((size_t)64)
#define TEST_HTTP_SERVER_RAW_OOM_BLOCKS ((size_t)16384)



/* 故障门开启后拒绝所有新的底层 Heap span。 */
typedef struct test_http_server_raw_oom_allocator {
	xatomic32 Gate;
	xatomic64 Calls;
	xatomic64 Denied;
} test_http_server_raw_oom_allocator;



/* 保存 OOM 尺寸类占用、请求和 Server 终态。 */
typedef struct test_http_server_raw_oom {
	test_http_server_raw_oom_allocator* Allocator;
	ptr* Held;
	size_t HeldCount;
	size_t HeldCapacity;
	xatomic32 Requests;
	xatomic32 Errors;
	xatomic32 Closed;
	xatomic32 Shutdown;
	xatomic32 Releases;
} test_http_server_raw_oom;



/* 故障门关闭时使用系统分配，开启时拒绝新 span。 */
static ptr testHttpServerRawOomAlloc(
	ptr pData,
	size_t iSize
)
{
	test_http_server_raw_oom_allocator* pAllocator =
		(test_http_server_raw_oom_allocator*)pData;

	(void)xrtAtomic64FetchAdd(
		&pAllocator->Calls,
		1,
		XMEMORY_RELAXED
	);
	if ( xrtAtomic32Load(
		&pAllocator->Gate,
		XMEMORY_ACQUIRE
	) != 0 ) {
		(void)xrtAtomic64FetchAdd(
			&pAllocator->Denied,
			1,
			XMEMORY_RELAXED
		);
		return NULL;
	}
	return malloc(iSize);
}



/* 重分配服从同一个底层故障门。 */
static ptr testHttpServerRawOomRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	test_http_server_raw_oom_allocator* pAllocator =
		(test_http_server_raw_oom_allocator*)pData;

	(void)xrtAtomic64FetchAdd(
		&pAllocator->Calls,
		1,
		XMEMORY_RELAXED
	);
	if ( xrtAtomic32Load(
		&pAllocator->Gate,
		XMEMORY_ACQUIRE
	) != 0 ) {
		(void)xrtAtomic64FetchAdd(
			&pAllocator->Denied,
			1,
			XMEMORY_RELAXED
		);
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 归还测试分配器成功取得的系统块。 */
static void testHttpServerRawOomFree(
	ptr pData,
	ptr pMemory
)
{
	(void)pData;
	free(pMemory);
}



/* 占满所有池化尺寸类，再保持底层故障门开启。 */
static void testHttpServerRawOomExhaust(
	test_http_server_raw_oom* pState
)
{
	xrtAtomic32Store(
		&pState->Allocator->Gate,
		1,
		XMEMORY_RELEASE
	);
	for ( size_t i = 1;
		i <= TEST_HTTP_SERVER_RAW_OOM_CLASSES;
		i++ ) {
		for ( ;; ) {
			ptr pMemory = xrtMalloc(i * 16u);

			if ( pMemory == NULL ) {
				xrtClearError();
				break;
			}
			testRequire(
				pState->HeldCount <
					pState->HeldCapacity,
				"HTTP server raw OOM exhaustion overflowed"
			);
			pState->Held[pState->HeldCount++] =
				pMemory;
		}
	}
}



/* 关闭故障门并归还尺寸类占用。 */
static void testHttpServerRawOomRestore(
	test_http_server_raw_oom* pState
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



/* 记录引用是否被失败的原始响应提交错误接管。 */
static void testHttpServerRawOomRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	test_http_server_raw_oom* pState =
		(test_http_server_raw_oom*)pContext;

	(void)pData;
	(void)iSize;
	(void)xrtAtomic32FetchAdd(
		&pState->Releases,
		1,
		XMEMORY_RELEASE
	);
}



/* 在复制或计划分配 OOM 后验证同一请求仍能提交普通响应。 */
static void testHttpServerRawOomRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	static const uint8 Wire[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 3\r\n"
		"Connection: close\r\n"
		"\r\n"
		"raw";
	test_http_server_raw_oom* pState =
		(test_http_server_raw_oom*)pData;
	xhttpbody* pBody = NULL;
	xnetref Ref;
	xnetresult Result;

	(void)pServer;
	testRequire(
		pRequest != NULL,
		"HTTP server raw OOM request is null"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Requests,
		1,
		XMEMORY_RELEASE
	);
	if ( TEST_HTTP_SERVER_RAW_OOM_PLAN ) {
		pBody = xrtHttpBodyBorrow(
			(xbytesview){ Wire, sizeof(Wire) - 1u }
		);
		testRequire(
			pBody != NULL,
			"HTTP server raw OOM Body fixture failed"
		);
	}
	Ref.Data = Wire;
	Ref.Size = sizeof(Wire) - 1u;
	Ref.Release = testHttpServerRawOomRelease;
	Ref.Context = pState;
	testHttpServerRawOomExhaust(pState);
	if ( TEST_HTTP_SERVER_RAW_OOM_REFS ) {
		Result = xrtHttpConnRespondRawRef(
			pConnection,
			&Ref,
			XHTTP_SERVER_RAW_NONE
		);
	} else if ( TEST_HTTP_SERVER_RAW_OOM_PLAN ) {
		Result = xrtHttpConnRespondRawBody(
			pConnection,
			pBody,
			XHTTP_SERVER_RAW_NONE
		);
	} else {
		Result = xrtHttpConnRespondRaw(
			pConnection,
			(xbytesview){ Wire, sizeof(Wire) - 1u },
			XHTTP_SERVER_RAW_NONE
		);
	}
	/*
		底层分配器完全关闭时，错误核心必须退回无分配的静态
		xrt.memory 错误；HTTP 域包装本身不能成为 OOM 前提。
	*/
	testRequire(
		(Result == XNET_RESULT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtAtomic32Load(
			&pState->Releases,
			XMEMORY_ACQUIRE
		 ) == 0),
		"HTTP server raw OOM error mismatch"
	);
	testHttpServerRawOomRestore(pState);
	testRequire(
		xrtHttpConnReply(
			pConnection,
			503,
			XRT_STR_LITERAL("text/plain"),
			XRT_BYTES_LITERAL("recovered")
		) == XNET_RESULT_OK,
		"HTTP server raw OOM final gate did not roll back"
	);
	xrtHttpBodyDestroy(pBody);
}



/* 原始响应构造 OOM 不应污染稳定连接终态。 */
static void testHttpServerRawOomError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_raw_oom* pState =
		(test_http_server_raw_oom*)pData;

	(void)pServer;
	(void)pConnection;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pState->Errors,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录恢复响应排空后的连接关闭。 */
static void testHttpServerRawOomClose(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_raw_oom* pState =
		(test_http_server_raw_oom*)pData;

	(void)pServer;
	(void)pConnection;
	testRequire(
		(Result == XNET_RESULT_OK) &&
		(pError == NULL),
		"HTTP server raw OOM close mismatch"
	);
	xrtAtomic32Store(
		&pState->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录恢复测试 Server 的排空终态。 */
static void testHttpServerRawOomShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_http_server_raw_oom* pState =
		(test_http_server_raw_oom*)pData;

	testRequire(
		xrtHttpServerState(pServer) ==
			XHTTP_SERVER_CLOSED,
		"HTTP server raw OOM shutdown mismatch"
	);
	xrtAtomic32Store(
		&pState->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



/* 在截止时间前等待一个原子终态。 */
static void testHttpServerRawOomWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline =
		xrtDeadlineAfter(UINT64_C(10000000));

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



/* 完整发送一条关闭连接的测试请求。 */
static void testHttpServerRawOomSend(
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
			"HTTP server raw OOM request send failed"
		);
		iOffset += iSent;
	}
}



/* 验证原始响应 OOM 回滚、普通响应恢复和资源终态。 */
int main(void)
{
	static const uint8 Request[] =
		"GET /oom HTTP/1.1\r\n"
		"Host: raw.test\r\n"
		"Connection: close\r\n"
		"\r\n";
	test_http_server_raw_oom_allocator AllocatorState;
	test_http_server_raw_oom State;
	xallocator Allocator;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents Events;
	xhttpserverstats Stats;
	xnetengine* pEngine;
	xhttpserver* pServer;
	xnetaddr Address;
	xnetsocket Client;
	char Response[1024];
	size_t iResponse = 0;

	memset(&AllocatorState, 0, sizeof(AllocatorState));
	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&AllocatorState.Gate, 0);
	xrtAtomic64Init(&AllocatorState.Calls, 0);
	xrtAtomic64Init(&AllocatorState.Denied, 0);
	xrtAtomic32Init(&State.Requests, 0);
	xrtAtomic32Init(&State.Errors, 0);
	xrtAtomic32Init(&State.Closed, 0);
	xrtAtomic32Init(&State.Shutdown, 0);
	xrtAtomic32Init(&State.Releases, 0);
	State.Allocator = &AllocatorState;
	State.HeldCapacity = TEST_HTTP_SERVER_RAW_OOM_BLOCKS;
	State.Held = (ptr*)malloc(
		State.HeldCapacity * sizeof(ptr)
	);
	testRequire(
		State.Held != NULL,
		"HTTP server raw OOM hold table failed"
	);
	Allocator.Context = &AllocatorState;
	Allocator.Alloc = testHttpServerRawOomAlloc;
	Allocator.Realloc = testHttpServerRawOomRealloc;
	Allocator.Free = testHttpServerRawOomFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"HTTP server raw OOM allocator install failed"
	);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) &&
		xrtNetEngineStart(pEngine),
		"HTTP server raw OOM engine start failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP server raw OOM address setup failed"
	);
	xrtHttpServerEventsInit(&Events);
	Events.Request = testHttpServerRawOomRequest;
	Events.Error = testHttpServerRawOomError;
	Events.Close = testHttpServerRawOomClose;
	Events.Shutdown = testHttpServerRawOomShutdown;
	Events.Data = &State;
	pServer = xrtHttpServerStart(
		pEngine,
		&ServerConfig,
		&Events
	);
	testRequire(
		(pServer != NULL) &&
		xrtHttpServerLocal(pServer, 0, &Address),
		"HTTP server raw OOM start failed"
	);

	Client = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		0
	);
	testRequire(
		(Client != NULL) &&
		(xrtNetSocketConnect(
			Client,
			&Address
		 ) == XNET_RESULT_OK),
		"HTTP server raw OOM connect failed"
	);
	testHttpServerRawOomSend(
		Client,
		Request,
		sizeof(Request) - 1u
	);
	while ( iResponse < (sizeof(Response) - 1u) ) {
		size_t iRead = 0;
		xnetresult Result = xrtNetSocketRecv(
			Client,
			Response + iResponse,
			sizeof(Response) - iResponse - 1u,
			&iRead
		);

		if ( Result == XNET_RESULT_CLOSED ) {
			break;
		}
		testRequire(
			(Result == XNET_RESULT_OK) &&
			(iRead != 0),
			"HTTP server raw OOM response receive failed"
		);
		iResponse += iRead;
	}
	Response[iResponse] = '\0';
	testRequire(
		(strstr(
			Response,
			"HTTP/1.1 503 Service Unavailable\r\n"
		 ) != NULL) &&
		(strstr(Response, "\r\n\r\nrecovered") != NULL),
		"HTTP server raw OOM recovery response mismatch"
	);
	testRequire(
		xrtNetSocketClose(Client),
		"HTTP server raw OOM client close failed"
	);
	testHttpServerRawOomWait(
		&State.Closed,
		1,
		"HTTP server raw OOM connection did not close"
	);
	testRequire(
		xrtHttpServerDrain(pServer),
		"HTTP server raw OOM drain failed"
	);
	testHttpServerRawOomWait(
		&State.Shutdown,
		1,
		"HTTP server raw OOM shutdown missing"
	);
	testRequire(
		xrtHttpServerStats(pServer, &Stats) &&
		(Stats.Accepted == 1) &&
		(Stats.Requests == 1) &&
		(Stats.Responses == 1) &&
		(Stats.Connections == 0) &&
		(xrtAtomic32Load(
			&State.Errors,
			XMEMORY_ACQUIRE
		 ) == 0) &&
		(xrtAtomic32Load(
			&State.Releases,
			XMEMORY_ACQUIRE
		 ) == 0) &&
		(xrtAtomic64Load(
			&AllocatorState.Denied,
			XMEMORY_ACQUIRE
		 ) > TEST_HTTP_SERVER_RAW_OOM_CLASSES),
		"HTTP server raw OOM terminal contract mismatch"
	);
	xrtHttpServerDestroy(pServer);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"HTTP server raw OOM engine destroy failed"
	);
	free(State.Held);
	printf(
		"[PASS] HTTP server raw OOM rollback (%s)\n",
		TEST_HTTP_SERVER_RAW_OOM_NAME
	);
	return 0;
}

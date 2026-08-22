#include "../test.h"



/* 运行时夹具用原子字段发布跨 Worker 生命周期状态。 */
typedef struct test_http_sse_server_runtime {
	xatomicptr Stream;
	xatomic32 Requested;
	xatomic32 Completed;
	xatomic32 Errors;
	xatomic32 Closed;
	xatomic32 Shutdown;
	xhttpresponse* Response;
	xnetresult Result;
	size_t Streamed;
} test_http_sse_server_runtime;



/* 在本地测试截止时间前等待指定状态。 */
static void testHttpSseServerRuntimeWait(
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



/* 等待 Close 回调返回并释放最后一个 Engine 活动对象。 */
static void testHttpSseServerRuntimeEngineDestroy(xnetengine* pEngine)
{
	xdeadline Deadline = xrtDeadlineAfter(
		UINT64_C(10000000)
	);

	while ( !xrtNetEngineDestroy(pEngine) ) {
		testRequire(
			(xrtGetError() != NULL) &&
			(xrtErrorCode(xrtGetError()) ==
			 XNET_ERROR_ENGINE_STOP) &&
			!xrtDeadlineExpired(Deadline),
			"SSE server runtime retained an Engine object"
		);
		xrtClearError();
		xrtThreadYield();
	}
}



/* 提交 SSE Reply，并把独立生产端发布给测试主线程。 */
static void testHttpSseServerRuntimeRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_http_sse_server_runtime* pState =
		(test_http_sse_server_runtime*)pData;
	xhttpbodystreamconfig Config = { 128, 8 };
	xhttpbodystream* pStream = NULL;
	xhttpreply* pReply = xrtHttpSseReplyCreate(
		&Config, &pStream
	);

	(void)pServer;
	testRequire(
		(xrtHttpServerRequestTarget(pRequest).Size == 7u) &&
		(memcmp(
			xrtHttpServerRequestTarget(pRequest).Data,
			"/events",
			7u
		 ) == 0),
		"SSE server runtime request target mismatch"
	);
	testRequire(
		(pReply != NULL) && (pStream != NULL) &&
		xrtHttpReplySetHeader(
			pReply,
			XRT_STR_LITERAL("Cache-Control"),
			XRT_STR_LITERAL("no-cache")
		) &&
		(xrtHttpConnRespond(
			pConnection,
			pReply
		 ) == XNET_RESULT_OK),
		"SSE server runtime response submission failed"
	);
	xrtHttpReplyDestroy(pReply);
	xrtAtomicPtrStore(
		&pState->Stream,
		pStream,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pState->Requested,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证客户端收到的已解帧正文保持事件提交顺序。 */
static bool testHttpSseServerRuntimeBody(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	static const char Expected[] =
		"data: first\n\n"
		"id: 2\n"
		"event: done\n"
		"data: second\n"
		"\n"
		": heartbeat\n";
	test_http_sse_server_runtime* pState =
		(test_http_sse_server_runtime*)pData;
	const xhttpfield* pType = xrtHttpResponseHeader(
		pResponse,
		XRT_STR_LITERAL("Content-Type")
	);

	(void)pCall;
	testRequire(
		(xrtHttpResponseStatus(pResponse) == 200) &&
		(pType != NULL) &&
		xrtHttpSseContentTypeValid(pType->Value),
		"SSE server runtime response headers mismatch"
	);
	testRequire(
		(pState->Streamed <= (sizeof(Expected) - 1u)) &&
		(Data.Size <=
		 ((sizeof(Expected) - 1u) - pState->Streamed)) &&
		(memcmp(
			Data.Data,
			Expected + pState->Streamed,
			Data.Size
		 ) == 0),
		"SSE server runtime body order mismatch"
	);
	pState->Streamed += Data.Size;
	return true;
}



/* 保存客户端唯一完成结果，并把 Response 所有权移交给主线程。 */
static void testHttpSseServerRuntimeDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	test_http_sse_server_runtime* pState =
		(test_http_sse_server_runtime*)pData;

	testRequire(
		(pCall != NULL) && (pResult != NULL) &&
		xrtNetWorkerIsCurrent(xrtHttpCallWorker(pCall)),
		"SSE server runtime completion Worker mismatch"
	);
	pState->Result = pResult->Result;
	pState->Response = pResult->Response;
	xrtAtomic32Store(
		&pState->Completed,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 HTTP Server 提升的运行时错误。 */
static void testHttpSseServerRuntimeError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_http_sse_server_runtime* pState =
		(test_http_sse_server_runtime*)pData;

	(void)pServer;
	(void)pConnection;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pState->Errors,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录本地 HTTP 连接唯一关闭。 */
static void testHttpSseServerRuntimeClose(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_sse_server_runtime* pState =
		(test_http_sse_server_runtime*)pData;

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



/* 记录 Server 排空后的唯一关闭通知。 */
static void testHttpSseServerRuntimeShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_http_sse_server_runtime* pState =
		(test_http_sse_server_runtime*)pData;

	testRequire(
		xrtHttpServerState(pServer) == XHTTP_SERVER_CLOSED,
		"SSE server runtime shutdown state mismatch"
	);
	xrtAtomic32Store(
		&pState->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证响应提交后生产、短写分帧、异步唤醒和正常 EOF。 */
int main(void)
{
	static const size_t ExpectedSize =
		sizeof("data: first\n\n") - 1u +
		sizeof("id: 2\nevent: done\ndata: second\n\n") - 1u +
		sizeof(": heartbeat\n") - 1u;
	test_http_sse_server_runtime State;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents ServerEvents;
	xhttpclientconfig ClientConfig;
	xhttpcalloptions Options;
	xnetengine* pEngine;
	xhttpserver* pServer;
	xhttpclient* pClient;
	xhttpcall* pCall;
	xhttpbodystream* pStream;
	xhttpserverstats Stats;
	xhttpsseevent Event;
	xnetaddr Address;
	char Endpoint[96];
	char Url[128];
	size_t iEndpoint;
	int iUrl;

	memset(&State, 0, sizeof(State));
	xrtAtomicPtrInit(&State.Stream, NULL);
	xrtAtomic32Init(&State.Requested, 0);
	xrtAtomic32Init(&State.Completed, 0);
	xrtAtomic32Init(&State.Errors, 0);
	xrtAtomic32Init(&State.Closed, 0);
	xrtAtomic32Init(&State.Shutdown, 0);

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) && xrtNetEngineStart(pEngine),
		"SSE server runtime engine start failed"
	);

	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"SSE server runtime address setup failed"
	);
	ServerConfig.WriteSize = 3;
	ServerConfig.WriteTimeout = UINT64_C(2000000);
	ServerConfig.RequestTimeout = UINT64_C(3000000);
	xrtHttpServerEventsInit(&ServerEvents);
	ServerEvents.Request = testHttpSseServerRuntimeRequest;
	ServerEvents.Error = testHttpSseServerRuntimeError;
	ServerEvents.Close = testHttpSseServerRuntimeClose;
	ServerEvents.Shutdown = testHttpSseServerRuntimeShutdown;
	ServerEvents.Data = &State;
	pServer = xrtHttpServerStart(
		pEngine,
		&ServerConfig,
		&ServerEvents
	);
	testRequire(
		(pServer != NULL) &&
		xrtHttpServerLocal(pServer, 0, &Address),
		"SSE server runtime start failed"
	);

	iEndpoint = xrtNetAddrEndpointText(
		&Address,
		Endpoint,
		sizeof(Endpoint)
	);
	testRequire(
		(iEndpoint != 0) && (iEndpoint < sizeof(Endpoint)),
		"SSE server runtime endpoint format failed"
	);
	iUrl = snprintf(
		Url,
		sizeof(Url),
		"http://%s/events",
		Endpoint
	);
	testRequire(
		(iUrl > 0) && ((size_t)iUrl < sizeof(Url)),
		"SSE server runtime URL format failed"
	);

	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Timeout = UINT64_C(5000000);
	ClientConfig.IdleTimeout = UINT64_C(5000000);
	pClient = xrtHttpClientCreate(pEngine, &ClientConfig);
	testRequire(
		pClient != NULL,
		"SSE server runtime client create failed"
	);
	xrtHttpCallOptionsInit(&Options);
	Options.ResponseBodyLimit = UINT64_MAX;
	Options.Events.Body = testHttpSseServerRuntimeBody;
	Options.Events.Data = &State;
	pCall = xrtHttpClientGet(
		pClient,
		(xstrview){ Url, (size_t)iUrl },
		&Options,
		testHttpSseServerRuntimeDone,
		&State
	);
	testRequire(
		pCall != NULL,
		"SSE server runtime request start failed"
	);

	testHttpSseServerRuntimeWait(
		&State.Requested,
		1,
		"SSE server runtime request missing"
	);
	pStream = (xhttpbodystream*)xrtAtomicPtrLoad(
		&State.Stream,
		XMEMORY_ACQUIRE
	);
	testRequire(
		pStream != NULL,
		"SSE server runtime producer was not published"
	);
	xrtSleep(20);
	testRequire(
		xrtAtomic32Load(
			&State.Completed,
			XMEMORY_ACQUIRE
		) == 0,
		"SSE server runtime completed before production"
	);

	memset(&Event, 0, sizeof(Event));
	Event.Id = XRT_STR_LITERAL("2");
	Event.Type = XRT_STR_LITERAL("done");
	Event.Data = XRT_STR_LITERAL("second");
	Event.Flags = XHTTP_SSE_EVENT_ID |
		XHTTP_SSE_EVENT_TYPE |
		XHTTP_SSE_EVENT_DATA;
	testRequire(
		(xrtHttpSseSend(
			pStream,
			XRT_STR_LITERAL("first")
		 ) == XHTTP_BODY_STREAM_OK) &&
		(xrtHttpSseSendEvent(
			pStream,
			&Event
		 ) == XHTTP_BODY_STREAM_OK) &&
		(xrtHttpSseSendComment(
			pStream,
			XRT_STR_LITERAL("heartbeat")
		 ) == XHTTP_BODY_STREAM_OK),
		"SSE server runtime event production failed"
	);
	xrtAtomicPtrStore(
		&State.Stream,
		NULL,
		XMEMORY_RELEASE
	);
	xrtHttpBodyStreamDestroy(pStream);

	testHttpSseServerRuntimeWait(
		&State.Completed,
		1,
		"SSE server runtime client did not complete"
	);
	testRequire(
		(State.Result == XNET_RESULT_OK) &&
		(State.Response != NULL) &&
		(xrtHttpResponseStatus(State.Response) == 200) &&
		(State.Streamed == ExpectedSize) &&
		(xrtHttpResponseBody(State.Response).Size == 0) &&
		(xrtAtomic32Load(
			&State.Errors,
			XMEMORY_ACQUIRE
		 ) == 0),
		"SSE server runtime completion contract mismatch"
	);
	xrtHttpResponseDestroy(State.Response);
	xrtHttpCallDestroy(pCall);
	xrtHttpClientDestroy(pClient);

	testRequire(
		xrtHttpServerStats(pServer, &Stats) &&
		(Stats.Requests == 1) &&
		(Stats.Responses == 1),
		"SSE server runtime statistics mismatch"
	);
	testRequire(
		xrtHttpServerDrain(pServer),
		"SSE server runtime drain failed"
	);
	testHttpSseServerRuntimeWait(
		&State.Shutdown,
		1,
		"SSE server runtime shutdown missing"
	);
	testHttpSseServerRuntimeWait(
		&State.Closed,
		1,
		"SSE server runtime connection did not close"
	);
	xrtHttpServerDestroy(pServer);
	testHttpSseServerRuntimeEngineDestroy(pEngine);
	printf("[PASS] HTTP SSE server runtime (select)\n");
	return 0;
}

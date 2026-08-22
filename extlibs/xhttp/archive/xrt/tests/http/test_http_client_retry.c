#include "../test.h"



#define TEST_HTTP_RETRY_STREAMS 8u



/* 每个场景使用独立 Engine，隔离连接关闭和 Timer 时序。 */
typedef enum test_http_retry_scenario {
	TEST_HTTP_RETRY_STATUS = 0,
	TEST_HTTP_RETRY_DISABLED,
	TEST_HTTP_RETRY_CALL_ENABLED,
	TEST_HTTP_RETRY_CALL_DISABLED,
	TEST_HTTP_RETRY_POST_BLOCKED,
	TEST_HTTP_RETRY_POST_UNSAFE,
	TEST_HTTP_RETRY_TRANSPORT,
	TEST_HTTP_RETRY_MIXED,
	TEST_HTTP_RETRY_LIMIT,
	TEST_HTTP_RETRY_CANCEL_WAIT,
	TEST_HTTP_RETRY_TOTAL_TIMEOUT
} test_http_retry_scenario;



/* 场景状态由 Listener、Stream 和客户端回调共同持有。 */
typedef struct test_http_retry {
	xnetengine* Engine;
	xnetlistener* Listener;
	xnetstream* Streams[TEST_HTTP_RETRY_STREAMS];
	xhttpclient* Client;
	xhttpcall* Call;
	xhttpresponse* Response;
	xatomic32 Accepted;
	xatomic32 Requests;
	xatomic32 Closed;
	xatomic32 Completed;
	xatomic32 ListenerClosed;
	test_http_retry_scenario Scenario;
	size_t ExpectedRequests;
	size_t ExpectedRetries;
	size_t HeaderCalls;
	size_t BodyCalls;
	size_t BodyBytes;
	uint16 Port;
	char Body[16];
} test_http_retry;



/* 在十秒边界内等待 Worker 发布指定计数。 */
static void testHttpRetryWait(
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



/* 等待调用进入重试退避阶段，避免取消操作命中上一阶段。 */
static void testHttpRetryWaitPhase(
	const xhttpcall* pCall,
	xhttpcallphase Expected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(10000000u);
	xhttpcallinfo Info;

	for ( ;; ) {
		testRequire(
			xrtHttpCallInfo(pCall, &Info),
			"HTTP retry call info failed"
		);
		if ( Info.Phase == Expected ) {
			return;
		}
		testRequire(
			(Info.State != XHTTP_CALL_SUCCEEDED) &&
			(Info.State != XHTTP_CALL_FAILED) &&
			(Info.State != XHTTP_CALL_CANCELLED) &&
			!xrtDeadlineExpired(Deadline),
			sMessage
		);
		xrtThreadYield();
	}
}



/* 测试域名始终解析到本机 IPv4 回环地址。 */
static xnetaddrlist* testHttpRetryLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)pData;
	testRequire(
		strcmp(sHost, "retry.test") == 0,
		"HTTP retry resolved an unexpected host"
	);
	if ( Family == XNET_FAMILY_IPV6 ) {
		return xrtNetAddrListCreate(NULL, 0);
	}
	testRequire(
		xrtNetAddrLoopback(
			&Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP retry resolver address failed"
	);
	return xrtNetAddrListCreate(&Address, 1);
}



/* 小响应必须原子进入有界发送队列，再由正常关闭排空。 */
static void testHttpRetryRespond(
	xnetstream* pStream,
	cstr sResponse,
	size_t iSize
)
{
	testRequire(
		xrtNetStreamSend(
			pStream,
			sResponse,
			iSize
		) == XNET_RESULT_OK,
		"HTTP retry response send failed"
	);
	testRequire(
		xrtNetStreamClose(pStream),
		"HTTP retry response close failed"
	);
}



/* 按场景和尝试序号发布临时状态、空响应断开或最终成功。 */
static void testHttpRetryRoute(
	test_http_retry* pState,
	xnetstream* pStream,
	uint32 iRequest
)
{
	static const char RetryNow[] =
		"HTTP/1.1 503 Service Unavailable\r\n"
		"Retry-After: 0\r\n"
		"Content-Length: 4\r\n"
		"Connection: close\r\n"
		"\r\n"
		"WAIT";
	static const char RetryLater[] =
		"HTTP/1.1 503 Service Unavailable\r\n"
		"Content-Length: 4\r\n"
		"Connection: close\r\n"
		"\r\n"
		"WAIT";
	static const char Success[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 2\r\n"
		"Connection: close\r\n"
		"\r\n"
		"OK";

	if ( ((pState->Scenario == TEST_HTTP_RETRY_TRANSPORT) &&
		 (iRequest == 1u)) ||
		((pState->Scenario == TEST_HTTP_RETRY_MIXED) &&
		 (iRequest == 2u)) ) {
		testRequire(
			xrtNetStreamAbort(pStream),
			"HTTP retry transport abort failed"
		);
		return;
	}
	if ( (pState->Scenario == TEST_HTTP_RETRY_LIMIT) ||
		(iRequest == 1u) ) {
		testHttpRetryRespond(
			pStream,
			(pState->Scenario == TEST_HTTP_RETRY_CANCEL_WAIT) ||
			(pState->Scenario == TEST_HTTP_RETRY_TOTAL_TIMEOUT) ?
				RetryLater : RetryNow,
			(pState->Scenario == TEST_HTTP_RETRY_CANCEL_WAIT) ||
			(pState->Scenario == TEST_HTTP_RETRY_TOTAL_TIMEOUT) ?
				sizeof(RetryLater) - 1u :
				sizeof(RetryNow) - 1u
		);
		return;
	}
	testHttpRetryRespond(
		pStream,
		Success,
		sizeof(Success) - 1u
	);
}



/* 提取一条定长测试请求，并在完整正文到达后路由。 */
static void testHttpRetryServerRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	test_http_retry* pState = (test_http_retry*)pData;
	char Request[4096];
	size_t iSize = xrtNetBufSize(pBuffer);
	size_t iHead = 0;
	size_t iWire;
	bool bPost;
	uint32 iRequest;

	testRequire(
		iSize < sizeof(Request),
		"HTTP retry request exceeded fixture capacity"
	);
	testRequire(
		xrtNetBufPeek(
			pBuffer,
			0,
			Request,
			iSize
		) == iSize,
		"HTTP retry request peek failed"
	);
	Request[iSize] = 0;
	for ( size_t i = 3u; i < iSize; i++ ) {
		if ( (Request[i - 3u] == '\r') &&
			(Request[i - 2u] == '\n') &&
			(Request[i - 1u] == '\r') &&
			(Request[i] == '\n') ) {
			iHead = i + 1u;
			break;
		}
	}
	if ( iHead == 0 ) {
		return;
	}
	bPost = (pState->Scenario == TEST_HTTP_RETRY_POST_BLOCKED) ||
		(pState->Scenario == TEST_HTTP_RETRY_POST_UNSAFE);
	iWire = iHead + (bPost ? 4u : 0u);
	if ( iSize < iWire ) {
		return;
	}
	testRequire(
		(bPost && (memcmp(Request, "POST ", 5u) == 0)) ||
		(!bPost && (memcmp(Request, "GET ", 4u) == 0)),
		"HTTP retry request method mismatch"
	);
	if ( bPost ) {
		testRequire(
			memcmp(Request + iHead, "DATA", 4u) == 0,
			"HTTP retry replayed body mismatch"
		);
	}
	testRequire(
		xrtNetBufConsume(
			pBuffer,
			iWire
		) == iWire,
		"HTTP retry request consume failed"
	);
	iRequest = xrtAtomic32FetchAdd(
		&pState->Requests,
		1,
		XMEMORY_ACQ_REL
	) + 1u;
	testHttpRetryRoute(pState, pStream, iRequest);
}



/* 对端先结束发送时同步关闭服务端测试流。 */
static void testHttpRetryServerEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	(void)xrtNetStreamClose(pStream);
}



/* 记录测试服务端流已离开关闭队列。 */
static void testHttpRetryServerClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_retry* pState = (test_http_retry*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pState->Closed,
		1,
		XMEMORY_ACQ_REL
	);
}



/* 接管每条本地连接并保留测试 Owner 引用。 */
static bool testHttpRetryAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	test_http_retry* pState = (test_http_retry*)pData;
	xnetstreamevents Events;
	uint32 iAccepted = xrtAtomic32Load(
		&pState->Accepted,
		XMEMORY_ACQUIRE
	);

	(void)pListener;
	testRequire(
		iAccepted < TEST_HTTP_RETRY_STREAMS,
		"HTTP retry accepted too many streams"
	);
	memset(&Events, 0, sizeof(Events));
	Events.Read = testHttpRetryServerRead;
	Events.End = testHttpRetryServerEnd;
	Events.Close = testHttpRetryServerClose;
	testRequire(
		xrtNetStreamSetEvents(
			pStream,
			&Events,
			pState
		),
		"HTTP retry stream event takeover failed"
	);
	pState->Streams[iAccepted] = pStream;
	(void)xrtAtomic32FetchAdd(
		&pState->Accepted,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 记录 Listener 已排空全部 Accept。 */
static void testHttpRetryListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_http_retry* pState = (test_http_retry*)pData;

	(void)pListener;
	xrtAtomic32Store(
		&pState->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 最终 Header 回调不得观察到被重试层隐藏的 503。 */
static bool testHttpRetryHeaders(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	ptr pData
)
{
	test_http_retry* pState = (test_http_retry*)pData;
	uint16 iExpected =
		(pState->Scenario == TEST_HTTP_RETRY_DISABLED) ||
		(pState->Scenario == TEST_HTTP_RETRY_CALL_DISABLED) ||
		(pState->Scenario == TEST_HTTP_RETRY_POST_BLOCKED) ||
		(pState->Scenario == TEST_HTTP_RETRY_LIMIT) ? 503u : 200u;

	(void)pCall;
	testRequire(
		xrtHttpResponseStatus(pResponse) == iExpected,
		"HTTP retry exposed an intermediate response"
	);
	pState->HeaderCalls++;
	return true;
}



/* 最终 Body 回调只收集可见响应正文。 */
static bool testHttpRetryBody(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	test_http_retry* pState = (test_http_retry*)pData;

	(void)pCall;
	(void)pResponse;
	testRequire(
		(pState->BodyBytes + Data.Size) <= sizeof(pState->Body),
		"HTTP retry visible body overflowed"
	);
	memcpy(
		pState->Body + pState->BodyBytes,
		Data.Data,
		Data.Size
	);
	pState->BodyBytes += Data.Size;
	pState->BodyCalls++;
	return true;
}



/* 核对最终响应、错误分类、可见事件和累计重试次数。 */
static void testHttpRetryDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	test_http_retry* pState = (test_http_retry*)pData;
	bool bCancelled =
		pState->Scenario == TEST_HTTP_RETRY_CANCEL_WAIT;
	bool bTimeout =
		pState->Scenario == TEST_HTTP_RETRY_TOTAL_TIMEOUT;
	uint16 iStatus =
		(pState->Scenario == TEST_HTTP_RETRY_DISABLED) ||
		(pState->Scenario == TEST_HTTP_RETRY_CALL_DISABLED) ||
		(pState->Scenario == TEST_HTTP_RETRY_POST_BLOCKED) ||
		(pState->Scenario == TEST_HTTP_RETRY_LIMIT) ? 503u : 200u;
	cstr sBody = iStatus == 503u ? "WAIT" : "OK";
	size_t iBody = iStatus == 503u ? 4u : 2u;

	if ( pResult->Info.Retries != pState->ExpectedRetries ) {
		fprintf(
			stderr,
			"[retry-count] scenario=%d result=%d "
			"error=%d retries=%zu expected=%zu\n",
			(int)pState->Scenario,
			(int)pResult->Result,
			(int)pResult->Info.Error,
			pResult->Info.Retries,
			pState->ExpectedRetries
		);
	}
	testRequire(
		(pCall != NULL) && (pResult != NULL) &&
		(pResult->Info.Retries == pState->ExpectedRetries),
		"HTTP retry result count mismatch"
	);
	if ( bCancelled || bTimeout ) {
		xhttpclienterror Error = bCancelled ?
			XHTTP_CLIENT_ERROR_CANCELLED :
			XHTTP_CLIENT_ERROR_TIMEOUT_TOTAL;
		xnetresult Result = bCancelled ?
			XNET_RESULT_CANCELLED : XNET_RESULT_TIMEOUT;

		testRequire(
			(pResult->Result == Result) &&
			(pResult->Response == NULL) &&
			(pResult->Error != NULL) &&
			(xrtErrorCode(pResult->Error) == (int64)Error) &&
			(strcmp(
				xrtErrorDomain(pResult->Error),
				"xrt.http.client"
			) == 0) &&
			(pResult->Info.Error == Error) &&
			(pResult->Info.Phase == XHTTP_CALL_PHASE_RETRY) &&
			(pState->HeaderCalls == 0) &&
			(pState->BodyCalls == 0),
			"HTTP retry wait terminal mismatch"
		);
	} else {
		testRequire(
			(pResult->Result == XNET_RESULT_OK) &&
			(pResult->Response != NULL) &&
			(pResult->Error == NULL) &&
			(xrtHttpResponseStatus(pResult->Response) == iStatus) &&
			(pState->HeaderCalls == 1u) &&
			(pState->BodyCalls != 0) &&
			(pState->BodyBytes == iBody) &&
			(memcmp(pState->Body, sBody, iBody) == 0),
			"HTTP retry visible response mismatch"
		);
		pState->Response = pResult->Response;
	}
	xrtAtomic32Store(
		&pState->Completed,
		1,
		XMEMORY_RELEASE
	);
}



/* 创建 Select Engine、本地 Listener 和启用指定重试策略的 Client。 */
static void testHttpRetryStart(
	test_http_retry* pState,
	test_http_retry_scenario Scenario
)
{
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xhttpclientconfig ClientConfig;
	xnetaddr Address;

	memset(pState, 0, sizeof(*pState));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	xrtAtomic32Init(&pState->Accepted, 0);
	xrtAtomic32Init(&pState->Requests, 0);
	xrtAtomic32Init(&pState->Closed, 0);
	xrtAtomic32Init(&pState->Completed, 0);
	xrtAtomic32Init(&pState->ListenerClosed, 0);
	pState->Scenario = Scenario;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2;
	pState->Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pState->Engine != NULL) &&
		xrtNetEngineStart(pState->Engine),
		"HTTP retry Select engine start failed"
	);
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ListenConfig.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP retry listener address failed"
	);
	ListenConfig.AcceptConcurrency = 4;
	ListenerEvents.Accept = testHttpRetryAccept;
	ListenerEvents.Close = testHttpRetryListenerClose;
	pState->Listener = xrtNetListen(
		pState->Engine,
		&ListenConfig,
		&ListenerEvents,
		NULL,
		pState
	);
	testRequire(
		(pState->Listener != NULL) &&
		xrtNetListenerLocal(
			pState->Listener,
			&Address
		),
		"HTTP retry listener creation failed"
	);
	pState->Port = Address.Port;

	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Resolver.Lookup = testHttpRetryLookup;
	ClientConfig.Dial.FallbackDelay = 1000u;
	ClientConfig.Dial.MaxAttempts = 1;
	ClientConfig.Retry.Flags =
		XHTTP_RETRY_STATUS |
		XHTTP_RETRY_TRANSPORT |
		XHTTP_RETRY_RESPECT_AFTER;
	ClientConfig.Retry.BaseDelay = 0;
	ClientConfig.Retry.MaxDelay = 300000u;
	ClientConfig.Retry.MaxRetries =
		(Scenario == TEST_HTTP_RETRY_DISABLED) ||
		(Scenario == TEST_HTTP_RETRY_CALL_ENABLED) ? 0u : 2u;
	if ( (Scenario == TEST_HTTP_RETRY_CANCEL_WAIT) ||
		(Scenario == TEST_HTTP_RETRY_TOTAL_TIMEOUT) ) {
		ClientConfig.Retry.BaseDelay = 300000u;
	}
	pState->Client = xrtHttpClientCreate(
		pState->Engine,
		&ClientConfig
	);
	testRequire(
		pState->Client != NULL,
		"HTTP retry client creation failed"
	);
}



/* 构造请求、冻结调用选项并提交一次高层调用。 */
static void testHttpRetryCall(test_http_retry* pState)
{
	xhttpcalloptions Options;
	xhttprequest* pRequest;
	bool bPost =
		(pState->Scenario == TEST_HTTP_RETRY_POST_BLOCKED) ||
		(pState->Scenario == TEST_HTTP_RETRY_POST_UNSAFE);
	char Url[128];
	int iLength;

	iLength = snprintf(
		Url,
		sizeof(Url),
		"http://retry.test:%u/test",
		(unsigned int)pState->Port
	);
	testRequire(
		(iLength > 0) && ((size_t)iLength < sizeof(Url)),
		"HTTP retry URL overflowed"
	);
	pRequest = xrtHttpRequestCreate(
		bPost ? XRT_STR_LITERAL("POST") :
			XRT_STR_LITERAL("GET"),
		(xstrview){ Url, (size_t)iLength }
	);
	testRequire(
		pRequest != NULL,
		"HTTP retry request creation failed"
	);
	if ( bPost ) {
		testRequire(
			xrtHttpRequestSetBytes(
				pRequest,
				(xbytesview){ (cbytes)"DATA", 4u },
				XRT_STR_LITERAL("text/plain")
			),
			"HTTP retry replayable body setup failed"
		);
	}
	xrtHttpCallOptionsInit(&Options);
	Options.Events.Headers = testHttpRetryHeaders;
	Options.Events.Body = testHttpRetryBody;
	Options.Events.Data = pState;
	Options.Timeout = 5000000u;
	Options.IdleTimeout = 1000000u;
	if ( pState->Scenario == TEST_HTTP_RETRY_POST_UNSAFE ) {
		Options.Retry.Flags = XHTTP_RETRY_UNSAFE;
	} else if ( pState->Scenario ==
		TEST_HTTP_RETRY_CALL_ENABLED ) {
		Options.Retry.Mode = XHTTP_RETRY_ENABLED;
	} else if ( pState->Scenario ==
		TEST_HTTP_RETRY_CALL_DISABLED ) {
		Options.Retry.Mode = XHTTP_RETRY_DISABLED;
	} else if (
		pState->Scenario == TEST_HTTP_RETRY_TOTAL_TIMEOUT
	) {
		Options.Timeout = 80000u;
		Options.IdleTimeout = 10000u;
	}
	pState->Call = xrtHttpClientDo(
		pState->Client,
		pRequest,
		&Options,
		testHttpRetryDone,
		pState
	);
	xrtHttpRequestDestroy(pRequest);
	testRequire(
		pState->Call != NULL,
		"HTTP retry call submission failed"
	);
}



/* 释放场景并证明所有 Stream、Listener 和 Timer 均已排空。 */
static void testHttpRetryStop(test_http_retry* pState)
{
	uint32 iAccepted = xrtAtomic32Load(
		&pState->Accepted,
		XMEMORY_ACQUIRE
	);

	testHttpRetryWait(
		&pState->Closed,
		iAccepted,
		"HTTP retry server streams did not close"
	);
	testRequire(
		xrtNetListenerClose(pState->Listener),
		"HTTP retry listener close failed"
	);
	testHttpRetryWait(
		&pState->ListenerClosed,
		1,
		"HTTP retry listener did not close"
	);
	xrtHttpResponseDestroy(pState->Response);
	xrtHttpCallDestroy(pState->Call);
	xrtHttpClientDestroy(pState->Client);
	for ( uint32 i = 0; i < iAccepted; i++ ) {
		xrtNetStreamDestroy(pState->Streams[i]);
	}
	xrtNetListenerDestroy(pState->Listener);
	testRequire(
		xrtNetEngineDestroy(pState->Engine),
		"HTTP retry engine destroy failed"
	);
}



/* 运行一个完整场景并核对实际请求数量。 */
static void testHttpRetryRun(
	test_http_retry_scenario Scenario,
	size_t iRequests,
	size_t iRetries
)
{
	test_http_retry State;

	testHttpRetryStart(&State, Scenario);
	State.ExpectedRequests = iRequests;
	State.ExpectedRetries = iRetries;
	testHttpRetryCall(&State);
	if ( Scenario == TEST_HTTP_RETRY_CANCEL_WAIT ) {
		testHttpRetryWait(
			&State.Requests,
			1,
			"HTTP retry cancellation request was not received"
		);
		testHttpRetryWaitPhase(
			State.Call,
			XHTTP_CALL_PHASE_RETRY,
			"HTTP retry call did not enter backoff"
		);
		testRequire(
			xrtHttpCallCancel(State.Call),
			"HTTP retry backoff cancellation failed"
		);
	}
	testHttpRetryWait(
		&State.Completed,
		1,
		"HTTP retry call did not complete"
	);
	testHttpRetryWait(
		&State.Requests,
		(uint32)iRequests,
		"HTTP retry request count was incomplete"
	);
	testRequire(
		xrtAtomic32Load(
			&State.Requests,
			XMEMORY_ACQUIRE
		) == (uint32)iRequests,
		"HTTP retry exceeded its request budget"
	);
	testHttpRetryStop(&State);
}



/* 覆盖状态、传输、重放、上限、取消和总超时契约。 */
int main(void)
{
	testHttpRetryRun(TEST_HTTP_RETRY_STATUS, 2u, 1u);
	testHttpRetryRun(TEST_HTTP_RETRY_DISABLED, 1u, 0u);
	testHttpRetryRun(TEST_HTTP_RETRY_CALL_ENABLED, 2u, 1u);
	testHttpRetryRun(TEST_HTTP_RETRY_CALL_DISABLED, 1u, 0u);
	testHttpRetryRun(TEST_HTTP_RETRY_POST_BLOCKED, 1u, 0u);
	testHttpRetryRun(TEST_HTTP_RETRY_POST_UNSAFE, 2u, 1u);
	testHttpRetryRun(TEST_HTTP_RETRY_TRANSPORT, 2u, 1u);
	testHttpRetryRun(TEST_HTTP_RETRY_MIXED, 3u, 2u);
	testHttpRetryRun(TEST_HTTP_RETRY_LIMIT, 3u, 2u);
	testHttpRetryRun(TEST_HTTP_RETRY_CANCEL_WAIT, 1u, 0u);
	testHttpRetryRun(TEST_HTTP_RETRY_TOTAL_TIMEOUT, 1u, 0u);
	printf("[PASS] HTTP client retry lifecycle (select)\n");
	return 0;
}

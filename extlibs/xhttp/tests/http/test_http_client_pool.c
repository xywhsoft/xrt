#include "../test.h"
#include "../../src/internal/xrt_http_client_runtime.h"



#if !defined(TEST_HTTP_POOL_BACKEND)
	#define TEST_HTTP_POOL_BACKEND XNET_PORT_SELECT
	#define TEST_HTTP_POOL_BACKEND_NAME "select"
#endif

#define TEST_HTTP_POOL_LIFECYCLE_ROUNDS 32u
#define TEST_HTTP_POOL_SERVER_LIMIT 4u



#if defined(TEST_HTTP_POOL_REDIRECT_DECOMPRESS)

static const uint8 TestHttpPoolGzip[] = {
	0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0xFF, 0xCB, 0x48, 0xCD, 0xC9, 0xC9, 0x57,
	0x48, 0xCE, 0xCF, 0x2D, 0x28, 0x4A, 0x2D, 0x2E,
	0x4E, 0x4D, 0x51, 0x28, 0xCF, 0x2F, 0xCA, 0x49,
	0x01, 0x00, 0xA1, 0x2D, 0x94, 0x53, 0x16, 0x00,
	0x00, 0x00
};

static const char TestHttpPoolPlain[] =
	"hello compressed world";

#endif



typedef enum test_http_pool_result {
	TEST_HTTP_POOL_SUCCESS = 0,
	TEST_HTTP_POOL_CANCELLED,
	TEST_HTTP_POOL_REJECTED
} test_http_pool_result;



typedef struct test_http_pool test_http_pool;



/* 每次完成上下文固定自己的预期终态和响应所有权。 */
typedef struct test_http_pool_call {
	test_http_pool* State;
	xhttpcall* Call;
	xhttpresponse* Response;
	xatomic32 Completed;
	test_http_pool_result Expected;
	size_t ExpectedRedirects;
	uint32 Order;
} test_http_pool_call;



/* 夹具保存最多两代服务端连接、共享 Client 和四个 Call。 */
struct test_http_pool {
	xnetengine* Engine;
	xnetlistener* Listener;
	xnetstream* Servers[TEST_HTTP_POOL_SERVER_LIMIT];
	xhttpclient* Client;
	test_http_pool_call Calls[4];
	xatomic32 Accepted;
	xatomic32 Requests;
	xatomic32 ServerClosed;
	xatomic32 ListenerClosed;
	xatomic32 CompletionSequence;
	uint32 HoldRequests;
	bool RedirectFirst;
	bool AllowReconnect;
};



/* 把一个 ASCII 大写字母折叠为小写，其余字节保持不变。 */
static unsigned char testHttpPoolAsciiLower(unsigned char iValue)
{
	if ( (iValue >= (unsigned char)'A') &&
		(iValue <= (unsigned char)'Z') ) {
		return (unsigned char)(iValue +
			((unsigned char)'a' - (unsigned char)'A'));
	}
	return iValue;
}



/* 按 HTTP 使用的 ASCII 大小写不敏感规则比较完整字符串。 */
static bool testHttpPoolAsciiEqual(cstr sLeft, cstr sRight)
{
	while ( (*sLeft != 0) && (*sRight != 0) ) {
		if ( testHttpPoolAsciiLower((unsigned char)*sLeft) !=
			testHttpPoolAsciiLower((unsigned char)*sRight) ) {
			return false;
		}
		sLeft++;
		sRight++;
	}
	return ((*sLeft == 0) && (*sRight == 0));
}



/* 按 ASCII 大小写不敏感规则判断文本是否以给定前缀开始。 */
static bool testHttpPoolAsciiStarts(cstr sText, cstr sPrefix)
{
	while ( *sPrefix != 0 ) {
		if ( (*sText == 0) ||
			(testHttpPoolAsciiLower((unsigned char)*sText) !=
			 testHttpPoolAsciiLower((unsigned char)*sPrefix)) ) {
			return false;
		}
		sText++;
		sPrefix++;
	}
	return true;
}



/* 在十秒边界内等待 Worker 发布指定原子值。 */
static void testHttpPoolWaitValue(
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



/* 等待连接池统计满足调用方给出的活动、空闲、关闭和等待数量。 */
static xhttpclientstats testHttpPoolWaitStats(
	xhttpclient* pClient,
	size_t iActive,
	size_t iIdle,
	size_t iClosing,
	size_t iWaiting,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(10000000u);
	xhttpclientstats Stats;

	for ( ;; ) {
		testRequire(
			xrtHttpClientStats(pClient, &Stats),
			"HTTP pool stats query failed"
		);
		if ( (Stats.ActiveConnections == iActive) &&
			(Stats.IdleConnections == iIdle) &&
			(Stats.ClosingConnections == iClosing) &&
			(Stats.WaitingCalls == iWaiting) ) {
			return Stats;
		}
		testRequire(
			!xrtDeadlineExpired(Deadline),
			sMessage
		);
		xrtThreadYield();
	}
}



/* 等待异步 Timer 取消和高层对象引用全部退出 Engine。 */
static void testHttpPoolWaitEngineIdle(
	xnetengine* pEngine,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(10000000u);
	xnetenginestats Stats;

	for ( ;; ) {
		testRequire(
			xrtNetEngineStats(pEngine, &Stats),
			"HTTP pool engine stats query failed"
		);
		if ( (Stats.PendingCommands == 0) &&
			(Stats.ActiveTimers == 0) &&
			(Stats.LiveObjects == 0) ) {
			return;
		}
		testRequire(
			!xrtDeadlineExpired(Deadline),
			sMessage
		);
		xrtThreadYield();
	}
}



/* 为固定测试域名返回本机 IPv4，排除系统 DNS 波动。 */
static xnetaddrlist* testHttpPoolLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)pData;
	testRequire(
		testHttpPoolAsciiEqual(sHost, "pool.test") ||
		testHttpPoolAsciiEqual(sHost, "other.test"),
		"HTTP pool resolved an unexpected host"
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
		"HTTP pool resolver address failed"
	);
	return xrtNetAddrListCreate(&Address, 1);
}



/* 发送一条保持连接的固定长度响应。 */
static void testHttpPoolRespond(xnetstream* pStream)
{
	#if defined(TEST_HTTP_POOL_REDIRECT_DECOMPRESS)
		static const char Head[] =
			"HTTP/1.1 200 OK\r\n"
			"Content-Encoding: gzip\r\n"
			"Content-Length: 42\r\n"
			"Connection: keep-alive\r\n"
			"\r\n";

		testRequire(
			(xrtNetStreamSend(
				pStream,
				Head,
				sizeof(Head) - 1u
			) == XNET_RESULT_OK) &&
			(xrtNetStreamSend(
				pStream,
				TestHttpPoolGzip,
				sizeof(TestHttpPoolGzip)
			) == XNET_RESULT_OK),
			"HTTP pool compressed response send failed"
		);
	#else
	static const char Response[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 2\r\n"
		"Connection: keep-alive\r\n"
		"\r\n"
		"OK";

	testRequire(
		xrtNetStreamSend(
			pStream,
			Response,
			sizeof(Response) - 1u
		) == XNET_RESULT_OK,
		"HTTP pool response send failed"
	);
	#endif
}



/* 首个请求返回可复用的相对重定向，验证下一跳能够复用同一连接。 */
static void testHttpPoolRedirect(xnetstream* pStream)
{
	#if defined(TEST_HTTP_POOL_REDIRECT_DECOMPRESS)
		static const char Response[] =
			"HTTP/1.1 302 Found\r\n"
			"Location: /final\r\n"
			"Content-Encoding: gzip\r\n"
			"Content-Length: 4\r\n"
			"Connection: keep-alive\r\n"
			"\r\n"
			"bad!";
	#else
	static const char Response[] =
		"HTTP/1.1 302 Found\r\n"
		"Location: /final\r\n"
		"Content-Length: 0\r\n"
		"Connection: keep-alive\r\n"
		"\r\n";
	#endif

	testRequire(
		xrtNetStreamSend(
			pStream,
			Response,
			sizeof(Response) - 1u
		) == XNET_RESULT_OK,
		"HTTP pool redirect response send failed"
	);
}



/* 接收一个或多个完整请求头，首请求可由主线程故意延迟响应。 */
static void testHttpPoolServerRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	test_http_pool* pState = (test_http_pool*)pData;
	char Request[2048];

	for ( ;; ) {
		size_t iSize = xrtNetBufSize(pBuffer);
		size_t iHeader = 0;
		cstr sHost;
		uint32 iRequest;

		if ( iSize < 4u ) {
			return;
		}
		testRequire(
			iSize < sizeof(Request),
			"HTTP pool request exceeded fixture capacity"
		);
		testRequire(
			xrtNetBufPeek(
				pBuffer,
				0,
				Request,
				iSize
			) == iSize,
			"HTTP pool request peek failed"
		);
		Request[iSize] = 0;
		for ( size_t i = 3; i < iSize; i++ ) {
			if ( (Request[i - 3u] == '\r') &&
				(Request[i - 2u] == '\n') &&
				(Request[i - 1u] == '\r') &&
				(Request[i] == '\n') ) {
				iHeader = i + 1u;
				break;
			}
		}
		if ( iHeader == 0 ) {
			return;
		}
		sHost = strstr(Request, "\r\nHost: ");
		testRequire(
			(iHeader >= 14u) &&
			(memcmp(Request, "GET /", 5u) == 0) &&
			(strstr(Request, " HTTP/1.1\r\n") != NULL) &&
			((sHost != NULL) &&
			 (testHttpPoolAsciiStarts(
				sHost + 8u,
				"pool.test:"
			 ) ||
			  testHttpPoolAsciiStarts(
				sHost + 8u,
				"other.test:"
			  ))),
			"HTTP pool emitted an invalid request"
		);
		testRequire(
			xrtNetBufConsume(
				pBuffer,
				iHeader
			) == iHeader,
			"HTTP pool request consume failed"
		);
		iRequest = xrtAtomic32FetchAdd(
			&pState->Requests,
			1,
			XMEMORY_ACQ_REL
		) + 1u;
		if ( iRequest > pState->HoldRequests ) {
			if ( pState->RedirectFirst &&
				(iRequest == 1u) ) {
				testHttpPoolRedirect(pStream);
			} else {
				testHttpPoolRespond(pStream);
			}
		}
	}
}



/* 收到客户端 FIN 后完成服务端正常关闭。 */
static void testHttpPoolServerEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	testRequire(
		xrtNetStreamClose(pStream),
		"HTTP pool server close failed"
	);
}



/* 累计每一代服务端连接释放套接字的唯一终态。 */
static void testHttpPoolServerClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_pool* pState = (test_http_pool*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pState->ServerClosed,
		1,
		XMEMORY_ACQ_REL
	);
}



/* 默认只接管首条连接，陈旧连接场景允许接管第二代连接。 */
static bool testHttpPoolAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	test_http_pool* pState = (test_http_pool*)pData;
	xnetstreamevents Events;
	uint32 iAccepted;

	(void)pListener;
	iAccepted = xrtAtomic32FetchAdd(
		&pState->Accepted,
		1,
		XMEMORY_ACQ_REL
	);
	if ( (iAccepted >= TEST_HTTP_POOL_SERVER_LIMIT) ||
		(!pState->AllowReconnect && (iAccepted != 0)) ) {
		return false;
	}
	memset(&Events, 0, sizeof(Events));
	Events.Read = testHttpPoolServerRead;
	Events.End = testHttpPoolServerEnd;
	Events.Close = testHttpPoolServerClose;
	testRequire(
		xrtNetStreamSetEvents(
			pStream,
			&Events,
			pState
		),
		"HTTP pool server event takeover failed"
	);
	pState->Servers[iAccepted] = pStream;
	return true;
}



/* 记录 Listener 已经排空全部 Accept。 */
static void testHttpPoolListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_http_pool* pState = (test_http_pool*)pData;

	(void)pListener;
	xrtAtomic32Store(
		&pState->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证成功、等待取消和池上限拒绝的稳定高层终态。 */
static void testHttpPoolDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	test_http_pool_call* pContext =
		(test_http_pool_call*)pData;

	testRequire(
		(pCall != NULL) && (pResult != NULL),
		"HTTP pool completion identity mismatch"
	);
	if ( pContext->Expected == TEST_HTTP_POOL_SUCCESS ) {
		testRequire(
			(pResult->Result == XNET_RESULT_OK) &&
			(pResult->Response != NULL) &&
			(pResult->Tcp == NULL) &&
			(pResult->Error == NULL) &&
			(xrtHttpResponseStatus(
				pResult->Response
			) == 200) &&
			#if defined(TEST_HTTP_POOL_REDIRECT_DECOMPRESS)
				(xrtHttpResponseBody(
					pResult->Response
				).Size ==
				 (sizeof(TestHttpPoolPlain) - 1u)) &&
				(memcmp(
					xrtHttpResponseBody(
						pResult->Response
					).Data,
					TestHttpPoolPlain,
					sizeof(TestHttpPoolPlain) - 1u
				) == 0) &&
				((xrtHttpResponseFlags(
					pResult->Response
				) & XHTTP_RESPONSE_DECOMPRESSED) != 0) &&
				(xrtHttpResponseOriginalEncoding(
					pResult->Response
				).Size == 4u) &&
				(memcmp(
					xrtHttpResponseOriginalEncoding(
						pResult->Response
					).Data,
					"gzip",
					4u
				) == 0)
			#else
			(xrtHttpResponseBody(
				pResult->Response
			).Size == 2u)
			#endif
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
				&& (pResult->Info.Redirects ==
					pContext->ExpectedRedirects)
			#endif
			,
			"HTTP pool success result mismatch"
		);
		pContext->Response = pResult->Response;
	} else if (
		pContext->Expected == TEST_HTTP_POOL_CANCELLED
	) {
		testRequire(
			(pResult->Result == XNET_RESULT_CANCELLED) &&
			(pResult->Response == NULL) &&
			(pResult->Error != NULL) &&
			(xrtErrorKind(pResult->Error) ==
			 XERR_CANCELLED) &&
			(xrtErrorCode(pResult->Error) ==
			 XHTTP_CLIENT_ERROR_CANCELLED),
			"HTTP pool waiting cancellation mismatch"
		);
	} else {
		testRequire(
			(pResult->Result == XNET_RESULT_ERROR) &&
			(pResult->Response == NULL) &&
			(pResult->Error != NULL) &&
			(xrtErrorKind(pResult->Error) ==
			 XERR_AGAIN) &&
			(xrtErrorCode(pResult->Error) ==
			 XHTTP_CLIENT_ERROR_POOL),
			"HTTP pool waiting rejection mismatch"
		);
	}
	pContext->Order = xrtAtomic32FetchAdd(
		&pContext->State->CompletionSequence,
		1,
		XMEMORY_ACQ_REL
	) + 1u;
	xrtAtomic32Store(
		&pContext->Completed,
		1,
		XMEMORY_RELEASE
	);
}



/* 建立共享 Engine、Listener 和带固定池策略的 Client。 */
static xnetaddr testHttpPoolStart(
	test_http_pool* pState,
	const xhttpclientpoolconfig* pPool
)
{
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xhttpclientconfig ClientConfig;
	xnetaddr Address = { 0 };

	memset(pState, 0, sizeof(*pState));
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	xrtAtomic32Init(&pState->Accepted, 0);
	xrtAtomic32Init(&pState->Requests, 0);
	xrtAtomic32Init(&pState->ServerClosed, 0);
	xrtAtomic32Init(&pState->ListenerClosed, 0);
	xrtAtomic32Init(&pState->CompletionSequence, 0);
	for ( size_t i = 0; i < 4u; i++ ) {
		pState->Calls[i].State = pState;
		xrtAtomic32Init(
			&pState->Calls[i].Completed,
			0
		);
	}

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_HTTP_POOL_BACKEND;
	EngineConfig.Workers = 2;
	pState->Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pState->Engine != NULL) &&
		xrtNetEngineStart(pState->Engine),
		"HTTP pool engine start failed"
	);
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ListenConfig.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP pool listener address failed"
	);
	ListenConfig.AcceptConcurrency = 4;
	ListenerEvents.Accept = testHttpPoolAccept;
	ListenerEvents.Close = testHttpPoolListenerClose;
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
		"HTTP pool listener creation failed"
	);

	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Resolver.Lookup = testHttpPoolLookup;
	ClientConfig.Dial.FallbackDelay = 1000u;
	ClientConfig.Dial.MaxAttempts = 1;
	ClientConfig.Pool = *pPool;
	pState->Client = xrtHttpClientCreate(
		pState->Engine,
		&ClientConfig
	);
	testRequire(
		pState->Client != NULL,
		"HTTP pool client creation failed"
	);
	return Address;
}



/* 为指定 Host 构造并提交一个由 Client 接管请求快照的 GET Call。 */
static void testHttpPoolCallHost(
	test_http_pool* pState,
	size_t iCall,
	xnetaddr Address,
	cstr sHost,
	cstr sTarget,
	test_http_pool_result Expected
)
{
	test_http_pool_call* pContext =
		&pState->Calls[iCall];
	xhttprequest* pRequest;
	char Url[160];
	int iLength;

	iLength = snprintf(
		Url,
		sizeof(Url),
		"http://%s:%u/%s",
		sHost,
		(unsigned int)Address.Port,
		sTarget
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Url)),
		"HTTP pool URL overflowed"
	);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		(xstrview){ Url, (size_t)iLength }
	);
	testRequire(
		pRequest != NULL,
		"HTTP pool request creation failed"
	);
	pContext->Expected = Expected;
	pContext->Call = xrtHttpClientDo(
		pState->Client,
		pRequest,
		NULL,
		testHttpPoolDone,
		pContext
	);
	xrtHttpRequestDestroy(pRequest);
	testRequire(
		pContext->Call != NULL,
		"HTTP pool call submission failed"
	);
}



/* 使用默认测试 Origin 提交 GET Call。 */
static void testHttpPoolCall(
	test_http_pool* pState,
	size_t iCall,
	xnetaddr Address,
	cstr sTarget,
	test_http_pool_result Expected
)
{
	testHttpPoolCallHost(
		pState,
		iCall,
		Address,
		"pool.test",
		sTarget,
		Expected
	);
}



/* 关闭夹具并验证所有网络对象均可同步释放。 */
static void testHttpPoolStop(test_http_pool* pState)
{
	uint32 iExpectedClosed = pState->AllowReconnect ?
		xrtAtomic32Load(
			&pState->Accepted,
			XMEMORY_ACQUIRE
		) : 1u;

	testRequire(
		xrtHttpClientCloseIdle(
			pState->Client
		) <= iExpectedClosed,
		"HTTP pool close-idle count mismatch"
	);
	testHttpPoolWaitValue(
		&pState->ServerClosed,
		iExpectedClosed,
		"HTTP pool server did not close"
	);
	(void)testHttpPoolWaitStats(
		pState->Client,
		0,
		0,
		0,
		0,
		"HTTP pool client close did not finish"
	);
	testRequire(
		xrtNetListenerClose(pState->Listener),
		"HTTP pool listener close failed"
	);
	testHttpPoolWaitValue(
		&pState->ListenerClosed,
		1,
		"HTTP pool listener did not close"
	);
	for ( size_t i = 0; i < 4u; i++ ) {
		xrtHttpResponseDestroy(
			pState->Calls[i].Response
		);
		xrtHttpCallDestroy(
			pState->Calls[i].Call
		);
	}
	xrtHttpClientDestroy(pState->Client);
	for ( size_t i = 0;
		i < TEST_HTTP_POOL_SERVER_LIMIT;
		i++ ) {
		xrtNetStreamDestroy(pState->Servers[i]);
	}
	xrtNetListenerDestroy(pState->Listener);
	testHttpPoolWaitEngineIdle(
		pState->Engine,
		"HTTP pool engine did not become idle"
	);
	testRequire(
		xrtNetEngineDestroy(pState->Engine),
		"HTTP pool engine destroy failed"
	);
}



/* Host 大小写不同的两个串行请求必须复用同一条 TCP 连接。 */
static void testHttpPoolReuse(void)
{
	test_http_pool State;
	xhttpclientpoolconfig Pool;
	xhttpclientstats Stats;
	xnetaddr Address;

	xrtHttpClientPoolConfigInit(&Pool);
	Pool.MaxConnections = 1;
	Pool.MaxConnectionsPerOrigin = 1;
	Pool.MaxIdle = 2;
	Pool.MaxIdlePerOrigin = 1;
	Pool.IdleTimeout = 0;
	Address = testHttpPoolStart(&State, &Pool);
	testHttpPoolCall(
		&State,
		0,
		Address,
		"one",
		TEST_HTTP_POOL_SUCCESS
	);
	testHttpPoolWaitValue(
		&State.Calls[0].Completed,
		1,
		"HTTP pool first call did not complete"
	);
	testHttpPoolCallHost(
		&State,
		1,
		Address,
		"POOL.TEST",
		"two",
		TEST_HTTP_POOL_SUCCESS
	);
	testHttpPoolWaitValue(
		&State.Calls[1].Completed,
		1,
		"HTTP pool reused call did not complete"
	);
	testHttpPoolWaitValue(
		&State.Requests,
		2,
		"HTTP pool server did not receive two requests"
	);
	Stats = testHttpPoolWaitStats(
		State.Client,
		0,
		1,
		0,
		0,
		"HTTP pool did not retain one idle connection"
	);
	testRequire(
		(xrtAtomic32Load(
			&State.Accepted,
			XMEMORY_ACQUIRE
		 ) == 1u) &&
		(Stats.RequestsStarted == 2u) &&
		(Stats.RequestsCompleted == 2u) &&
		(Stats.ConnectionsOpened == 1u) &&
		(Stats.ConnectionsReused == 1u) &&
		(Stats.ConnectionsClosed == 0u),
		"HTTP pool reuse statistics mismatch"
	);
	testRequire(
		xrtHttpClientCloseIdle(State.Client) == 1u,
		"HTTP pool did not close its reusable connection"
	);
	testHttpPoolWaitValue(
		&State.ServerClosed,
		1,
		"HTTP pool reusable connection did not close"
	);
	testHttpPoolStop(&State);
}



/* 硬连接与等待上限必须支持取消、拒绝和同源直接交接。 */
static void testHttpPoolWaitAndCancel(void)
{
	test_http_pool State;
	xhttpclientpoolconfig Pool;
	xhttpclientstats Stats;
	xnetaddr Address;

	xrtHttpClientPoolConfigInit(&Pool);
	Pool.MaxConnections = 1;
	Pool.MaxConnectionsPerOrigin = 1;
	Pool.MaxWaiting = 1;
	Pool.MaxWaitingPerOrigin = 1;
	Pool.MaxIdle = 1;
	Pool.MaxIdlePerOrigin = 1;
	Pool.IdleTimeout = 0;
	Address = testHttpPoolStart(&State, &Pool);
	State.HoldRequests = 1;
	testHttpPoolCall(
		&State,
		0,
		Address,
		"hold",
		TEST_HTTP_POOL_SUCCESS
	);
	testHttpPoolWaitValue(
		&State.Requests,
		1,
		"HTTP pool held request did not arrive"
	);
	testHttpPoolCall(
		&State,
		1,
		Address,
		"cancel",
		TEST_HTTP_POOL_CANCELLED
	);
	(void)testHttpPoolWaitStats(
		State.Client,
		1,
		0,
		0,
		1,
		"HTTP pool call did not enter the wait queue"
	);
	testHttpPoolCall(
		&State,
		2,
		Address,
		"reject",
		TEST_HTTP_POOL_REJECTED
	);
	testHttpPoolWaitValue(
		&State.Calls[2].Completed,
		1,
		"HTTP pool did not reject an excess waiter"
	);
	testRequire(
		xrtHttpCallCancel(State.Calls[1].Call),
		"HTTP pool waiting cancellation failed"
	);
	testHttpPoolWaitValue(
		&State.Calls[1].Completed,
		1,
		"HTTP pool cancelled waiter did not complete"
	);
	testHttpPoolCall(
		&State,
		3,
		Address,
		"handoff",
		TEST_HTTP_POOL_SUCCESS
	);
	(void)testHttpPoolWaitStats(
		State.Client,
		1,
		0,
		0,
		1,
		"HTTP pool replacement waiter did not queue"
	);
	testHttpPoolRespond(State.Servers[0]);
	testHttpPoolWaitValue(
		&State.Calls[0].Completed,
		1,
		"HTTP pool held call did not complete"
	);
	testHttpPoolWaitValue(
		&State.Calls[3].Completed,
		1,
		"HTTP pool direct handoff did not complete"
	);
	testHttpPoolWaitValue(
		&State.Requests,
		2,
		"HTTP pool direct handoff did not reuse transport"
	);
	Stats = testHttpPoolWaitStats(
		State.Client,
		0,
		1,
		0,
		0,
		"HTTP pool handoff connection did not become idle"
	);
	testRequire(
		(xrtAtomic32Load(
			&State.Accepted,
			XMEMORY_ACQUIRE
		 ) == 1u) &&
		(Stats.RequestsStarted == 4u) &&
		(Stats.RequestsCompleted == 4u) &&
		(Stats.ConnectionsOpened == 1u) &&
		(Stats.ConnectionsReused == 1u) &&
		(Stats.PoolWaits == 2u) &&
		(Stats.PoolRejected == 1u),
		"HTTP pool wait lifecycle statistics mismatch"
	);
	testHttpPoolStop(&State);
}



/* 已经取得取消门的 Call 不得在取消分发结束后重新进入池等待队列。 */
static void testHttpPoolCancelledAcquire(void)
{
	xhttpclient Client;
	xhttpcall Active;
	xhttpcall Cancelled;
	xhttpclientpoolconfig Pool;
	bool bReady = false;

	memset(&Client, 0, sizeof(Client));
	memset(&Active, 0, sizeof(Active));
	memset(&Cancelled, 0, sizeof(Cancelled));
	xrtHttpClientPoolConfigInit(&Pool);
	Pool.MaxConnections = 1u;
	Pool.MaxConnectionsPerOrigin = 1u;
	Pool.MaxWaiting = 1u;
	Pool.MaxWaitingPerOrigin = 1u;
	Client.Config.Pool = Pool;
	testRequire(
		__xrtHttpPoolInit(&Client),
		"HTTP pool cancelled acquire fixture failed"
	);
	Active.Client = &Client;
	Active.Host = "pool.test";
	Active.Port = 80u;
	Cancelled.Client = &Client;
	Cancelled.Host = "pool.test";
	Cancelled.Port = 80u;
	xrtAtomic32Init(&Cancelled.CancelGate, 1u);
	testRequire(
		__xrtHttpPoolAcquire(&Active, &bReady) &&
		bReady && Active.PoolReserved,
		"HTTP pool active reservation fixture failed"
	);
	bReady = false;
	testRequire(
		__xrtHttpPoolAcquire(&Cancelled, &bReady) &&
		bReady && !Cancelled.PoolWaiting &&
		(Cancelled.PoolOrigin == NULL) &&
		(xrtAtomic64Load(
			&Client.PoolConnections,
			XMEMORY_RELAXED
		) == 1u) &&
		(xrtAtomic64Load(
			&Client.PoolWaiting,
			XMEMORY_RELAXED
		) == 0u),
		"HTTP pool queued a previously cancelled call"
	);
	__xrtHttpPoolFinish(&Active);
	testRequire(
		(xrtAtomic64Load(
			&Client.PoolConnections,
			XMEMORY_RELAXED
		) == 0u) &&
		(xrtAtomic64Load(
			&Client.PoolWaiting,
			XMEMORY_RELAXED
		) == 0u) &&
		(xrtAtomic64Load(
			&Client.PoolLive,
			XMEMORY_RELAXED
		) == 0u),
		"HTTP pool cancelled acquire cleanup mismatch"
	);
	__xrtHttpPoolUnit(&Client);
}



/*
	全局槽位释放后必须优先服务最早可运行的 Origin，
	不能由较晚的同源等待者持续抢占刚归还的连接。
*/
static void testHttpPoolGlobalFairness(void)
{
	test_http_pool State;
	xhttpclientpoolconfig Pool;
	xhttpclientstats Stats;
	xnetaddr Address;

	xrtHttpClientPoolConfigInit(&Pool);
	Pool.MaxConnections = 1;
	Pool.MaxConnectionsPerOrigin = 1;
	Pool.MaxWaiting = 3;
	Pool.MaxWaitingPerOrigin = 2;
	Pool.MaxIdle = 1;
	Pool.MaxIdlePerOrigin = 1;
	Pool.IdleTimeout = 0;
	Address = testHttpPoolStart(&State, &Pool);
	State.AllowReconnect = true;
	State.HoldRequests = 1;
	testHttpPoolCall(
		&State,
		0,
		Address,
		"hold-a",
		TEST_HTTP_POOL_SUCCESS
	);
	testHttpPoolWaitValue(
		&State.Requests,
		1,
		"HTTP pool fairness first request did not arrive"
	);
	testHttpPoolCallHost(
		&State,
		1,
		Address,
		"other.test",
		"first-b",
		TEST_HTTP_POOL_SUCCESS
	);
	testHttpPoolCall(
		&State,
		2,
		Address,
		"second-a",
		TEST_HTTP_POOL_SUCCESS
	);
	(void)testHttpPoolWaitStats(
		State.Client,
		1,
		0,
		0,
		2,
		"HTTP pool fairness calls did not queue"
	);
	testHttpPoolRespond(State.Servers[0]);
	testHttpPoolWaitValue(
		&State.Calls[0].Completed,
		1,
		"HTTP pool fairness first call did not complete"
	);
	testHttpPoolWaitValue(
		&State.Calls[1].Completed,
		1,
		"HTTP pool fairness older cross-origin call starved"
	);
	testHttpPoolWaitValue(
		&State.Calls[2].Completed,
		1,
		"HTTP pool fairness later same-origin call did not complete"
	);
	testHttpPoolWaitValue(
		&State.Requests,
		3,
		"HTTP pool fairness requests did not all arrive"
	);
	Stats = testHttpPoolWaitStats(
		State.Client,
		0,
		1,
		0,
		0,
		"HTTP pool fairness final connection did not become idle"
	);
	testRequire(
		(State.Calls[1].Order < State.Calls[2].Order) &&
		(xrtAtomic32Load(
			&State.Accepted,
			XMEMORY_ACQUIRE
		 ) == 3u) &&
		(Stats.ConnectionsOpened == 3u) &&
		(Stats.ConnectionsReused == 0u) &&
		(Stats.ConnectionsClosed == 2u) &&
		(Stats.PoolWaits == 2u),
		"HTTP pool global fairness contract mismatch"
	);
	testHttpPoolStop(&State);
}



/*
	较早等待者若仍被自己的 Origin 上限阻塞，不能阻止后续可运行
	等待者接管同源连接，也不能因此产生额外拨号。
*/
static void testHttpPoolBlockedHead(void)
{
	test_http_pool State;
	xhttpclientpoolconfig Pool;
	xhttpclientstats Stats;
	xnetaddr Address;

	xrtHttpClientPoolConfigInit(&Pool);
	Pool.MaxConnections = 2;
	Pool.MaxConnectionsPerOrigin = 1;
	Pool.MaxWaiting = 2;
	Pool.MaxWaitingPerOrigin = 1;
	Pool.MaxIdle = 2;
	Pool.MaxIdlePerOrigin = 1;
	Pool.IdleTimeout = 0;
	Address = testHttpPoolStart(&State, &Pool);
	State.AllowReconnect = true;
	State.HoldRequests = 2;
	testHttpPoolCall(
		&State,
		0,
		Address,
		"hold-a",
		TEST_HTTP_POOL_SUCCESS
	);
	testHttpPoolWaitValue(
		&State.Requests,
		1,
		"HTTP pool blocked-head A request did not arrive"
	);
	testHttpPoolCallHost(
		&State,
		1,
		Address,
		"other.test",
		"hold-b",
		TEST_HTTP_POOL_SUCCESS
	);
	testHttpPoolWaitValue(
		&State.Requests,
		2,
		"HTTP pool blocked-head B request did not arrive"
	);
	testHttpPoolCallHost(
		&State,
		2,
		Address,
		"other.test",
		"wait-b",
		TEST_HTTP_POOL_SUCCESS
	);
	testHttpPoolCall(
		&State,
		3,
		Address,
		"wait-a",
		TEST_HTTP_POOL_SUCCESS
	);
	(void)testHttpPoolWaitStats(
		State.Client,
		2,
		0,
		0,
		2,
		"HTTP pool blocked-head calls did not queue"
	);
	testHttpPoolRespond(State.Servers[0]);
	testHttpPoolWaitValue(
		&State.Calls[3].Completed,
		1,
		"HTTP pool blocked head stalled runnable A call"
	);
	testRequire(
		!xrtAtomic32Load(
			&State.Calls[2].Completed,
			XMEMORY_ACQUIRE
		),
		"HTTP pool blocked B waiter ran before its Origin slot opened"
	);
	testHttpPoolRespond(State.Servers[1]);
	testHttpPoolWaitValue(
		&State.Calls[2].Completed,
		1,
		"HTTP pool blocked B waiter did not resume"
	);
	Stats = testHttpPoolWaitStats(
		State.Client,
		0,
		2,
		0,
		0,
		"HTTP pool blocked-head transports did not become idle"
	);
	testRequire(
		(xrtAtomic32Load(
			&State.Accepted,
			XMEMORY_ACQUIRE
		 ) == 2u) &&
		(Stats.ConnectionsOpened == 2u) &&
		(Stats.ConnectionsReused == 2u) &&
		(Stats.ConnectionsClosed == 0u) &&
		(Stats.PoolWaits == 2u),
		"HTTP pool blocked-head reuse contract mismatch"
	);
	testHttpPoolStop(&State);
}



/* 单清扫 Timer 必须移除过期空闲连接并释放全部配额。 */
static void testHttpPoolIdleTimeout(void)
{
	test_http_pool State;
	xhttpclientpoolconfig Pool;
	xhttpclientstats Stats;
	xnetaddr Address;

	xrtHttpClientPoolConfigInit(&Pool);
	Pool.MaxConnections = 1;
	Pool.MaxConnectionsPerOrigin = 1;
	Pool.MaxIdle = 1;
	Pool.MaxIdlePerOrigin = 1;
	Pool.IdleTimeout = 30000u;
	Address = testHttpPoolStart(&State, &Pool);
	testHttpPoolCall(
		&State,
		0,
		Address,
		"expire",
		TEST_HTTP_POOL_SUCCESS
	);
	testHttpPoolWaitValue(
		&State.Calls[0].Completed,
		1,
		"HTTP pool expiring call did not complete"
	);
	testHttpPoolWaitValue(
		&State.ServerClosed,
		1,
		"HTTP pool idle timeout did not close transport"
	);
	Stats = testHttpPoolWaitStats(
		State.Client,
		0,
		0,
		0,
		0,
		"HTTP pool idle timeout retained pool state"
	);
	testRequire(
		(Stats.RequestsStarted == 1u) &&
		(Stats.RequestsCompleted == 1u) &&
		(Stats.ConnectionsOpened == 1u) &&
		(Stats.ConnectionsReused == 0u) &&
		(Stats.ConnectionsClosed == 1u) &&
		(xrtHttpClientCloseIdle(State.Client) == 0u),
		"HTTP pool idle timeout statistics mismatch"
	);
	testHttpPoolStop(&State);
}



/*
	空闲连接收到多余字节或 FIN 后必须立即淘汰，
	下一次请求只能建立一条全新连接。
*/
static void testHttpPoolRetireStale(bool bHalfClose)
{
	test_http_pool State;
	xhttpclientpoolconfig Pool;
	xhttpclientstats Stats;
	xnetaddr Address;

	xrtHttpClientPoolConfigInit(&Pool);
	Pool.MaxConnections = 1;
	Pool.MaxConnectionsPerOrigin = 1;
	Pool.MaxIdle = 1;
	Pool.MaxIdlePerOrigin = 1;
	Pool.IdleTimeout = 0;
	Address = testHttpPoolStart(&State, &Pool);
	State.AllowReconnect = true;
	testHttpPoolCall(
		&State,
		0,
		Address,
		"first",
		TEST_HTTP_POOL_SUCCESS
	);
	testHttpPoolWaitValue(
		&State.Calls[0].Completed,
		1,
		"HTTP pool stale first call did not complete"
	);
	(void)testHttpPoolWaitStats(
		State.Client,
		0,
		1,
		0,
		0,
		"HTTP pool stale transport did not become idle"
	);

	/* 模拟对端在空闲期提前发送下一响应，或关闭发送方向。 */
	if ( bHalfClose ) {
		testRequire(
			xrtNetStreamShutdownWrite(
				State.Servers[0]
			),
			"HTTP pool stale peer half-close failed"
		);
	} else {
		testHttpPoolRespond(State.Servers[0]);
	}
	testHttpPoolWaitValue(
		&State.ServerClosed,
		1,
		"HTTP pool did not retire stale transport"
	);
	Stats = testHttpPoolWaitStats(
		State.Client,
		0,
		0,
		0,
		0,
		"HTTP pool stale transport retained quota"
	);
	testRequire(
		(Stats.RequestsStarted == 1u) &&
		(Stats.RequestsCompleted == 1u) &&
		(Stats.ConnectionsOpened == 1u) &&
		(Stats.ConnectionsReused == 0u) &&
		(Stats.ConnectionsClosed == 1u),
		"HTTP pool stale retirement statistics mismatch"
	);

	/* 配额释放后，第二次调用必须建立并使用第二代连接。 */
	testHttpPoolCall(
		&State,
		1,
		Address,
		"second",
		TEST_HTTP_POOL_SUCCESS
	);
	testHttpPoolWaitValue(
		&State.Calls[1].Completed,
		1,
		"HTTP pool stale replacement call did not complete"
	);
	testHttpPoolWaitValue(
		&State.Requests,
		2,
		"HTTP pool stale replacement request did not arrive"
	);
	Stats = testHttpPoolWaitStats(
		State.Client,
		0,
		1,
		0,
		0,
		"HTTP pool replacement transport did not become idle"
	);
	testRequire(
		(xrtAtomic32Load(
			&State.Accepted,
			XMEMORY_ACQUIRE
		 ) == 2u) &&
		(Stats.RequestsStarted == 2u) &&
		(Stats.RequestsCompleted == 2u) &&
		(Stats.ConnectionsOpened == 2u) &&
		(Stats.ConnectionsReused == 0u) &&
		(Stats.ConnectionsClosed == 1u),
		"HTTP pool stale replacement statistics mismatch"
	);
	testHttpPoolStop(&State);
}



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT) && \
	defined(TEST_HTTP_POOL_REDIRECT_ONLY)

/* 同源重定向的下一跳必须直接接管刚归还连接池的传输。 */
static void testHttpPoolRedirectReuse(void)
{
	test_http_pool State;
	xhttpclientpoolconfig Pool;
	xhttpclientstats Stats;
	xnetaddr Address;

	xrtHttpClientPoolConfigInit(&Pool);
	Pool.MaxConnections = 1;
	Pool.MaxConnectionsPerOrigin = 1;
	Pool.MaxIdle = 1;
	Pool.MaxIdlePerOrigin = 1;
	Pool.IdleTimeout = 0;
	Address = testHttpPoolStart(&State, &Pool);
	State.RedirectFirst = true;
	State.Calls[0].ExpectedRedirects = 1;
	testHttpPoolCall(
		&State,
		0,
		Address,
		"start",
		TEST_HTTP_POOL_SUCCESS
	);
	testHttpPoolWaitValue(
		&State.Calls[0].Completed,
		1,
		"HTTP pool redirect call did not complete"
	);
	testHttpPoolWaitValue(
		&State.Requests,
		2,
		"HTTP pool redirect did not issue two requests"
	);
	Stats = testHttpPoolWaitStats(
		State.Client,
		0,
		1,
		0,
		0,
		"HTTP pool redirect transport did not become idle"
	);
	testRequire(
		(xrtAtomic32Load(
			&State.Accepted,
			XMEMORY_ACQUIRE
		 ) == 1u) &&
		(Stats.RequestsStarted == 1u) &&
		(Stats.RequestsCompleted == 1u) &&
		(Stats.ConnectionsOpened == 1u) &&
		(Stats.ConnectionsReused == 1u) &&
		(Stats.RedirectsFollowed == 1u),
		"HTTP pool redirect reuse statistics mismatch"
	);
	testRequire(
		xrtHttpClientCloseIdle(State.Client) == 1u,
		"HTTP pool redirect connection did not close"
	);
	testHttpPoolWaitValue(
		&State.ServerClosed,
		1,
		"HTTP pool redirect server did not close"
	);
	testHttpPoolStop(&State);
}

#endif



/* 重复覆盖关闭回调竞态，再验证空闲 Timer 的独立回收路径。 */
int main(void)
{
	#if defined(TEST_HTTP_POOL_REDIRECT_ONLY)
		testHttpPoolRedirectReuse();
		printf(
			#if defined(TEST_HTTP_POOL_REDIRECT_DECOMPRESS)
				"[PASS] HTTP decompression redirect pool reuse (%s)\n",
			#else
			"[PASS] HTTP redirect pool reuse (%s)\n",
			#endif
			TEST_HTTP_POOL_BACKEND_NAME
		);
		return 0;
	#endif

	testHttpPoolCancelledAcquire();
	for ( size_t i = 0;
		i < TEST_HTTP_POOL_LIFECYCLE_ROUNDS;
		i++ ) {
		testHttpPoolReuse();
		testHttpPoolWaitAndCancel();
	}
	testHttpPoolGlobalFairness();
	testHttpPoolBlockedHead();
	testHttpPoolIdleTimeout();
	testHttpPoolRetireStale(false);
	testHttpPoolRetireStale(true);
	printf(
		"[PASS] HTTP client connection pool (%s)\n",
		TEST_HTTP_POOL_BACKEND_NAME
	);
	return 0;
}


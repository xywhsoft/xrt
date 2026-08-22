#include "../test.h"



typedef enum test_http_sse_terminal_scenario {
	TEST_HTTP_SSE_REJECT_MIME = 0,
	TEST_HTTP_SSE_REJECT_STATUS,
	TEST_HTTP_SSE_CALLBACK,
	TEST_HTTP_SSE_CLOSE_MESSAGE,
	TEST_HTTP_SSE_PARSE,
	TEST_HTTP_SSE_RECONNECT_LIMIT,
	TEST_HTTP_SSE_CANCEL_TIMER
} test_http_sse_terminal_scenario;



/* 终态夹具为每个场景建立一条完全本地的独立 HTTP 连接。 */
typedef struct test_http_sse_terminal {
	xnetengine* Engine;
	xnetlistener* Listener;
	xnetstream* Stream;
	xhttpclient* Http;
	xhttpsseclient* Client;
	xcancel* Cancel;
	xatomic32 Accepted;
	xatomic32 Responded;
	xatomic32 StreamClosed;
	xatomic32 ListenerClosed;
	xatomic32 Opened;
	xatomic32 Messages;
	xatomic32 Retrying;
	xatomic32 Closed;
	test_http_sse_terminal_scenario Scenario;
	xhttpsseclosereason Reason;
	xhttpsseclienterror Error;
	xerrkind ErrorKind;
	bool HasError;
	bool HasCause;
} test_http_sse_terminal;



/* 在固定截止时间前等待一个终态计数。 */
static void testHttpSseTerminalWait(
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



/* 为测试主机返回本机 IPv4，不触碰外部解析器。 */
static xnetaddrlist* testHttpSseTerminalLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)pData;
	testRequire(
		strcmp(sHost, "terminal.test") == 0,
		"SSE terminal resolved an unexpected host"
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
		"SSE terminal loopback address failed"
	);
	return xrtNetAddrListCreate(&Address, 1);
}



/* 判断客户端请求 Header 是否已经完整到达。 */
static size_t testHttpSseTerminalHeader(
	const char* sData,
	size_t iSize
)
{
	for ( size_t i = 3; i < iSize; i++ ) {
		if ( (sData[i - 3u] == '\r') &&
			(sData[i - 2u] == '\n') &&
			(sData[i - 1u] == '\r') &&
			(sData[i] == '\n') ) {
			return i + 1u;
		}
	}
	return 0;
}



/* 按场景返回固定响应，不复用任何生产服务或外部服务器。 */
static xbytesview testHttpSseTerminalResponse(
	test_http_sse_terminal_scenario Scenario
)
{
	static const char RejectMime[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: 0\r\n"
		"Connection: close\r\n\r\n";
	static const char RejectStatus[] =
		"HTTP/1.1 503 Unavailable\r\n"
		"Content-Type: text/event-stream\r\n"
		"Content-Length: 0\r\n"
		"Connection: close\r\n\r\n";
	static const char Callback[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/event-stream\r\n"
		"Content-Length: 9\r\n"
		"Connection: close\r\n\r\n"
		"data: x\n\n";
	static const char CloseMessage[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/event-stream\r\n"
		"Content-Length: 18\r\n"
		"Connection: close\r\n\r\n"
		"data: x\n\n"
		"data: y\n\n";
	static const uint8 Parse[] = {
		'H', 'T', 'T', 'P', '/', '1', '.', '1', ' ',
		'2', '0', '0', ' ', 'O', 'K', '\r', '\n',
		'C', 'o', 'n', 't', 'e', 'n', 't', '-', 'T',
		'y', 'p', 'e', ':', ' ', 't', 'e', 'x', 't', '/',
		'e', 'v', 'e', 'n', 't', '-', 's', 't', 'r', 'e',
		'a', 'm', '\r', '\n',
		'C', 'o', 'n', 't', 'e', 'n', 't', '-', 'L', 'e',
		'n', 'g', 't', 'h', ':', ' ', '1', '2', '\r', '\n',
		'C', 'o', 'n', 'n', 'e', 'c', 't', 'i', 'o', 'n',
		':', ' ', 'c', 'l', 'o', 's', 'e', '\r', '\n',
		'\r', '\n',
		'd', 'a', 't', 'a', ':', ' ',
		0xF0u, 0x28u, 0x8Cu, 0x28u, '\n', '\n'
	};
	static const char Empty[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/event-stream\r\n"
		"Content-Length: 0\r\n"
		"Connection: close\r\n\r\n";

	if ( Scenario == TEST_HTTP_SSE_REJECT_MIME ) {
		return (xbytesview){
			(cbytes)RejectMime,
			sizeof(RejectMime) - 1u
		};
	}
	if ( Scenario == TEST_HTTP_SSE_REJECT_STATUS ) {
		return (xbytesview){
			(cbytes)RejectStatus,
			sizeof(RejectStatus) - 1u
		};
	}
	if ( Scenario == TEST_HTTP_SSE_CALLBACK ) {
		return (xbytesview){
			(cbytes)Callback,
			sizeof(Callback) - 1u
		};
	}
	if ( Scenario == TEST_HTTP_SSE_CLOSE_MESSAGE ) {
		return (xbytesview){
			(cbytes)CloseMessage,
			sizeof(CloseMessage) - 1u
		};
	}
	if ( Scenario == TEST_HTTP_SSE_PARSE ) {
		return (xbytesview){ Parse, sizeof(Parse) };
	}
	return (xbytesview){
		(cbytes)Empty,
		sizeof(Empty) - 1u
	};
}



/* 收到完整请求后发送当前场景响应并正常关闭写侧。 */
static void testHttpSseTerminalRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	test_http_sse_terminal* pState =
		(test_http_sse_terminal*)pData;
	char Request[2048];
	size_t iSize = xrtNetBufSize(pBuffer);
	size_t iHeader;
	uint32 iExpected = 0;
	xbytesview Response;

	if ( (iSize == 0) || xrtAtomic32Load(
		&pState->Responded,
		XMEMORY_ACQUIRE
	) ) {
		return;
	}
	testRequire(
		iSize < sizeof(Request),
		"SSE terminal request exceeded fixture capacity"
	);
	testRequire(
		xrtNetBufPeek(
			pBuffer, 0, Request, iSize
		) == iSize,
		"SSE terminal request peek failed"
	);
	iHeader = testHttpSseTerminalHeader(
		Request, iSize
	);
	if ( iHeader == 0 ) {
		return;
	}
	testRequire(
		(iHeader >= 24u) &&
		(memcmp(
			Request,
			"GET /terminal HTTP/1.1\r\n",
			24u
		) == 0),
		"SSE terminal request line mismatch"
	);
	testRequire(
		xrtAtomic32CompareExchange(
			&pState->Responded,
			&iExpected,
			1,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		),
		"SSE terminal emitted a duplicate response"
	);
	testRequire(
		xrtNetBufConsume(
			pBuffer, iHeader
		) == iHeader,
		"SSE terminal request consume failed"
	);
	Response = testHttpSseTerminalResponse(
		pState->Scenario
	);
	testRequire(
		xrtNetStreamSend(
			pStream,
			Response.Data,
			Response.Size
		) == XNET_RESULT_OK,
		"SSE terminal response send failed"
	);
	testRequire(
		xrtNetStreamClose(pStream),
		"SSE terminal response close failed"
	);
}



/* 对端先结束时完成服务端正常关闭。 */
static void testHttpSseTerminalEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	if ( xrtNetStreamState(pStream) != XNET_STREAM_CLOSED ) {
		(void)xrtNetStreamClose(pStream);
	}
}



/* 记录服务端连接已经释放底层传输。 */
static void testHttpSseTerminalStreamClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_sse_terminal* pState =
		(test_http_sse_terminal*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	xrtAtomic32Store(
		&pState->StreamClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 接管当前场景唯一的一条回环服务端连接。 */
static bool testHttpSseTerminalAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	test_http_sse_terminal* pState =
		(test_http_sse_terminal*)pData;
	xnetstreamevents Events;
	uint32 iExpected = 0;

	(void)pListener;
	testRequire(
		xrtAtomic32CompareExchange(
			&pState->Accepted,
			&iExpected,
			1,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		),
		"SSE terminal accepted an unexpected connection"
	);
	pState->Stream = pStream;
	memset(&Events, 0, sizeof(Events));
	Events.Read = testHttpSseTerminalRead;
	Events.End = testHttpSseTerminalEnd;
	Events.Close = testHttpSseTerminalStreamClose;
	testRequire(
		xrtNetStreamSetEvents(
			pStream,
			&Events,
			pState
		),
		"SSE terminal server event takeover failed"
	);
	return true;
}



/* 记录 Listener 已经排空在途 Accept。 */
static void testHttpSseTerminalListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_http_sse_terminal* pState =
		(test_http_sse_terminal*)pData;

	(void)pListener;
	xrtAtomic32Store(
		&pState->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录成功通过响应分类的流。 */
static bool testHttpSseTerminalOpen(
	xhttpsseclient* pClient,
	const xhttpresponse* pResponse,
	ptr pData
)
{
	test_http_sse_terminal* pState =
		(test_http_sse_terminal*)pData;

	(void)pClient;
	testRequire(
		xrtHttpResponseStatus(pResponse) == 200,
		"SSE terminal Open status mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Opened,
		1,
		XMEMORY_ACQ_REL
	);
	return true;
}



/* 回调失败场景发布自定义 cause，其余场景不应收到消息。 */
static bool testHttpSseTerminalMessage(
	xhttpsseclient* pClient,
	const xhttpssemessage* pMessage,
	ptr pData
)
{
	test_http_sse_terminal* pState =
		(test_http_sse_terminal*)pData;
	xerror* pError;

	testRequire(
		((pState->Scenario == TEST_HTTP_SSE_CALLBACK) ||
		 (pState->Scenario == TEST_HTTP_SSE_CLOSE_MESSAGE)) &&
		(pMessage->Data.Size == 1u) &&
		(pMessage->Data.Data[0] == 'x'),
		"SSE terminal received an unexpected message"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Messages,
		1,
		XMEMORY_ACQ_REL
	);
	if ( pState->Scenario == TEST_HTTP_SSE_CLOSE_MESSAGE ) {
		testRequire(
			xrtHttpSseClientClose(pClient),
			"SSE terminal callback close was rejected"
		);
		return true;
	}
	pError = xrtErrorCreate(
		XERR_MEMORY,
		"test.http.sse.callback",
		71,
		"fixture callback failed"
	);
	testRequire(
		pError != NULL,
		"SSE terminal callback error allocation failed"
	);
	xrtSetError(pError);
	xrtErrorFree(pError);
	return false;
}



/* 记录已经安排的长延迟 Timer，主线程随后取消整个会话。 */
static void testHttpSseTerminalRetrying(
	xhttpsseclient* pClient,
	size_t iReconnect,
	uint64 iDelay,
	const xerror* pError,
	ptr pData
)
{
	test_http_sse_terminal* pState =
		(test_http_sse_terminal*)pData;

	(void)pClient;
	testRequire(
		(pState->Scenario ==
		 TEST_HTTP_SSE_CANCEL_TIMER) &&
		(iReconnect == 1u) &&
		(iDelay == 5000u) &&
		(pError == NULL),
		"SSE terminal retry Timer mismatch"
	);
	xrtAtomic32Store(
		&pState->Retrying,
		1,
		XMEMORY_RELEASE
	);
}



/* 复制唯一 Close 的稳定错误分类，不借用终态错误对象。 */
static void testHttpSseTerminalClose(
	xhttpsseclient* pClient,
	xhttpsseclosereason Reason,
	const xerror* pError,
	ptr pData
)
{
	test_http_sse_terminal* pState =
		(test_http_sse_terminal*)pData;

	testRequire(
		(xrtAtomic32Load(
			&pState->Closed,
			XMEMORY_ACQUIRE
		 ) == 0) &&
		(xrtHttpSseClientState(pClient) ==
		 XHTTP_SSE_CLIENT_CLOSED) &&
		(xrtHttpSseClientError(pClient) == pError),
		"SSE terminal Close was duplicated or incomplete"
	);
	pState->Reason = Reason;
	pState->HasError = pError != NULL;
	if ( pError != NULL ) {
		testRequire(
			strcmp(
				xrtErrorDomain(pError),
				"xrt.http.sse.client"
			) == 0,
			"SSE terminal leaked a lower-layer error domain"
		);
		pState->Error =
			(xhttpsseclienterror)xrtErrorCode(pError);
		pState->ErrorKind = xrtErrorKind(pError);
		pState->HasCause = xrtErrorCause(pError) != NULL;
	}
	/* 全部普通字段写完后再发布终态，供主线程 acquire 读取。 */
	xrtAtomic32Store(
		&pState->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 返回每个场景应发布的唯一上层终态分类。 */
static void testHttpSseTerminalExpected(
	test_http_sse_terminal_scenario Scenario,
	xhttpsseclosereason* pReason,
	xhttpsseclienterror* pError,
	xerrkind* pKind,
	bool* pHasError,
	bool* pCause
)
{
	if ( (Scenario == TEST_HTTP_SSE_REJECT_MIME) ||
		(Scenario == TEST_HTTP_SSE_REJECT_STATUS) ) {
		*pReason = XHTTP_SSE_CLOSE_REJECTED;
		*pError = XHTTP_SSE_CLIENT_ERROR_RESPONSE;
		*pKind = XERR_PROTOCOL;
		*pHasError = true;
		*pCause = false;
	} else if ( Scenario == TEST_HTTP_SSE_CALLBACK ) {
		*pReason = XHTTP_SSE_CLOSE_CALLBACK;
		*pError = XHTTP_SSE_CLIENT_ERROR_CALLBACK;
		*pKind = XERR_MEMORY;
		*pHasError = true;
		*pCause = true;
	} else if ( Scenario == TEST_HTTP_SSE_CLOSE_MESSAGE ) {
		*pReason = XHTTP_SSE_CLOSE_USER;
		*pError = 0;
		*pKind = XERR_NONE;
		*pHasError = false;
		*pCause = false;
	} else if ( Scenario == TEST_HTTP_SSE_PARSE ) {
		*pReason = XHTTP_SSE_CLOSE_PARSE;
		*pError = XHTTP_SSE_CLIENT_ERROR_PARSE;
		*pKind = XERR_PROTOCOL;
		*pHasError = true;
		*pCause = true;
	} else if (
		Scenario == TEST_HTTP_SSE_RECONNECT_LIMIT
	) {
		*pReason = XHTTP_SSE_CLOSE_RECONNECT_LIMIT;
		*pError = XHTTP_SSE_CLIENT_ERROR_RECONNECT;
		*pKind = XERR_RANGE;
		*pHasError = true;
		*pCause = false;
	} else {
		*pReason = XHTTP_SSE_CLOSE_CANCELLED;
		*pError = XHTTP_SSE_CLIENT_ERROR_CANCELLED;
		*pKind = XERR_CANCELLED;
		*pHasError = true;
		*pCause = false;
	}
}



/* 运行一个独立终态场景并验证所有资源可以排空。 */
static void testHttpSseTerminalRun(
	test_http_sse_terminal_scenario Scenario
)
{
	test_http_sse_terminal State;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xhttpclientconfig HttpConfig;
	xhttpsseclientconfig Config;
	xhttpsseclientevents Events;
	xhttpsseclientinfo Info;
	xhttpsseclosereason ExpectedReason;
	xhttpsseclienterror ExpectedError;
	xerrkind ExpectedKind;
	bool bExpectedError;
	bool bExpectedCause;
	xnetaddr Address;
	char Url[128];
	int iLength;

	memset(&State, 0, sizeof(State));
	State.Scenario = Scenario;
	xrtAtomic32Init(&State.Accepted, 0);
	xrtAtomic32Init(&State.Responded, 0);
	xrtAtomic32Init(&State.StreamClosed, 0);
	xrtAtomic32Init(&State.ListenerClosed, 0);
	xrtAtomic32Init(&State.Opened, 0);
	xrtAtomic32Init(&State.Messages, 0);
	xrtAtomic32Init(&State.Retrying, 0);
	xrtAtomic32Init(&State.Closed, 0);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"SSE terminal Engine start failed"
	);
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ListenConfig.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"SSE terminal Listener address failed"
	);
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	ListenerEvents.Accept = testHttpSseTerminalAccept;
	ListenerEvents.Close = testHttpSseTerminalListenerClose;
	State.Listener = xrtNetListen(
		State.Engine,
		&ListenConfig,
		&ListenerEvents,
		NULL,
		&State
	);
	testRequire(
		(State.Listener != NULL) &&
		xrtNetListenerLocal(
			State.Listener, &Address
		),
		"SSE terminal Listener start failed"
	);
	xrtHttpClientConfigInit(&HttpConfig);
	HttpConfig.Resolver.Lookup =
		testHttpSseTerminalLookup;
	HttpConfig.Dial.MaxAttempts = 1;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		HttpConfig.Pool.MaxIdle = 0;
	#endif
	State.Http = xrtHttpClientCreate(
		State.Engine, &HttpConfig
	);
	testRequire(
		State.Http != NULL,
		"SSE terminal HTTP runtime creation failed"
	);
	iLength = snprintf(
		Url,
		sizeof(Url),
		"http://terminal.test:%u/terminal",
		(unsigned int)Address.Port
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Url)),
		"SSE terminal URL overflowed"
	);
	xrtHttpSseClientConfigInit(&Config);
	Config.MaxReconnects = 0;
	if ( Scenario == TEST_HTTP_SSE_PARSE ) {
		Config.Parser.Utf8Policy = XUTF_STRICT;
	}
	if ( Scenario == TEST_HTTP_SSE_CANCEL_TIMER ) {
		State.Cancel = xrtCancelCreate();
		testRequire(
			State.Cancel != NULL,
			"SSE terminal cancel token creation failed"
		);
		Config.Http.Cancel = State.Cancel;
		Config.MaxReconnects = SIZE_MAX;
		Config.RetryMin = 5000;
		Config.RetryMax = 5000;
	}
	memset(&Events, 0, sizeof(Events));
	Events.Open = testHttpSseTerminalOpen;
	Events.Message = testHttpSseTerminalMessage;
	Events.Retrying = testHttpSseTerminalRetrying;
	Events.Close = testHttpSseTerminalClose;
	Events.Data = &State;
	State.Client = xrtHttpSseConnect(
		State.Http,
		(xstrview){ Url, (size_t)iLength },
		&Config,
		&Events
	);
	testRequire(
		State.Client != NULL,
		"SSE terminal connection submission failed"
	);
	if ( Scenario == TEST_HTTP_SSE_CANCEL_TIMER ) {
		testHttpSseTerminalWait(
			&State.Retrying,
			1,
			"SSE terminal reconnect Timer was not scheduled"
		);
		testRequire(
			xrtCancelRequest(State.Cancel),
			"SSE terminal cancellation was rejected"
		);
	}
	testHttpSseTerminalWait(
		&State.Closed,
		1,
		"SSE terminal Close was not published"
	);
	testHttpSseTerminalExpected(
		Scenario,
		&ExpectedReason,
		&ExpectedError,
		&ExpectedKind,
		&bExpectedError,
		&bExpectedCause
	);
	testRequire(
		xrtHttpSseClientInfo(
			State.Client, &Info
		) &&
		(State.Reason == ExpectedReason) &&
		(State.HasError == bExpectedError) &&
		(!bExpectedError ||
		 ((State.Error == ExpectedError) &&
		  (State.ErrorKind == ExpectedKind))) &&
		(State.HasCause == bExpectedCause) &&
		(Info.State == XHTTP_SSE_CLIENT_CLOSED) &&
		(Info.Reconnects ==
		 (Scenario == TEST_HTTP_SSE_CANCEL_TIMER ? 1u : 0u)) &&
		(xrtAtomic32Load(
			&State.Opened,
			XMEMORY_ACQUIRE
		) == ((Scenario >= TEST_HTTP_SSE_CALLBACK) ? 1u : 0u)) &&
		(xrtAtomic32Load(
			&State.Messages,
			XMEMORY_ACQUIRE
		) == ((Scenario == TEST_HTTP_SSE_CALLBACK) ||
		      (Scenario == TEST_HTTP_SSE_CLOSE_MESSAGE) ? 1u : 0u)),
		"SSE terminal classification mismatch"
	);
	testHttpSseTerminalWait(
		&State.StreamClosed,
		1,
		"SSE terminal server stream did not close"
	);
	testRequire(
		xrtNetListenerClose(State.Listener),
		"SSE terminal Listener close failed"
	);
	testHttpSseTerminalWait(
		&State.ListenerClosed,
		1,
		"SSE terminal Listener did not close"
	);
	xrtHttpSseClientDestroy(State.Client);
	xrtCancelDestroy(State.Cancel);
	xrtHttpClientDestroy(State.Http);
	xrtNetStreamDestroy(State.Stream);
	xrtNetListenerDestroy(State.Listener);
	testRequire(
		xrtNetEngineDestroy(State.Engine),
		"SSE terminal Engine destroy failed"
	);
}



/* 覆盖协议、回调、Parser、重连上限和 Timer 取消终态。 */
int main(void)
{
	for ( int i = TEST_HTTP_SSE_REJECT_MIME;
		i <= TEST_HTTP_SSE_CANCEL_TIMER;
		i++ ) {
		testHttpSseTerminalRun(
			(test_http_sse_terminal_scenario)i
		);
	}
	printf("[PASS] HTTP SSE client terminal matrix\n");
	return 0;
}

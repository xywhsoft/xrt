#include "../test.h"



typedef struct test_http_sse_client test_http_sse_client;



/* 每条回环连接保存独立的响应状态，避免多 Worker 共享可变解析位置。 */
typedef struct test_http_sse_peer {
	test_http_sse_client* State;
	xnetstream* Stream;
	uint32 Index;
	bool Responded;
} test_http_sse_peer;



/* SSE 生命周期夹具只跨线程共享原子计数和终态快照。 */
struct test_http_sse_client {
	xnetengine* Engine;
	xnetlistener* Listener;
	xhttpclient* Http;
	xhttpsseclient* Client;
	test_http_sse_peer Peers[3];
	xatomic32 Accepted;
	xatomic32 Requests;
	xatomic32 StreamsClosed;
	xatomic32 ListenerClosed;
	xatomic32 Opened;
	xatomic32 Messages;
	xatomic32 RetryUpdates;
	xatomic32 Retrying;
	xatomic32 Paused;
	xatomic32 Closed;
	xhttpsseclosereason CloseReason;
	bool CloseHadError;
};



/* 在本地测试截止时间前等待原子计数达到目标值。 */
static void testHttpSseClientWait(
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



/* 为固定测试主机返回本机 IPv4，不访问系统 DNS 或外部网络。 */
static xnetaddrlist* testHttpSseClientLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)pData;
	testRequire(
		strcmp(sHost, "sse.test") == 0,
		"SSE client resolved an unexpected host"
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
		"SSE client loopback address failed"
	);
	return xrtNetAddrListCreate(&Address, 1);
}



/* 返回完整 HTTP/1 Header 长度，尚未收全时返回零。 */
static size_t testHttpSseClientHeaderSize(
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



/* 判断完整 Header 是否包含一个精确字段行。 */
static bool testHttpSseClientHeaderLine(
	const char* sRequest,
	size_t iHeader,
	cstr sLine
)
{
	size_t iLength = strlen(sLine);

	if ( iLength > iHeader ) {
		return false;
	}
	for ( size_t i = 0; i <= (iHeader - iLength); i++ ) {
		if ( ((i == 0) || ((i >= 2u) &&
			 ((sRequest[i - 2u] == '\r') &&
			  (sRequest[i - 1u] == '\n')))) &&
			(memcmp(sRequest + i, sLine, iLength) == 0) ) {
			return true;
		}
	}
	return false;
}



/* 服务端消费一次 GET，并按连接次序发送事件流或 204 停止响应。 */
static void testHttpSseClientServerRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	static const char FirstResponse[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/event-stream; charset=utf-8\r\n"
		"Connection: close\r\n"
		"\r\n"
		"retry: 1\n"
		"id: alpha\n"
		"data: A\n"
		"\n"
		"id: beta\n"
		"data: B\n"
		"\n";
	static const char RedirectResponse[] =
		"HTTP/1.1 307 Temporary Redirect\r\n"
		"Location: /moved\r\n"
		"Content-Length: 0\r\n"
		"Connection: close\r\n"
		"\r\n";
	static const char StopResponse[] =
		"HTTP/1.1 204 No Content\r\n"
		"Connection: close\r\n"
		"\r\n";
	test_http_sse_peer* pPeer =
		(test_http_sse_peer*)pData;
	test_http_sse_client* pState = pPeer->State;
	char Request[4096];
	size_t iSize = xrtNetBufSize(pBuffer);
	size_t iHeader;
	const char* sResponse;
	size_t iResponse;

	if ( pPeer->Responded || (iSize == 0) ) {
		return;
	}
	testRequire(
		iSize < sizeof(Request),
		"SSE client request exceeded fixture capacity"
	);
	testRequire(
		xrtNetBufPeek(
			pBuffer,
			0,
			Request,
			iSize
		) == iSize,
		"SSE client request peek failed"
	);
	iHeader = testHttpSseClientHeaderSize(
		Request, iSize
	);
	if ( iHeader == 0 ) {
		return;
	}
	testRequire(
		(iHeader >= 21u) &&
		(memcmp(
			Request,
			pPeer->Index == 0 ?
				"GET /events HTTP/1.1\r\n" :
				"GET /moved HTTP/1.1\r\n",
			pPeer->Index == 0 ? 22u : 21u
		) == 0) &&
		testHttpSseClientHeaderLine(
			Request,
			iHeader,
			"Accept: text/event-stream\r\n"
		),
		"SSE client emitted an invalid EventSource request"
	);
	if ( pPeer->Index == 0 ) {
		testRequire(
			!testHttpSseClientHeaderLine(
				Request,
				iHeader,
				"Last-Event-ID: beta\r\n"
			),
			"SSE client sent Last-Event-ID on the first request"
		);
		sResponse = RedirectResponse;
		iResponse = sizeof(RedirectResponse) - 1u;
	} else if ( pPeer->Index == 1 ) {
		testRequire(
			!testHttpSseClientHeaderLine(
				Request,
				iHeader,
				"Last-Event-ID: beta\r\n"
			),
			"SSE client sent Last-Event-ID before an event"
		);
		sResponse = FirstResponse;
		iResponse = sizeof(FirstResponse) - 1u;
	} else {
		testRequire(
			testHttpSseClientHeaderLine(
				Request,
				iHeader,
				"Last-Event-ID: beta\r\n"
			),
			"SSE client did not persist Last-Event-ID"
		);
		sResponse = StopResponse;
		iResponse = sizeof(StopResponse) - 1u;
	}
	pPeer->Responded = true;
	testRequire(
		xrtNetBufConsume(
			pBuffer, iHeader
		) == iHeader,
		"SSE client request consume failed"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Requests,
		1,
		XMEMORY_ACQ_REL
	);
	testRequire(
		xrtNetStreamSend(
			pStream,
			sResponse,
			iResponse
		) == XNET_RESULT_OK,
		"SSE client response send failed"
	);
	testRequire(
		xrtNetStreamClose(pStream),
		"SSE client response close failed"
	);
}



/* 对端结束时完成仍未启动的正常关闭。 */
static void testHttpSseClientServerEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	if ( xrtNetStreamState(pStream) != XNET_STREAM_CLOSED ) {
		(void)xrtNetStreamClose(pStream);
	}
}



/* 记录一条回环服务端连接已经释放传输。 */
static void testHttpSseClientServerClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_sse_peer* pPeer =
		(test_http_sse_peer*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pPeer->State->StreamsClosed,
		1,
		XMEMORY_ACQ_REL
	);
}



/* 为每次自动重连接管一条独立服务端连接。 */
static bool testHttpSseClientAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	test_http_sse_client* pState =
		(test_http_sse_client*)pData;
	uint32 iIndex = xrtAtomic32FetchAdd(
		&pState->Accepted,
		1,
		XMEMORY_ACQ_REL
	);
	test_http_sse_peer* pPeer;
	xnetstreamevents Events;

	(void)pListener;
	testRequire(
		iIndex < 3u,
		"SSE client opened an unexpected connection"
	);
	pPeer = &pState->Peers[iIndex];
	pPeer->State = pState;
	pPeer->Stream = pStream;
	pPeer->Index = iIndex;
	memset(&Events, 0, sizeof(Events));
	Events.Read = testHttpSseClientServerRead;
	Events.End = testHttpSseClientServerEnd;
	Events.Close = testHttpSseClientServerClose;
	testRequire(
		xrtNetStreamSetEvents(
			pStream,
			&Events,
			pPeer
		),
		"SSE client server event takeover failed"
	);
	return true;
}



/* 记录 Listener 已经排空全部在途 Accept。 */
static void testHttpSseClientListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	test_http_sse_client* pState =
		(test_http_sse_client*)pData;

	(void)pListener;
	xrtAtomic32Store(
		&pState->ListenerClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证每次 200 EventSource 响应只发布一次 Open。 */
static bool testHttpSseClientOpen(
	xhttpsseclient* pClient,
	const xhttpresponse* pResponse,
	ptr pData
)
{
	test_http_sse_client* pState =
		(test_http_sse_client*)pData;

	(void)pClient;
	testRequire(
		(xrtHttpResponseStatus(pResponse) == 200) &&
		(xrtNetEngineCurrent(pState->Engine) != NULL),
		"SSE client Open callback context mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Opened,
		1,
		XMEMORY_ACQ_REL
	);
	return true;
}



/* 验证消息顺序，并在首条消息回调内施加传输背压。 */
static bool testHttpSseClientMessage(
	xhttpsseclient* pClient,
	const xhttpssemessage* pMessage,
	ptr pData
)
{
	test_http_sse_client* pState =
		(test_http_sse_client*)pData;
	uint32 iIndex = xrtAtomic32FetchAdd(
		&pState->Messages,
		1,
		XMEMORY_ACQ_REL
	);
	char Expected = iIndex == 0 ? 'A' : 'B';

	testRequire(
		(iIndex < 2u) &&
		(pMessage->Data.Size == 1u) &&
		(pMessage->Data.Data[0] == Expected) &&
		(pMessage->Type.Size == 7u) &&
		(memcmp(pMessage->Type.Data, "message", 7u) == 0) &&
		(pMessage->LastEventId.Size ==
		 (iIndex == 0 ? 5u : 4u)) &&
		(memcmp(
			pMessage->LastEventId.Data,
			iIndex == 0 ? "alpha" : "beta",
			pMessage->LastEventId.Size
		) == 0),
		"SSE client message content mismatch"
	);
	if ( iIndex == 0 ) {
		testRequire(
			xrtHttpSseClientPause(pClient),
			"SSE client callback pause was rejected"
		);
		xrtAtomic32Store(
			&pState->Paused,
			1,
			XMEMORY_RELEASE
		);
	}
	return true;
}



/* 验证服务端毫秒 retry 更新先于第一次断线生效。 */
static bool testHttpSseClientRetry(
	xhttpsseclient* pClient,
	uint64 iRetry,
	ptr pData
)
{
	test_http_sse_client* pState =
		(test_http_sse_client*)pData;

	(void)pClient;
	testRequire(
		iRetry == 1u,
		"SSE client retry update mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->RetryUpdates,
		1,
		XMEMORY_ACQ_REL
	);
	return true;
}



/* 验证 EOF 只安排一次有界重连。 */
static void testHttpSseClientRetrying(
	xhttpsseclient* pClient,
	size_t iReconnect,
	uint64 iDelay,
	const xerror* pError,
	ptr pData
)
{
	test_http_sse_client* pState =
		(test_http_sse_client*)pData;

	(void)pClient;
	testRequire(
		(iReconnect == 1u) &&
		(iDelay == 1u) &&
		(pError == NULL),
		"SSE client reconnect callback mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Retrying,
		1,
		XMEMORY_ACQ_REL
	);
}



/* 复制唯一终态，204 必须正常停止且不携带错误。 */
static void testHttpSseClientClose(
	xhttpsseclient* pClient,
	xhttpsseclosereason Reason,
	const xerror* pError,
	ptr pData
)
{
	test_http_sse_client* pState =
		(test_http_sse_client*)pData;

	pState->CloseReason = Reason;
	pState->CloseHadError = pError != NULL;
	testRequire(
		(xrtHttpSseClientState(pClient) ==
		 XHTTP_SSE_CLIENT_CLOSED) &&
		(xrtHttpSseClientError(pClient) == pError),
		"SSE client terminal publication mismatch"
	);
	xrtAtomic32Store(
		&pState->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 覆盖暂停尾段、跨线程恢复、EOF 重连、Last-Event-ID 和 204 停止。 */
static void testHttpSseClientLifecycle(void)
{
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xhttpsseclientinfo) + 2u];
	} InfoStorage;
	test_http_sse_client State;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetlistenerevents ListenerEvents;
	xhttpclientconfig HttpConfig;
	xhttpsseclientconfig Config;
	xhttpsseclientevents Events;
	xhttpsseclientinfo Info;
	xhttpsseclientinfo UnalignedInfo;
	xhttpsseclientinfo* pUnalignedInfo =
		(xhttpsseclientinfo*)(void*)(InfoStorage.Bytes + 1u);
	xnetaddr Address;
	char Url[128];
	int iLength;

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.Accepted, 0);
	xrtAtomic32Init(&State.Requests, 0);
	xrtAtomic32Init(&State.StreamsClosed, 0);
	xrtAtomic32Init(&State.ListenerClosed, 0);
	xrtAtomic32Init(&State.Opened, 0);
	xrtAtomic32Init(&State.Messages, 0);
	xrtAtomic32Init(&State.RetryUpdates, 0);
	xrtAtomic32Init(&State.Retrying, 0);
	xrtAtomic32Init(&State.Paused, 0);
	xrtAtomic32Init(&State.Closed, 0);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"SSE client Engine start failed"
	);
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ListenConfig.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"SSE client listener address failed"
	);
	ListenConfig.AcceptConcurrency = 4;
	memset(&ListenerEvents, 0, sizeof(ListenerEvents));
	ListenerEvents.Accept = testHttpSseClientAccept;
	ListenerEvents.Close = testHttpSseClientListenerClose;
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
		"SSE client Listener start failed"
	);
	xrtHttpClientConfigInit(&HttpConfig);
	HttpConfig.Resolver.Lookup =
		testHttpSseClientLookup;
	HttpConfig.Dial.FallbackDelay = 1000u;
	HttpConfig.Dial.MaxAttempts = 1;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		HttpConfig.Pool.MaxIdle = 0;
	#endif
	State.Http = xrtHttpClientCreate(
		State.Engine, &HttpConfig
	);
	testRequire(
		State.Http != NULL,
		"SSE client HTTP runtime creation failed"
	);
	iLength = snprintf(
		Url,
		sizeof(Url),
		"http://sse.test:%u/events",
		(unsigned int)Address.Port
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Url)),
		"SSE client URL overflowed"
	);
	xrtHttpSseClientConfigInit(&Config);
	Config.RetryMin = 1;
	Config.RetryMax = 1;
	memset(&Events, 0, sizeof(Events));
	Events.Open = testHttpSseClientOpen;
	Events.Message = testHttpSseClientMessage;
	Events.Retry = testHttpSseClientRetry;
	Events.Retrying = testHttpSseClientRetrying;
	Events.Close = testHttpSseClientClose;
	Events.Data = &State;
	State.Client = xrtHttpSseConnect(
		State.Http,
		(xstrview){ Url, (size_t)iLength },
		&Config,
		&Events
	);
	testRequire(
		State.Client != NULL,
		"SSE client connection submission failed"
	);
	testHttpSseClientWait(
		&State.Paused,
		1,
		"SSE client did not pause on the first message"
	);
	xrtSleep(20);
	testRequire(
		xrtHttpSseClientPaused(State.Client) &&
		(xrtAtomic32Load(
			&State.Messages,
			XMEMORY_ACQUIRE
		) == 1u) &&
		(xrtAtomic32Load(
			&State.Closed,
			XMEMORY_ACQUIRE
		) == 0),
		"SSE client delivered through the pause gate"
	);
	testRequire(
		xrtHttpSseClientResume(State.Client) &&
		!xrtHttpSseClientResume(State.Client),
		"SSE client cross-thread resume coalescing failed"
	);
	testHttpSseClientWait(
		&State.Closed,
		1,
		"SSE client did not reach terminal state"
	);
	testRequire(
		xrtHttpSseClientInfo(
			State.Client, &Info
		) &&
		(Info.State == XHTTP_SSE_CLIENT_CLOSED) &&
		(Info.Status == 204) &&
		(Info.Retry == 1u) &&
		(Info.Messages == 2u) &&
		(Info.Comments == 0) &&
		(Info.RetryUpdates == 1u) &&
		(Info.Reconnects == 1u) &&
		!Info.Paused &&
		(State.CloseReason == XHTTP_SSE_CLOSE_STOP) &&
		!State.CloseHadError &&
		(xrtAtomic32Load(
			&State.Opened,
			XMEMORY_ACQUIRE
		) == 1u) &&
		(xrtAtomic32Load(
			&State.RetryUpdates,
			XMEMORY_ACQUIRE
		) == 1u) &&
		(xrtAtomic32Load(
			&State.Retrying,
			XMEMORY_ACQUIRE
		) == 1u) &&
		(xrtAtomic32Load(
			&State.Requests,
			XMEMORY_ACQUIRE
		) == 3u),
		"SSE client lifecycle snapshot mismatch"
	);
	memset(InfoStorage.Bytes, 0xA5, sizeof(InfoStorage.Bytes));
	testRequire(
		xrtHttpSseClientInfo(
			State.Client, pUnalignedInfo
		),
		"SSE client rejected an unaligned info output"
	);
	memcpy(&UnalignedInfo, pUnalignedInfo, sizeof(UnalignedInfo));
	testRequire(
		(memcmp(&UnalignedInfo, &Info, sizeof(Info)) == 0) &&
		(InfoStorage.Bytes[0] == 0xA5u) &&
		(InfoStorage.Bytes[sizeof(InfoStorage.Bytes) - 1u] == 0xA5u),
		"SSE client unaligned info snapshot mismatch"
	);
	testRequire(
		!xrtHttpSseClientInfo(
			State.Client,
			(xhttpsseclientinfo*)(void*)State.Client
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SSE client accepted an info output overlapping the session"
	);
	xrtClearError();
	testRequire(
		!xrtHttpSseClientInfo(
			State.Client,
			(xhttpsseclientinfo*)(uintptr_t)(UINTPTR_MAX - 1u)
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"SSE client accepted a wrapping info output"
	);
	xrtClearError();
	testHttpSseClientWait(
		&State.StreamsClosed,
		3,
		"SSE client server streams did not close"
	);
	testRequire(
		xrtNetListenerClose(State.Listener),
		"SSE client Listener close failed"
	);
	testHttpSseClientWait(
		&State.ListenerClosed,
		1,
		"SSE client Listener did not close"
	);
	xrtHttpSseClientDestroy(State.Client);
	xrtHttpClientDestroy(State.Http);
	for ( size_t i = 0; i < 3u; i++ ) {
		xrtNetStreamDestroy(State.Peers[i].Stream);
	}
	xrtNetListenerDestroy(State.Listener);
	testRequire(
		xrtNetEngineDestroy(State.Engine),
		"SSE client Engine destroy failed"
	);
}



/* 运行完全本地的 EventSource 生命周期回归。 */
int main(void)
{
	testHttpSseClientLifecycle();
	printf(
		"[PASS] HTTP SSE client lifecycle (select)\n"
	);
	return 0;
}
